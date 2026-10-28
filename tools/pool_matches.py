#!/usr/bin/env python3
"""Pool fastchess pentanomial results into one Elo figure with a correct 95% error bar.

A long SPRT or fixed-games match is run here in chunks - a few hundred pairs at a time, often on more than
one runner - and the chunks then have to be added up. That pooling was re-implemented in a throwaway script
four times in one day (2026-10-28), and one of those four had a wrong error-bar formula that printed every
bar 1.959964x too large; it was caught only because a second implementation disagreed. This is the tool so
there is no fifth: it lives in the tree, test/pool_matches_test.py asserts it against real measured matches,
and `make check` runs that.

    python3 tools/pool_matches.py 160,482,732,497,129
    python3 tools/pool_matches.py 49,119,191,125,16 48,125,186,115,26      # two chunks, pooled
    python3 tools/pool_matches.py --log ~/sprt/logs/run-a.log --log ~/sprt/logs/run-b.log
    python3 tools/pool_matches.py --elos=-7.64,-10.08,-4.17,+5.56 --bars-95=15.8,15.8,15.8,15.8

A `Ptnml(0-2)` vector is fastchess's count of PAIRS by the pair's score: a pair is the same opening played
both ways round, so its score is one of 0, 0.25, 0.5, 0.75, 1 and the five counts are how many pairs scored
each. Pairing is why the pentanomial bar is the right one - the two games of a pair share an opening, so
they are not independent and a bar computed per game is too narrow.

THE MATHS, in one place, because four scattered copies is what this file replaces:

    p   = sum(score * count) / N                     the count-weighted mean pair score, N = PAIRS
    se  = sqrt((E[score^2] - p^2) / N)               the ONE-SIGMA standard error of p
    Elo = -400 * log10(1/p - 1)
    the 95% interval is Elo evaluated at p -+ 1.959964 * se, and the bar is its half-width
    LOS = Phi((p - 0.5) / se)                        the probability the candidate is the stronger engine

TWO TRAPS, both of which cost real work on 2026-10-28:

 1. **fastchess's `+-` is a 95% bar, not one sigma.** Comparing a chunk-to-chunk scatter (a standard
    deviation, one sigma) straight against it understates the dispersion by a factor of about two: a scatter
    ratio of 0.44 was read as "2.3x under-dispersed, something is suppressing variance" when the same
    numbers converted to one sigma came out at an entirely ordinary 0.86. So every bar this tool prints says
    `95%` or `1 sigma` on the same line, and scatter_from_95_bars divides by Z_95 before it compares anything.
 2. **Double-applying the 1.959964.** Taking an already-95% se and multiplying by 1.959964 again inflates
    every bar by that factor, silently, in a figure that still looks plausible. The one place `Z_95` is
    applied to a standard error here is `analyse`, where the two interval ends are taken, and the test
    asserts the bars to two decimal places against measured matches, so re-applying it anywhere else turns
    those assertions red.
"""

from __future__ import annotations

import argparse
import math
import re
import sys
from collections.abc import Iterable, Sequence
from dataclasses import dataclass

# A pair's score, by its `Ptnml(0-2)` slot: both games lost, a loss and a draw, ... , both games won.
PAIR_SCORES = (0.0, 0.25, 0.5, 0.75, 1.0)

# The two-sided 95% normal deviate. fastchess's `+-` is THIS many standard errors, not one, and nothing in
# this file multiplies by it twice: see trap 2 in the module docstring.
Z_95 = 1.959964

# `Ptnml(0-2): [160, 482, 732, 497, 129]` as fastchess prints it, and the same five numbers however else a
# log has been mangled on the way (brackets dropped, commas turned to spaces).
PTNML_PATTERN = re.compile(r"Ptnml\(0-2\)\s*[:=]?\s*\[?\s*(\d+)[,\s]+(\d+)[,\s]+(\d+)[,\s]+(\d+)[,\s]+(\d+)")


@dataclass(frozen=True)
class MatchResult:
    """One pentanomial result - a chunk, or the pool of several - and everything derived from it."""

    label: str
    counts: tuple[int, int, int, int, int]
    pairs: int
    score_fraction: float  #: p, the mean pair score (0.5 is level)
    standard_error: float  #: the ONE-SIGMA standard error of p
    elo: float
    elo_low_95: float  #: Elo at p - Z_95 * se
    elo_high_95: float  #: Elo at p + Z_95 * se
    elo_bar_95: float  #: half-width of that interval - what fastchess prints as `+-`
    elo_sigma_1: float  #: the same bar at one sigma: elo_bar_95 / Z_95, for comparing against a scatter
    los: float  #: probability the candidate is stronger, as a fraction

    @property
    def games(self) -> int:
        """Games played: a pair is two games."""
        return 2 * self.pairs


@dataclass(frozen=True)
class Scatter:
    """How the chunks' own Elos are spread, against how far apart their error bars say they should be.

    Every figure here is ONE SIGMA. The bars that come in are 95% bars (fastchess's, or this tool's
    `elo_bar_95`) and scatter_from_95_bars divides them by Z_95 first - trap 1 in the module docstring.
    """

    chunk_elos: tuple[float, ...]
    chunk_sigmas_1: tuple[float, ...]  #: each chunk's bar at one sigma
    mean_elo: float  #: the unweighted mean the scatter is measured about
    observed_sd: float  #: sample sd of the chunk Elos about that mean (n-1 degrees of freedom)
    nominal_sigma_1: float  #: rms of the chunks' own one-sigma bars - the sd expected if all is well
    ratio: float  #: observed_sd / nominal_sigma_1; 1.0 is ordinary, both sides one sigma
    chi_square: float
    degrees_of_freedom: int
    chi_square_p: float  #: upper-tail probability of that chi-square - how surprising the spread is


def elo_of(score_fraction: float) -> float:
    """Elo of a score fraction. @param score_fraction p in (0, 1). @return the logistic Elo difference."""
    if not 0.0 < score_fraction < 1.0:
        return math.inf if score_fraction >= 1.0 else -math.inf
    return -400.0 * math.log10(1.0 / score_fraction - 1.0)


def normal_cdf(z: float) -> float:
    """Phi(z), the standard normal cumulative distribution. @param z the deviate. @return the probability."""
    return 0.5 * (1.0 + math.erf(z / math.sqrt(2.0)))


def _gamma_q(a: float, x: float) -> float:
    """Regularised upper incomplete gamma Q(a, x) = 1 - P(a, x), to machine precision.

    Numerical Recipes' two branches: the series for P below the crossover, Lentz's continued fraction for Q
    above it. Here only to put a p-value beside the chi-square - a dispersion number with no p-value beside
    it is the kind of figure that gets eyeballed into a wrong conclusion, which is how trap 1 happened.
    """
    if x <= 0.0:
        return 1.0
    log_prefactor = -x + a * math.log(x) - math.lgamma(a)
    if x < a + 1.0:
        term = 1.0 / a
        total = term
        for n in range(1, 1000):
            term *= x / (a + n)
            total += term
            if abs(term) < abs(total) * 1e-16:
                break
        return 1.0 - total * math.exp(log_prefactor)
    tiny = 1e-300
    b = x + 1.0 - a
    c = 1.0 / tiny
    d = 1.0 / b
    h = d
    for i in range(1, 1000):
        an = -i * (i - a)
        b += 2.0
        d = an * d + b
        if abs(d) < tiny:
            d = tiny
        c = b + an / c
        if abs(c) < tiny:
            c = tiny
        d = 1.0 / d
        delta = d * c
        h *= delta
        if abs(delta - 1.0) < 1e-16:
            break
    return h * math.exp(log_prefactor)


def chi_square_p_value(chi_square: float, degrees_of_freedom: int) -> float:
    """Upper-tail probability of a chi-square. @param chi_square the statistic. @param degrees_of_freedom df.
    @return P(chi-square_df >= chi_square), or 1.0 when there are no degrees of freedom to spend."""
    if degrees_of_freedom <= 0:
        return 1.0
    return _gamma_q(0.5 * degrees_of_freedom, 0.5 * chi_square)


def analyse(counts: Iterable[int], label: str = "") -> MatchResult:
    """Everything derivable from one `Ptnml(0-2)` vector.

    @param counts five pair counts, in `Ptnml(0-2)` order.
    @param label what to call this chunk in the report.
    @return the result, with its one-sigma standard error and its 95% Elo interval.
    """
    vector = validated_counts(counts)
    pairs = sum(vector)
    score_sum = sum(score * count for score, count in zip(PAIR_SCORES, vector))
    square_sum = sum(score * score * count for score, count in zip(PAIR_SCORES, vector))
    score_fraction = score_sum / pairs
    # The one-sigma standard error of p: the pair-score variance over the number of PAIRS. Pairs, not games:
    # the two games of a pair share an opening and are not independent.
    variance = square_sum / pairs - score_fraction * score_fraction
    standard_error = math.sqrt(max(variance, 0.0) / pairs)
    elo = elo_of(score_fraction)
    # The ONE place Z_95 is applied to a standard error in this file.
    elo_low = elo_of(score_fraction - Z_95 * standard_error)
    elo_high = elo_of(score_fraction + Z_95 * standard_error)
    bar_95 = 0.5 * (elo_high - elo_low)
    los = normal_cdf((score_fraction - 0.5) / standard_error) if standard_error > 0.0 else float(score_fraction > 0.5)
    return MatchResult(
        label=label,
        counts=vector,
        pairs=pairs,
        score_fraction=score_fraction,
        standard_error=standard_error,
        elo=elo,
        elo_low_95=elo_low,
        elo_high_95=elo_high,
        elo_bar_95=bar_95,
        elo_sigma_1=bar_95 / Z_95,
        los=los,
    )


def validated_counts(counts: Iterable[int]) -> tuple[int, int, int, int, int]:
    """Five non-negative integers summing to at least one pair. @param counts the candidate vector.
    @return it as a tuple. Raises ValueError on anything else, rather than computing a plausible wrong Elo."""
    vector = tuple(counts)
    if len(vector) != len(PAIR_SCORES):
        raise ValueError(f"a Ptnml(0-2) vector has {len(PAIR_SCORES)} counts, got {len(vector)}: {vector}")
    if any(int(count) != count or count < 0 for count in vector):
        raise ValueError(f"pair counts must be non-negative integers, got {vector}")
    if sum(vector) <= 0:
        raise ValueError("no pairs at all: every count is zero")
    return (int(vector[0]), int(vector[1]), int(vector[2]), int(vector[3]), int(vector[4]))


def scatter_from_95_bars(chunk_elos: Iterable[float], chunk_bars_95: Iterable[float]) -> Scatter:
    """Chunk-to-chunk dispersion, from Elos and their 95% bars.

    The bars come in as 95% bars - fastchess's `+-`, or this tool's `elo_bar_95` - and are divided by Z_95
    HERE, once, before anything is compared. Skipping that conversion is trap 1 in the module docstring: it
    makes an ordinary spread look like a factor-of-two anomaly in whichever direction the reader expects.

    @param chunk_elos each chunk's Elo. @param chunk_bars_95 each chunk's 95% bar, same order.
    @return the scatter diagnostics, every figure one sigma.
    """
    elos = tuple(float(value) for value in chunk_elos)
    bars = tuple(float(value) for value in chunk_bars_95)
    if len(elos) != len(bars):
        raise ValueError(f"{len(elos)} chunk Elos but {len(bars)} bars")
    if len(elos) < 2:
        raise ValueError("scatter needs at least two chunks")
    if any(bar <= 0.0 for bar in bars):
        raise ValueError(f"every 95% bar must be positive, got {bars}")
    sigmas = tuple(bar / Z_95 for bar in bars)
    mean_elo = sum(elos) / len(elos)
    squared_deviations = [(elo - mean_elo) ** 2 for elo in elos]
    degrees_of_freedom = len(elos) - 1
    observed_sd = math.sqrt(sum(squared_deviations) / degrees_of_freedom)
    nominal = math.sqrt(sum(sigma * sigma for sigma in sigmas) / len(sigmas))
    chi_square = sum(deviation / (sigma * sigma) for deviation, sigma in zip(squared_deviations, sigmas))
    return Scatter(
        chunk_elos=elos,
        chunk_sigmas_1=sigmas,
        mean_elo=mean_elo,
        observed_sd=observed_sd,
        nominal_sigma_1=nominal,
        ratio=observed_sd / nominal,
        chi_square=chi_square,
        degrees_of_freedom=degrees_of_freedom,
        chi_square_p=chi_square_p_value(chi_square, degrees_of_freedom),
    )


def pool(chunk_counts: Iterable[Sequence[int]]) -> tuple[MatchResult, list[MatchResult], Scatter | None]:
    """Pool several `Ptnml(0-2)` vectors into one result.

    Pooling is the SUM OF THE COUNTS, not an average of the chunks' Elos: the pooled bar then follows from
    the pooled counts by the one formula above, which is what makes a chunk of 100 pairs weigh a fifth of a
    chunk of 500 without any weighting code to get wrong.

    @param chunk_counts one or more five-element vectors.
    @return the pooled result, the per-chunk results, and the scatter (None for a single chunk).
    """
    vectors = [validated_counts(counts) for counts in chunk_counts]
    if not vectors:
        raise ValueError("no chunks given")
    chunks = [analyse(vector, label=f"chunk {index}") for index, vector in enumerate(vectors, start=1)]
    pooled = analyse([sum(column) for column in zip(*vectors)], label="pooled")
    scatter = None
    if len(chunks) >= 2 and all(chunk.elo_bar_95 > 0.0 and math.isfinite(chunk.elo) for chunk in chunks):
        scatter = scatter_from_95_bars([chunk.elo for chunk in chunks], [chunk.elo_bar_95 for chunk in chunks])
    return pooled, chunks, scatter


def parse_vector(text: str) -> tuple[int, int, int, int, int]:
    """One vector from the command line: `160,482,732,497,129`, with or without brackets or spaces."""
    numbers = re.findall(r"-?\d+", text)
    if len(numbers) != len(PAIR_SCORES):
        raise ValueError(f"'{text}' is not five numbers (a Ptnml(0-2) vector)")
    return validated_counts(int(number) for number in numbers)


def vectors_from_log(text: str) -> list[tuple[int, int, int, int, int]]:
    """Every `Ptnml(0-2)` vector in a fastchess log, in the order it was printed.

    fastchess reprints the block as the match proceeds, so a completed run's log holds many of these and the
    LAST is the whole chunk. Returning them all and letting the caller take the last keeps that decision
    visible instead of hiding it in a regex.
    """
    return [validated_counts(int(number) for number in match.groups()) for match in PTNML_PATTERN.finditer(text)]


def format_result(result: MatchResult) -> str:
    """One result as a block of lines. Every bar says which it is - `95%` or `1 sigma`."""
    counts = ", ".join(str(count) for count in result.counts)
    lines = [
        f"{result.label}:",
        f"  Ptnml(0-2)        {counts}   ({result.pairs} pairs, {result.games} games)",
        f"  score             {100.0 * result.score_fraction:.4f} %",
        f"  Elo               {result.elo:+.2f}",
    ]
    if math.isfinite(result.elo_bar_95):
        lines += [
            f"  95% interval      {result.elo_low_95:+.2f} .. {result.elo_high_95:+.2f}",
            f"  95% bar           +-{result.elo_bar_95:.2f}   <- this is what fastchess prints as `+-`",
            f"  1 sigma           +-{result.elo_sigma_1:.2f}   <- the 95% bar / {Z_95}; compare a scatter "
            "against THIS",
        ]
    else:
        # Rather than print `+-nan`. A score of 0% or 100%, or an interval that runs off the end of the Elo
        # scale, has no finite bar; saying so is the point of the tool, and a figure is not invented for it.
        lines.append(
            "  95% bar           none: the score is at or beyond the end of the Elo scale, so the interval "
            "has no finite width"
        )
    lines.append(f"  LOS               {100.0 * result.los:.2f} %   (that the candidate is stronger)")
    return "\n".join(lines)


def format_scatter(scatter: Scatter) -> str:
    """The scatter diagnostics as a block of lines, every figure one sigma and labelled so."""
    elos = ", ".join(f"{elo:+.2f}" for elo in scatter.chunk_elos)
    sigmas = ", ".join(f"{sigma:.2f}" for sigma in scatter.chunk_sigmas_1)
    verdict = "ordinary"
    if scatter.chi_square_p < 0.01:
        verdict = "the chunks disagree by more than their bars allow - look for a changed machine or net"
    elif scatter.chi_square_p > 0.99:
        verdict = "suspiciously little spread - look for chunks that are not independent"
    return "\n".join(
        [
            f"chunk scatter (all one sigma - the 95% bars were divided by {Z_95} first):",
            f"  chunk Elos        {elos}",
            f"  chunk 1 sigma     {sigmas}",
            f"  mean Elo          {scatter.mean_elo:+.2f}   (unweighted, the mean the spread is measured about)",
            f"  observed sd       {scatter.observed_sd:.2f}   (sample sd, {scatter.degrees_of_freedom} df)",
            f"  nominal 1 sigma   {scatter.nominal_sigma_1:.2f}   (rms of the chunks' own 1 sigma bars)",
            f"  observed/nominal  {scatter.ratio:.2f}   (1.0 is ordinary; both sides are 1 sigma)",
            f"  chi-square        {scatter.chi_square:.2f} on {scatter.degrees_of_freedom} df, "
            f"p = {scatter.chi_square_p:.3f}   ({verdict})",
        ]
    )


def main() -> int:
    """Read the vectors asked for, pool them, print the report. @return the process exit status."""
    parser = argparse.ArgumentParser(
        description="Pool fastchess Ptnml(0-2) vectors into one Elo with a correct 95% bar.",
        epilog="A bar printed here says `95%` or `1 sigma`: fastchess's `+-` is a 95% bar, not one sigma.",
    )
    parser.add_argument("vectors", nargs="*", help="a chunk's five pair counts, e.g. 160,482,732,497,129")
    parser.add_argument(
        "--log",
        action="append",
        default=[],
        metavar="FILE",
        help="a fastchess log to take the last Ptnml(0-2) vector from; repeatable, one chunk per file",
    )
    parser.add_argument(
        "--elos",
        metavar="LIST",
        help="scatter only: chunk Elos, comma-separated (with --bars-95, and no vectors). Write it with an "
        "equals sign - --elos=-7.64,-10.08 - because a list starting with a minus sign is otherwise read "
        "as an option",
    )
    parser.add_argument(
        "--bars-95",
        metavar="LIST",
        help="scatter only: each chunk's 95%% bar, comma-separated, in the same order as --elos",
    )
    arguments = parser.parse_args()

    if (arguments.elos is None) != (arguments.bars_95 is None):
        sys.exit("--elos and --bars-95 go together")
    if arguments.elos is not None:
        if arguments.vectors or arguments.log:
            sys.exit("--elos/--bars-95 describe chunks already summarised; give no vectors or logs with them")
        try:
            scatter = scatter_from_95_bars(
                [float(value) for value in arguments.elos.replace(",", " ").split()],
                [float(value) for value in arguments.bars_95.replace(",", " ").split()],
            )
        except ValueError as problem:
            sys.exit(f"pool_matches: {problem}")
        print(format_scatter(scatter))
        return 0

    chunks: list[tuple[int, int, int, int, int]] = []
    try:
        chunks.extend(parse_vector(text) for text in arguments.vectors)
        for path in arguments.log:
            with open(path, encoding="utf-8", errors="replace") as log_file:
                found = vectors_from_log(log_file.read())
            if not found:
                sys.exit(f"{path}: no Ptnml(0-2) line in this log")
            print(f"pool_matches: {path}: {len(found)} Ptnml(0-2) line(s), taking the last: {list(found[-1])}")
            chunks.append(found[-1])
    except (OSError, ValueError) as problem:
        sys.exit(f"pool_matches: {problem}")
    if not chunks:
        sys.exit("pool_matches: nothing to pool - give one or more Ptnml(0-2) vectors, or --log FILE")

    pooled, per_chunk, scatter = pool(chunks)
    if len(per_chunk) > 1:
        for chunk in per_chunk:
            print(format_result(chunk))
            print()
    print(format_result(pooled))
    if scatter is not None:
        print()
        print(format_scatter(scatter))
    return 0


if __name__ == "__main__":
    sys.exit(main())

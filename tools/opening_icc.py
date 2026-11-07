#!/usr/bin/env python3
"""How much a finite opening set widens a pentanomial bar, measured from the games rather than asserted.

`tools/pool_matches.py` is right about what it claims: its bar is the standard error of a mean over
INDEPENDENT pairs. Every pawnstar match here draws its openings from `nnue/openings.epd`, so a match with
more pairs than that file has openings replays some of them, and two pairs that start from the same position
are not independent.

EVERY FIGURE IN THIS FILE WAS MEASURED WHEN `openings.epd` HELD 306 POSITIONS - which it did until
2026-10-30, when the opening book it is built from was replaced (the book it had been built from was TSCP's,
whose public-domain claim turned out to be false) and the set grew to thousands. At 306 a long match replayed
each opening about 13 times over 4000 pairs and about 20 over 6000, and the measured ICC was 0.07-0.13. A
wider set replays each opening proportionately less often, so the correction this tool applies is smaller
than those figures - it is still worth applying, and the tool reads the set's real size out of the PGN rather
than assuming any of this, but a reader should not take "13 replays" as current. Some openings simply suit one net better than the other, and replaying them does not buy new
information about the engines at the rate the formula assumes.

The cost is a design effect, from the intraclass correlation of pair scores grouped by opening:

    one-way random-effects ANOVA on the pair scores, grouped by opening:
      k  = openings drawn, N = pairs, n_i = pairs on opening i
      MSB = SSB / (k - 1),  MSW = SSW / (N - k)
      m_anova     = (N - sum(n_i^2) / N) / (k - 1)      the ANOVA coefficient of the between component
      var_between = (MSB - MSW) / m_anova               E[MSB] = var_within + m_anova * var_between
      ICC         = var_between / (var_between + MSW)
      m_effective = sum(n_i^2) / N                      the cluster size an average PAIR sits in
      Deff        = 1 + (m_effective - 1) * ICC         variance inflation of the grand mean
      corrected standard error = nominal standard error * sqrt(Deff)

and the corrected 95% interval is the Elo curve evaluated at p -+ 1.959964 * that corrected standard error.
`pool_matches.py` supplies p, the nominal standard error and the Elo transform, so there is exactly one copy
of the pentanomial formula in the tree and this tool's `nominal` lines reproduce `pool_matches.py` exactly -
which is also how a mistake here is caught.

FOUR THINGS THIS TOOL IS DELIBERATELY CAREFUL ABOUT, each one a way the first version of it went wrong:

 1. **Unequal group sizes.** The familiar `ICC = (MSB - MSW) / (MSB + (m - 1) * MSW)` and
    `Deff = 1 + (m - 1) * ICC` are the BALANCED-design shortcuts, with m the mean group size. Openings are
    drawn `order=random`, so the group sizes are not equal: the count on one opening is binomial, and over
    4000 pairs on 306 openings their spread is wide enough to matter. Two different averages are needed -
    `m_anova` in the ICC, `m_effective` in the design effect - and both exceed the plain mean, so the
    shortcut UNDERSTATES the inflation. The three agree exactly on a balanced design, which the test pins.
    `design_effect_at_mean_size` is reported beside the right one so the difference is visible rather than a
    silent disagreement with an earlier figure computed the shortcut way.
 2. **Whose side the score is on.** A pair's score is the CANDIDATE's, so the candidate has to be named.
    The engine names in a pawnstar match PGN are `candidate` and `baseline` (`tools/pawnstar-match.sh` sets
    them), and a tool that guesses wrong does not fail - it silently reports the sign-flipped match. So
    `--candidate` names it, the default is `candidate`, and a PGN in which no game names that engine is an
    error that says which names it did find.
 3. **Excluding zero is two-sided.** A corrected interval excludes zero when its LOW end is above zero or
    its HIGH end is below it. Testing only the low end calls every negative result "does not exclude zero".
 4. **What this does NOT measure.** It is a correlation correction and nothing else. It says nothing about
    whether the set's positions are REPRESENTATIVE of the positions the engine will meet - a bias the
    correction cannot see and more games cannot reduce - and nothing about chunk-to-chunk heterogeneity,
    which is `pool_matches.py`'s chunk-scatter block and a separate question (see the note below).

ICC AND CHUNK SCATTER MEASURE DIFFERENT THINGS, and `Deff` is neither a substitute for the scatter
diagnostics nor the other way round - and the two must NOT be multiplied together. Grouping by opening
ACROSS the whole pool, a chunk-level difference - a machine under different load, a drifted binary - lands
in the WITHIN-opening variance and so pushes the ICC DOWN. Conversely the chunk-scatter test is nearly
blind to opening reuse: within one 500-pair chunk an opening recurs only about 1.7 times, so reuse inflates
a single chunk's own variance by a few per cent, while at the same time the chunks all draw from the same
306 openings, which makes them RESEMBLE each other. Those two pull opposite ways and very nearly cancel:
measured over the v20-vs-v12 12+0.12 campaign, the scatter ratio the ICC predicts is **0.975** - slightly
UNDER one. So an observed over-dispersion is evidence of something else (conditions, load, drift), not of
opening reuse, and multiplying a Birge factor by `sqrt(Deff)` double-counts, because both are corrections
to the same pooled variance and the Birge factor's baseline is not ICC-aware. Correct a pooled bar by this
design effect, and treat residual chunk over-dispersion as a separate heterogeneity factor on the
WITHIN-opening component alone.

Reuse bites at the POOLED level, where each opening carries 13 or 20 pairs, and it is the pooled bar that
this tool corrects. As the pairs grow, the corrected variance does not go to zero but towards `ICC / k` of
the pair-score variance - a floor set by the number of OPENINGS, not the number of games. More games on the
same openings in the same conditions cannot buy past it; more openings can.

WHAT THE CORRELATION TURNED OUT TO BE, measured on the v20-vs-v12 campaign on reckless-ci-lin-0 and lin-2,
2026-10-30, because the mechanism decides what the floor means. It is NOT a stable "this opening suits the
candidate" effect: the per-opening pair-score means of the two machines' arms correlate at only r = +0.07
(t = 1.21 on 296 df) over the 298 openings they both drew, where a stable opening effect of the measured
size would show r of about 0.4. It IS replay of the SAME GAME: the engine is near-deterministic at a fixed
budget with its book off, so on one machine a replayed opening often produces a verbatim repeat - 5.4% of
the 8,000 games in the lin-2 arm were exact repeats of another game in it (one line appeared 13 times),
against 0.5% in the lin-0 arm, and the two arms' ICCs (0.140 and 0.061) rank the same way. A verbatim
repeat carries no information at all, which is why the inflation is real; and because the duplicates are
machine-local, the floor is a floor for a run on one machine rather than for the opening set as such. A
permutation null confirms the estimator itself is unbiased: shuffling which opening each pair is attributed
to gives ICC -0.001 +- 0.004, against the measured +0.075 - twenty sigma.

AND THE ICC IS STABLE IN THE PAIR COUNT, WHICH IS WHAT LETS ONE MATCH'S VALUE BE USED FOR ANOTHER'S. Over
the lin-2 arm's own chunks, taken two, four, six and eight at a time - the same machine, the same engines,
the same stream of games - the ICC measures 0.188, 0.144, 0.144 and 0.140 as the pairs per opening go 3.4,
7.0, 10.5 and 13.9. Flat from about seven pairs an opening upward, which is what the model says it should
be: the ICC is a property of the conditions and `m_effective` carries the growth. So the lin-0 arm's 0.061
and the lin-2 arm's 0.140 are a real difference between the two MACHINES, not an artefact of their
different sizes, and a match with no PGN of its own is best corrected with the ICC measured on the machine
it ran on - at that match's own `m_effective`, never at the one the ICC was measured at.

THE WHOLE RECOUNT OF A FIXED-GAMES MATCH lives here, so a match directory needs no analysis script of its own.
This took in `analyze.py` from the Trebuchet 1.0 strength matches of 2026-11-07 and the `opening_icc.py` kept
beside the net-swap games of 2026-10-30, neither of which was in a repository. Beside the correction the report
gives W/D/L and how each game ended, recounted from the `Result` tags and so independent of fastchess's totals;
each chunk's own figures and `pool_matches.py`'s scatter; `--expect-games`, which refuses a partial or malformed
run rather than letting it print a quotable number; and, past 97%, a Clopper-Pearson limit on the win rate.

Two cross-checks sit under the corrected interval, reported rather than substituted:

  * **The balanced-design shortcut**, taken as those two scripts took it: `ICC = (MSB - MSW) / (MSB + (m - 1) MSW)`
    and `Deff = 1 + (m - 1) ICC` at the mean group size, then the nominal half-width times `sqrt(Deff)` either
    side of the Elo. It is here because figures were pre-registered and quoted that way, and a quoted figure has
    to stay checkable; it is not the better estimate (trap 1 above).
  * **The cluster-robust standard error** of the mean, clustering pairs by opening, which assumes no ICC model at
    all: `se^2 = k / (k - 1) * sum_i (sum_j (x_ij - mean))^2 / N^2`. Its half-width is taken from the Elo curve at
    `p -+ 1.959964 * se`, then laid either side of the Elo.

The three disagree in a known way, and on the two matches that prompted this they were close. Measured, 95%:

    Trebuchet vs Pawnstar 2.2.0, 4000 games, +144.30    Deff   interval
        this tool's correction                          1.947  +134.35 .. +154.49
        balanced shortcut, as quoted                    1.895  +134.37 .. +154.24
        cluster-robust                                 (1.900) +134.36 .. +154.25
    runF vs cap2, 4000 games, +9.82
        this tool's correction                          1.798  +1.18 .. +18.47
        balanced shortcut, as quoted                    1.758  +1.27 .. +18.36
        cluster-robust                                 (1.778) +1.22 .. +18.41

The cluster-robust design effect in brackets is its squared ratio to the nominal standard error. It falls between
the other two both times, beside the shortcut on the first match and midway on the second, so these two matches do
not by themselves show the shortcut to be too narrow: its fault is the one the unequal-sizes fixture pins, and here
the sizes vary little, 4 to 15 pairs on a mean of 6.71.

    python3 tools/opening_icc.py chunk-a.pgn chunk-b.pgn ...
    python3 tools/opening_icc.py --candidate trebuchet --expect-games 4000 chunk-*.pgn
    python3 tools/opening_icc.py --candidate candidate --label "12+0.12 pooled" ~/sprt/**/runF-*.pgn
    python3 tools/opening_icc.py --per-file chunk-a.pgn chunk-b.pgn
"""

from __future__ import annotations

import argparse
import math
import re
import sys
from collections import Counter, defaultdict
from collections.abc import Iterable, Sequence
from dataclasses import dataclass
from pathlib import Path

# tools/pool_matches.py holds the one copy of the pentanomial formula and the Elo transform. Importing it by
# the path of this file - rather than relying on the working directory - is what lets the tool be run from
# anywhere, which is how it is actually invoked on the runners.
sys.path.insert(0, str(Path(__file__).resolve().parent))

import pool_matches  # noqa: E402  (the path above is what makes this import work from any directory)

#: `[White "candidate"]` and every other PGN tag pair.
TAG_PATTERN = re.compile(r'^\[(\w+)\s+"(.*)"\]\s*$')

#: A game's score for White. Anything else - `*` above all, an unfinished game - is not a result.
WHITE_SCORES = {"1-0": 1.0, "0-1": 0.0, "1/2-1/2": 0.5}

#: What `tools/pawnstar-match.sh` names the engine under test.
DEFAULT_CANDIDATE_NAME = "candidate"

#: From this far to one side, the logistic Elo and a normal bar stop describing the score.
EXTREME_SCORE_FRACTION = 0.97


@dataclass(frozen=True)
class PairObservation:
    """One colour-reversed pair of games: the opening it started from and the candidate's share of it."""

    source: str  #: the file it was read from
    round_label: str  #: the `Round` tag, which is what makes a pair within one fastchess run
    opening: str  #: the `FEN` tag, verbatim - the grouping key
    score: float  #: the candidate's share of the pair: 0, 0.25, 0.5, 0.75 or 1
    #: `(candidate_is_white, candidate's score)` per game, which the pair score alone cannot give: 0.5 may be two
    #: draws or a win and a loss.
    games: tuple[tuple[bool, float], ...] = ()


@dataclass(frozen=True)
class ParseReport:
    """What the PGNs held, and everything that was dropped on the way, counted rather than ignored."""

    files: tuple[str, ...]
    games_seen: int
    games_missing_tags: int  #: no `Round`, `White`, `Black`, `Result` or `FEN` - not a match game
    rounds_seen: int
    pairs_kept: int
    rounds_not_two_games: int  #: a round with one game, or more than two
    rounds_unfinished: int  #: a round holding a game whose result is not one of the three
    rounds_same_colour: int  #: two games that did not swap colours, so not a colour-reversed pair
    rounds_mixed_opening: int  #: two games of one round that did not share a `FEN`
    engine_names: tuple[str, ...]  #: every name seen as White or Black, for diagnosing `--candidate`
    rounds_without_candidate: int = 0  #: a game whose two sides were not the candidate and one other
    terminations: tuple[tuple[str, int], ...] = ()  #: how many games ended each way, from `Termination`
    engine_identities: tuple[tuple[str, int], ...] = ()  #: `EngineWhiteName` / `EngineBlackName`, per side played
    mean_ply_count: float | None = None  #: over the games that state a `PlyCount`
    first_game_start: str = ""  #: the earliest `GameStartTime`
    last_game_end: str = ""  #: the latest `GameEndTime`

    @property
    def rounds_dropped(self) -> int:
        """Rounds that did not become a pair. @return the count."""
        return (
            self.rounds_not_two_games
            + self.rounds_unfinished
            + self.rounds_same_colour
            + self.rounds_mixed_opening
            + self.rounds_without_candidate
        )


@dataclass(frozen=True)
class IntraclassCorrelation:
    """The one-way ANOVA on pair scores grouped by opening, and the design effect that follows from it."""

    pairs: int  #: N
    openings: int  #: k, the openings actually drawn
    mean_group_size: float  #: N / k
    smallest_group: int
    largest_group: int
    mean_square_between: float
    mean_square_within: float
    degrees_of_freedom_between: int
    degrees_of_freedom_within: int
    anova_coefficient: float  #: m_anova, the coefficient of the between component in E[MSB]
    effective_group_size: float  #: m_effective = sum(n_i^2) / N
    variance_between: float  #: the raw between-opening variance component, which may be negative
    icc_raw: float  #: the estimate as it comes out, negative when openings are no more alike than chance
    icc: float  #: the same, floored at 0 - a negative correlation is not used to NARROW a bar
    design_effect: float  #: 1 + (m_effective - 1) * icc
    icc_at_mean_size: float  #: the balanced-design shortcut's own ICC, floored at 0
    design_effect_at_mean_size: float  #: the balanced-design shortcut, reported only for comparison

    @property
    def bar_inflation(self) -> float:
        """What the nominal bar is multiplied by. @return sqrt(design effect)."""
        return math.sqrt(self.design_effect)

    @property
    def f_statistic(self) -> float:
        """The ANOVA's F for openings. @return MSB / MSW, infinite when nothing varies within an opening."""
        return self.mean_square_between / self.mean_square_within if self.mean_square_within > 0.0 else math.inf

    @property
    def variance_floor_fraction(self) -> float:
        """The share of the pair-score variance the corrected variance tends to as the pairs grow.

        `Deff / N` tends to `icc / k` once `m_effective` is `N / k`, so this is the floor the bar cannot be
        reduced past by playing more games on the same openings IN THE SAME CONDITIONS - the measured source
        of the correlation is verbatim game repeats, which are machine-local, so a different machine
        decorrelates them. @return `icc / openings`.
        """
        return self.icc / self.openings


@dataclass(frozen=True)
class CorrectedInterval:
    """A `pool_matches.MatchResult` with its standard error scaled by the measured design effect."""

    nominal: pool_matches.MatchResult
    correlation: IntraclassCorrelation
    standard_error: float  #: the nominal one-sigma standard error times sqrt(design effect)
    elo_low_95: float
    elo_high_95: float
    elo_bar_95: float
    los: float  #: recomputed on the widened standard error, so it is the LOS the correction implies

    @property
    def excludes_zero(self) -> bool:
        """Whether the corrected interval is wholly on one side of zero - either side. @return that."""
        return self.elo_low_95 > 0.0 or self.elo_high_95 < 0.0

    def excludes(self, elo: float) -> bool:
        """Whether an Elo lies outside the corrected interval. @param elo the value. @return that."""
        return not self.elo_low_95 <= elo <= self.elo_high_95


@dataclass(frozen=True)
class CrossChecks:
    """The two other 95% half-widths, each laid symmetrically either side of the Elo - see the module docstring."""

    balanced_half_width: float  #: the nominal half-width times the shortcut's sqrt(Deff)
    cluster_robust_standard_error: float  #: of p, clustering pairs by opening
    cluster_robust_half_width: float  #: half the Elo curve's width at p -+ 1.959964 * that standard error
    cluster_robust_design_effect: float  #: that standard error's squared ratio to the nominal one


def pair_score_of_round(games: Sequence[tuple[bool, str]]) -> float | None:
    """The candidate's share of one round's two games, or None when the round is not a usable pair.

    @param games `(candidate_is_white, result)` per game of the round, in the order read.
    @return the share in [0, 1] - one of 0, 0.25, 0.5, 0.75, 1 - or None if a result is not a result.
    """
    total = 0.0
    for candidate_is_white, result in games:
        if result not in WHITE_SCORES:
            return None
        white_score = WHITE_SCORES[result]
        total += white_score if candidate_is_white else 1.0 - white_score
    return total / len(games)


def candidate_scores_of_round(games: Sequence[tuple[bool, str]]) -> tuple[tuple[bool, float], ...]:
    """Each game of a round as the candidate's colour and score, for a round whose results are all results.

    @param games `(candidate_is_white, result)` per game. @return `(candidate_is_white, score)` per game.
    """
    return tuple(
        (candidate_is_white, WHITE_SCORES[result] if candidate_is_white else 1.0 - WHITE_SCORES[result])
        for candidate_is_white, result in games
    )


def win_draw_loss(pairs: Iterable[PairObservation], is_white: bool | None = None) -> tuple[int, int, int]:
    """The candidate's wins, draws and losses over the games of some pairs.

    @param pairs the pairs. @param is_white count only the games it played as White (True) or Black (False).
    @return wins, draws, losses.
    """
    scores = [
        score
        for pair in pairs
        for candidate_is_white, score in pair.games
        if is_white is None or candidate_is_white == is_white
    ]
    return scores.count(1.0), scores.count(0.5), scores.count(0.0)


def pentanomial_counts(scores: Iterable[float]) -> tuple[int, int, int, int, int]:
    """Pair scores as a `Ptnml(0-2)` vector, so `pool_matches.analyse` computes the nominal figures.

    @param scores pair scores, each one of 0, 0.25, 0.5, 0.75, 1.
    @return the five counts. Raises ValueError on a score that is not one of the five, rather than rounding
    it into the nearest slot and reporting a plausible wrong Elo.
    """
    counts = [0, 0, 0, 0, 0]
    for score in scores:
        slot = round(score * 4.0)
        if not 0 <= slot <= 4 or abs(score * 4.0 - slot) > 1e-9:
            raise ValueError(f"{score} is not a pair score (one of {pool_matches.PAIR_SCORES})")
        counts[slot] += 1
    return pool_matches.validated_counts(counts)


def measure_intraclass_correlation(groups: Sequence[Sequence[float]]) -> IntraclassCorrelation:
    """One-way random-effects ANOVA on pair scores grouped by opening.

    Written for UNEQUAL group sizes, which is what `order=random` produces: `m_anova` in the ICC and
    `m_effective` in the design effect are different averages of the group sizes, and both collapse to the
    common size on a balanced design.

    @param groups one list of pair scores per opening drawn; empty groups are ignored.
    @return the correlation and its design effect. Raises ValueError without two openings, or without a
    degree of freedom left within them, rather than dividing by zero.
    """
    sizes = [len(group) for group in groups if group]
    used = [group for group in groups if group]
    if len(used) < 2:
        raise ValueError(f"an ICC needs at least two openings with pairs on them, got {len(used)}")
    pairs = sum(sizes)
    openings = len(used)
    if pairs - openings < 1:
        raise ValueError(
            f"{pairs} pairs over {openings} openings leaves no within-opening degrees of freedom: "
            "there is no replication to measure a correlation from"
        )
    grand_mean = sum(sum(group) for group in used) / pairs
    group_means = [sum(group) / len(group) for group in used]
    sum_of_squared_sizes = sum(size * size for size in sizes)

    sum_squares_between = sum(size * (mean - grand_mean) ** 2 for size, mean in zip(sizes, group_means))
    sum_squares_within = sum((value - mean) ** 2 for group, mean in zip(used, group_means) for value in group)
    degrees_between = openings - 1
    degrees_within = pairs - openings
    mean_square_between = sum_squares_between / degrees_between
    mean_square_within = sum_squares_within / degrees_within

    # E[MSB] = var_within + m_anova * var_between, with m_anova this combination of the sizes. On a balanced
    # design it is the common group size, which is where the familiar shortcut comes from.
    anova_coefficient = (pairs - sum_of_squared_sizes / pairs) / degrees_between
    variance_between = (mean_square_between - mean_square_within) / anova_coefficient
    total_variance = variance_between + mean_square_within
    icc_raw = variance_between / total_variance if total_variance != 0.0 else 0.0
    icc = max(0.0, icc_raw)
    mean_group_size = pairs / openings
    # The balanced-design shortcut: its own ICC, not the one above, so that it is the whole of that shortcut.
    shortcut_denominator = mean_square_between + (mean_group_size - 1.0) * mean_square_within
    icc_at_mean_size = (
        max(0.0, (mean_square_between - mean_square_within) / shortcut_denominator) if shortcut_denominator else 0.0
    )

    # The design effect of the grand mean weights each group by its size twice over, so the cluster size
    # that matters is the one an average PAIR sits in - larger than the mean whenever the sizes vary.
    effective_group_size = sum_of_squared_sizes / pairs
    return IntraclassCorrelation(
        pairs=pairs,
        openings=openings,
        mean_group_size=mean_group_size,
        smallest_group=min(sizes),
        largest_group=max(sizes),
        mean_square_between=mean_square_between,
        mean_square_within=mean_square_within,
        degrees_of_freedom_between=degrees_between,
        degrees_of_freedom_within=degrees_within,
        anova_coefficient=anova_coefficient,
        effective_group_size=effective_group_size,
        variance_between=variance_between,
        icc_raw=icc_raw,
        icc=icc,
        design_effect=1.0 + (effective_group_size - 1.0) * icc,
        icc_at_mean_size=icc_at_mean_size,
        design_effect_at_mean_size=1.0 + (mean_group_size - 1.0) * icc_at_mean_size,
    )


def correct_interval(
    nominal: pool_matches.MatchResult, correlation: IntraclassCorrelation
) -> CorrectedInterval:
    """Widen a pooled result's standard error by a measured design effect.

    @param nominal the result `pool_matches.analyse` produced from the same pairs.
    @param correlation the design effect to apply.
    @return the corrected interval, its bar, and the LOS the widened standard error implies.
    """
    standard_error = nominal.standard_error * correlation.bar_inflation
    probability = nominal.score_fraction
    elo_low = pool_matches.elo_of(probability - pool_matches.Z_95 * standard_error)
    elo_high = pool_matches.elo_of(probability + pool_matches.Z_95 * standard_error)
    los = (
        pool_matches.normal_cdf((probability - 0.5) / standard_error)
        if standard_error > 0.0
        else float(probability > 0.5)
    )
    return CorrectedInterval(
        nominal=nominal,
        correlation=correlation,
        standard_error=standard_error,
        elo_low_95=elo_low,
        elo_high_95=elo_high,
        elo_bar_95=0.5 * (elo_high - elo_low),
        los=los,
    )


def cluster_robust_standard_error(groups: Sequence[Sequence[float]]) -> float:
    """The standard error of the grand mean of pair scores, clustered by opening, with no ICC model.

    @param groups one list of pair scores per opening drawn; empty groups are ignored.
    @return `sqrt(k / (k - 1) * sum_i (sum_j (x_ij - mean))^2) / N`. Raises ValueError with under two openings.
    """
    used = [group for group in groups if group]
    if len(used) < 2:
        raise ValueError(f"a cluster-robust standard error needs at least two openings, got {len(used)}")
    pairs = sum(len(group) for group in used)
    grand_mean = sum(sum(group) for group in used) / pairs
    total = sum(sum(value - grand_mean for value in group) ** 2 for group in used)
    return math.sqrt(total * len(used) / (len(used) - 1)) / pairs


def cross_checks(corrected: CorrectedInterval, groups: Sequence[Sequence[float]]) -> CrossChecks:
    """The two other half-widths for a corrected result. @param corrected the result.
    @param groups the pair scores behind it, by opening. @return them."""
    nominal = corrected.nominal
    robust_error = cluster_robust_standard_error(groups)
    robust_low = pool_matches.elo_of(nominal.score_fraction - pool_matches.Z_95 * robust_error)
    robust_high = pool_matches.elo_of(nominal.score_fraction + pool_matches.Z_95 * robust_error)
    return CrossChecks(
        balanced_half_width=nominal.elo_bar_95 * math.sqrt(corrected.correlation.design_effect_at_mean_size),
        cluster_robust_standard_error=robust_error,
        cluster_robust_half_width=0.5 * (robust_high - robust_low),
        cluster_robust_design_effect=(
            (robust_error / nominal.standard_error) ** 2 if nominal.standard_error > 0.0 else math.inf
        ),
    )


def parse_pairs(paths: Sequence[str], candidate_name: str = DEFAULT_CANDIDATE_NAME) -> tuple[list[PairObservation], ParseReport]:
    """Every complete colour-reversed pair in a set of fastchess PGNs.

    A pair is one `Round` within one file - two games, opposite colours, the same `FEN`. Rounds are keyed by
    file as well as by number, because each chunk is its own fastchess run and numbers its rounds from one.

    @param paths the PGN files, read in the order given.
    @param candidate_name the engine whose score a pair's score is.
    @return the pairs and the parse report. Raises ValueError when no game names the candidate, listing the
    names that were there instead - the failure mode that otherwise reports a sign-flipped match in silence.
    """
    rounds: dict[tuple[str, str], dict] = {}
    order: list[tuple[str, str]] = []
    engine_names: set[str] = set()
    games_seen = 0
    games_missing_tags = 0
    terminations: Counter = Counter()
    identities: Counter = Counter()
    ply_counts: list[int] = []
    game_starts: list[str] = []
    game_ends: list[str] = []

    for path in paths:
        with open(path, encoding="utf-8", errors="replace") as handle:
            text = handle.read()
        for block in re.split(r"\n(?=\[Event )", text):
            if "[Event " not in block:
                continue
            games_seen += 1
            tags: dict[str, str] = {}
            for line in block.splitlines():
                match = TAG_PATTERN.match(line)
                if match:
                    tags[match.group(1)] = match.group(2)
            white, black = tags.get("White"), tags.get("Black")
            if white:
                engine_names.add(white)
            if black:
                engine_names.add(black)
            terminations[tags.get("Termination", "(none stated)")] += 1
            identities.update(tags[tag] for tag in ("EngineWhiteName", "EngineBlackName") if tag in tags)
            if tags.get("PlyCount", "").isdigit():
                ply_counts.append(int(tags["PlyCount"]))
            if tags.get("GameStartTime"):
                game_starts.append(tags["GameStartTime"])
            if tags.get("GameEndTime"):
                game_ends.append(tags["GameEndTime"])
            round_label, result, opening = tags.get("Round"), tags.get("Result"), tags.get("FEN")
            if not (round_label and white and black and result and opening):
                games_missing_tags += 1
                continue
            key = (path, round_label)
            if key not in rounds:
                rounds[key] = {"openings": set(), "games": [], "sides": [], "has_candidate_twice_or_never": False}
                order.append(key)
            rounds[key]["openings"].add(opening)
            rounds[key]["games"].append((white == candidate_name, result))
            rounds[key]["sides"].append((white, black))
            if (white == candidate_name) == (black == candidate_name):
                rounds[key]["has_candidate_twice_or_never"] = True

    if candidate_name not in engine_names:
        raise ValueError(
            f"no game names '{candidate_name}' as White or Black, so every pair would be scored from the "
            f"wrong side; the engines in these files are {sorted(engine_names)} - name one with --candidate"
        )

    pairs: list[PairObservation] = []
    not_two_games = unfinished = same_colour = mixed_opening = without_candidate = 0
    for key in order:
        info = rounds[key]
        if len(info["games"]) != 2:
            not_two_games += 1
            continue
        if len(info["openings"]) != 1:
            mixed_opening += 1
            continue
        if info["has_candidate_twice_or_never"]:
            without_candidate += 1
            continue
        first_sides, second_sides = info["sides"]
        if first_sides != second_sides[::-1]:
            same_colour += 1
            continue
        score = pair_score_of_round(info["games"])
        if score is None:
            unfinished += 1
            continue
        pairs.append(
            PairObservation(
                source=key[0],
                round_label=key[1],
                opening=next(iter(info["openings"])),
                score=score,
                games=candidate_scores_of_round(info["games"]),
            )
        )

    report = ParseReport(
        files=tuple(paths),
        games_seen=games_seen,
        games_missing_tags=games_missing_tags,
        rounds_seen=len(order),
        pairs_kept=len(pairs),
        rounds_not_two_games=not_two_games,
        rounds_unfinished=unfinished,
        rounds_same_colour=same_colour,
        rounds_mixed_opening=mixed_opening,
        engine_names=tuple(sorted(engine_names)),
        rounds_without_candidate=without_candidate,
        terminations=tuple(sorted(terminations.items())),
        engine_identities=tuple(sorted(identities.items())),
        mean_ply_count=sum(ply_counts) / len(ply_counts) if ply_counts else None,
        first_game_start=min(game_starts, default=""),
        last_game_end=max(game_ends, default=""),
    )
    return pairs, report


def integrity_problems(report: ParseReport, expected_games: int) -> list[str]:
    """Why a run must not be quoted: a game count that is not the one planned, or anything that was dropped.

    A partial chunk prints a well-formed Elo with an honest bar, so a dead run reads as a finished one unless the
    count it should have reached is stated and checked.

    @param report what was read. @param expected_games the games the run was to play.
    @return one line per problem, empty when the run is whole.
    """
    problems = []
    if report.games_seen != expected_games:
        problems.append(f"{report.games_seen:,} games read, {expected_games:,} expected")
    if report.games_missing_tags:
        problems.append(f"{report.games_missing_tags:,} games without the Round/White/Black/Result/FEN tags")
    for count, what in (
        (report.rounds_not_two_games, "not two games"),
        (report.rounds_unfinished, "holding an unfinished game"),
        (report.rounds_same_colour, "whose colours were not reversed"),
        (report.rounds_mixed_opening, "whose two games differ in FEN"),
        (report.rounds_without_candidate, "with a game the candidate was not one side of"),
    ):
        if count:
            problems.append(f"{count:,} rounds {what}")
    return problems


def group_by_opening(pairs: Iterable[PairObservation]) -> list[list[float]]:
    """Pair scores gathered by the opening they started from. @param pairs the pairs. @return the groups."""
    grouped: dict[str, list[float]] = defaultdict(list)
    for pair in pairs:
        grouped[pair.opening].append(pair.score)
    return list(grouped.values())


def analyse_pairs(pairs: Sequence[PairObservation], label: str = "") -> CorrectedInterval:
    """The whole measurement: nominal figures from `pool_matches`, then the measured correction.

    @param pairs the pairs to pool. @param label what to call the result.
    @return the corrected interval, carrying the nominal result and the correlation inside it.
    """
    groups = group_by_opening(pairs)
    correlation = measure_intraclass_correlation(groups)
    nominal = pool_matches.analyse(pentanomial_counts(pair.score for pair in pairs), label=label or "pooled")
    return correct_interval(nominal, correlation)


def format_extreme_score(wins: int, draws: int, losses: int) -> str | None:
    """What to say about a score too one-sided for its Elo and normal bar to mean much.

    @param wins the candidate's wins. @param draws its draws. @param losses its losses.
    @return the block, or None when the score is not that far to either side. The limit is on the leader's WIN
    rate with every draw treated as a non-win, so it is conservative, and it is what its Elo is read from.
    """
    games = wins + draws + losses
    points = wins + 0.5 * draws
    if EXTREME_SCORE_FRACTION > points / games > 1.0 - EXTREME_SCORE_FRACTION:
        return None
    is_candidate_leading = points / games >= EXTREME_SCORE_FRACTION
    leader_wins = wins if is_candidate_leading else losses
    limit = pool_matches.clopper_pearson_lower_limit(leader_wins, games)
    elo = pool_matches.elo_of(limit)
    lines = [
        f"extreme score: {points}/{games}. The Elo at this score is ill-conditioned and a normal bar does not "
        "describe it.",
        f"  Clopper-Pearson 95% lower limit on the {'candidate' if is_candidate_leading else 'other engine'}'s win "
        f"rate, every draw a non-win: {limit:.5f}, so the candidate is "
        + (f"at least {elo:+.1f} Elo" if is_candidate_leading else f"at most {-elo:+.1f} Elo"),
    ]
    if leader_wins == games:
        lines.append("  Every game went one way: the point estimate is infinite and only the limit is reportable.")
    return "\n".join(lines)


def format_win_draw_loss(pairs: Sequence[PairObservation], report: ParseReport, candidate_name: str) -> str:
    """The recount from the `Result` tags and how the games went. @param pairs the pairs kept.
    @param report what was read. @param candidate_name whose side the count is from. @return the block."""

    def tally(is_white: bool | None = None) -> str:
        wins, draws, losses = win_draw_loss(pairs, is_white)
        return f"+{wins} ={draws} -{losses}"

    lines = [
        f"{candidate_name}, recounted from the Result tags:",
        f"  overall           {tally()}",
        f"  as White          {tally(True)}",
        f"  as Black          {tally(False)}",
        f"  terminations      {', '.join(f'{name} {count:,}' for name, count in report.terminations)}",
    ]
    if report.engine_identities:
        lines.append(f"  identities        {', '.join(f'{name} {count:,}' for name, count in report.engine_identities)}")
    if report.mean_ply_count is not None:
        lines.append(f"  mean PlyCount     {report.mean_ply_count:.1f}")
    if report.first_game_start and report.last_game_end:
        lines.append(f"  played            {report.first_game_start} .. {report.last_game_end}")
    return "\n".join(lines)


def format_chunks(paths: Sequence[str], pairs: Sequence[PairObservation]) -> str:
    """Each file's own figures and, from `pool_matches.py`, how far apart they are. @param paths the files.
    @param pairs the pairs kept. @return the block."""
    lines = ["chunks:"]
    vectors = []
    for path in paths:
        of_file = [pair for pair in pairs if pair.source == path]
        if not of_file:
            lines.append(f"  {Path(path).name}: no complete pair")
            continue
        wins, draws, losses = win_draw_loss(of_file)
        vector = pentanomial_counts(pair.score for pair in of_file)
        vectors.append(vector)
        result = pool_matches.analyse(vector)
        lines.append(
            f"  {Path(path).name}: {result.games:,} games  +{wins} ={draws} -{losses}  Ptnml(0-2) {list(vector)}  "
            f"Elo {result.elo:+.2f} +-{result.elo_bar_95:.2f} (nominal 95%)"
        )
    if len(vectors) >= 2:
        scatter = pool_matches.pool(vectors)[2]
        if scatter is not None:
            lines.append(pool_matches.format_scatter(scatter))
    return "\n".join(lines)


def format_cross_checks(corrected: CorrectedInterval, checks: CrossChecks) -> str:
    """The two other intervals, under the corrected one. @param corrected the result.
    @param checks its cross-checks. @return the block."""
    elo = corrected.nominal.elo
    correlation = corrected.correlation
    return "\n".join(
        [
            "cross-checks, reported rather than substituted, as Elo +- half-width:",
            f"  balanced shortcut ICC {correlation.icc_at_mean_size:.5f}, design effect "
            f"{correlation.design_effect_at_mean_size:.4f}   95% +-{checks.balanced_half_width:.2f}   interval "
            f"{elo - checks.balanced_half_width:+.2f} .. {elo + checks.balanced_half_width:+.2f}",
            f"  cluster-robust    design effect {checks.cluster_robust_design_effect:.4f} (measured, no ICC model)   "
            f"95% +-{checks.cluster_robust_half_width:.2f}   interval "
            f"{elo - checks.cluster_robust_half_width:+.2f} .. {elo + checks.cluster_robust_half_width:+.2f}",
        ]
    )


def format_parse_report(report: ParseReport) -> str:
    """The parse report as a block of lines, every drop counted. @param report it. @return the block."""
    lines = [
        "games read:",
        f"  files             {len(report.files)}",
        f"  games             {report.games_seen:,}",
        f"  rounds            {report.rounds_seen:,}",
        f"  complete pairs    {report.pairs_kept:,}",
        f"  engines named     {', '.join(report.engine_names)}",
    ]
    if report.games_missing_tags:
        lines.append(f"  games without the Round/White/Black/Result/FEN tags: {report.games_missing_tags:,}")
    if report.rounds_dropped:
        lines += [
            f"  rounds dropped    {report.rounds_dropped:,}"
            f"   (not two games {report.rounds_not_two_games:,}, unfinished {report.rounds_unfinished:,}, "
            f"colours not swapped {report.rounds_same_colour:,}, openings differ {report.rounds_mixed_opening:,}, "
            f"candidate not one side {report.rounds_without_candidate:,})"
        ]
    return "\n".join(lines)


def format_correlation(correlation: IntraclassCorrelation) -> str:
    """The ANOVA and the design effect as a block of lines. @param correlation it. @return the block."""
    return "\n".join(
        [
            "opening replay:",
            f"  openings drawn    {correlation.openings:,}",
            f"  pairs per opening {correlation.mean_group_size:.2f} mean"
            f"   (min {correlation.smallest_group}, max {correlation.largest_group})",
            f"  MS between/within {correlation.mean_square_between:.6f} / "
            f"{correlation.mean_square_within:.6f}   (df {correlation.degrees_of_freedom_between:,} / "
            f"{correlation.degrees_of_freedom_within:,}), F {correlation.f_statistic:.3f}"
            + (
                "   (few openings replayed, so this ICC is barely measured)"
                if correlation.mean_group_size < 2.0
                else ""
            ),
            f"  ANOVA coefficient {correlation.anova_coefficient:.4f}   (m_anova, not the mean - the sizes "
            "are unequal)",
            f"  ICC (raw)         {correlation.icc_raw:+.5f}",
            f"  ICC (floored)     {correlation.icc:.5f}   "
            + ("a negative estimate is not used to narrow a bar" if correlation.icc_raw < 0.0 else ""),
            f"  effective size    {correlation.effective_group_size:.4f}   (m_effective, the cluster an "
            "average pair sits in)",
            f"  design effect     {correlation.design_effect:.4f}   -> bar inflation x"
            f"{correlation.bar_inflation:.4f}",
            f"  same at mean size {correlation.design_effect_at_mean_size:.4f}   (the balanced-design "
            "shortcut; the formula understates when the group sizes vary)",
            f"  variance floor    {100.0 * correlation.variance_floor_fraction:.4f} % of the pair-score "
            f"variance   (ICC / {correlation.openings} openings - more games in the same conditions "
            "cannot go below it)",
        ]
    )


def format_corrected(corrected: CorrectedInterval) -> str:
    """The nominal and corrected intervals side by side. @param corrected it. @return the block."""
    nominal = corrected.nominal
    zero = "excludes zero" if corrected.excludes_zero else "CONTAINS ZERO"
    return "\n".join(
        [
            f"{nominal.label}:",
            f"  Ptnml(0-2)        {', '.join(str(count) for count in nominal.counts)}"
            f"   ({nominal.pairs:,} pairs, {nominal.games:,} games)",
            f"  score             {100.0 * nominal.score_fraction:.4f} %",
            f"  Elo               {nominal.elo:+.2f}",
            f"  nominal 1 sigma   {nominal.standard_error:.6f} of p   (pool_matches.py's, unchanged)",
            f"  nominal 95% bar   +-{nominal.elo_bar_95:.2f}   interval {nominal.elo_low_95:+.2f} .. "
            f"{nominal.elo_high_95:+.2f}",
            f"  nominal LOS       {100.0 * nominal.los:.2f} %",
            f"  CORRECTED 1 sigma {corrected.standard_error:.6f} of p   (x"
            f"{corrected.correlation.bar_inflation:.4f})",
            f"  CORRECTED 95% bar +-{corrected.elo_bar_95:.2f}   interval {corrected.elo_low_95:+.2f} .. "
            f"{corrected.elo_high_95:+.2f}   ({zero})",
            f"  CORRECTED LOS     {100.0 * corrected.los:.2f} %   (that the candidate is stronger)",
        ]
    )


def main() -> int:
    """Read the PGNs asked for, measure the correlation, print the report. @return the exit status."""
    parser = argparse.ArgumentParser(
        description="Correct a pooled pentanomial bar for the correlation a replayed opening set induces.",
        epilog="The nominal figures come from tools/pool_matches.py and reproduce it exactly; this tool "
        "only widens them by a design effect measured from the games.",
    )
    parser.add_argument("pgn", nargs="+", help="fastchess PGN files, pooled together unless --per-file")
    parser.add_argument(
        "--candidate",
        default=DEFAULT_CANDIDATE_NAME,
        metavar="NAME",
        help=f"the engine a pair's score belongs to, as the PGN names it (default: {DEFAULT_CANDIDATE_NAME})",
    )
    parser.add_argument("--label", default="pooled", help="what to call the pooled result in the report")
    parser.add_argument(
        "--expect-games",
        type=int,
        metavar="GAMES",
        help="the games the run was to play: any other count, or any round that is not a whole colour-reversed "
        "pair, prints REFUSED and exits non-zero with no figures",
    )
    parser.add_argument(
        "--per-file",
        action="store_true",
        help="also measure each file on its own, before the pool (a short chunk replays an opening barely "
        "more than once, so its own ICC is not measurable - the correction belongs to the pool)",
    )
    parser.add_argument(
        "--compare-elo",
        type=float,
        metavar="ELO",
        help="also report whether the corrected interval excludes this Elo, e.g. the other arm's estimate",
    )
    arguments = parser.parse_args()

    try:
        pairs, report = parse_pairs(arguments.pgn, arguments.candidate)
    except (OSError, ValueError) as problem:
        sys.exit(f"opening_icc: {problem}")
    if arguments.expect_games is not None:
        problems = integrity_problems(report, arguments.expect_games)
        if problems:
            print("REFUSED: these PGNs are not the whole run:")
            for problem in problems:
                print(f"  {problem}")
            return 1
    if not pairs:
        sys.exit("opening_icc: no complete colour-reversed pair in these files")

    print(format_parse_report(report))
    print()
    print(format_win_draw_loss(pairs, report, arguments.candidate))
    print()
    if len(arguments.pgn) > 1:
        print(format_chunks(arguments.pgn, pairs))
        print()
    extreme = format_extreme_score(*win_draw_loss(pairs))
    if extreme is not None:
        print(extreme)
        print()

    if arguments.per_file:
        for path in arguments.pgn:
            of_file = [pair for pair in pairs if pair.source == path]
            if not of_file:
                print(f"{path}: no complete pair")
                print()
                continue
            try:
                corrected = analyse_pairs(of_file, label=path)
            except ValueError as problem:
                print(f"{path}: {problem}")
                print()
                continue
            print(format_correlation(corrected.correlation))
            print(format_corrected(corrected))
            print()

    try:
        pooled = analyse_pairs(pairs, label=arguments.label)
    except ValueError as problem:
        # No replication is an answer rather than a failure: the nominal bar stands, and says so.
        print(f"opening replay: not measurable - {problem}; the nominal bar stands")
        counts = pentanomial_counts(pair.score for pair in pairs)
        print(pool_matches.format_result(pool_matches.analyse(counts, label=arguments.label)))
        return 0
    print(format_correlation(pooled.correlation))
    print(format_corrected(pooled))
    print(format_cross_checks(pooled, cross_checks(pooled, group_by_opening(pairs))))
    if arguments.compare_elo is not None:
        verdict = "EXCLUDES" if pooled.excludes(arguments.compare_elo) else "contains"
        print(f"  vs {arguments.compare_elo:+.2f} Elo    the corrected interval {verdict} it")
    return 0


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""tools/pool_matches.py against real measured matches, and against the two traps that cost a day.

Registered as the ctest `pool_matches`, so `make check` runs it. Reports one `[PASS]`/`[FAIL]` line per
check and a summary line, the same shape as test/test_report.h's C++ suites, and exits non-zero on any
failure.

THE FIXTURES ARE MEASURED, NOT CONSTRUCTED. Every `Ptnml(0-2)` vector below came out of a completed
fastchess match on these runners on 2026-10-28, and each expected Elo and bar was reproduced by two
independent implementations before it was written down. That is the whole point: a pooling formula checked
only against its own output is the defect this file exists to prevent (see tools/pool_matches.py's
docstring, and the `print_net_header` story in tools/net_header_report.h - a decoder kept outside the tree
read its fields from the wrong offset for days).

WHAT THE ASSERTIONS CATCH, deliberately:

  * the bars, to two decimal places. Applying the 95% deviate twice - the bug that was actually written on
    2026-10-28, in a scratchpad script - inflates every one of them by 1.959964x, so each `95% bar` check
    below goes red at once (verified: injecting it turned exactly those three red and left the Elos and the
    pair counts green, which is the signature to recognise);
  * the 95%-to-one-sigma conversion in the scatter diagnostics. Dropping it makes a nominal sigma 1.959964x
    too large and the dispersion ratio that much too small - an ordinary 0.85 reads as 0.43, which is how a
    perfectly normal spread got called "2.3x under-dispersed" that day. `nominal 1 sigma` and
    `observed/nominal` below both go red without it (verified);
  * that pooling sums the counts rather than averaging the chunks' Elos, which is what makes a short chunk
    weigh less than a long one;
  * the input validation, because a tool that returns a plausible number for a malformed vector is worse
    than one that stops.

ONE FIGURE IN THE BRIEF THIS WAS WRITTEN FROM DID NOT RECONCILE, and the value here is the computed one.
The second match below ([208, 455, 704, 493, 140]) was handed over with an expected 95% bar of +-8.41; it
is +-8.23. Its own Elo (-8.51) and LOS (2.13%) were handed over too and both reproduce exactly, and those
two between them pin the statistic: LOS = Phi((p - 0.5) / se) with p = 0.48775 and LOS = 2.128% forces
se = 0.006040, and that se gives +-8.23 by the interval formula AND by the independent logistic-derivative
route asserted below. A bar of +-8.41 would need se = 0.006170, which would put LOS at 2.37%. So 8.41
cannot belong to this vector; 8.23 is asserted. (The third row's "pairs 2000" was 2000 GAMES - 1000 pairs,
500 per chunk - which the pair-count assertions record.)
"""

from __future__ import annotations

import math
import sys
from pathlib import Path

# No `tools/__pycache__` left behind: a ctest run must not write into the source tree, and the one import
# below is not worth a cache.
sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "tools"))

import pool_matches  # noqa: E402  (the path above is what makes this import work from any directory)

failure_count = 0


def check(has_passed: bool, summary: str) -> bool:
    """Report one result line - `[PASS] <summary>` or `[FAIL] <summary>` - and tally a failure.

    @param has_passed whether the check passed. @param summary what was checked. @return has_passed.
    """
    global failure_count
    print(f"{'[PASS]' if has_passed else '[FAIL]'} {summary}")
    if not has_passed:
        failure_count += 1
    return has_passed


def check_close(actual: float, expected: float, places: int, summary: str) -> bool:
    """A number to a fixed number of decimal places, with both values in the line when it fails.

    @param actual what the tool produced. @param expected the measured value. @param places decimal places.
    @param summary what was checked. @return whether it matched.
    """
    rounded_actual = round(actual, places)
    has_passed = rounded_actual == round(expected, places)
    detail = summary if has_passed else f"{summary} - got {actual:.6f} (to {places} dp {rounded_actual})"
    return check(has_passed, detail)


def check_raises(call, summary: str) -> bool:
    """A call that must raise ValueError. @param call the thunk. @param summary what was checked."""
    try:
        call()
    except ValueError:
        return check(True, summary)
    return check(False, f"{summary} - no ValueError was raised")


def test_single_chunk_matches() -> None:
    """The two completed 2000-pair matches, each one vector, each figure measured."""
    first = pool_matches.analyse([160, 482, 732, 497, 129])
    check(first.pairs == 2000 and first.games == 4000, "match A: 2000 pairs, 4000 games")
    check_close(100.0 * first.score_fraction, 49.4125, 4, "match A: score 49.4125 %")
    check_close(first.elo, -4.08, 2, "match A: Elo -4.08")
    check_close(first.elo_bar_95, 7.87, 2, "match A: 95% bar +-7.87")
    check_close(100.0 * first.los, 15.45, 2, "match A: LOS 15.45 %")

    second = pool_matches.analyse([208, 455, 704, 493, 140])
    check(second.pairs == 2000 and second.games == 4000, "match B: 2000 pairs, 4000 games")
    check_close(second.elo, -8.51, 2, "match B: Elo -8.51")
    # +-8.23, not the +-8.41 this fixture arrived with: see the module docstring. LOS, below, is what pins it.
    check_close(second.elo_bar_95, 8.23, 2, "match B: 95% bar +-8.23")
    check_close(100.0 * second.los, 2.13, 2, "match B: LOS 2.13 %")


def test_pooling_two_chunks() -> None:
    """The multi-chunk path: two 500-pair chunks of one 2000-game match, pooled."""
    pooled, chunks, scatter = pool_matches.pool([[49, 119, 191, 125, 16], [48, 125, 186, 115, 26]])
    check(pooled.counts == (97, 244, 377, 240, 42), "pooled: the counts are summed slot by slot")
    check(pooled.pairs == 1000 and pooled.games == 2000, "pooled: 1000 pairs, 2000 games (500 pairs a chunk)")
    check_close(pooled.elo, -19.83, 2, "pooled: Elo -19.83")
    check_close(pooled.elo_bar_95, 10.95, 2, "pooled: 95% bar +-10.95")
    check(len(chunks) == 2 and scatter is not None, "pooled: both chunks reported, with scatter")
    check(
        all(chunk.pairs == 500 for chunk in chunks),
        "pooled: each chunk keeps its own 500 pairs",
    )
    # The pooled bar is narrower than either chunk's: twice the pairs, and no chunk's bar is reused.
    check(
        pooled.elo_bar_95 < min(chunk.elo_bar_95 for chunk in chunks),
        "pooled: the pooled 95% bar is narrower than either chunk's",
    )


def test_pooling_weighs_by_pairs() -> None:
    """Pooling sums counts; it does not average the chunks' Elos. A short chunk must weigh less."""
    long_chunk = [160, 482, 732, 497, 129]  # 2000 pairs, Elo -4.08
    short_chunk = [8, 0, 0, 0, 0]  # 8 pairs, every one lost: Elo -infinity on its own
    pooled, chunks, _ = pool_matches.pool([long_chunk, short_chunk])
    mean_of_chunk_elos = sum(chunk.elo for chunk in chunks) / len(chunks)
    check(
        math.isinf(chunks[1].elo) and not math.isfinite(mean_of_chunk_elos),
        "weighting: a chunk scoring 0 % has an infinite Elo, so averaging chunk Elos is not pooling",
    )
    check(
        math.isfinite(pooled.elo) and pooled.elo < chunks[0].elo,
        "weighting: the pooled Elo stays finite and moves a little towards the short chunk",
    )
    check(
        pooled.counts == (168, 482, 732, 497, 129) and pooled.pairs == 2008,
        "weighting: the pooled vector is the slot-by-slot sum (2008 pairs)",
    )
    # And it says there is no bar rather than printing `+-nan`, which is the same discipline as the rest of
    # this file: a figure that cannot be computed is not invented.
    short_report = pool_matches.format_result(chunks[1])
    check(
        "none: the score is at or beyond the end of the Elo scale" in short_report
        and "nan" not in short_report,
        "weighting: a chunk with no finite bar says so instead of printing a nan",
    )


def test_scatter_diagnostics() -> None:
    """Four chunk Elos with their 95% bars - the case the 95%-to-one-sigma conversion was got wrong on."""
    scatter = pool_matches.scatter_from_95_bars([-7.64, -10.08, -4.17, 5.56], [15.8, 15.8, 15.8, 15.8])
    check_close(scatter.observed_sd, 6.87, 2, "scatter: observed sd 6.87 (sample sd about the mean)")
    check_close(scatter.nominal_sigma_1, 8.06, 2, "scatter: nominal 1 sigma 8.06 (= 15.8 / 1.959964)")
    check_close(scatter.ratio, 0.85, 2, "scatter: observed/nominal 0.85, both sides one sigma")
    check_close(scatter.chi_square, 2.18, 2, "scatter: chi-square 2.18")
    check(scatter.degrees_of_freedom == 3, "scatter: 3 degrees of freedom for four chunks")
    check_close(scatter.mean_elo, -4.08, 2, "scatter: the unweighted mean chunk Elo is -4.08")
    # The reading that matters: 0.85 is ordinary. Had the 95% bars been compared un-converted, the ratio
    # would have been 0.43 and read as a factor-of-two anomaly - which is exactly what happened that day.
    check(
        0.2 < scatter.chi_square_p < 0.8,
        f"scatter: p = {scatter.chi_square_p:.3f} - an ordinary spread, not an anomaly",
    )
    check_close(
        scatter.ratio * pool_matches.Z_95,
        1.6704,
        3,
        "scatter: skipping the 95%-to-1-sigma conversion would give 0.43 instead of 0.85",
    )


def test_the_two_bars_are_distinguishable() -> None:
    """Every bar the tool carries is labelled, and the two differ by exactly the 95% deviate."""
    result = pool_matches.analyse([160, 482, 732, 497, 129])
    check_close(
        result.elo_sigma_1 * pool_matches.Z_95,
        result.elo_bar_95,
        9,
        "bars: the 1 sigma figure times 1.959964 is the 95% bar, exactly",
    )
    check_close(result.elo_sigma_1, 4.01, 2, "bars: match A's one-sigma Elo bar is +-4.01, not +-7.87")
    report = pool_matches.format_result(result)
    check(
        "95% bar" in report and "1 sigma" in report and "+-7.87" in report and "+-4.01" in report,
        "bars: the printed report names both bars as 95% and 1 sigma, with both values",
    )
    scatter_report = pool_matches.format_scatter(
        pool_matches.scatter_from_95_bars([-7.64, -10.08, -4.17, 5.56], [15.8] * 4)
    )
    check(
        "one sigma" in scatter_report and "1.959964" in scatter_report,
        "bars: the scatter report says its figures are one sigma and names the divisor",
    )


def test_bar_by_an_independent_route() -> None:
    """The bar again, from the logistic derivative instead of the interval ends - two routes, one answer.

    d(Elo)/dp = 400 / (ln(10) * p * (1 - p)), so the bar is about that times Z_95 * se. The agreement is
    only approximate (the Elo curve is not straight), which is why this asserts 0.2% rather than equality -
    but it is nowhere near 1.959964x, so it catches a doubled deviate on its own.
    """
    for counts in ([160, 482, 732, 497, 129], [208, 455, 704, 493, 140], [97, 244, 377, 240, 42]):
        result = pool_matches.analyse(counts)
        probability = result.score_fraction
        slope = 400.0 / (math.log(10.0) * probability * (1.0 - probability))
        linear_bar = slope * pool_matches.Z_95 * result.standard_error
        check(
            abs(linear_bar - result.elo_bar_95) < 0.002 * result.elo_bar_95,
            f"independent route: {counts} bar +-{result.elo_bar_95:.2f} agrees with the derivative "
            f"({linear_bar:.2f})",
        )


def test_chi_square_tail() -> None:
    """The chi-square p-value, against values from a table."""
    check_close(pool_matches.chi_square_p_value(3.841459, 1), 0.05, 4, "chi-square: 3.841459 on 1 df is p = 0.05")
    check_close(pool_matches.chi_square_p_value(5.991465, 2), 0.05, 4, "chi-square: 5.991465 on 2 df is p = 0.05")
    check_close(pool_matches.chi_square_p_value(11.344867, 3), 0.01, 4, "chi-square: 11.344867 on 3 df is p = 0.01")
    check_close(pool_matches.chi_square_p_value(0.0, 3), 1.0, 9, "chi-square: 0 on 3 df is p = 1")
    check(pool_matches.chi_square_p_value(5.0, 0) == 1.0, "chi-square: no degrees of freedom spends no p-value")


def test_level_and_edges() -> None:
    """A level match, and the scores the Elo formula cannot express."""
    level = pool_matches.analyse([100, 200, 400, 200, 100])
    check_close(level.score_fraction, 0.5, 9, "level: a symmetric vector scores exactly 50 %")
    check_close(level.elo, 0.0, 9, "level: Elo 0")
    check_close(100.0 * level.los, 50.0, 6, "level: LOS 50 %")
    check(math.isinf(pool_matches.elo_of(1.0)) and pool_matches.elo_of(1.0) > 0, "edges: a clean sweep is +inf Elo")
    check(math.isinf(pool_matches.elo_of(0.0)) and pool_matches.elo_of(0.0) < 0, "edges: no score at all is -inf Elo")


def test_input_is_validated() -> None:
    """A malformed vector stops the tool; it does not get a plausible Elo."""
    check_raises(lambda: pool_matches.analyse([1, 2, 3, 4]), "validation: four counts is not a Ptnml(0-2) vector")
    check_raises(lambda: pool_matches.analyse([1, 2, 3, 4, 5, 6]), "validation: six counts is refused")
    check_raises(lambda: pool_matches.analyse([1, -2, 3, 4, 5]), "validation: a negative count is refused")
    check_raises(lambda: pool_matches.analyse([0, 0, 0, 0, 0]), "validation: no pairs at all is refused")
    check_raises(lambda: pool_matches.parse_vector("160,482,732,497"), "validation: a short command-line vector")
    check_raises(
        lambda: pool_matches.scatter_from_95_bars([-7.64, -10.08], [15.8]),
        "validation: as many bars as Elos, or nothing",
    )
    check_raises(
        lambda: pool_matches.scatter_from_95_bars([-7.64], [15.8]),
        "validation: one chunk has no scatter to measure",
    )
    check_raises(
        lambda: pool_matches.scatter_from_95_bars([-7.64, -10.08], [15.8, 0.0]),
        "validation: a zero-width bar would divide by zero",
    )
    pooled, chunks, scatter = pool_matches.pool([[160, 482, 732, 497, 129]])
    check(scatter is None and len(chunks) == 1, "validation: a single chunk pools to itself with no scatter")


def test_vectors_are_read_back() -> None:
    """The two input routes: a vector on the command line, and a fastchess log."""
    expected = (160, 482, 732, 497, 129)
    check(pool_matches.parse_vector("160,482,732,497,129") == expected, "input: a comma-separated vector")
    check(pool_matches.parse_vector("[160, 482, 732, 497, 129]") == expected, "input: brackets and spaces")
    log_text = (
        "Results of candidate vs baseline (8+0.08, 1t, 16MB, openings.epd):\n"
        "Elo: -20.87 +/- 15.24, nElo: -25.11 +/- 18.29\n"
        "Ptnml(0-2): [49, 119, 191, 125, 16]\n"
        "...more games...\n"
        "Ptnml(0-2): [160, 482, 732, 497, 129]\n"
        "Games: 4000 Wins: 1234 Losses: 1270 Draws: 1496 Points: 1982.0 (49.55 %)\n"
    )
    found = pool_matches.vectors_from_log(log_text)
    check(len(found) == 2, "input: every Ptnml(0-2) line in a log is found")
    check(found[-1] == expected, "input: the last Ptnml(0-2) line is the completed chunk")
    check(pool_matches.vectors_from_log("no pentanomial here") == [], "input: a log with no vector yields none")


def main() -> int:
    """Run every case. @return 0 when every check passed, 1 otherwise."""
    test_single_chunk_matches()
    test_pooling_two_chunks()
    test_pooling_weighs_by_pairs()
    test_scatter_diagnostics()
    test_the_two_bars_are_distinguishable()
    test_bar_by_an_independent_route()
    test_chi_square_tail()
    test_level_and_edges()
    test_input_is_validated()
    test_vectors_are_read_back()
    verdict = "[PASS]" if failure_count == 0 else "[FAIL]"
    print(f"{verdict} pool_matches: {failure_count} failed check(s)")
    return 0 if failure_count == 0 else 1


if __name__ == "__main__":
    sys.exit(main())

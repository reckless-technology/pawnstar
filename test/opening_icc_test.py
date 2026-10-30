#!/usr/bin/env python3
"""tools/opening_icc.py against hand-computed ANOVA fixtures and against tools/pool_matches.py.

Registered as the ctest `opening_icc`, so `make check` runs it. One `[PASS]`/`[FAIL]` line per check and a
summary line, the same shape as test/pool_matches_test.py and test/test_report.h's C++ suites, exiting
non-zero on any failure.

THE STATISTICAL FIXTURES ARE HAND-COMPUTED, NOT RECORDED FROM THE TOOL. Each of the four below is small
enough to do the one-way ANOVA by hand, and the expected ICC and design effect in each case are written out
in the docstring of the check that asserts them, with the sums of squares. That is what makes them
independent: a checker asserted against its own output is the defect this file exists to prevent, and this
tree has the scar - `tools/pool_matches.py` was written because a pooling formula lived only in scratchpad
scripts and one of four copies applied the 95% deviate twice, and `tools/net_header_report.h` was written
because an `od` decoder kept outside the tree read `king_buckets=8` as 64 for days.

The four cases are chosen to pin the parts that can be got wrong independently of one another:

  * **perfect correlation** - three openings whose pairs are identical within each opening. MSW is exactly
    zero, so ICC is exactly 1 and the design effect is exactly the cluster size. This is the ceiling: at
    ICC 1 a replayed opening buys nothing at all and 6 pairs on 3 openings are worth 3 pairs.
  * **no correlation at all** - three openings each holding one lost and one won pair. Every opening mean is
    the grand mean, so the between sum of squares is exactly zero and the raw ICC comes out at exactly
    -1.0: pairs sharing an opening are LESS alike than chance. It is floored at 0 and the bar is left alone,
    because a negative estimate must not be used to narrow a bar.
  * **a third of the way** - two openings, ICC exactly 1/3 and the design effect exactly 4/3, which catches
    a sign slip or a swapped mean square that the two extremes above do not.
  * **unequal group sizes** - one opening with a single pair and one with three. This is the case the first
    version of this tool got wrong: the balanced-design shortcut `1 + (mean - 1) * ICC` gives 2.0 where the
    right answer is 2.5, so the shortcut UNDERSTATES the inflation. Both are asserted, and the test also
    pins that the three size averages agree exactly on a balanced design, which is why the shortcut looks
    correct until the sizes vary.

AND THE NOMINAL FIGURES ARE ANCHORED TO THE COMMITTED pool_matches FIXTURE. 2000 pair scores are built that
sum to `[160, 482, 732, 497, 129]` - match A of test/pool_matches_test.py - and the tool's nominal Elo and
nominal 95% bar are asserted at -4.08 and +-7.87, the same two numbers that file pins against a completed
match on these runners on 2026-10-28. So this tool cannot drift away from `pool_matches.py` without one of
these checks going red, which is the whole reason it imports it rather than carrying a second copy of the
pentanomial formula.
"""

from __future__ import annotations

import math
import sys
from pathlib import Path

# No `tools/__pycache__` left behind: a ctest run must not write into the source tree.
sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "tools"))

import opening_icc  # noqa: E402  (the path above is what makes this import work from any directory)
import pool_matches  # noqa: E402

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

    @param actual what the tool produced. @param expected the hand-computed value. @param places places.
    @param summary what was checked. @return whether it matched.
    """
    rounded_actual = round(actual, places)
    has_passed = rounded_actual == round(expected, places)
    detail = summary if has_passed else f"{summary} - got {actual:.9f} (to {places} dp {rounded_actual})"
    return check(has_passed, detail)


def check_raises(call, summary: str) -> bool:
    """A call that must raise ValueError. @param call the thunk. @param summary what was checked."""
    try:
        call()
    except ValueError:
        return check(True, summary)
    return check(False, f"{summary} - no ValueError was raised")


def game_block(round_label: str, white: str, black: str, result: str, fen: str) -> str:
    """One PGN game as fastchess writes it, with the five tags this tool reads.

    @param round_label the `Round` tag. @param white the White engine. @param black the Black engine.
    @param result the `Result` tag. @param fen the `FEN` tag. @return the block, blank line included.
    """
    return (
        '[Event "Fastchess Tournament"]\n'
        f'[Round "{round_label}"]\n'
        f'[White "{white}"]\n'
        f'[Black "{black}"]\n'
        f'[Result "{result}"]\n'
        '[SetUp "1"]\n'
        f'[FEN "{fen}"]\n'
        '[TimeControl "12+0.12"]\n'
        "\n"
        f"1. e4 {{+0.21/12 0.120s}} e5 {{-0.18/12 0.130s}} {result}\n"
        "\n"
    )


OPENING_A = "rnbqkb1r/ppp2ppp/4pn2/3p4/2PP4/2N5/PP2PPPP/R1BQKBNR w KQkq - 0 1"
OPENING_B = "rnbqkbnr/pp1ppppp/8/2p5/4P3/5N2/PPPP1PPP/RNBQKB1R b KQkq - 1 2"
OPENING_C = "r1bqkbnr/pppp1ppp/2n5/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R b KQkq - 5 4"


def fixture_pgn() -> str:
    """A PGN holding three usable pairs and one of each way a round is not a pair.

    Rounds 1 and 2 share opening A (so A carries two pairs), round 3 is on opening B. Round 4 holds a single
    game, round 5 an unfinished one, round 6 two games that never swapped colours, and round 7 two games
    whose `FEN` tags differ. @return the PGN text.
    """
    return "".join(
        [
            # Round 1, opening A: candidate wins both ways round -> pair score 1.0
            game_block("1", "candidate", "baseline", "1-0", OPENING_A),
            game_block("1", "baseline", "candidate", "0-1", OPENING_A),
            # Round 2, opening A: both drawn -> 0.5
            game_block("2", "candidate", "baseline", "1/2-1/2", OPENING_A),
            game_block("2", "baseline", "candidate", "1/2-1/2", OPENING_A),
            # Round 3, opening B: candidate loses both ways round -> 0.0
            game_block("3", "candidate", "baseline", "0-1", OPENING_B),
            game_block("3", "baseline", "candidate", "1-0", OPENING_B),
            # Round 4: one game only
            game_block("4", "candidate", "baseline", "1-0", OPENING_C),
            # Round 5: one game unfinished
            game_block("5", "candidate", "baseline", "1-0", OPENING_C),
            game_block("5", "baseline", "candidate", "*", OPENING_C),
            # Round 6: colours never swapped
            game_block("6", "candidate", "baseline", "1-0", OPENING_C),
            game_block("6", "candidate", "baseline", "1-0", OPENING_C),
            # Round 7: the two games are not on the same opening
            game_block("7", "candidate", "baseline", "1-0", OPENING_B),
            game_block("7", "baseline", "candidate", "1-0", OPENING_C),
        ]
    )


def write_fixture_pgn(directory: Path) -> Path:
    """Write the fixture PGN into a directory. @param directory where. @return the path written."""
    path = directory / "opening_icc_fixture.pgn"
    path.write_text(fixture_pgn(), encoding="utf-8")
    return path


def test_perfect_correlation() -> None:
    """Three openings, identical pairs within each: MSW is 0, so ICC is 1 and the design effect is the size.

    groups [[0, 0], [0.5, 0.5], [1, 1]], grand mean 0.5.
      SSB = 2*(0-.5)^2 + 2*(.5-.5)^2 + 2*(1-.5)^2 = 1.0, df 2 -> MSB = 0.5
      SSW = 0, df 3 -> MSW = 0
      m_anova = (6 - (4+4+4)/6) / 2 = 2,  var_between = (0.5 - 0) / 2 = 0.25
      ICC = 0.25 / (0.25 + 0) = 1,  m_effective = 12/6 = 2,  Deff = 1 + (2-1)*1 = 2
    """
    correlation = opening_icc.measure_intraclass_correlation([[0.0, 0.0], [0.5, 0.5], [1.0, 1.0]])
    check(correlation.pairs == 6 and correlation.openings == 3, "perfect: 6 pairs over 3 openings")
    check_close(correlation.mean_square_between, 0.5, 9, "perfect: MS between 0.5")
    check_close(correlation.mean_square_within, 0.0, 9, "perfect: MS within 0 (no spread within an opening)")
    check_close(correlation.anova_coefficient, 2.0, 9, "perfect: the ANOVA coefficient is the group size, 2")
    check_close(correlation.icc_raw, 1.0, 9, "perfect: ICC 1 exactly")
    check_close(correlation.design_effect, 2.0, 9, "perfect: design effect 2 - two pairs worth one")
    check_close(correlation.bar_inflation, math.sqrt(2.0), 9, "perfect: bar inflation sqrt(2)")
    check_close(
        correlation.design_effect_at_mean_size,
        correlation.design_effect,
        9,
        "perfect: balanced sizes, so the mean-size shortcut agrees exactly",
    )
    check_close(
        correlation.effective_group_size,
        correlation.mean_group_size,
        9,
        "perfect: balanced sizes, so m_effective is the mean group size",
    )


def test_no_correlation_is_floored() -> None:
    """Three openings each holding one 0 and one 1: the between sum of squares is 0 and the raw ICC is -1.

    groups [[0, 1], [0, 1], [0, 1]], every opening mean 0.5 = the grand mean.
      SSB = 0, df 2 -> MSB = 0
      SSW = 3 * ((0-.5)^2 + (1-.5)^2) = 1.5, df 3 -> MSW = 0.5
      m_anova = (6 - (4+4+4)/6) / 2 = 2,  var_between = (0 - 0.5) / 2 = -0.25
      ICC(raw) = -0.25 / (-0.25 + 0.5) = -1 exactly, floored to 0, so Deff = 1 and the bar is untouched.
    """
    correlation = opening_icc.measure_intraclass_correlation([[0.0, 1.0], [0.0, 1.0], [0.0, 1.0]])
    check_close(correlation.mean_square_between, 0.0, 9, "none: MS between 0 (every opening mean is grand)")
    check_close(correlation.mean_square_within, 0.5, 9, "none: MS within 0.5")
    check_close(correlation.icc_raw, -1.0, 9, "none: the raw ICC is -1, which is a real estimate")
    check_close(correlation.icc, 0.0, 9, "none: it is floored at 0")
    check_close(correlation.design_effect, 1.0, 9, "none: design effect 1 - the nominal bar stands")
    check_close(correlation.bar_inflation, 1.0, 9, "none: no inflation")
    check_close(correlation.variance_floor_fraction, 0.0, 9, "none: no variance floor at ICC 0")
    report = opening_icc.format_correlation(correlation)
    check(
        "a negative estimate is not used to narrow a bar" in report,
        "none: the report says why a negative estimate did not narrow the bar",
    )


def test_a_third() -> None:
    """Two openings, ICC exactly 1/3 - the case between the two extremes, which catches a swapped term.

    groups [[0, 0.5], [0.5, 1.0]], means 0.25 and 0.75, grand mean 0.5.
      SSB = 2*(.25-.5)^2 + 2*(.75-.5)^2 = 0.25, df 1 -> MSB = 0.25
      SSW = (0-.25)^2+(.5-.25)^2+(.5-.75)^2+(1-.75)^2 = 0.25, df 2 -> MSW = 0.125
      m_anova = (4 - (4+4)/4) / 1 = 2,  var_between = (0.25 - 0.125) / 2 = 0.0625
      ICC = 0.0625 / (0.0625 + 0.125) = 1/3,  m_effective = 8/4 = 2,  Deff = 1 + 1*(1/3) = 4/3
    """
    correlation = opening_icc.measure_intraclass_correlation([[0.0, 0.5], [0.5, 1.0]])
    check_close(correlation.mean_square_between, 0.25, 9, "third: MS between 0.25")
    check_close(correlation.mean_square_within, 0.125, 9, "third: MS within 0.125")
    check_close(correlation.variance_between, 0.0625, 9, "third: between-opening variance component 0.0625")
    check_close(correlation.icc, 1.0 / 3.0, 9, "third: ICC exactly 1/3")
    check_close(correlation.design_effect, 4.0 / 3.0, 9, "third: design effect exactly 4/3")
    check_close(correlation.variance_floor_fraction, 1.0 / 6.0, 9, "third: variance floor ICC/k = 1/6")


def test_unequal_group_sizes() -> None:
    """One opening with a single pair, one with three: the shortcut gives 2.0 where the answer is 2.5.

    groups [[0.0], [1.0, 1.0, 1.0]], grand mean 0.75.
      SSB = 1*(0-.75)^2 + 3*(1-.75)^2 = 0.5625 + 0.1875 = 0.75, df 1 -> MSB = 0.75
      SSW = 0, df 2 -> MSW = 0
      m_anova = (4 - (1+9)/4) / 1 = 1.5,  var_between = 0.75 / 1.5 = 0.5,  ICC = 0.5 / (0.5 + 0) = 1
      m_effective = (1 + 9) / 4 = 2.5  while the MEAN size is 4/2 = 2.0
      Deff = 1 + (2.5 - 1) * 1 = 2.5, where the balanced shortcut would say 1 + (2 - 1) * 1 = 2.0

    This is the defect the first version of this tool carried: at unequal sizes the mean understates the
    cluster an average pair sits in, so the shortcut understates the inflation - here by 20%, and in the
    direction that makes a bar look narrower than it is.
    """
    correlation = opening_icc.measure_intraclass_correlation([[0.0], [1.0, 1.0, 1.0]])
    check_close(correlation.anova_coefficient, 1.5, 9, "unequal: the ANOVA coefficient is 1.5, not the mean 2")
    check_close(correlation.icc, 1.0, 9, "unequal: ICC 1 (no spread within either opening)")
    check_close(correlation.mean_group_size, 2.0, 9, "unequal: the mean group size is 2.0")
    check_close(correlation.effective_group_size, 2.5, 9, "unequal: m_effective is 2.5, larger than the mean")
    check_close(correlation.design_effect, 2.5, 9, "unequal: design effect 2.5")
    check_close(
        correlation.design_effect_at_mean_size,
        2.0,
        9,
        "unequal: the balanced-design shortcut says 2.0 - it understates",
    )
    check(
        correlation.design_effect > correlation.design_effect_at_mean_size,
        "unequal: the correct design effect exceeds the shortcut whenever the sizes vary",
    )
    check(correlation.smallest_group == 1 and correlation.largest_group == 3, "unequal: sizes 1 and 3 reported")


def test_nominal_figures_are_pool_matches() -> None:
    """2000 pair scores summing to match A of test/pool_matches_test.py: Elo -4.08, 95% bar +-7.87.

    The pair scores are built from that committed vector and then spread over 306 openings in order, so the
    nominal block must reproduce `pool_matches.analyse([160, 482, 732, 497, 129])` to the decimal place -
    which is the point of importing `pool_matches` instead of carrying a second pentanomial formula.
    """
    match_a = (160, 482, 732, 497, 129)
    scores: list[float] = []
    for score, count in zip(pool_matches.PAIR_SCORES, match_a):
        scores.extend([score] * count)
    check(len(scores) == 2000, "anchor: 2000 pair scores built from the committed vector")
    check(
        opening_icc.pentanomial_counts(scores) == match_a,
        "anchor: those scores go back to [160, 482, 732, 497, 129] slot for slot",
    )

    # Spread over 306 openings the way the match set is replayed, round robin so the sizes are 6 or 7.
    pairs = [
        opening_icc.PairObservation(
            source="anchor.pgn", round_label=str(index), opening=f"opening-{index % 306}", score=score
        )
        for index, score in enumerate(scores)
    ]
    corrected = opening_icc.analyse_pairs(pairs, label="anchor")
    nominal = corrected.nominal
    check(nominal.pairs == 2000 and nominal.games == 4000, "anchor: 2000 pairs, 4000 games")
    check_close(nominal.elo, -4.08, 2, "anchor: nominal Elo -4.08, as pool_matches pins it")
    check_close(nominal.elo_bar_95, 7.87, 2, "anchor: nominal 95% bar +-7.87, as pool_matches pins it")
    check_close(100.0 * nominal.los, 15.45, 2, "anchor: nominal LOS 15.45 %")
    check(
        corrected.correlation.openings == 306,
        "anchor: all 306 openings drawn, 6 or 7 pairs each",
    )
    check(
        corrected.correlation.smallest_group == 6 and corrected.correlation.largest_group == 7,
        "anchor: the group sizes are 6 and 7 (2000 pairs does not divide by 306)",
    )
    check_close(
        corrected.standard_error,
        nominal.standard_error * corrected.correlation.bar_inflation,
        12,
        "anchor: the corrected standard error is exactly the nominal one times sqrt(Deff)",
    )
    check(
        corrected.correlation.design_effect >= 1.0 and corrected.elo_bar_95 >= nominal.elo_bar_95,
        "anchor: a correction never narrows the bar",
    )
    # Round robin over sorted scores makes every opening mean nearly the grand mean, so that grouping floors
    # the ICC at 0 and leaves the bar alone - which is a correct answer but exercises no inflation. The same
    # 2000 scores grouped in CONTIGUOUS blocks of 7 instead puts near-identical scores together, which is
    # the opposite extreme, and is what checks the Elo transform under a real correction.
    blocked = [
        opening_icc.PairObservation(
            source="anchor.pgn", round_label=str(index), opening=f"opening-{index // 7}", score=score
        )
        for index, score in enumerate(scores)
    ]
    clustered = opening_icc.analyse_pairs(blocked, label="anchor clustered")
    check_close(clustered.nominal.elo, -4.08, 2, "anchor: regrouping does not change the nominal Elo")
    check_close(clustered.nominal.elo_bar_95, 7.87, 2, "anchor: nor the nominal bar - only the correction")
    check_close(correlation_floor := corrected.correlation.icc, 0.0, 9, "anchor: round robin floors the ICC at 0")
    check(
        clustered.correlation.icc > 0.8 and clustered.correlation.bar_inflation > 2.0,
        f"anchor: blocking by opening gives ICC {clustered.correlation.icc:.4f} and inflation "
        f"x{clustered.correlation.bar_inflation:.4f} - a real correction to test the Elo transform on",
    )
    # The Elo curve is not straight, so the bar ratio is only approximately sqrt(Deff) - but it is nowhere
    # near 1.0, which is what a correction applied to the wrong quantity would give.
    inflation = clustered.correlation.bar_inflation
    check(
        abs(clustered.elo_bar_95 / clustered.nominal.elo_bar_95 - inflation) < 0.02 * inflation,
        f"anchor: the corrected bar is sqrt(Deff) = {inflation:.4f} times the nominal, to within 2%",
    )
    check(
        correlation_floor == 0.0 and clustered.elo_bar_95 > 2.0 * corrected.elo_bar_95,
        "anchor: the two groupings of the same 2000 pairs give very different bars, so the grouping is read",
    )


def test_correction_is_applied_to_an_interval() -> None:
    """The corrected interval, its LOS, and the two-sided zero and point-estimate tests."""
    nominal = pool_matches.analyse([160, 482, 732, 497, 129], label="interval")
    doubling = opening_icc.measure_intraclass_correlation([[0.0, 0.0], [0.5, 0.5], [1.0, 1.0]])
    corrected = opening_icc.correct_interval(nominal, doubling)
    check_close(corrected.correlation.design_effect, 2.0, 9, "interval: a design effect of exactly 2 applied")
    check_close(
        corrected.standard_error, nominal.standard_error * math.sqrt(2.0), 12, "interval: se times sqrt(2)"
    )
    check(
        corrected.elo_bar_95 > nominal.elo_bar_95 and corrected.los > nominal.los,
        "interval: a wider bar moves the LOS towards 50 % (this match is behind, so LOS rises)",
    )
    check(not corrected.excludes_zero, "interval: -4.08 +-11 contains zero")
    check(corrected.excludes(+20.0), "interval: and it excludes +20 Elo")
    check(not corrected.excludes(-4.08), "interval: and contains its own point estimate")

    # The two-sided test, which the first version of this tool got wrong by checking only the low end.
    behind = pool_matches.analyse([400, 500, 200, 500, 400], label="behind")
    far_behind = opening_icc.correct_interval(
        pool_matches.analyse([900, 600, 200, 200, 100], label="far behind"),
        opening_icc.measure_intraclass_correlation([[0.0, 1.0], [0.0, 1.0], [0.0, 1.0]]),
    )
    check(behind.score_fraction == 0.5, "interval: the symmetric vector is level, as a control")
    check(
        far_behind.nominal.elo < 0.0 and far_behind.excludes_zero and far_behind.elo_high_95 < 0.0,
        "interval: a decisively NEGATIVE result excludes zero too - the test is two-sided",
    )


def test_pgn_is_parsed_into_pairs() -> None:
    """The PGN reader: three usable pairs, and one of each way a round fails to be one."""
    import tempfile

    with tempfile.TemporaryDirectory() as directory:
        path = write_fixture_pgn(Path(directory))
        pairs, report = opening_icc.parse_pairs([str(path)])

        check(report.games_seen == 13, f"pgn: all 13 games read (got {report.games_seen})")
        check(report.rounds_seen == 7, f"pgn: 7 rounds seen (got {report.rounds_seen})")
        check(report.pairs_kept == 3, f"pgn: 3 complete pairs kept (got {report.pairs_kept})")
        check(report.rounds_not_two_games == 1, "pgn: the single-game round is counted, not silently lost")
        check(report.rounds_unfinished == 1, "pgn: the round holding a `*` result is counted")
        check(report.rounds_same_colour == 1, "pgn: the round that never swapped colours is counted")
        check(report.rounds_mixed_opening == 1, "pgn: the round whose two games differ in FEN is counted")
        check(report.rounds_dropped == 4, "pgn: four rounds dropped in all")
        check(
            report.engine_names == ("baseline", "candidate"),
            f"pgn: both engine names reported (got {report.engine_names})",
        )

        scores = {pair.round_label: pair.score for pair in pairs}
        check(scores == {"1": 1.0, "2": 0.5, "3": 0.0}, f"pgn: the pair scores are 1, 0.5, 0 (got {scores})")
        check(
            all(pair.source == str(path) for pair in pairs),
            "pgn: every pair records the file it came from, so --per-file can split them",
        )

        groups = sorted(opening_icc.group_by_opening(pairs), key=len, reverse=True)
        check(
            [sorted(group) for group in groups] == [[0.5, 1.0], [0.0]],
            f"pgn: rounds 1 and 2 group on one opening, round 3 on another (got {groups})",
        )

        # Scoring from the wrong side is the failure that does not look like one, so it is an error.
        check_raises(
            lambda: opening_icc.parse_pairs([str(path)], candidate_name="runF"),
            "pgn: an engine name that is in no game is refused, not scored from the wrong side",
        )
        try:
            opening_icc.parse_pairs([str(path)], candidate_name="runF")
        except ValueError as problem:
            check(
                "candidate" in str(problem) and "baseline" in str(problem),
                "pgn: and the refusal lists the names that were actually there",
            )

        # Scored as the baseline instead, the three pairs must be exactly the complements.
        as_baseline, _ = opening_icc.parse_pairs([str(path)], candidate_name="baseline")
        check(
            sorted(pair.score for pair in as_baseline) == [0.0, 0.5, 1.0]
            and {pair.round_label: pair.score for pair in as_baseline} == {"1": 0.0, "2": 0.5, "3": 1.0},
            "pgn: naming the other engine gives the complementary scores, which is the sign the tool controls",
        )


def test_round_scoring() -> None:
    """A pair's score, over the four results a game can carry."""
    check(opening_icc.pair_score_of_round([(True, "1-0"), (False, "0-1")]) == 1.0, "score: candidate wins both")
    check(opening_icc.pair_score_of_round([(True, "0-1"), (False, "1-0")]) == 0.0, "score: candidate loses both")
    check(
        opening_icc.pair_score_of_round([(True, "1/2-1/2"), (False, "1/2-1/2")]) == 0.5,
        "score: both drawn is 0.5",
    )
    check(
        opening_icc.pair_score_of_round([(True, "1-0"), (False, "1-0")]) == 0.5,
        "score: a win apiece is 0.5 (the candidate was White in one and Black in the other)",
    )
    check(
        opening_icc.pair_score_of_round([(True, "1-0"), (False, "1/2-1/2")]) == 0.75,
        "score: a win and a draw is 0.75",
    )
    check(opening_icc.pair_score_of_round([(True, "1-0"), (False, "*")]) is None, "score: an unfinished game")
    check(opening_icc.pair_score_of_round([(True, "*"), (False, "1-0")]) is None, "score: in either order")


def test_input_is_validated() -> None:
    """A measurement that cannot be made stops the tool; it does not get a plausible design effect."""
    check_raises(
        lambda: opening_icc.measure_intraclass_correlation([[0.0, 0.5, 1.0]]),
        "validation: one opening has no between-opening variance to measure",
    )
    check_raises(
        lambda: opening_icc.measure_intraclass_correlation([]),
        "validation: no openings at all is refused",
    )
    check_raises(
        lambda: opening_icc.measure_intraclass_correlation([[0.5], [1.0]]),
        "validation: one pair per opening leaves no within-opening degrees of freedom",
    )
    check_raises(
        lambda: opening_icc.pentanomial_counts([0.3]),
        "validation: 0.3 is not a pair score, so it is refused rather than rounded into a slot",
    )
    check_raises(
        lambda: opening_icc.pentanomial_counts([]),
        "validation: no pairs at all is refused by pool_matches.validated_counts",
    )
    check(
        opening_icc.pentanomial_counts([0.0, 0.25, 0.5, 0.75, 1.0]) == (1, 1, 1, 1, 1),
        "validation: the five legal pair scores map to the five slots in order",
    )
    # An empty group is ignored rather than counted as an opening with no pairs, which would skew k.
    correlation = opening_icc.measure_intraclass_correlation([[0.0, 0.5], [0.5, 1.0], []])
    check(correlation.openings == 2, "validation: an empty group is not counted as an opening")


def main() -> int:
    """Run every case. @return 0 when every check passed, 1 otherwise."""
    test_perfect_correlation()
    test_no_correlation_is_floored()
    test_a_third()
    test_unequal_group_sizes()
    test_nominal_figures_are_pool_matches()
    test_correction_is_applied_to_an_interval()
    test_pgn_is_parsed_into_pairs()
    test_round_scoring()
    test_input_is_validated()
    verdict = "[PASS]" if failure_count == 0 else "[FAIL]"
    print(f"{verdict} opening_icc: {failure_count} failed check(s)")
    return 0 if failure_count == 0 else 1


if __name__ == "__main__":
    sys.exit(main())

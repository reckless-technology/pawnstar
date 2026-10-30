#!/usr/bin/env python3
"""tools/pgn_nps.py against hand-summed fixtures, and against the attribution trap that swaps two engines.

Registered as the ctest `pgn_nps`, so `make check` runs it. One `[PASS]`/`[FAIL]` line per check and a
summary line, the same shape as test/pool_matches_test.py and test/opening_icc_test.py.

THE FIXTURES ARE HAND-SUMMED. Every node count and every search time in the PGN text below was chosen so
the totals can be added up by eye, and the expected figures are written out in the docstring of each check.
A measurement tool checked against its own output is the defect this file exists to prevent, and the
practice that says so is this workspace's own: a checker kept only on the machines drifts and starts
printing confident wrong values.

WHAT THE ASSERTIONS CATCH, deliberately:

  * **aggregate rather than averaged rates.** The first fixture gives one engine a 1-node move and a
    999,000-node move. sum(nodes)/sum(seconds) is 1,000,000 nps; the mean of the two moves' own `nps`
    figures is 500,000. The assertion is on 1,000,000, so averaging the per-move rates goes red.
  * **the side to move comes from the opening.** The second game starts from a Black-to-move EPD, so the
    Black engine's comment is the first in the game. A tool that assumed White moves first would credit
    every one of that game's moves to the wrong engine, and the two engines' totals below are chosen to be
    different so that swap cannot pass.
  * **the alternation advances on every move, not only on parsable ones.** The third game's first move is a
    book move, whose comment carries no `n=`. The version of this tool that advanced its index only on
    node-bearing comments credited that game's remaining three moves as if they were moves 1, 2 and 3 -
    1,000,000 nodes to the candidate and 500,000 to the baseline, where the truth is 500,000 and 1,000,000.
    The fixture asserts the right way round, so that bug cannot pass; and the two numbers are the swap of
    each other, which is the signature to recognise.
  * **a comment that is missing altogether is refused, not guessed.** The fifth game declares four plies and
    carries three comments, so there is no way to know which move lost its comment and the alternation
    cannot be trusted. That game is counted as inconsistent and contributes nothing, and the count is
    printed rather than passed over.
"""

from __future__ import annotations

import sys
import tempfile
from pathlib import Path

# No `tools/__pycache__` left behind: a ctest run must not write into the source tree.
sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "tools"))

import pgn_nps  # noqa: E402  (the path above is what makes this import work from any directory)

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
    """A number to a fixed number of decimal places. @param actual it. @param expected the hand-sum.
    @param places decimal places. @param summary what was checked. @return whether it matched."""
    has_passed = round(actual, places) == round(expected, places)
    detail = summary if has_passed else f"{summary} - got {actual:.6f}"
    return check(has_passed, detail)


WHITE_TO_MOVE_FEN = "rnbqkb1r/ppp2ppp/4pn2/3p4/2PP4/2N5/PP2PPPP/R1BQKBNR w KQkq - 0 1"
BLACK_TO_MOVE_FEN = "rnbqkbnr/pp1ppppp/8/2p5/4P3/5N2/PPPP1PPP/RNBQKB1R b KQkq - 1 2"

#: Game 1: candidate is White and moves first, from a White-to-move opening. Four moves.
#:   candidate  move 1: 1 node in 0.001s   move 3: 999,000 nodes in 0.999s  -> 999,001 n / 1.000 s
#:   baseline   move 2: 200,000 nodes in 1.0s  move 4: 100,000 nodes in 1.0s -> 300,000 n / 2.000 s
GAME_ONE = f"""[Event "Fastchess Tournament"]
[Round "1"]
[White "candidate"]
[Black "baseline"]
[Result "1-0"]
[SetUp "1"]
[FEN "{WHITE_TO_MOVE_FEN}"]
[PlyCount "4"]
[TimeControl "8+0.08"]

1. cxd5 {{+0.79/10 0.001s, n=1, sd=3, nps=1000}} exd5 {{-0.32/11 1.0s, n=200000, sd=21, nps=200000}}
2. Bg5 {{+0.92/10 0.999s, n=999000, sd=29, nps=1000000}} Be7 {{-0.41/12 1.0s, n=100000, sd=18, nps=100000}} 1-0

"""

#: Game 2: a BLACK-to-move opening, so the Black engine's comment is first. Two moves.
#:   baseline is Black here and moves first: 50,000 nodes in 1.0s
#:   candidate is White and moves second:    7,000 nodes in 0.001s
GAME_TWO = f"""[Event "Fastchess Tournament"]
[Round "2"]
[White "candidate"]
[Black "baseline"]
[Result "0-1"]
[SetUp "1"]
[FEN "{BLACK_TO_MOVE_FEN}"]
[PlyCount "2"]
[TimeControl "8+0.08"]

1... cxd4 {{-0.10/9 1.0s, n=50000, sd=14, nps=50000}} 2. Nxd4 {{+0.05/8 0.001s, n=7000, sd=9, nps=7000000}} 0-1

"""

#: Game 3: the first move is a book move whose comment carries no node counts. The comment is still THERE,
#: so the alternation is sound and the other three moves are attributed normally:
#:   candidate (White, moves 1 and 3): the book move, then 500,000 nodes in 1.0s
#:   baseline  (Black, moves 2 and 4): 400,000 nodes in 1.0s, then 600,000 in 1.0s = 1,000,000 n / 2.0 s
#: Advancing the index only on the node-bearing comments - the bug - swaps those two figures exactly.
GAME_THREE = f"""[Event "Fastchess Tournament"]
[Round "3"]
[White "candidate"]
[Black "baseline"]
[Result "1/2-1/2"]
[SetUp "1"]
[FEN "{WHITE_TO_MOVE_FEN}"]
[PlyCount "4"]
[TimeControl "8+0.08"]

1. cxd5 {{book}} exd5 {{-0.32/11 1.0s, n=400000, sd=21, nps=400000}}
2. Bg5 {{+0.92/10 1.0s, n=500000, sd=29, nps=500000}} Be7 {{-0.41/12 1.0s, n=600000, sd=18, nps=600000}} 1/2-1/2

"""

#: Game 4: no engine names at all - counted as skipped, not quietly dropped.
GAME_FOUR = """[Event "Fastchess Tournament"]
[Round "4"]
[Result "*"]
[PlyCount "0"]

*

"""

#: Game 5: four plies declared, three comments present. One move's comment is gone, and nothing says which,
#: so the alternation cannot be trusted for any of them: the game is refused and counted.
GAME_FIVE = f"""[Event "Fastchess Tournament"]
[Round "5"]
[White "candidate"]
[Black "baseline"]
[Result "1/2-1/2"]
[SetUp "1"]
[FEN "{WHITE_TO_MOVE_FEN}"]
[PlyCount "4"]
[TimeControl "8+0.08"]

1. cxd5 exd5 {{-0.32/11 1.0s, n=11000000, sd=21, nps=11000000}}
2. Bg5 {{+0.92/10 1.0s, n=12000000, sd=29, nps=12000000}} Be7 {{-0.41/12 1.0s, n=13000000, sd=18, nps=13000000}} 1/2-1/2

"""


def write_pgn(directory: Path, name: str, text: str) -> str:
    """Write a PGN fixture. @param directory where. @param name the file name. @param text its content.
    @return the path as a string."""
    path = directory / name
    path.write_text(text, encoding="utf-8")
    return str(path)


def test_aggregate_rate_and_sides() -> None:
    """Games 1 and 2 together, with every total hand-summed.

    candidate: game 1 moves 1 and 3 (1 + 999,000 nodes in 0.001 + 0.999 s) plus game 2's second move
               (7,000 nodes in 0.001 s) = 1,006,001 nodes in 1.001 s -> 1,004,996.00 nps
    baseline:  game 1 moves 2 and 4 (200,000 + 100,000 nodes in 1.0 + 1.0 s) plus game 2's FIRST move,
               because that game starts from a Black-to-move opening (50,000 nodes in 1.0 s)
               = 350,000 nodes in 3.0 s -> 116,666.67 nps
    """
    with tempfile.TemporaryDirectory() as directory:
        path = write_pgn(Path(directory), "two.pgn", GAME_ONE + GAME_TWO)
        report = pgn_nps.measure([path])
        check(report.games == 2, f"rate: both games read (got {report.games})")
        check(report.games_inconsistent == 0 and report.games_without_tags == 0, "rate: nothing skipped")
        check(
            report.time_controls == ("8+0.08",),
            f"rate: the time control is reported beside the figures (got {report.time_controls})",
        )
        check(
            tuple(report.engines) == ("baseline", "candidate"),
            f"rate: both engines present (got {tuple(report.engines)})",
        )
        candidate, baseline = report.engines["candidate"], report.engines["baseline"]

        check(candidate.nodes == 1_006_001, f"rate: candidate 1,006,001 nodes (got {candidate.nodes:,})")
        check_close(candidate.seconds, 1.001, 6, "rate: candidate 1.001 search seconds")
        check_close(candidate.nodes_per_second, 1_006_001 / 1.001, 2, "rate: candidate 1,004,996 nps")
        check(candidate.moves == 3, f"rate: candidate made 3 moves (got {candidate.moves})")
        check(candidate.peak_move_nps == 7_000_000, "rate: candidate's peak single-move nps is 7,000,000")
        check(candidate.deepest_seldepth == 29, "rate: candidate's deepest seldepth is 29")

        check(baseline.nodes == 350_000, f"rate: baseline 350,000 nodes (got {baseline.nodes:,})")
        check_close(baseline.seconds, 3.0, 6, "rate: baseline 3.0 search seconds")
        check_close(baseline.nodes_per_second, 350_000 / 3.0, 2, "rate: baseline 116,666.67 nps")
        check(baseline.moves == 3, f"rate: baseline made 3 moves (got {baseline.moves})")

        # Averaging the per-move rates instead of aggregating gives candidate (1,000 + 1,000,000 +
        # 7,000,000)/3 = 2,667,000 - nowhere near the right answer, which is the point of the fixture.
        check(
            abs(candidate.nodes_per_second - 2_667_000.0) > 1_000_000.0,
            "rate: the figure is sum(nodes)/sum(seconds), not the mean of the per-move nps",
        )
        # And the two engines' totals differ, so crediting game 2 to the wrong side cannot pass.
        check(
            candidate.nodes != baseline.nodes and candidate.seconds != baseline.seconds,
            "rate: the two engines' totals differ, so a swapped attribution cannot pass these checks",
        )


def test_side_order_reads_the_opening() -> None:
    """The side to move comes from the `FEN` tag, not from an assumption that White starts."""
    check(
        pgn_nps.side_order("candidate", "baseline", WHITE_TO_MOVE_FEN) == ("candidate", "baseline"),
        "sides: a White-to-move opening puts the White engine first",
    )
    check(
        pgn_nps.side_order("candidate", "baseline", BLACK_TO_MOVE_FEN) == ("baseline", "candidate"),
        "sides: a Black-to-move opening puts the BLACK engine first",
    )
    check(
        pgn_nps.side_order("candidate", "baseline", "") == ("candidate", "baseline"),
        "sides: with no FEN tag at all the game starts from the standard position, White first",
    )


def test_a_book_comment_does_not_shift_the_attribution() -> None:
    """Game 3's book move carries no node counts, and the three real moves still land on the right engines.

    The comment is present, so the alternation is sound: the candidate moved 1st and 3rd and so owns the
    500,000-node move, and the baseline moved 2nd and 4th and owns 400,000 + 600,000 = 1,000,000. The bug
    this pins - advancing the index only on node-bearing comments - gives exactly the swap of those two.
    """
    with tempfile.TemporaryDirectory() as directory:
        path = write_pgn(Path(directory), "book.pgn", GAME_THREE)
        report = pgn_nps.measure([path])
        check(report.games == 1 and report.games_inconsistent == 0, "book: the game is used, not refused")
        candidate, baseline = report.engines["candidate"], report.engines["baseline"]
        check(
            candidate.nodes == 500_000 and baseline.nodes == 1_000_000,
            f"book: 500,000 to the candidate and 1,000,000 to the baseline - NOT the swap "
            f"(got {candidate.nodes:,} / {baseline.nodes:,})",
        )
        check(
            candidate.moves == 2 and candidate.moves_with_counts == 1,
            f"book: the candidate's book move is counted as a move with no counts "
            f"(got {candidate.moves} moves, {candidate.moves_with_counts} with counts)",
        )
        check(
            baseline.moves == 2 and baseline.moves_with_counts == 2,
            "book: and both of the baseline's moves carried counts",
        )


def test_a_missing_comment_refuses_the_game() -> None:
    """Game 5 declares four plies and holds three comments, so it is refused and counted, not guessed."""
    with tempfile.TemporaryDirectory() as directory:
        path = write_pgn(Path(directory), "mixed.pgn", GAME_ONE + GAME_FIVE + GAME_FOUR)
        report = pgn_nps.measure([path])
        check(report.games == 3, f"refusal: three games read (got {report.games})")
        check(report.games_inconsistent == 1, f"refusal: one game refused (got {report.games_inconsistent})")
        check(report.games_without_tags == 1, "refusal: and the game with no engine names counted separately")
        candidate = report.engines["candidate"]
        baseline = report.engines["baseline"]
        check(
            candidate.nodes == 999_001 and baseline.nodes == 300_000,
            f"refusal: only game 1's nodes were counted (got {candidate.nodes:,} / {baseline.nodes:,})",
        )
        check(
            candidate.moves == 2 and baseline.moves == 2,
            "refusal: and only game 1's moves - the refused game contributed no moves either",
        )
        printed = pgn_nps.format_report(report)
        check(
            "disagreed with PlyCount" in printed and "without White/Black tags" in printed,
            "refusal: both kinds of skip are printed, so a quiet drop cannot hide in the report",
        )


def test_report_names_what_is_behind_a_figure() -> None:
    """The printed report carries the games, the time controls and the move counts beside every rate."""
    with tempfile.TemporaryDirectory() as directory:
        path = write_pgn(Path(directory), "two.pgn", GAME_ONE + GAME_TWO)
        printed = pgn_nps.format_report(pgn_nps.measure([path]))
        check("games read        2" in printed, "report: the game count is printed")
        check("8+0.08" in printed, "report: the time control is printed")
        check("ABSOLUTE nps" in printed, "report: the rate column says it is absolute, not a percentage")
        check("rate ratio" in printed, "report: two engines get a like-for-like ratio under the same load")
        check(
            "1,004,996" in printed and "116,667" in printed,
            f"report: both hand-summed rates appear in it\n{printed}",
        )


def test_no_games_is_an_error_not_a_zero() -> None:
    """A file with no usable game yields no engines, which `main` turns into an error rather than a 0 nps."""
    with tempfile.TemporaryDirectory() as directory:
        path = write_pgn(Path(directory), "empty.pgn", "nothing resembling a PGN here\n")
        report = pgn_nps.measure([path])
        check(report.games == 0 and not report.engines, "empty: a file with no game gives no engines")
        nameless = write_pgn(Path(directory), "nameless.pgn", GAME_FOUR)
        report = pgn_nps.measure([nameless])
        check(
            report.games == 1 and report.games_without_tags == 1 and not report.engines,
            "empty: a game with no engine names is counted as skipped and credited to nobody",
        )


def main() -> int:
    """Run every case. @return 0 when every check passed, 1 otherwise."""
    test_aggregate_rate_and_sides()
    test_side_order_reads_the_opening()
    test_a_book_comment_does_not_shift_the_attribution()
    test_a_missing_comment_refuses_the_game()
    test_report_names_what_is_behind_a_figure()
    test_no_games_is_an_error_not_a_zero()
    verdict = "[PASS]" if failure_count == 0 else "[FAIL]"
    print(f"{verdict} pgn_nps: {failure_count} failed check(s)")
    return 0 if failure_count == 0 else 1


if __name__ == "__main__":
    sys.exit(main())

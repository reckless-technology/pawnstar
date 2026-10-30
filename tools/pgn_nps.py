#!/usr/bin/env python3
"""Absolute nodes per second per engine, measured INSIDE a match at the concurrency it actually ran at.

`bench` and `tools/rate.sh` measure an idle machine. A match does not run on an idle machine: lin-0's
"16 cores" are 16 threads on 8 physical cores, and at the harness default concurrency of 14 each engine
gets well under half the rate an idle bench reports - so an idle figure overstates match speed by about
2.2x there, and a candidate that benches 6.4% faster can be no faster at all under load. A percentage of a
machine's own idle rate is also not comparable across machines. Both of those are fixed by reporting the
ABSOLUTE rate, taken from the match's own PGN.

fastchess writes one comment per move holding what the engine reported for it:

    1. cxd5 {+0.79/10 0.182s, n=594048, sd=29, nps=1302736} exd5 {-0.32/11 0.218s, n=...}

so `sum(nodes) / sum(seconds)` per engine is the aggregate rate over the whole match. Summing nodes and
seconds separately - rather than averaging the per-move `nps` - is what weights a long think correctly; a
mean of per-move rates is dominated by the many near-instant moves.

TWO THINGS THIS IS CAREFUL ABOUT:

 1. **Whose move is whose.** There is nothing in a move comment that names the engine, so a move is
    attributed by alternating from the side to move in the game's `[FEN]` header - which is why the opening
    file's side to move is read rather than assumed to be White. The trap is that the alternation must
    advance on EVERY move, not only on the moves that carry a parsable `n=`: the first version of this tool
    advanced its index inside the match loop for the node-bearing comments only, so one comment without an
    `n=` silently swapped the two engines' figures for the whole rest of the game. Here the side advances
    per comment and the node counts are taken from the comments that have them, and a game whose comment
    count disagrees with its `[PlyCount]` is counted as inconsistent and reported rather than averaged in.
 2. **A rate is about a machine, a concurrency and a moment.** The report prints the games, the time
    controls seen and the move count behind every figure, because a nodes-per-second number with none of
    that beside it is the kind of figure that gets compared against one measured under different conditions.
    The machine and the date are the caller's to record - see the repository's practice on that.

    python3 tools/pgn_nps.py ~/sprt/lin0-tc12/pgn/runF-v12-lin0tc12a.pgn ...
"""

from __future__ import annotations

import argparse
import re
import sys
from collections.abc import Sequence
from dataclasses import dataclass, field

#: `[White "candidate"]` and every other PGN tag pair.
TAG_PATTERN = re.compile(r'^\[(\w+)\s+"(.*)"\]\s*$')

#: Every move comment, in order, whether or not it carries node counts - the alternation depends on that.
COMMENT_PATTERN = re.compile(r"\{[^{}]*\}")

#: `{+1.02/16 0.459s, n=594048, sd=29, nps=1302736}`, and the same with a mate score or a negative eval.
SEARCH_PATTERN = re.compile(r"([0-9]*\.?[0-9]+)s,\s*n=(\d+),\s*sd=(\d+),\s*nps=(\d+)")


@dataclass
class EngineRate:
    """What one engine did across every game it played in these files."""

    name: str
    moves: int = 0  #: moves attributed to it
    moves_with_counts: int = 0  #: of those, the moves whose comment carried n= and nps=
    nodes: int = 0
    seconds: float = 0.0
    peak_move_nps: int = 0
    deepest_seldepth: int = 0

    @property
    def nodes_per_second(self) -> float:
        """The aggregate rate - total nodes over total search seconds. @return it, or 0 with no time."""
        return self.nodes / self.seconds if self.seconds > 0.0 else 0.0


@dataclass
class NpsReport:
    """Every engine's rate, and what was read to get there."""

    engines: dict[str, EngineRate] = field(default_factory=dict)
    files: tuple[str, ...] = ()
    games: int = 0
    games_without_tags: int = 0
    games_inconsistent: int = 0  #: comment count disagreed with `[PlyCount]`, so not attributed
    time_controls: tuple[str, ...] = ()


def side_order(white: str, black: str, fen: str) -> tuple[str, str]:
    """Which engine moves first in a game, from the side to move in its opening.

    @param white the White engine. @param black the Black engine. @param fen the `FEN` tag, possibly empty.
    @return the two names, the one on move first.
    """
    fields = fen.split()
    is_white_to_move = len(fields) < 2 or fields[1] != "b"
    return (white, black) if is_white_to_move else (black, white)


def measure(paths: Sequence[str]) -> NpsReport:
    """Read the PGNs and aggregate each engine's nodes and search seconds.

    @param paths the PGN files.
    @return the report. A game missing `White`, `Black` or its moves is counted and skipped; a game whose
    comment count disagrees with `[PlyCount]` is counted as inconsistent and skipped, because attributing
    its moves would be guessing.
    """
    report = NpsReport(files=tuple(paths))
    rates: dict[str, EngineRate] = {}
    time_controls: set[str] = set()

    for path in paths:
        with open(path, encoding="utf-8", errors="replace") as handle:
            text = handle.read()
        for block in re.split(r"\n(?=\[Event )", text):
            if "[Event " not in block:
                continue
            report.games += 1
            tags: dict[str, str] = {}
            body: list[str] = []
            for line in block.splitlines():
                match = TAG_PATTERN.match(line)
                if match:
                    tags[match.group(1)] = match.group(2)
                else:
                    body.append(line)
            white, black = tags.get("White"), tags.get("Black")
            if not white or not black:
                report.games_without_tags += 1
                continue
            if "TimeControl" in tags:
                time_controls.add(tags["TimeControl"])

            comments = COMMENT_PATTERN.findall("\n".join(body))
            declared_plies = tags.get("PlyCount")
            if declared_plies is not None and declared_plies.isdigit() and int(declared_plies) != len(comments):
                # Not averaged in: one missing comment shifts the alternation and swaps the two engines for
                # the rest of the game, which is the defect this check exists to refuse.
                report.games_inconsistent += 1
                continue

            order = side_order(white, black, tags.get("FEN", ""))
            for index, comment in enumerate(comments):
                name = order[index % 2]
                rate = rates.setdefault(name, EngineRate(name=name))
                rate.moves += 1
                found = SEARCH_PATTERN.search(comment)
                if not found:
                    continue
                seconds, nodes, seldepth, move_nps = found.groups()
                rate.moves_with_counts += 1
                rate.nodes += int(nodes)
                rate.seconds += float(seconds)
                rate.peak_move_nps = max(rate.peak_move_nps, int(move_nps))
                rate.deepest_seldepth = max(rate.deepest_seldepth, int(seldepth))

    report.engines = dict(sorted(rates.items()))
    report.time_controls = tuple(sorted(time_controls))
    return report


def format_report(report: NpsReport) -> str:
    """The report as a block of lines, with what was behind every figure. @param report it. @return it."""
    lines = [
        f"games read        {report.games:,} in {len(report.files)} file(s)",
        f"time controls     {', '.join(report.time_controls) or 'none in the tags'}",
    ]
    if report.games_without_tags:
        lines.append(f"games skipped     {report.games_without_tags:,} without White/Black tags")
    if report.games_inconsistent:
        lines.append(
            f"games skipped     {report.games_inconsistent:,} whose move-comment count disagreed with "
            "PlyCount (attributing them would guess which engine moved)"
        )
    lines.append("")
    lines.append(
        f"{'engine':12} {'moves':>9} {'with counts':>12} {'total nodes':>17} {'search s':>12} "
        f"{'ABSOLUTE nps':>14} {'peak move nps':>14} {'max seldepth':>13}"
    )
    for rate in report.engines.values():
        lines.append(
            f"{rate.name:12} {rate.moves:>9,} {rate.moves_with_counts:>12,} {rate.nodes:>17,} "
            f"{rate.seconds:>12,.1f} {rate.nodes_per_second:>14,.0f} {rate.peak_move_nps:>14,} "
            f"{rate.deepest_seldepth:>13}"
        )
    if len(report.engines) == 2:
        first, second = report.engines.values()
        if second.nodes_per_second > 0.0:
            ratio = first.nodes_per_second / second.nodes_per_second
            lines += [
                "",
                f"{first.name} / {second.name} rate ratio   {ratio:.4f}   (both under the SAME match load, "
                "which is what an idle bench cannot tell you)",
            ]
    return "\n".join(lines)


def main() -> int:
    """Read the PGNs asked for and print each engine's absolute rate. @return the exit status."""
    parser = argparse.ArgumentParser(
        description="Absolute nodes per second per engine, from a match's own PGN comments.",
        epilog="An idle bench overstates match speed - on a machine whose cores are threads, by about 2.2x. "
        "Record the machine and the date beside any figure taken from here.",
    )
    parser.add_argument("pgn", nargs="+", help="fastchess PGN files, aggregated together")
    arguments = parser.parse_args()
    try:
        report = measure(arguments.pgn)
    except OSError as problem:
        sys.exit(f"pgn_nps: {problem}")
    if not report.engines:
        sys.exit("pgn_nps: no game with both engine names in these files")
    print(format_report(report))
    return 0


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""tools/match_core.py: the plan's refusals and matrix, the fastchess command, the wrapper, the probe and a chunk.

Registered as the ctest `match_core`, so `make check` runs it. One `[PASS]`/`[FAIL]` line per check and a summary
line, the same shape as test/pgn_nps_test.py. No fastchess and no real engine: a four-line shell script plays the
engine's UCI side and another plays fastchess, so the test holds on any machine with a POSIX shell.

THE COMMAND IS PINNED LITERALLY. The reusable workflow is only a faithful replacement for tools/pawnstar-match.sh if
it hands fastchess the same flags in the same order, so the expected list below is that script's own, written out.
"""

from __future__ import annotations

import hashlib
import json
import os
import stat
import sys
import tempfile
from pathlib import Path

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "tools"))

import match_core  # noqa: E402

failure_count = 0


def check(has_passed: bool, summary: str) -> bool:
    """Report one `[PASS]`/`[FAIL]` line and tally a failure. @return has_passed."""
    global failure_count
    print(f"{'[PASS]' if has_passed else '[FAIL]'} {summary}")
    if not has_passed:
        failure_count += 1
    return has_passed


def raises_match_error(call, fragment: str) -> bool:
    """Whether `call()` raises MatchError whose message contains `fragment`."""
    try:
        call()
    except match_core.MatchError as error:
        return fragment in str(error)
    return False


def write_executable(path: Path, text: str) -> None:
    path.write_text(text)
    path.chmod(path.stat().st_mode | stat.S_IXUSR)


FAKE_ENGINE = """#!/bin/sh
echo "env=$TEST_NET threads=$TEST_THREADS" >&2
while read line; do
  case "$line" in
    uci) echo "id name fake"; echo "uciok" ;;
    isready) echo "readyok" ;;
    go*) echo "info string net loaded: $TEST_NET"; echo "bestmove e2e4" ;;
    quit) exit 0 ;;
  esac
done
"""

FAKE_FASTCHESS = """#!/bin/sh
printf '%s\\n' "$@" > "$FAKE_ARGV_FILE"
echo "Games: $FAKE_GAMES"
exit 0
"""


def make_inputs(root: Path, probe_expect: str = "") -> Path:
    """An unpacked artifact with a Pawnstar-shaped manifest and a three-line opening set under a '#' header."""
    inputs = root / "inputs"
    inputs.mkdir()
    (inputs / "candidate").mkdir()
    (inputs / "baseline").mkdir()
    for role in ("candidate", "baseline"):
        write_executable(inputs / role / "engine", FAKE_ENGINE)
        (inputs / f"{role}.bin").write_text(role)
    text = "# header\n\nfen one\nfen two\nfen three\n"
    (inputs / "openings.epd").write_text(text)
    side = lambda role: {"name": role, "command": "{inputs}/%s/engine" % role,
                         "options": {"EvalFile": "{inputs}/%s.bin" % role},
                         "env": {"TEST_NET": "{inputs}/%s.bin" % role, "TEST_THREADS": "1"}}
    manifest = {"format": 1, "candidate": side("candidate"), "baseline": side("baseline"),
                "each_options": {"OwnBook": "false"},
                "openings": {"path": "openings.epd", "sha256": hashlib.sha256(text.encode()).hexdigest()}}
    if probe_expect:
        manifest["probe_expect"] = probe_expect
    (inputs / "manifest.json").write_text(json.dumps(manifest))
    return inputs


def plan_arguments(inputs: Path, out: Path, games: int, chunks: int, **extra) -> list[str]:
    arguments = ["plan", "--inputs", str(inputs), "--out", str(out), "--games", str(games), "--chunks", str(chunks),
                 "--github-output", str(out / "github_output")]
    for key, value in extra.items():
        arguments += [f"--{key.replace('_', '-')}", value]
    return arguments


def test_matrix() -> None:
    matrix, expect, last = match_core.build_matrix(4000, 20, 1, ["2"], 6433)
    check(len(matrix) == 20 and expect == 4000 and last == 2000, "4000 games in 20 chunks: 20 slices, openings 1..2000")
    check(matrix[0] == {"index": 0, "games": 200, "start": 1, "concurrency": "2"}
          and matrix[19]["start"] == 1901, "the slices are consecutive: chunk 0 starts at 1, chunk 19 at 1901")
    starts = [(m["start"], m["start"] + m["games"] // 2 - 1) for m in matrix]
    check(all(a[1] + 1 == b[0] for a, b in zip(starts, starts[1:])), "no slice overlaps or skips an opening")
    _, expect, _ = match_core.build_matrix(7, 2, 5, ["2"], 100)
    check(expect == 8, "an odd game count rounds up to whole pairs (7 games -> 8)")
    matrix, _, _ = match_core.build_matrix(12, 3, 5, ["2", "3"], 100)
    check([m["concurrency"] for m in matrix] == ["2", "3", "2"], "a concurrency list is dealt out round-robin")
    check(raises_match_error(lambda: match_core.build_matrix(400, 4, 150, ["2"], 300), "replay openings"),
          "a match that would run off the end of the opening set is refused (200 pairs from 150 of 300)")


def test_plan(root: Path) -> None:
    inputs = make_inputs(root)
    out = root / "plan"
    out.mkdir()
    status = match_core.main(plan_arguments(inputs, out, 4, 2))
    stripped = (out / "openings.epd").read_text()
    check(status == 0 and stripped == "fen one\nfen two\nfen three\n", "plan strips the '#' header and blank lines")
    outputs = (out / "github_output").read_text()
    matrix = json.loads(outputs.split("matrix=")[1].splitlines()[0])
    check("expect_games=4" in outputs and [m["start"] for m in matrix["include"]] == [1, 2],
          "plan writes the matrix and the expected game count to GITHUB_OUTPUT")
    check(match_core.main(plan_arguments(inputs, out, 4, 2, openings_sha256="0" * 64)) == 1,
          "plan refuses an opening set that does not match a SHA-256 given as an input")
    check(match_core.main(plan_arguments(inputs, out, 8, 2)) == 1, "plan refuses 4 pairs from a 3-position set")
    manifest = json.loads((inputs / "manifest.json").read_text())
    manifest["openings"].pop("sha256")
    (inputs / "manifest.json").write_text(json.dumps(manifest))
    check(match_core.main(plan_arguments(inputs, out, 4, 2)) == 1, "plan refuses an opening set with no SHA-256 at all")
    check(match_core.main(plan_arguments(inputs, out, 4, 2, openings_sha256=hashlib.sha256(
        b"# header\n\nfen one\nfen two\nfen three\n").hexdigest())) == 0, "the SHA-256 may come from the input instead")


def test_command_and_wrapper(root: Path) -> None:
    inputs = make_inputs(root)
    manifest = match_core.read_manifest(str(inputs))
    scratch = root / "scratch"
    scratch.mkdir()
    wrappers = {}
    for role in match_core.ROLES:
        wrappers[role] = str(scratch / f"{role}.sh")
        match_core.write_wrapper(wrappers[role], manifest[role], str(inputs))
    text = Path(wrappers["candidate"]).read_text().splitlines()[-1]
    absolute = str(inputs.resolve())
    check(text == f"exec env TEST_NET={absolute}/candidate.bin TEST_THREADS=1 {absolute}/candidate/engine \"$@\"",
          "the wrapper sets the side's environment, then execs its engine")
    arguments = match_core.make_parser().parse_args(
        ["chunk", "--inputs", str(inputs), "--openings", "o.epd", "--fastchess", "fc", "--start", "101", "--games",
         "200", "--hash", "64", "--depth", "8", "--out", "out", "--label", "chunk-1", "--concurrency", "2"])
    arguments.pgn = "chunk-1.pgn"
    expected = ["fc",
                "-engine", f"cmd={wrappers['candidate']}", "name=candidate", f"option.EvalFile={absolute}/candidate.bin",
                "-engine", f"cmd={wrappers['baseline']}", "name=baseline", f"option.EvalFile={absolute}/baseline.bin",
                "-each", "proto=uci", "option.Threads=1", "option.Hash=64", "option.OwnBook=false", "depth=8",
                "-rounds", "100", "-games", "2", "-repeat",
                "-openings", "file=o.epd", "format=epd", "order=sequential", "start=101",
                "-draw", "movenumber=40", "movecount=8", "score=20", "-resign", "movecount=3", "score=600",
                "-pgnout", "file=chunk-1.pgn", "nodes=true", "seldepth=true", "nps=true",
                "-concurrency", "2"]
    check(match_core.fastchess_command(arguments, manifest, wrappers) == expected,
          "the fastchess command is tools/pawnstar-match.sh's, flag for flag (depth 8)")
    arguments.depth, arguments.time_control, arguments.baseline_time_control = "", "8+0.08", "4+0.04"
    command = match_core.fastchess_command(arguments, manifest, wrappers)
    each = command[command.index("-each"):command.index("-rounds")]
    check(command.count("tc=8+0.08") == 1 and command.count("tc=4+0.04") == 1 and not any(a.startswith("tc=") for a in each),
          "time odds put each side's tc on its own engine, none in -each")
    arguments.depth, arguments.baseline_time_control = "8", ""
    check(raises_match_error(lambda: match_core.fastchess_command(arguments, manifest, wrappers), "exactly one"),
          "a depth and a time control together are refused")


def run_chunk(root: Path, games_played: int, probe_expect: str = "") -> int:
    inputs = make_inputs(root, probe_expect)
    fake = root / "fastchess"
    write_executable(fake, FAKE_FASTCHESS)
    os.environ["FAKE_ARGV_FILE"] = str(root / "argv.txt")
    os.environ["FAKE_GAMES"] = str(games_played)
    return match_core.main(["chunk", "--inputs", str(inputs), "--openings", str(inputs / "openings.epd"),
                            "--fastchess", str(fake), "--start", "1", "--games", "4", "--depth", "4",
                            "--out", str(root / "out"), "--label", "chunk-0"])


def test_chunk_and_probe() -> None:
    with tempfile.TemporaryDirectory() as directory:
        root = Path(directory)
        status = run_chunk(root, 4)
        check(status == 0 and (root / "out" / "chunk-0.log").read_text().strip() == "Games: 4",
              "a chunk probes both engines, runs fastchess and keeps its output in <label>.log")
        check("name=candidate" in (root / "argv.txt").read_text(), "fastchess was given the generated command")
    with tempfile.TemporaryDirectory() as directory:
        check(run_chunk(Path(directory), 3) == 1, "a chunk that played fewer games than it was given fails")
    with tempfile.TemporaryDirectory() as directory:
        check(run_chunk(Path(directory), 4, "net loaded: .*[.]bin") == 0,
              "probe_expect is matched against the probe's output")
    with tempfile.TemporaryDirectory() as directory:
        check(run_chunk(Path(directory), 4, "never printed") == 1, "a probe_expect the engine does not satisfy fails")


def main() -> int:
    """Run every case. @return 0 when every check passed, 1 otherwise."""
    test_matrix()
    for case in (test_plan, test_command_and_wrapper):
        with tempfile.TemporaryDirectory() as directory:
            case(Path(directory))
    test_chunk_and_probe()
    verdict = "[PASS]" if failure_count == 0 else "[FAIL]"
    print(f"{verdict} match_core: {failure_count} failed check(s)")
    return 0 if failure_count == 0 else 1


if __name__ == "__main__":
    sys.exit(main())

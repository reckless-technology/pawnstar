#!/usr/bin/env python3
"""match_core.py - the engine-neutral half of the reusable match workflow (.github/workflows/match-core.yml).

Two subcommands, both standard library only:

  plan   reads <inputs>/manifest.json, verifies the opening set against its SHA-256, writes the comment-stripped
         opening set and the chunk matrix, and refuses a match that would replay an opening.
  chunk  plays one slice of the match: builds the fastchess command from the manifest, probes each engine once,
         runs fastchess, and checks the game count.

THE MANIFEST (<artifact>/manifest.json), the one thing a caller writes besides its build:

  {
    "format": 1,
    "candidate": {
      "name":    "Pawnstar 2.4 (abc1234)",          display name only; fastchess always names the sides
                                                      `candidate` and `baseline`, which the pool tools read
      "command": "{inputs}/candidate/pawnstar",     the engine executable
      "args":    [],                                 optional, passed to the engine on every run
      "options": {"EvalFile": "{inputs}/candidate.bin"},   UCI options for this side (option.NAME=VALUE)
      "env":     {"PAWNSTAR_EVALFILE": "{inputs}/candidate.bin"}   environment for this side, set by a wrapper
    },
    "baseline": { ... the same ... },
    "each_options": {"OwnBook": "false"},            optional UCI options for both sides (-each)
    "openings": {"path": "openings.epd", "sha256": "..."},   optional; see the workflow's openings_path input
    "probe_expect": "regex"                          optional; the probe's output must match it, e.g. a line the
                                                      engine prints when its net loaded
  }

`{inputs}` in a command, an argument, an option value or an environment value is the absolute path of the unpacked
artifact. Every other path is the caller's to make absolute. Threads=1 and Hash=<hash_mb> are applied to both sides
by this tool (-each), as are the clock and the adjudication.

THE FLAGS are the ones tools/pawnstar-match.sh plays Pawnstar's matches with, so a result from either route is the
same measurement: `-games 2 -repeat`, `order=sequential start=N` (one position per colour-reversed pair, in file
order), `-draw movenumber=40 movecount=8 score=20 -resign movecount=3 score=600`, and a PGN with nodes, seldepth
and nps in the move comments, which tools/pgn_nps.py reads.
"""

import argparse
import hashlib
import json
import math
import os
import re
import shlex
import subprocess
import sys
import tempfile
import threading
import time

ROLES = ("candidate", "baseline")
ADJUDICATION = ["-draw", "movenumber=40", "movecount=8", "score=20", "-resign", "movecount=3", "score=600"]
PGN_FLAGS = ["nodes=true", "seldepth=true", "nps=true"]
PROBE_DEPTH = 4
PROBE_SECONDS = 120


class MatchError(Exception):
    """A reason to refuse the match, printed as a GitHub error annotation by main()."""


def read_manifest(inputs):
    path = os.path.join(inputs, "manifest.json")
    try:
        with open(path) as handle:
            manifest = json.load(handle)
    except (OSError, ValueError) as error:
        raise MatchError(f"cannot read {path}: {error}")
    if manifest.get("format") != 1:
        raise MatchError(f"{path}: format must be 1, not {manifest.get('format')!r}")
    for role in ROLES:
        side = manifest.get(role)
        if not isinstance(side, dict) or not side.get("command"):
            raise MatchError(f"{path}: '{role}' needs at least a 'command'")
        for key, kind in (("args", list), ("options", dict), ("env", dict)):
            if not isinstance(side.get(key, kind()), kind):
                raise MatchError(f"{path}: '{role}.{key}' must be a {kind.__name__}")
    if not isinstance(manifest.get("each_options", {}), dict):
        raise MatchError(f"{path}: 'each_options' must be an object")
    return manifest


def expand(text, inputs):
    """Substitute {inputs}. Only that token: a UCI option value may legitimately contain other braces."""
    return str(text).replace("{inputs}", os.path.abspath(inputs))


def sha256_of(path):
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        for block in iter(lambda: handle.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def strip_openings(text):
    """fastchess hands every non-empty line of an EPD file to its FEN parser, so a '#' header would be a position."""
    return [line for line in text.splitlines() if line.strip() and not line.lstrip().startswith("#")]


# ---------------------------------------------------------------------------------------------------------------
# plan
# ---------------------------------------------------------------------------------------------------------------

def build_matrix(games, chunks, first, concurrency, opening_count):
    """Disjoint slices of the opening set, one position per pair. Raises MatchError rather than replay."""
    if games < 2 or chunks < 1 or first < 1:
        raise MatchError(f"games {games}, chunks {chunks}, first opening {first}: all must be positive "
                         "(games at least 2)")
    pairs_total = (games + 1) // 2
    chunks = min(chunks, pairs_total)
    per_chunk = math.ceil(pairs_total / chunks)
    last = first + pairs_total - 1
    if last > opening_count:
        raise MatchError(f"{pairs_total} pairs from opening {first} need openings up to {last}, but the set has "
                         f"{opening_count}: a match this long would replay openings")
    matrix, done = [], 0
    for index in range(chunks):
        pairs = min(per_chunk, pairs_total - done)
        if pairs <= 0:
            break
        matrix.append({"index": index, "games": pairs * 2, "start": first + done,
                       "concurrency": concurrency[index % len(concurrency)]})
        done += pairs
    return matrix, pairs_total * 2, last


def command_plan(args):
    manifest = read_manifest(args.inputs)
    declared = manifest.get("openings") or {}
    opening_path = args.openings_file or (os.path.join(args.inputs, declared["path"]) if declared.get("path") else "")
    if not opening_path or not os.path.isfile(opening_path):
        raise MatchError("no opening set: the manifest has no openings.path and no openings_path input was given")
    wanted = (args.openings_sha256 or declared.get("sha256") or "").strip().lower()
    if not wanted:
        raise MatchError("the opening set has no SHA-256 (manifest openings.sha256 or the openings_sha256 input)")
    actual = sha256_of(opening_path)
    if actual != wanted:
        raise MatchError(f"the opening set is {actual}, not the {wanted} that was declared")
    with open(opening_path) as handle:
        openings = strip_openings(handle.read())
    os.makedirs(args.out, exist_ok=True)
    with open(os.path.join(args.out, "openings.epd"), "w") as handle:
        handle.write("\n".join(openings) + "\n")

    concurrency = [c.strip() for c in args.concurrency.split(",") if c.strip()] or ["2"]
    matrix, expect_games, last = build_matrix(args.games, args.chunks, args.first, concurrency, len(openings))
    print(f"openings sha256 {actual} verified; {len(openings)} positions after stripping comments")
    print(f"{len(matrix)} chunks, {expect_games} games, openings {args.first}..{last} of {len(openings)}")
    for role in ROLES:
        print(f"{role}: {manifest[role].get('name', '?')}")
    if args.github_output:
        with open(args.github_output, "a") as out:
            out.write("matrix=" + json.dumps({"include": matrix}) + "\n")
            out.write(f"expect_games={expect_games}\n")
    return 0


# ---------------------------------------------------------------------------------------------------------------
# chunk
# ---------------------------------------------------------------------------------------------------------------

def write_wrapper(path, side, inputs):
    """A one-line shell wrapper giving one side its environment: fastchess has no environment flag.

    `exec` leaves no extra process behind, and /proc/self/exe still points at the real engine.
    """
    command = [expand(side["command"], inputs)] + [expand(a, inputs) for a in side.get("args", [])]
    env = [f"{key}={expand(value, inputs)}" for key, value in sorted(side.get("env", {}).items())]
    with open(path, "w") as handle:
        handle.write("#!/bin/sh\n# generated by match_core.py - gives this engine its environment\n")
        handle.write("exec " + ("env " + " ".join(shlex.quote(e) for e in env) + " " if env else "")
                     + " ".join(shlex.quote(c) for c in command) + ' "$@"\n')
    os.chmod(path, 0o755)


def option_flags(options, inputs):
    return [f"option.{name}={expand(value, inputs)}" for name, value in options.items()]


def fastchess_command(args, manifest, wrappers):
    """The fastchess argument list. Order matches tools/pawnstar-match.sh so the two routes are one measurement."""
    inputs = args.inputs
    if bool(args.depth) == bool(args.time_control):
        raise MatchError("give exactly one of --depth and --time-control")
    clock = [f"depth={args.depth}"] if args.depth else [f"tc={args.time_control}"]
    candidate_clock, baseline_clock, each_clock = [], [], clock
    if args.baseline_time_control:
        if args.depth:
            raise MatchError("--baseline-time-control needs a time control, not a depth")
        # Given on the engine and NOT in -each: -each is applied after the engines and would overwrite it.
        candidate_clock, baseline_clock, each_clock = clock, [f"tc={args.baseline_time_control}"], []
    pairs = (args.games + 1) // 2
    each = ["proto=uci", "option.Threads=1", f"option.Hash={args.hash}"]
    each += option_flags(manifest.get("each_options", {}), inputs)
    command = [args.fastchess]
    for role, clock_flags in zip(ROLES, (candidate_clock, baseline_clock)):
        command += ["-engine", f"cmd={wrappers[role]}", f"name={role}"]
        command += option_flags(manifest[role].get("options", {}), inputs) + clock_flags
    command += ["-each"] + each + each_clock
    command += ["-rounds", str(pairs), "-games", "2", "-repeat"]
    command += ["-openings", f"file={args.openings}", "format=epd", "order=sequential", f"start={args.start}"]
    command += ADJUDICATION
    command += ["-pgnout", f"file={args.pgn}"] + PGN_FLAGS
    command += ["-concurrency", str(args.concurrency)]
    return command


def probe(role, wrapper, manifest, args):
    """Start the engine the way fastchess will, make it search once, and require a bestmove.

    Generic by necessity: it proves the engine starts, takes its options and searches. That the net loaded is the
    engine's to say, through the manifest's `probe_expect`, or a preflight command that checks it properly.
    """
    inputs = args.inputs
    options = {"Threads": "1", "Hash": str(args.hash)}
    options.update({k: expand(v, inputs) for k, v in manifest.get("each_options", {}).items()})
    options.update({k: expand(v, inputs) for k, v in manifest[role].get("options", {}).items()})
    script = "uci\n"
    script += "".join(f"setoption name {name} value {value}\n" for name, value in options.items())
    script += f"isready\nposition startpos\ngo depth {PROBE_DEPTH}\n"
    process = subprocess.Popen([wrapper], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                               text=True)
    process.stdin.write(script)
    process.stdin.flush()
    output, deadline = [], time.time() + PROBE_SECONDS

    def read_lines():
        for line in process.stdout:
            output.append(line.rstrip("\n"))

    reader = threading.Thread(target=read_lines, daemon=True)
    reader.start()
    try:
        while time.time() < deadline and reader.is_alive() and not any(l.startswith("bestmove") for l in list(output)):
            time.sleep(0.05)
    finally:
        try:
            process.stdin.write("quit\n")
            process.stdin.flush()
        except (BrokenPipeError, OSError):
            pass
        try:
            process.wait(timeout=10)
        except subprocess.TimeoutExpired:
            process.kill()
    text = "\n".join(output)
    if not any(line.startswith("uciok") for line in output):
        raise MatchError(f"probe: the {role} engine never said uciok. Its output:\n{text}")
    if not any(line.startswith("bestmove") for line in output):
        raise MatchError(f"probe: the {role} engine gave no bestmove at depth {PROBE_DEPTH}. Its output:\n{text}")
    expect = manifest.get("probe_expect")
    if expect and not re.search(expect, text):
        raise MatchError(f"probe: the {role} engine's output does not match probe_expect {expect!r}:\n{text}")
    print(f"probe: {role} started, took its options and searched to depth {PROBE_DEPTH}: OK")


def command_chunk(args):
    manifest = read_manifest(args.inputs)
    os.makedirs(args.out, exist_ok=True)
    scratch = tempfile.mkdtemp(prefix="match_core.")
    wrappers = {}
    for role in ROLES:
        wrappers[role] = os.path.abspath(os.path.join(scratch, f"{role}.sh"))
        write_wrapper(wrappers[role], manifest[role], args.inputs)
    args.pgn = os.path.abspath(os.path.join(args.out, f"{args.label}.pgn"))
    args.openings = os.path.abspath(args.openings)
    args.fastchess = os.path.abspath(args.fastchess)
    command = fastchess_command(args, manifest, wrappers)

    print("machine:", os.uname().nodename)
    print("command:\n ", " ".join(shlex.quote(c) for c in command))
    sys.stdout.flush()
    if not args.skip_probe:
        for role in ROLES:
            probe(role, wrappers[role], manifest, args)
    if args.dry_run:
        print("--dry-run: nothing played")
        return 0

    log_path = os.path.join(args.out, f"{args.label}.log")
    started = time.time()
    with open(log_path, "w") as log:
        process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        output = []
        for line in process.stdout:
            sys.stdout.write(line)
            log.write(line)
            output.append(line)
        status = process.wait()
    elapsed = max(1, int(time.time() - started))
    played = re.findall(r"Games: (\d+)", "".join(output))
    print(f"\nwall time: {elapsed} s; games played: {played[-1] if played else 'unreadable'}; "
          f"fastchess exited {status}")
    if status != 0:
        raise MatchError(f"fastchess exited {status}")
    if not played or int(played[-1]) != args.games:
        raise MatchError(f"{played[-1] if played else 'no'} games played, {args.games} expected")
    return 0


# ---------------------------------------------------------------------------------------------------------------

def make_parser():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    commands = parser.add_subparsers(dest="command", required=True)

    plan = commands.add_parser("plan", help="verify the openings and write the chunk matrix")
    plan.add_argument("--inputs", required=True, help="the unpacked artifact holding manifest.json")
    plan.add_argument("--out", required=True, help="where the stripped openings.epd goes")
    plan.add_argument("--games", type=int, required=True)
    plan.add_argument("--chunks", type=int, required=True)
    plan.add_argument("--first", type=int, default=1, help="first opening, 1-based")
    plan.add_argument("--concurrency", default="2", help="games at once; a comma list is dealt out round-robin")
    plan.add_argument("--openings-file", default="", help="an opening set outside the artifact")
    plan.add_argument("--openings-sha256", default="", help="overrides the manifest's SHA-256")
    plan.add_argument("--github-output", default=os.environ.get("GITHUB_OUTPUT", ""))
    plan.set_defaults(handler=command_plan)

    chunk = commands.add_parser("chunk", help="play one slice of the match")
    chunk.add_argument("--inputs", required=True)
    chunk.add_argument("--openings", required=True, help="the stripped openings.epd the plan wrote")
    chunk.add_argument("--fastchess", required=True)
    chunk.add_argument("--start", type=int, required=True, help="first opening of this slice, 1-based")
    chunk.add_argument("--games", type=int, required=True)
    chunk.add_argument("--concurrency", type=int, default=2)
    chunk.add_argument("--hash", type=int, default=64)
    chunk.add_argument("--time-control", default="")
    chunk.add_argument("--baseline-time-control", default="")
    chunk.add_argument("--depth", default="")
    chunk.add_argument("--out", required=True, help="PGN, log and scratch land here")
    chunk.add_argument("--label", required=True, help="file stem, e.g. chunk-3")
    chunk.add_argument("--skip-probe", action="store_true", help="the caller's preflight has done it")
    chunk.add_argument("--dry-run", action="store_true")
    chunk.set_defaults(handler=command_chunk)
    return parser


def main(argv=None):
    args = make_parser().parse_args(argv)
    try:
        return args.handler(args)
    except MatchError as error:
        print(f"::error::{error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())

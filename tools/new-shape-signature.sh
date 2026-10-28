#!/bin/sh
# The cross-port gate for the SHARED NNUE SHAPE — horizontal king mirroring plus 8 material output heads
# — measured over the deterministic synthetic net at a fixed depth.
#
# WHY THIS EXISTS. The shared shape is behind a compile-time switch that defaults off (a C++ define, a
# Cargo feature, a Go build tag), and until this script landed nothing in any of the three CI systems built
# or searched at that shape: every job exercised the default. A gated path with no CI rots silently, and the
# cost of that is already measured — pawnstar-go's transposition table defaulted to 512 MB against the
# other two ports' 64 MB from May until 2026-10-27, through every release, because the only cross-port gate
# that ran was `bench` and `bench` pins the table to 64 MB in all three. A gate that controls for a
# difference cannot detect it. This path is about to carry the net the whole project depends on, so it gets
# a gate before it carries it, not after.
#
# WHAT IT CHECKS, in this order, and the order matters:
#
#   1. the synthetic net's SIZE and SHA-256, before anything searches it. The node counts below are
#      meaningless over a different net, and a drifted generator would read as a search regression — which
#      is the most expensive way to be told. Fail on the hash, not on the symptom.
#   2. the net's HEADER, decoded here from the raw bytes rather than asked of the engine, so this is an
#      independent reader against the engine's own: the RNNUE1 magic, format_version 2, the mirroring flag,
#      8 output heads, payload_bytes 12,617,760 and king_bucket_map_crc 0x0A20060D4E9586E0. Those values
#      ARE the shape; a build that stamps anything else is not at the shape this table was measured for.
#   3. the six fixed-depth node counts and best moves, against the table below.
#
# THE HASH RULE. `--hash <MB>` is MANDATORY and deliberately not defaulted, and its value is printed beside
# every node count. Table size changes the collision pattern and so the node count at a fixed depth, so a
# node count with no table size next to it is not a number anybody can compare — and the harness that first
# produced the table below set no Hash at all, which is exactly what let the 512-vs-64 default through. A
# default here would be a number the caller did not choose, i.e. another silent normalisation. The header
# also prints the build's OWN advertised default, so a reader can see when the two differ; the ports'
# advertised defaults are gated separately (each repo's UCI suite pins the line, and
# reckless-nnue/scripts/hash-default-parity.sh compares all three), because this script pins the value and
# therefore cannot see a default drift.
#
# WHY A TABLE OF CONSTANTS RATHER THAN THREE BINARIES IN ONE PLACE. A true cross-port job would need
# pawnstar, pawnstar-rs and pawnstar-go built side by side, which across three repositories means a
# cross-repo artifact download, a token with access to the others, and a job that goes red when an
# unrelated repository's CI is red or its last green run has aged out of retention. Three independent
# assertions against one shared constant catch any port drifting, at that port's own commit, with the blame
# on the change that caused it — the same mechanism as the default shape's `bench` signature, which has
# held the three ports bit-exact for years. What it cannot catch is the constant itself being wrong, and
# that is what checks 1 and 2 close: if the generator moves, the SHA-256 fails; if the shape moves, the
# header fails. Re-blessing the table in three repositories at once is then a deliberate act, and the
# `reckless-nnue` skill is the written record of what the numbers are.
#
# THIS FILE IS BYTE-IDENTICAL IN pawnstar, pawnstar-rs AND pawnstar-go, on purpose: the three ports' gate is
# one script and one table, so `diff` is the whole audit. Change it in all three or in none.
#
# Usage:
#   tools/new-shape-signature.sh --hash <MB> --net <synthetic net> [--depth N] <engine>
#
# Write the net with this port's own generator first — all three emit the same bytes, which is half the
# proof on its own, because two ports comparing node counts have to be searching the same net:
#
#   C++   cmake -S . -B build-new-shape -DPAWNSTAR_NNUE_KING_MIRRORING=ON -DPAWNSTAR_NNUE_OUTPUT_BUCKETS=8
#         build-new-shape/test_nnue_incremental --write-synthetic-net synthetic.bin
#   Rust  CARGO_TARGET_DIR=target-newshape cargo run --release --features nnue-shared-shape \
#             --bin write_synthetic_net -- synthetic.bin
#   Go    make synthetic-net NET=synthetic.bin KING_MIRRORING=1 OUTPUT_BUCKETS=8
#
# Exit status: 0 when the net and all six positions are exactly as expected, 1 on any mismatch, 2 on a
# usage or environment error.

set -u

# ---------------------------------------------------------------------------------------------------------
# The expected net. Produced by all three ports' generators from one fixed seed through fixed integer
# arithmetic, so the bytes are identical on every platform and in every port.
# ---------------------------------------------------------------------------------------------------------
EXPECTED_SHA256=4abfdb7bf3f6753fb53d8fed3b524750f7c06031cd5464445f18075f8ad4871b
EXPECTED_FILE_BYTES=12617824

# The RNNUE1 header fields that define the shared shape (offsets from reckless-nnue, "The RNNUE1 container").
EXPECTED_MAGIC=524e4e5545310000 # "RNNUE1\0\0"
EXPECTED_FORMAT_VERSION=2       # mirroring on WITH eight output heads; 1 is v12's shape
EXPECTED_FEATURES_PER_BUCKET=768
EXPECTED_KING_BUCKETS=8
EXPECTED_HIDDEN_SIZE=1024
EXPECTED_OUTPUT_BUCKETS=8
EXPECTED_FLAGS=1 # bit 0 = horizontal king mirroring
EXPECTED_MAP_CRC=0a20060d4e9586e0
EXPECTED_PAYLOAD_BYTES=12617760

DEPTH=8
HASH=
NET=
ENGINE=
failures=0

usage() {
    echo "usage: $0 --hash <MB> --net <synthetic net> [--depth N] <engine>" >&2
    exit 2
}

while [ $# -gt 0 ]; do
    case "$1" in
        --hash)    HASH="${2:-}"; shift 2 ;;
        --net)     NET="${2:-}"; shift 2 ;;
        --depth)   DEPTH="${2:-}"; shift 2 ;;
        -h|--help) usage ;;
        -*)        echo "$0: unknown option $1" >&2; usage ;;
        *)         ENGINE="$1"; shift ;;
    esac
done

[ -n "$ENGINE" ] || usage
[ -n "$NET" ] || { echo "$0: --net is required" >&2; usage; }
[ -n "$HASH" ] || {
    echo "$0: --hash <MB> is required and is deliberately not defaulted. A cross-port node count with no" >&2
    echo "$0: table size beside it is not comparable: pass the same value to every port." >&2
    usage
}
[ -f "$ENGINE" ] || { echo "$0: no engine at '$ENGINE'" >&2; exit 2; }
[ -f "$NET" ] || { echo "$0: no net at '$NET' — write it with this port's generator first" >&2; exit 2; }

# ---------------------------------------------------------------------------------------------------------
# Check 1: the net's size and SHA-256, before anything searches it.
# ---------------------------------------------------------------------------------------------------------
report() {
    # report <ok|fail> <description>
    if [ "$1" = ok ]; then
        printf 'ok    %s\n' "$2"
    else
        printf 'FAIL  %s\n' "$2"
        failures=$((failures + 1))
    fi
}

expect() {
    # expect <description> <got> <want>
    if [ "$2" = "$3" ]; then
        report ok "$1: $2"
    else
        report fail "$1: got $2, want $3"
    fi
}

sha256_of() {
    # Fed on STDIN, not named as an argument: GNU sha256sum escapes a filename that contains a backslash by
    # prefixing its whole output line with one, which on Windows (where a path is C:\...) silently turns the
    # digest into "\4abfdb7b..." and fails the comparison for the wrong reason. With no filename to print
    # there is nothing to escape. macOS has shasum rather than sha256sum.
    if command -v sha256sum >/dev/null 2>&1; then
        sha256sum < "$1" | cut -d' ' -f1
    elif command -v shasum >/dev/null 2>&1; then
        shasum -a 256 < "$1" | cut -d' ' -f1
    else
        echo "$0: neither sha256sum nor shasum is available" >&2
        exit 2
    fi
}

printf 'engine        %s\n' "$ENGINE"
printf 'net           %s\n\n' "$NET"
echo "The synthetic net — checked BEFORE it is searched, because a drifted net would read as a search"
echo "regression rather than as the generator moving."
expect 'net file bytes' "$(wc -c < "$NET" | tr -d ' ')" "$EXPECTED_FILE_BYTES"
expect 'net SHA-256' "$(sha256_of "$NET")" "$EXPECTED_SHA256"

# ---------------------------------------------------------------------------------------------------------
# Check 2: the RNNUE1 header, decoded from the raw bytes here rather than asked of the engine.
# ---------------------------------------------------------------------------------------------------------
# net_field_hex <offset> <byte count> — those bytes of the net as one big-endian hex string, i.e. the
# little-endian field decoded. Assembled byte by byte rather than with `od -tu2`/`-tu8`, whose multi-byte
# formats follow the HOST's byte order and so would read a different number on a big-endian machine.
net_field_hex() {
    od -An -tx1 -j "$1" -N "$2" "$NET" | tr -d ' \n' \
        | awk -v n="$2" '{ for (i = n; i >= 1; i--) printf "%s", substr($0, 2 * i - 1, 2) }'
}

# net_field_decimal <offset> <byte count>
net_field_decimal() {
    printf '%d' "0x$(net_field_hex "$1" "$2")"
}

echo
echo "The RNNUE1 header, read out of the file independently of the engine that wrote it. These values ARE"
echo "the shared shape; a build that stamps anything else is not at the shape this table was measured for."
# The magic is 8 ASCII bytes in file order, not a little-endian integer, so it is read forwards.
expect 'magic (RNNUE1\0\0)' "$(od -An -tx1 -j 0 -N 8 "$NET" | tr -d ' \n')" "$EXPECTED_MAGIC"
expect 'format_version' "$(net_field_decimal 8 2)" "$EXPECTED_FORMAT_VERSION"
expect 'features_per_bucket' "$(net_field_decimal 12 2)" "$EXPECTED_FEATURES_PER_BUCKET"
expect 'king_buckets' "$(net_field_decimal 14 2)" "$EXPECTED_KING_BUCKETS"
expect 'hidden_size' "$(net_field_decimal 16 2)" "$EXPECTED_HIDDEN_SIZE"
expect 'output_buckets' "$(net_field_decimal 18 2)" "$EXPECTED_OUTPUT_BUCKETS"
expect 'flags (bit 0 = mirroring)' "$(net_field_decimal 20 1)" "$EXPECTED_FLAGS"
expect 'king_bucket_map_crc' "0x$(net_field_hex 32 8)" "0x$EXPECTED_MAP_CRC"
expect 'payload_bytes' "$(net_field_decimal 40 8)" "$EXPECTED_PAYLOAD_BYTES"

if [ "$failures" -ne 0 ]; then
    echo
    echo "FAIL: the synthetic net is not the one the node counts below were measured over, so there is no"
    echo "      point searching it. Fix the generator or the shape — do NOT re-bless the node counts."
    exit 1
fi

# ---------------------------------------------------------------------------------------------------------
# Check 3: the six fixed-depth node counts.
# ---------------------------------------------------------------------------------------------------------
# Single-threaded through both routes each port offers, because the node count is only a signature when the
# search is serial. PAWNSTAR_EVALFILE is how the C++ and Rust engines are given a net at startup, which they
# need: at this shape they rightly REFUSE the shipped v12 net and, since a refused net is fatal rather than a
# silent fall back to the embedded one, a build started without it exits before answering `uci`. The Go port
# takes the net through `setoption name EvalFile` below, which all three accept.
export PAWNSTAR_THREADS=1
export PAWNSTAR_EVALFILE="$NET"

advertised=$(printf 'uci\nquit\n' | "$ENGINE" 2>/dev/null | tr -d '\r' \
    | sed -n 's/^option name Hash type spin default \([0-9]*\).*/\1/p' | head -1)

# GNU sleep and Git Bash's both take a fraction; a strict POSIX sleep does not, so fall back to whole
# seconds rather than spinning on an error.
if sleep 0.2 2>/dev/null; then POLL=0.2; else POLL=1; fi

echo
printf 'hash          %s MB, pinned by setoption (this build advertises default %s)\n' \
    "$HASH" "${advertised:-UNKNOWN}"
printf 'depth         %s, single-threaded, book off\n' "$DEPTH"
printf 'poll          %ss, waiting for bestmove (never a fixed sleep)\n\n' "$POLL"

work=$(mktemp -d) || { echo "$0: cannot make a working directory" >&2; exit 2; }
trap 'rm -rf "$work"' EXIT HUP INT TERM

check_one() {
    # check_one <expected nodes> <expected best move> <position>
    want_nodes="$1"
    want_best="$2"
    pos="$3"
    out="$work/engine.out"
    : > "$out"
    # `quit` is sent only once `bestmove` has appeared in the engine's OWN output. `go` hands the search to
    # a worker thread, so a `quit` on its heels cancels it and still prints a bestmove — from a search that
    # never finished, which is a plausible-looking wrong number. A fixed sleep instead would be both slower
    # than it needs to be and flaky on a loaded runner, and this job gates merges, so it must not be flaky.
    { printf 'setoption name Hash value %s\n' "$HASH"
      printf 'setoption name Threads value 1\n'
      printf 'setoption name OwnBook value false\n'
      printf 'setoption name EvalFile value %s\n' "$NET"
      printf 'isready\n'
      printf 'position %s\ngo depth %s\n' "$pos" "$DEPTH"
      waited=0
      while ! grep -q '^bestmove ' "$out" 2>/dev/null; do
          sleep "$POLL"
          waited=$((waited + 1))
          # A deadline, so a wedged engine fails the job instead of hanging it. The slowest of these six
          # positions is about 1.1M nodes, seconds on any machine this runs on.
          [ "$waited" -lt 1800 ] || break
      done
      printf 'quit\n'; } | "$ENGINE" > "$out" 2>/dev/null
    text=$(tr -d '\r' < "$out")
    # Two kinds of line begin "info depth N": the completed iteration, and a currmove progress report. Only
    # the first carries `nodes`, so filter on that — taking the last matching line alone picks up a currmove
    # report and reads as a missing node count on a search that in fact finished. The info line is
    # right-aligned, so squeeze the runs of spaces before splitting it.
    nodes=$(printf '%s\n' "$text" | tr -s ' ' | grep "^info depth $DEPTH " | grep ' nodes ' | tail -1 \
            | sed -n 's/.*nodes \([0-9]*\).*/\1/p')
    best=$(printf '%s\n' "$text" | grep '^bestmove ' | tail -1 | cut -d' ' -f2)
    nodes=${nodes:-MISSING}
    best=${best:-MISSING}
    # The Hash value is printed on EVERY line, not stated once at the top: a line of this output copied into
    # a report or a commit message has to carry the table size with it or it is not a comparable number.
    if [ "$nodes" = "$want_nodes" ] && [ "$best" = "$want_best" ]; then
        printf 'ok    hash %-5s nodes %-9s bestmove %-6s %s\n' "$HASH" "$nodes" "$best" "$pos"
    else
        printf 'FAIL  hash %-5s nodes %-9s bestmove %-6s %s\n' "$HASH" "$nodes" "$best" "$pos"
        printf '      expected  nodes %-9s bestmove %-6s\n' "$want_nodes" "$want_best"
        failures=$((failures + 1))
    fi
}

# The six positions and their reference results: depth 8, single-threaded, book off, Hash 64 MB, over the
# synthetic net checked above. Reproduced by all three ports — C++ (clang 22 on Windows, clang 18 on
# reckless-ci-lin-2), Rust and Go — on 2026-10-27. These are the numbers; do not adjust them to match a
# build. The last two put the kings on d1/d8 and e1/e8, either side of the mirror boundary, so the search
# spends its time in positions whose perspectives keep crossing it: the case that is SILENT when the
# incremental refresh rule is wrong, because on the mirrored map a king stepping d1->e1 keeps its bucket
# while every feature row that perspective reads moves.
check_one 2709    g1f3 'startpos'
check_one 1100203 f3d3 'fen r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1'
check_one 117442  b4f4 'fen 8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1'
check_one 183574  a1b1 'fen r3k2r/pb3p2/5npp/n2p4/1p1PPB2/6P1/P2N1PBP/R3K2R w KQkq - 0 1'
check_one 125721  e2e4 'fen 3k4/pppppppp/8/8/8/8/PPPPPPPP/3K4 w - - 0 1'
check_one 134448  e2e3 'fen 4k3/pppppppp/8/8/8/8/PPPPPPPP/4K3 w - - 0 1'

echo
if [ "$failures" -ne 0 ]; then
    echo "FAIL: $failures check(s) did not match at the shared NNUE shape, Hash $HASH MB, depth $DEPTH."
    echo "      The three ports are bit-exact with each other at a fixed depth with the table sizes equal,"
    echo "      so a mismatch here is a divergence from the other two, not a local curiosity. Treat it as a"
    echo "      configuration question before a code one — compare the advertised Hash default first"
    echo "      (reckless-nnue/scripts/hash-default-parity.sh) — and never re-bless the table to quieten it."
    exit 1
fi

echo "OK: the shared NNUE shape searches identically to the other two ports — 6/6 positions at depth"
echo "    $DEPTH, Hash $HASH MB, over synthetic net $EXPECTED_SHA256."
exit 0

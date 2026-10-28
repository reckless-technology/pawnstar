#!/usr/bin/env bash
# Run an SPRT between two NNUE nets with fastchess. Requires fastchess on PATH.
#
# NNUE is Pawnstar's only evaluator (the handcrafted eval was removed), so an SPRT always compares two
# nets: the candidate ($NET) against a baseline ($BASELINE_NET). Net selection is purely via the EvalFile
# UCI option.
#
# Usage: run_sprt.sh <net.bin> <openings.epd> [rounds] [depth]
# Env:
#   BASELINE_NET   REQUIRED: the opponent net (NNUE with this net).
#   TC             time control (e.g. "8+0.08"); if set, used instead of fixed depth. Fixed depth
#                  isolates eval quality; time control measures real strength incl. the speed cost of a
#                  wider/heavier net.
#   BIN_A, BIN_B   engine binaries for the candidate / baseline (default ./build/pawnstar for both).
#                  Different net widths need different builds (kHiddenSize is compile-time), so a
#                  cross-width SPRT points BIN_A and BIN_B at the two separately-built binaries.
#   CAND_STARTUP_NET  for a cross-arch candidate (BIN_A) whose default/embedded net is incompatible: a net
#                  to give BIN_A at *startup* (via PAWNSTAR_EVALFILE) so it survives the UCI handshake.
#                  Without it, such a candidate fatal-exits before the per-engine EvalFile is applied.
#   ELO0, ELO1     SPRT hypothesis bounds (default 0 / 10).
#   PROBE_DEPTH    depth for the pre-flight search per engine (default 8). See "Pre-flight" below.
#
# Pre-flight. Before a single game is played this script prints both nets' RNNUE1 headers and then starts
# each engine exactly as fastchess will, hands it its net through `setoption name EvalFile`, and makes it
# search - refusing to run the match if either engine refused its net or did not really search. That is
# cheap insurance against the one failure that wastes a whole SPRT: an engine that keeps a different net
# and plays on. Both halves of it have been got wrong here before, and both are worth naming:
#
#   - a probe that pipes `quit` straight after `go` CANCELS the search it is meant to be proving. Measured:
#     `go depth 8` with an immediate `quit` returned `b1a3` instantly with no `info` lines at all, where the
#     same position with the quit delayed searched 33,388 nodes and played `g1f3`. So this probe waits for
#     an `info depth` line AND the `bestmove` before it sends `quit`, and prints the node count it saw.
#   - a header display that decodes the retired 32-byte `PSN1` layout prints garbage for an RNNUE1 net
#     (`magic=RNNU v12613/in0/buckets2/hidden64/...`) - plausible-looking garbage, which is worse than
#     none. The field offsets below are the 64-byte RNNUE1 ones (nnue/README.md section 2, src/nnue.h).
set -euo pipefail

NET="${1:?net.bin}"
OPENINGS="${2:?openings.epd}"
ROUNDS="${3:-500}"
DEPTH="${4:-8}"
BASELINE_NET="${BASELINE_NET:?set BASELINE_NET=<baseline net.bin> (NNUE is the only evaluator; SPRT is net-vs-net)}"

REPO="$(cd "$(dirname "$0")/.." && pwd)"
# build/pawnstar is the stable symlink the build maintains to the latest version-named binary
# (pawnstar_<major>_<minor>_<build>). Override with BIN_A / BIN_B to A/B two specific versions.
PS="$REPO/build/pawnstar"
BIN_A="${BIN_A:-$PS}"
BIN_B="${BIN_B:-$PS}"
ELO0="${ELO0:-0}"
ELO1="${ELO1:-10}"
PROBE_DEPTH="${PROBE_DEPTH:-8}"

# Scratch files this run creates, removed on the way out however it ends. One trap for the lot: a second
# `trap ... EXIT` replaces the first rather than adding to it, which would silently leak whichever file was
# registered earlier.
SCRATCH_FILES=()
SCRATCH_DIRS=()
trap 'if [ ${#SCRATCH_FILES[@]} -gt 0 ]; then rm -f "${SCRATCH_FILES[@]}"; fi
      if [ ${#SCRATCH_DIRS[@]} -gt 0 ]; then rm -rf "${SCRATCH_DIRS[@]}"; fi' EXIT

# fastchess hands every non-empty line of an EPD file to its FEN parser, so a '#' provenance header - which
# nnue/openings.epd carries, saying where its positions came from and that their source is public domain -
# would be read as a position. Strip comments and blank lines into a scratch copy and give fastchess that.
# A file with no comments is passed straight through, so this is invisible to any other opening set.
if grep -q '^[[:space:]]*#' "$OPENINGS"; then
    STRIPPED_OPENINGS="$(mktemp "${TMPDIR:-/tmp}/sprt_openings.XXXXXX.epd")"
    SCRATCH_FILES+=("$STRIPPED_OPENINGS")
    grep -v '^[[:space:]]*#' "$OPENINGS" | grep -v '^[[:space:]]*$' > "$STRIPPED_OPENINGS"
    echo "run_sprt.sh: stripped the comment header from $OPENINGS ($(wc -l < "$STRIPPED_OPENINGS") openings)"
    OPENINGS="$STRIPPED_OPENINGS"
fi

# Single-threaded engines: faster per node, avoids Lazy SMP non-determinism, frees cores for concurrency.
export PAWNSTAR_THREADS=1
# The net distinction must come ONLY from the per-engine EvalFile options below; make sure neither side
# inherits a net path from the environment.
unset PAWNSTAR_NNUE PAWNSTAR_EVALFILE

# Cross-arch SPRT: when BIN_A is built for a *different* architecture than its default/embedded net, it
# rejects that net and fatal-exits during the UCI handshake — before fastchess can send setoption EvalFile.
# Set CAND_STARTUP_NET=<net for BIN_A> to wrap the candidate so it loads a compatible net at *startup* (via
# PAWNSTAR_EVALFILE); the per-engine EvalFile below still applies normally. The baseline (BIN_B) is assumed
# to start fine on its own embedded/default net. Unset => behaviour is unchanged.
CAND_CMD="$BIN_A"
if [ -n "${CAND_STARTUP_NET:-}" ]; then
    CAND_CMD="$(mktemp "${TMPDIR:-/tmp}/sprt_cand_wrapper.XXXXXX.sh")"
    SCRATCH_FILES+=("$CAND_CMD")
    printf '#!/bin/sh\nexec env PAWNSTAR_EVALFILE=%q %q "$@"\n' "$CAND_STARTUP_NET" "$BIN_A" > "$CAND_CMD"
    chmod +x "$CAND_CMD"
fi

# Per-engine clock spec: a fixed search depth (isolates eval quality) unless TC is set (measures real
# strength including the speed cost of a heavier net). Built as an array so it expands to one -each token.
if [ -n "${TC:-}" ]; then
    tc_args=(tc="$TC")
else
    tc_args=(depth="$DEPTH")
fi

# --- Pre-flight: the nets' headers, then each engine really searching with the net it is given. ---------

# Print a net's 64-byte RNNUE1 header, field by field, little-endian throughout (nnue/README.md section 2:
# magic at 0, format_version 8, header_bytes 10, features_per_bucket 12, king_buckets 14, hidden_size 16,
# output_buckets 18, flags 20, activation 21, qa 22, qb 24, eval_scale 26, reserved 28, king_bucket_map_crc
# 32, payload_bytes 40, payload_crc 48, header_crc 56). Two nets of the same size are then a handful of
# numbers to compare rather than two arrays to read. @param label what to call it. @param net the file.
read_net_header() {
    local label="$1" net="$2" bytes magic file_bytes payload_present
    if [ ! -r "$net" ]; then
        echo "run_sprt.sh: the $label net '$net' cannot be read" >&2
        return 1
    fi
    read -r -a bytes <<< "$(od -An -v -tu1 -N64 "$net" | tr '\n' ' ')"
    if [ "${#bytes[@]}" -lt 64 ]; then
        echo "run_sprt.sh: the $label net '$net' is ${#bytes[@]} bytes, too short for the 64-byte RNNUE1 header" >&2
        return 1
    fi
    # Little-endian integer of `width` bytes at `off`. bytes[] is this function's local, visible here
    # because bash scopes dynamically; a 64-bit value above 2^63 wraps negative, which prints correctly
    # as unsigned with %x.
    net_le() {
        local off="$1" width="$2" value=0 i
        for ((i = width - 1; i >= 0; i--)); do value=$((value * 256 + bytes[off + i])); done
        printf '%s' "$value"
    }
    local mirroring=off
    if ((bytes[20] & 1)); then mirroring=on; fi
    magic=$(head -c 8 "$net" | tr -d '\000' | tr -c '[:print:]' '?')
    file_bytes=$(wc -c < "$net")
    payload_present=$((file_bytes - 64))
    printf 'run_sprt.sh: %s net %s\n' "$label" "$net"
    printf '  %-20s %s\n' magic "$magic"
    printf '  %-20s %s\n' format_version "$(net_le 8 2)"
    printf '  %-20s %s\n' header_bytes "$(net_le 10 2)"
    printf '  %-20s %s\n' features_per_bucket "$(net_le 12 2)"
    printf '  %-20s %s\n' king_buckets "$(net_le 14 2)"
    printf '  %-20s %s\n' hidden_size "$(net_le 16 2)"
    printf '  %-20s %s\n' output_buckets "$(net_le 18 2)"
    printf '  %-20s %s (king mirroring %s)\n' flags "$(net_le 20 1)" "$mirroring"
    printf '  %-20s %s\n' activation "$(net_le 21 1)"
    printf '  %-20s %s\n' qa "$(net_le 22 2)"
    printf '  %-20s %s\n' qb "$(net_le 24 2)"
    printf '  %-20s %s\n' eval_scale "$(net_le 26 2)"
    printf '  %-20s 0x%016x\n' king_bucket_map_crc "$(net_le 32 8)"
    printf '  %-20s %s\n' payload_bytes "$(net_le 40 8)"
    printf '  %-20s 0x%016x\n' payload_crc "$(net_le 48 8)"
    printf '  %-20s 0x%016x\n' header_crc "$(net_le 56 8)"
    printf '  %-20s %s bytes (%s after the header)\n' file "$file_bytes" "$payload_present"
    # Said as a note rather than a refusal: the engine checks this properly, and the probe below is what
    # decides. Printing it keeps a truncated or an appended-to file from looking sound in this report.
    if [ "$payload_present" != "$(net_le 40 8)" ]; then
        printf '  NOTE: the bytes after the header do not match payload_bytes - this net will be refused.\n'
    fi
    if [ $((bytes[28] | bytes[29] | bytes[30] | bytes[31])) -ne 0 ]; then
        printf '  NOTE: reserved is not zero - this net will be refused.\n'
    fi
}

# Start one engine exactly as fastchess will, give it its net, and make it search. Fails - and so stops the
# whole match - if the engine refused the net it was handed, or if it produced no `info depth` line, which
# is what "it did not really search" looks like from outside.
# @param label what to call it. @param cmd the engine command. @param net the net to set.
probe_engine() {
    local label="$1" cmd="$2" net="$3"
    local work out err in_fd out_fd pid bestmove nodes loaded
    work=$(mktemp -d "${TMPDIR:-/tmp}/sprt_probe.XXXXXX")
    SCRATCH_DIRS+=("$work")
    out="$work/out" # a transcript of its stdout, for the failure report
    err="$work/err" # its stderr, which is where a refusal a GUI would miss still lands
    : > "$out"
    : > "$err"
    # A coprocess, so every answer is read before the next command is sent - the same shape test/uci_test.sh
    # uses, and the reason a `quit` can never overtake a search here.
    coproc PROBE { "$cmd" 2> "$err"; }
    pid=$PROBE_PID
    in_fd=${PROBE[1]}
    out_fd=${PROBE[0]}

    # Read lines until one matches $1, up to $2 seconds between lines, keeping the transcript. Non-zero if
    # it times out or the engine closes its output, so a dead engine is not waited out.
    probe_wait() {
        local re="$1" limit="$2" line
        while IFS= read -r -t "$limit" line <&"$out_fd"; do
            printf '%s\n' "$line" >> "$out"
            if [[ "$line" =~ $re ]]; then return 0; fi # spelled out, not `&& return 0`, under set -e
        done
        return 1
    }
    probe_fail() {
        echo "run_sprt.sh: pre-flight FAILED for the $label engine ($cmd) with net '$net': $1" >&2
        echo "--- its stdout ---" >&2
        cat "$out" >&2
        echo "--- its stderr ---" >&2
        cat "$err" >&2
        exec {in_fd}>&- {out_fd}<&-
        kill "$pid" 2> /dev/null || true
        wait "$pid" 2> /dev/null || true
        return 1
    }

    printf 'uci\n' >&"$in_fd"
    probe_wait '^uciok' 30 || probe_fail "no uciok" || return 1
    printf 'isready\n' >&"$in_fd"
    probe_wait '^readyok' 30 || probe_fail "no readyok after uci" || return 1
    printf 'setoption name EvalFile value %s\n' "$net" >&"$in_fd"
    printf 'isready\n' >&"$in_fd"
    probe_wait '^readyok' 30 || probe_fail "no readyok after setoption EvalFile" || return 1

    # **The refusal check.** This is only possible because a refusal is loud: it reaches stdout as an `info
    # string` and stderr as a plain line, and names the net the engine kept instead. An engine quietly
    # playing a net other than the one it was given is what wastes a whole SPRT, and it is now visible from
    # outside - which is the entire reason this check can exist.
    if grep -q 'NNUE: refusing net' "$out" "$err"; then
        probe_fail "it REFUSED the net it was given, so it would play a different net for the whole match" ||
            return 1
    fi
    if grep -q 'keeping the net already loaded' "$out" "$err"; then
        probe_fail "it kept a different net - the net for this side is not in play" || return 1
    fi

    # Now a real search. Two things have to be out of the way first, and the second one bit this probe on
    # its first run: the engine's own book is ON by default, so `go` from the start position answered
    # `bestmove c2c4` instantly with no `info` lines at all - indistinguishable from an engine that did not
    # search. The book is turned off here for the probe only; the match itself is unaffected. And `position`
    # is set explicitly rather than relying on whatever the engine starts at.
    printf 'setoption name OwnBook value false\n' >&"$in_fd"
    printf 'setoption name Threads value 1\n' >&"$in_fd"
    printf 'position startpos\n' >&"$in_fd"
    printf 'isready\n' >&"$in_fd"
    probe_wait '^readyok' 30 || probe_fail "no readyok after position startpos" || return 1
    # `quit` goes only after the bestmove: sending it after `go` cancels the search and the probe then
    # proves nothing (see the note at the top of this script).
    printf 'go depth %s\n' "$PROBE_DEPTH" >&"$in_fd"
    probe_wait '^info depth' 180 ||
        probe_fail "no 'info depth' line at depth $PROBE_DEPTH - it did not search" || return 1
    probe_wait '^bestmove' 600 || probe_fail "an info line but no bestmove at depth $PROBE_DEPTH" || return 1
    printf 'quit\n' >&"$in_fd"
    exec {in_fd}>&- {out_fd}<&-
    wait "$pid" 2> /dev/null || true

    # `|| true` on each: `pipefail` is set, so a grep that matches nothing would otherwise end the script
    # here - and whether the engine logged a loaded-net line is a thing to report, not to die over.
    bestmove=$(grep '^bestmove' "$out" | tail -1 || true)
    nodes=$(sed -nE "s/^info depth +$PROBE_DEPTH .* nodes +([0-9]+).*/\1/p" "$out" | tail -1 || true)
    loaded=$(grep "NNUE: loaded net" "$err" | tail -1 || true)
    printf "run_sprt.sh: pre-flight OK for the %s engine: depth %s, %s nodes, '%s'%s\n" \
        "$label" "$PROBE_DEPTH" "${nodes:-?}" "$bestmove" "${loaded:+ [${loaded#*NNUE: }]}"
}

read_net_header candidate "$NET"
read_net_header baseline "$BASELINE_NET"
probe_engine candidate "$CAND_CMD" "$NET"
probe_engine baseline "$BIN_B" "$BASELINE_NET"

# The two engines differ ONLY by their EvalFile (the net). -games 2 -repeat plays each opening twice with
# colours reversed; the draw/resign adjudication ends decided games early to save time without biasing the
# result; -sprt stops as soon as elo0 vs elo1 is decided at the given alpha/beta error rates.
fastchess \
    -engine cmd="$CAND_CMD" name=cand option.EvalFile="$NET" \
    -engine cmd="$BIN_B" name=base option.EvalFile="$BASELINE_NET" \
    -each proto=uci "${tc_args[@]}" \
    -rounds "$ROUNDS" -games 2 -repeat \
    -openings file="$OPENINGS" format=epd order=random \
    -draw movenumber=40 movecount=8 score=20 -resign movecount=3 score=600 \
    -sprt elo0="$ELO0" elo1="$ELO1" alpha=0.05 beta=0.05 \
    -concurrency "$(nproc)"

#!/usr/bin/env bash
# UCI protocol regression test. Drives the actual engine binary over stdin/stdout with synchronized I/O
# (read the engine's responses before sending the next command, so `quit` never cancels a running search)
# and asserts on the protocol behaviour. Usage: test/uci_test.sh [path-to-engine]   (default build/pawnstar)
#
# Why a script and not a C++ unit test: these features (seldepth / currmove / searchmoves, the uciok/readyok
# handshake) are about the engine's behaviour *as a UCI process*, so the only faithful test drives the
# built binary the way a GUI does.
set -u

ENGINE="${1:-build/pawnstar}"
if [ ! -x "$ENGINE" ]; then
    echo "[FAIL] UCI: engine binary '$ENGINE' not found or not executable (run 'make' first)"
    exit 1
fi

fails=0
pass() { echo "[PASS] $1"; }
fail() {
    echo "[FAIL] $1"
    fails=$((fails + 1))
}

# Drive the engine through a coprocess so responses can be read synchronously.
coproc ENG { "$ENGINE" 2>/dev/null; }
send() { printf '%s\n' "$1" >&"${ENG[1]}"; }

# read_until <extended-regex> <timeout-seconds>: echo every line received until one matches the regex
# (then stop). Returns non-zero if the timeout elapses without a match.
read_until() {
    local re="$1" timeout="${2:-15}" line
    while IFS= read -r -t "$timeout" line <&"${ENG[0]}"; do
        printf '%s\n' "$line"
        [[ "$line" =~ $re ]] && return 0
    done
    return 1
}

# 1) uci handshake: id + option advertisements + uciok
send "uci"
out=$(read_until '^uciok' 10)
echo "$out" | grep -q '^id name Pawnstar' && pass "uci: id name" || fail "uci: missing 'id name Pawnstar'"
echo "$out" | grep -q '^uciok' && pass "uci: uciok" || fail "uci: missing uciok"
echo "$out" | grep -q 'option name Threads' && pass "uci: advertises Threads option" || fail "uci: no Threads option"

# 2) isready / readyok handshake
send "isready"
read_until '^readyok' 10 >/dev/null && pass "isready: readyok" || fail "isready: no readyok"

# Deterministic, real searches: single thread and no opening book (otherwise `go` returns a book move
# instantly, before any search, and emits no info lines).
send "setoption name Threads value 1"
send "setoption name OwnBook value false"

# 3) info seldepth: present on a real search and >= the nominal depth (quiescence extends beyond it).
send "ucinewgame"
send "position startpos"
send "go depth 12"
out=$(read_until '^bestmove' 120)
# The last iteration's summary line: once a search runs past the ~3 s currmove gate (a slow or
# instrumented build), `info depth N currmove ...` lines follow it and carry no seldepth.
last_info=$(echo "$out" | grep '^info depth.* seldepth ' | tail -1)
if [ -n "$last_info" ]; then
    d=$(echo "$last_info" | sed -E 's/.*info depth +([0-9]+).*/\1/')
    s=$(echo "$last_info" | sed -E 's/.*seldepth +([0-9]+).*/\1/')
    if [ -n "$s" ] && [ "$s" -ge "$d" ]; then
        pass "info seldepth present and >= depth (seldepth $s >= depth $d)"
    else
        fail "info seldepth bad/absent: '$last_info'"
    fi
else
    fail "go depth 12 produced no info lines (book not disabled? search not run?)"
fi

# 4) go searchmoves restricts the root. Force a move the engine would never choose (a2a3); if the filter
#    works the bestmove must be exactly that. (A normal search from startpos returns a real developing move.)
send "position startpos"
send "go searchmoves a2a3 depth 10"
out=$(read_until '^bestmove' 120)
bm=$(echo "$out" | grep '^bestmove')
echo "$bm" | grep -q '^bestmove a2a3' && pass "go searchmoves restricts root (forced a2a3)" || fail "searchmoves: expected 'bestmove a2a3', got '$bm'"

# 5) info currmove / currmovenumber: emitted as each root move begins, once a search has run past the ~3s reporting
#    gate. The search runs until the first such line (or the timeout), then stops. No fixed movetime: a single root
#    move can grow its node count several times over from one iteration to the next, so on a slow machine - a small
#    shared runner, an instrumented coverage build - it can straddle the whole of any fixed window past the gate and
#    leave nothing to report inside it.
send "position startpos"
send "go infinite"
read_until 'currmove [a-h][1-8][a-h][1-8].* currmovenumber [0-9]' 120 >/dev/null &&
    pass "info currmove/currmovenumber emitted on a long search" || fail "currmove/currmovenumber not emitted"
send "stop"
read_until '^bestmove' 30 >/dev/null || fail "currmove: no bestmove after stop"

# 5b) A `stop` written straight after `go`, with no pause, ends the search every time. The root search once
#     cleared the cancel flag as it began, so a stop that reached the engine before its worker got that far was
#     lost and the engine never answered (every iteration hung). A lost stop wedges the engine, so it is killed.
is_engine_wedged=0
stops_answered=0
for _ in $(seq 1 200); do
    send "position startpos"
    send "go infinite"
    send "stop"
    if ! read_until '^bestmove' 30 >/dev/null; then
        is_engine_wedged=1
        break
    fi
    stops_answered=$((stops_answered + 1))
done
[ "$stops_answered" -eq 200 ] && pass "go infinite + stop at once: 200/200 answered" ||
    fail "go infinite + stop at once: only $stops_answered/200 answered before a stop was lost"

# 5c) A `go` value that is not an integer, or missing, is ignored with its keyword - it once ended the engine
#     (std::stoi threw) - so each of these is a plain `go`, which `stop` ends.
for line in "go depth abc" "go movetime" "go wtime x btime y" "go nodes 12x"; do
    [ "$is_engine_wedged" -eq 0 ] || break
    send "position startpos"
    send "$line"
    send "stop"
    if read_until '^bestmove' 30 >/dev/null; then
        pass "$line: the engine survives and stop answers"
    else
        fail "$line: no bestmove after stop (the engine died or hung)"
        is_engine_wedged=1
    fi
done
send "isready"
[ "$is_engine_wedged" -eq 0 ] && read_until '^readyok' 10 >/dev/null && pass "still responsive after malformed go values" ||
    fail "not responsive after the stop and malformed-value checks"

if [ "$is_engine_wedged" -eq 0 ]; then
    send "quit"
    wait "$ENG_PID" 2>/dev/null
else
    kill -9 "$ENG_PID" 2>/dev/null
fi

# 5d) `quit` written straight after `go infinite`: the engine exits every time (a lost quit left it searching for
#     ever). A fresh process each time, killed after 30 s if it has not exited; the first that hangs ends the loop.
quits_exited=0
for _ in $(seq 1 20); do
    printf 'setoption name OwnBook value false\nposition startpos\ngo infinite\nquit\n' | "$ENGINE" >/dev/null 2>&1 &
    quit_pid=$!
    for _ in $(seq 1 300); do
        kill -0 "$quit_pid" 2>/dev/null || break
        sleep 0.1
    done
    if kill -0 "$quit_pid" 2>/dev/null; then
        kill -9 "$quit_pid" 2>/dev/null
        wait "$quit_pid" 2>/dev/null
        break
    fi
    wait "$quit_pid" 2>/dev/null
    quits_exited=$((quits_exited + 1))
done
[ "$quits_exited" -eq 20 ] && pass "go infinite + quit at once: the engine exits, 20/20" ||
    fail "go infinite + quit at once: an engine never exited (after $quits_exited that did)"

# 6) bench determinism: single-threaded bench is deterministic, so two runs must agree (guards the
#    uninitialised-read class of bug that made node counts wobble). Run as a plain pipe — bench is
#    synchronous on the main thread and quits cleanly.
b1=$(printf 'bench\nquit\n' | "$ENGINE" 2>/dev/null | grep -oE '^[0-9]+ nodes' | grep -oE '^[0-9]+')
b2=$(printf 'bench\nquit\n' | "$ENGINE" 2>/dev/null | grep -oE '^[0-9]+ nodes' | grep -oE '^[0-9]+')
if [ -n "$b1" ] && [ "$b1" = "$b2" ]; then
    pass "bench is deterministic across runs ($b1 nodes)"
else
    fail "bench nondeterministic: run1='$b1' run2='$b2'"
fi

# 7) Startup: where the engine finds its net and book, and the copies embedded in the binary when it finds neither.
#    Each run answers `isready` and quits; the lines before `ready` say which copies were loaded.
engine_dir=$(cd "$(dirname "$ENGINE")" && pwd)
engine_abs="$engine_dir/$(basename "$ENGINE")"
scratch=$(mktemp -d)
trap 'rm -rf "$scratch"' EXIT

# From a directory holding neither: both are found beside the executable's directory (the repository root, for
# build/pawnstar), so neither embedded copy is needed.
out=$(cd "$scratch" && printf 'isready\nquit\n' | "$engine_abs" 2>/dev/null)
if echo "$out" | grep -q '^readyok' && ! echo "$out" | grep -q 'embedded'; then
    pass "startup elsewhere: net and book found relative to the executable"
else
    fail "startup elsewhere: expected readyok with no embedded fallback, got: $out"
fi

# A copy of the executable alone in an empty directory: the embedded book and the embedded net. (A sanitized Windows
# build loads its runtime DLL from beside the executable, so any DLLs there go with it.)
case "$ENGINE" in *.exe) copy="$scratch/engine_copy.exe" ;; *) copy="$scratch/engine_copy" ;; esac
cp -L "$ENGINE" "$copy"
cp "$engine_dir"/*.dll "$scratch"/ 2>/dev/null || true
out=$(cd "$scratch" && printf 'bookmoves\neval\nisready\nquit\n' | "$copy" 2>/dev/null)
echo "$out" | grep -q 'using the embedded book' && pass "startup alone: falls back to the embedded book" ||
    fail "startup alone: no embedded-book message in: $out"
echo "$out" | grep -q 'using the embedded net' && pass "startup alone: falls back to the embedded net" ||
    fail "startup alone: no embedded-net message in: $out"
echo "$out" | grep -q '^MOVE   COUNT' && echo "$out" | grep -q '^evaluation ' && echo "$out" | grep -q '^readyok' &&
    pass "startup alone: the embedded book and net both work" || fail "startup alone: book/eval/readyok missing: $out"

# PAWNSTAR_EVALFILE names the net: a missing file falls back to the embedded net, the shipped file loads directly.
out=$(printf 'isready\nquit\n' | PAWNSTAR_EVALFILE=no/such/net.bin "$ENGINE" 2>/dev/null)
echo "$out" | grep -q 'using the embedded net' && echo "$out" | grep -q '^readyok' &&
    pass "PAWNSTAR_EVALFILE=<missing>: the embedded net" || fail "PAWNSTAR_EVALFILE=<missing>: got: $out"
out=$(printf 'eval\nquit\n' | PAWNSTAR_EVALFILE=nnue/pawnstar-v12.bin "$ENGINE" 2>/dev/null)
echo "$out" | grep -q '^evaluation ' && ! echo "$out" | grep -q 'embedded' &&
    pass "PAWNSTAR_EVALFILE=<the shipped net>: loaded from the file" || fail "PAWNSTAR_EVALFILE=<net>: got: $out"

# A net that loads says so, as a refusal does: `info string` on stdout, where a UCI GUI reads it, and the plain
# line on stderr, which a match harness keeps. It was once written to stderr alone with the `info string` prefix
# on it - a protocol line on the wrong stream - so no GUI ever saw which net was playing.
out=$(printf 'isready\nquit\n' | PAWNSTAR_EVALFILE=nnue/pawnstar-v12.bin "$ENGINE" 2>"$scratch/loaded_net.err")
err=$(cat "$scratch/loaded_net.err")
echo "$out" | grep -qxF "info string NNUE: loaded net 'nnue/pawnstar-v12.bin'" &&
    pass "startup: stdout says, as an info string, which net loaded" ||
    fail "startup: stdout lacks the loaded-net info string: $out"
echo "$err" | grep -qxF "NNUE: loaded net 'nnue/pawnstar-v12.bin'" &&
    pass "startup: stderr carries the loaded net too, with no protocol prefix" ||
    fail "startup: stderr lacks the plain loaded-net line: $err"
echo "$err" | grep -q 'info string' &&
    fail "startup: stderr carries an info string, a protocol line on the wrong stream: $err" ||
    pass "startup: no info string on stderr"

# A net file that EXISTS and is REFUSED is fatal: the engine must not fall back to the embedded net and
# must not play. This is the trap the fallback used to leave open — a mismatched net through
# PAWNSTAR_EVALFILE started an engine that played the embedded net instead, so a match reported a plausible
# Elo for a net that never played a move. The refusal must reach stderr too, because a match harness keeps
# stderr and routinely drops `info string` lines.
refused_net="$scratch/refused_net.bin"
# A readable file that is not a net at all: read, no RNNUE1 magic, refused.
printf 'this is not a net, it is just some bytes' > "$refused_net"
out=$(printf 'isready\nquit\n' | PAWNSTAR_EVALFILE="$refused_net" "$ENGINE" 2>"$scratch/refused_net.err")
status=$?
err=$(cat "$scratch/refused_net.err")
[ "$status" -ne 0 ] && pass "PAWNSTAR_EVALFILE=<a refused net>: exits non-zero" ||
    fail "PAWNSTAR_EVALFILE=<a refused net>: expected a non-zero exit, got $status with: $out"
! echo "$out" | grep -q 'readyok' && pass "PAWNSTAR_EVALFILE=<a refused net>: never reaches readyok" ||
    fail "PAWNSTAR_EVALFILE=<a refused net>: answered readyok with no net: $out"
! echo "$out" | grep -q 'using the embedded net' &&
    pass "PAWNSTAR_EVALFILE=<a refused net>: does not fall back to the embedded net" ||
    fail "PAWNSTAR_EVALFILE=<a refused net>: fell back to the embedded net: $out"
echo "$err" | grep -q 'refusing net' && echo "$err" | grep -q 'FATAL' &&
    pass "PAWNSTAR_EVALFILE=<a refused net>: the reason and the FATAL are on stderr" ||
    fail "PAWNSTAR_EVALFILE=<a refused net>: stderr lacks the reason: $err"
# A truncated copy of the real net: the magic is right, so this is refused by a later check.
head -c 4096 nnue/pawnstar-v12.bin > "$scratch/truncated_net.bin"
out=$(printf 'isready\nquit\n' | PAWNSTAR_EVALFILE="$scratch/truncated_net.bin" "$ENGINE" 2>/dev/null)
status=$?
[ "$status" -ne 0 ] && ! echo "$out" | grep -q 'readyok' &&
    pass "PAWNSTAR_EVALFILE=<a truncated net>: fatal, no readyok" ||
    fail "PAWNSTAR_EVALFILE=<a truncated net>: expected a non-zero exit, got $status with: $out"

# 8) `setoption name EvalFile` with a net this build refuses, against the running engine. Keeping the net
#    already loaded is right - every check the loader makes happens before a weight is written - but being
#    QUIET about it was the defect: the engine answered `readyok` with no refusal the operator could act on,
#    so a match harness set EvalFile, saw readyok, and credited a thousand games to a net that never played a
#    move. So three things are asserted here, against the real binary rather than in process: the refusal
#    naming the field and both values reaches stdout AND stderr (a harness keeps stderr and routinely drops
#    `info` lines), a line says which net is still playing, and the engine then really searches with it.
crc64_xz() { # CRC-64/XZ of the decimal byte values in "$@" (nnue/README.md section 2). Reflected, so the
    # table constant is 0xC96C5795D7870F42; the >> is masked because bash shifts arithmetically and the
    # register is 64-bit signed. 56 bytes per call, so a table would buy nothing.
    local crc=$((0xFFFFFFFFFFFFFFFF)) b i
    for b in "$@"; do
        crc=$((crc ^ b))
        for ((i = 0; i < 8; i++)); do
            if ((crc & 1)); then
                crc=$((((crc >> 1) & 0x7FFFFFFFFFFFFFFF) ^ 0xC96C5795D7870F42))
            else
                crc=$(((crc >> 1) & 0x7FFFFFFFFFFFFFFF))
            fi
        done
    done
    printf '%s' $((crc ^ 0xFFFFFFFFFFFFFFFF))
}

shipped_net=nnue/pawnstar-v12.bin
# The check value every CRC-64/XZ implementation must reproduce, asserted before this CRC is trusted to
# build a net file: a wrong CRC here would hand the engine a net refused as a *corrupt header* rather than
# for the field changed below, and the case would pass while testing the wrong refusal.
if [ "$(crc64_xz 49 50 51 52 53 54 55 56 57)" = "$((0x995DC9BBDF1939FA))" ]; then
    pass "EvalFile refusal: this script's CRC-64/XZ reproduces the check value for '123456789'"
else
    fail "EvalFile refusal: this script's CRC-64/XZ is wrong (got $(crc64_xz 49 50 51 52 53 54 55 56 57))"
fi

# An incompatible net: the shipped net with one header field changed and header_crc recomputed, so the field
# check is what fires. hidden_size is read out of the file rather than written down, so this says what the
# engine says whichever shape the build is at.
read -r -a net_header <<< "$(od -An -v -tu1 -N64 "$shipped_net" | tr '\n' ' ')"
stored_header_crc=0
for i in 7 6 5 4 3 2 1 0; do stored_header_crc=$((stored_header_crc * 256 + net_header[56 + i])); done
if [ "$(crc64_xz "${net_header[@]:0:56}")" = "$stored_header_crc" ]; then
    pass "EvalFile refusal: recomputing the shipped net's header_crc agrees with the file"
else
    fail "EvalFile refusal: recomputed header_crc disagrees with the shipped net's own"
fi
hidden_size=$((net_header[16] + net_header[17] * 256))
bad_hidden_size=$((hidden_size / 2))
net_header[16]=$((bad_hidden_size & 0xFF))
net_header[17]=$(((bad_hidden_size >> 8) & 0xFF))
new_header_crc=$(crc64_xz "${net_header[@]:0:56}")
for i in 0 1 2 3 4 5 6 7; do net_header[56 + i]=$(((new_header_crc >> (8 * i)) & 0xFF)); done
header_escapes=""
for b in "${net_header[@]}"; do header_escapes+=$(printf '\\x%02x' "$b"); done
incompatible_net="$scratch/incompatible_net.bin"
cp "$shipped_net" "$incompatible_net"
printf '%b' "$header_escapes" > "$scratch/incompatible_header.bin"
dd if="$scratch/incompatible_header.bin" of="$incompatible_net" conv=notrunc 2>/dev/null
# The path reaches the engine inside a UCI command on **stdin**, and MSYS converts a POSIX path to a
# Windows one only in a native process's arguments and environment - never in what it reads. So the
# checks above, which name their nets through PAWNSTAR_EVALFILE, get `C:/...` for free while this one
# would hand the engine a literal `/tmp/...` it cannot open, and the case would pass on the wrong
# refusal ("it cannot be opened") instead of on the field check. A no-op where there is no cygpath.
if command -v cygpath >/dev/null 2>&1; then
    incompatible_net=$(cygpath -m "$incompatible_net")
fi
expected_reason="hidden_size is $(printf '%#x' "$bad_hidden_size") in the file, $(printf '%#x' "$hidden_size") in this engine"

# A second engine of its own, because the coprocess above discarded stderr and half of what is asserted here
# is on stderr.
evalfile_err="$scratch/evalfile.err"
coproc EVAL { "$ENGINE" 2>"$evalfile_err"; }
eval_send() { printf '%s\n' "$1" >&"${EVAL[1]}"; }
eval_read_until() {
    local re="$1" timeout="${2:-15}" line
    while IFS= read -r -t "$timeout" line <&"${EVAL[0]}"; do
        printf '%s\n' "$line"
        [[ "$line" =~ $re ]] && return 0
    done
    return 1
}
# Node counts of two searches are only comparable from the same starting state, so each is preceded by
# `ucinewgame` (which empties the transposition table and the eval cache) and `position startpos`. Single
# threaded with the book off, so the search is deterministic and the two counts must be equal.
search_depth=8
eval_send "uci"
eval_read_until '^uciok' 10 > /dev/null || fail "EvalFile refusal: no uciok"
eval_send "setoption name Threads value 1"
eval_send "setoption name OwnBook value false"
err_offset=$(($(wc -c < "$evalfile_err") + 1))
eval_send "setoption name EvalFile value $shipped_net"
eval_send "isready"
loaded=$(eval_read_until '^readyok' 20) || fail "EvalFile refusal: no readyok after loading the shipped net"
loaded_err=$(tail -c +"$err_offset" "$evalfile_err")
echo "$loaded" | grep -qxF "info string NNUE: loaded net '$shipped_net'" &&
    pass "setoption EvalFile <the shipped net>: stdout says, as an info string, which net loaded" ||
    fail "setoption EvalFile <the shipped net>: stdout lacks the loaded-net info string: $loaded"
echo "$loaded_err" | grep -qxF "NNUE: loaded net '$shipped_net'" &&
    pass "setoption EvalFile <the shipped net>: stderr carries the loaded net too" ||
    fail "setoption EvalFile <the shipped net>: stderr lacks the plain loaded-net line: $loaded_err"
eval_send "ucinewgame"
eval_send "position startpos"
eval_send "go depth $search_depth"
before=$(eval_read_until '^bestmove' 120)
bestmove_before=$(echo "$before" | sed -nE 's/^(bestmove [a-h][1-8][a-h][1-8][qrbn]?).*/\1/p' | tail -1)
nodes_before=$(echo "$before" | sed -nE "s/^info depth +$search_depth .* nodes +([0-9]+).*/\1/p" | tail -1)
if [ -n "$bestmove_before" ] && [ -n "$nodes_before" ] && [ "$nodes_before" -gt 0 ]; then
    pass "EvalFile refusal: the search before it is real ('$bestmove_before', $nodes_before nodes)"
else
    fail "EvalFile refusal: no real search before it (bestmove '$bestmove_before', nodes '$nodes_before')"
fi

# Read only the stderr written from here on. The file is not truncated: the engine holds it open at its own
# offset, so truncating would leave a hole of NUL bytes that command substitution then strips with a warning.
err_offset=$(($(wc -c < "$evalfile_err") + 1))
eval_send "setoption name EvalFile value $incompatible_net"
eval_send "isready"
refusal=$(eval_read_until '^readyok' 20)
refusal_err=$(tail -c +"$err_offset" "$evalfile_err")
kept_line="NNUE: keeping the net already loaded ('$shipped_net'); '$incompatible_net' was refused and is not in play"

echo "$refusal" | grep -qF "info string NNUE: refusing net '$incompatible_net': $expected_reason" &&
    pass "EvalFile refusal: stdout carries the refusal, naming the field and both values" ||
    fail "EvalFile refusal: stdout lacks \"refusing net '$incompatible_net': $expected_reason\": $refusal"
echo "$refusal_err" | grep -qF "NNUE: refusing net '$incompatible_net': $expected_reason" &&
    pass "EvalFile refusal: stderr carries the refusal too (a harness drops info lines)" ||
    fail "EvalFile refusal: stderr lacks the reason: $refusal_err"
echo "$refusal" | grep -qF "info string $kept_line" &&
    pass "EvalFile refusal: stdout names the net that is still playing" ||
    fail "EvalFile refusal: stdout lacks \"info string $kept_line\": $refusal"
echo "$refusal_err" | grep -qF "$kept_line" &&
    pass "EvalFile refusal: stderr names the net that is still playing" ||
    fail "EvalFile refusal: stderr lacks \"$kept_line\": $refusal_err"
echo "$refusal" | grep -q '^readyok' && pass "EvalFile refusal: still answers readyok" ||
    fail "EvalFile refusal: no readyok after the refusal"

# And it must still SEARCH, with the net it kept: info lines, a bestmove, and - from the same cleared state,
# single threaded - the same move and the same node count as before the refused net was offered.
eval_send "ucinewgame"
eval_send "position startpos"
eval_send "go depth $search_depth"
after=$(eval_read_until '^bestmove' 120)
bestmove_after=$(echo "$after" | sed -nE 's/^(bestmove [a-h][1-8][a-h][1-8][qrbn]?).*/\1/p' | tail -1)
nodes_after=$(echo "$after" | sed -nE "s/^info depth +$search_depth .* nodes +([0-9]+).*/\1/p" | tail -1)
echo "$after" | grep -q '^info depth' && pass "EvalFile refusal: the engine still emits info depth lines" ||
    fail "EvalFile refusal: no info depth line after it - the search did not run: $after"
if [ -n "$bestmove_after" ] && [ "$bestmove_after" = "$bestmove_before" ]; then
    pass "EvalFile refusal: the kept net still plays '$bestmove_after'"
else
    fail "EvalFile refusal: bestmove changed after a refused net ('$bestmove_before' -> '$bestmove_after')"
fi
if [ -n "$nodes_after" ] && [ "$nodes_after" = "$nodes_before" ]; then
    pass "EvalFile refusal: the kept net searches as it did ($nodes_after nodes, as before)"
else
    fail "EvalFile refusal: node count moved after a refused net ($nodes_before -> '$nodes_after'), so the net playing is not the one that was kept"
fi
eval_send "quit"
wait "$EVAL_PID" 2> /dev/null

# The end of input without `quit` ends the engine cleanly.
out=$(printf 'isready\n' | "$ENGINE" 2>/dev/null)
status=$?
[ "$status" -eq 0 ] && echo "$out" | grep -q '^readyok' && pass "end of input: exits 0 without quit" ||
    fail "end of input: exit $status, output: $out"

if [ "$fails" -eq 0 ]; then
    echo "[PASS] UCI: protocol handshakes, seldepth, searchmoves, currmove, stop and quit at once, malformed go values, bench determinism, startup resources, a refused EvalFile that says which net is still playing"
    exit 0
fi
echo "[FAIL] UCI: $fails check(s) failed"
exit 1

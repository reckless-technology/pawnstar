#!/usr/bin/env bash
#
# pawnstar-match.sh - run a Pawnstar net-versus-net match with fastchess on a CI machine.
#
# NNUE is Pawnstar's only evaluator, so net-strength testing is always one net against another: a
# candidate against a baseline, the two sides differing only by their net (and, when the nets are
# different architectures, by which binary loads them).
#
# THE POINT OF THIS SCRIPT IS THE PREFLIGHT. A net file carries a 64-byte self-describing RNNUE1 header
# (magic, format version, header size, features per bucket, king buckets, hidden size, output buckets,
# flags, activation, QA, QB, eval scale, king-bucket-map CRC, the payload length and two CRC-64s) and
# the engine validates every field against its compile-time constants in src/nnue.h. When they disagree,
# nothing a match runner notices goes wrong - which is the whole problem. Measured on these machines:
#
#   * a mismatched net given at startup through PAWNSTAR_EVALFILE is refused and the engine EXITS before
#     the handshake, which fastchess shows only as an engine that would not start. Older builds fell
#     back to the net embedded in the binary and played on - so the match completed and reported a
#     plausible Elo for a net that never played a move;
#   * a mismatched net given through "setoption name EvalFile" is refused and the net ALREADY LOADED
#     keeps playing, so the match runs to completion and credits every game to a net that never played;
#   * a VARIANT engine (built at another architecture) can load neither the on-disk nor the embedded
#     stock net, so it exits non-zero at startup, before the handshake - unless it is handed a
#     compatible net through PAWNSTAR_EVALFILE, which this script does for every engine it runs;
#   * the engine says all of this on BOTH streams on purpose (src/nnue.h RefuseNet, src/input_handling.h,
#     src/main.cpp), because a harness keeps stderr and a GUI reads the `info string` on stdout. This
#     probe keeps both streams, so either copy satisfies it.
#
# So every (binary, net) pair is proved here, before a single game, by running the EXACT command the
# match will run and making it search - and the run is refused loudly if the engine refuses the net,
# keeps another, fails to start or dies. One wasted 50-minute match paid for this check.
#
# THE HEADER IS DECODED BY tools/print_net_header, NOT HERE. This script used to carry its own `od`
# decoder, and on 2026-10-28 that decoder was found reading the header's uint16 fields from byte offset 4
# when the RNNUE1 magic is 8 bytes: every label was two fields out, so an 8-king-bucket net printed
# `buckets2` and the real bucket count surfaced under the `qb` label. No match result was ever wrong -
# the engine's validation is independent of this display - but a verification tool printing confident
# wrong values is worse than one printing nothing. One format deserves one decoder: tools/print_net_header
# (tools/net_header_report.h, tested by test/net_header_test.cpp, which copies nnue::NetHeader out of the
# file bytes rather than transcribing a table of offsets). This script parses its key=value output and
# decodes nothing itself.
#
# Usage:
#   pawnstar-match.sh --candidate <net.bin> [options]
#   pawnstar-match.sh --print-header <net.bin> [--repo <dir>]
#
# Required:
#   --candidate <net.bin>        The net being screened. Stamped, or raw with --stamp-raw-with.
#
# Options:
#   --baseline <net.bin>         Opponent net. Default: <repo>/nnue/pawnstar-v12.bin, the shipped net
#                                (1024 wide, 8 king buckets, one output head).
#   --candidate-binary <path>    Engine for the candidate. Default: <repo>/build/pawnstar.
#   --baseline-binary <path>     Engine for the baseline. Default: <repo>/build/pawnstar.
#                                A candidate of a different architecture needs its own build: make one
#                                with build-variant.sh and pass it here. The preflight refuses the run
#                                if a net and its binary disagree.
#   --stamp-raw-with <stamp_net> Stamp the candidate (raw bullet output) into a scratch copy with this
#                                stamp_net before using it. IT MUST BE A stamp_net BUILT AT THE NET'S
#                                OWN ARCHITECTURE - stamp_net writes the header from its own compiled
#                                constants, so a 512-wide net stamped by a stock 1024-wide stamp_net
#                                gets a 1024-wide header and is then refused. build-variant.sh builds
#                                the engine and its matching stamp_net together for exactly this.
#   --depth <n>                  Fixed search depth. Default 8. Isolates evaluation quality from the
#                                speed cost of a wider net, and is fully deterministic.
#   --time-control <spec>        A time control instead of fixed depth, e.g. "8+0.08". Measures real
#                                strength, speed cost included. Mutually exclusive with --depth.
#   --games <n>                  Play exactly n games (rounded up to a whole colour-reversed pair).
#   --sprt <elo0>:<elo1>         Run an SPRT with those bounds instead, alpha = beta = 0.05.
#                                Default when neither is given: --sprt 0:10.
#   --concurrency <n>            Concurrent games. Default: (cores - 2), at least 1, which leaves the
#                                GitHub Actions runner on this machine able to work.
#   --hash <mb>                  Transposition table per engine, MB. Default 16.
#   --openings <file.epd>        Opening set. Default: <repo>/nnue/openings.epd. A '#' provenance
#                                header is stripped into a scratch copy (fastchess would read those
#                                lines as positions).
#   --repo <dir>                 The pawnstar checkout. Default: $PAWNSTAR_REPO, else $HOME/pawnstar.
#   --print-net-header <path>    The print_net_header tool to decode headers with. Default: a
#                                print_net_header in the SAME DIRECTORY as --candidate-binary if there
#                                is one - so its shape verdict is about the engine that will actually
#                                play - else the first of $PAWNSTAR_PRINT_NET_HEADER,
#                                <repo>/build/print_net_header, <repo>/build-new-shape/print_net_header,
#                                or one on the PATH. Whichever is used, the preflight says whose build
#                                the shape comparison is against: a decoder's `shape_matches_build=no`
#                                is about THAT BUILD, and this script gates on `integrity=ok` and on
#                                the depth-4 load probe, never on the shape verdict.
#   --label <text>               Names the log file. Default: the candidate net's base name.
#   --log-dir <dir>              Where the log goes. Default: $HOME/sprt/logs.
#   --pgn <file>                 Also write the games as PGN.
#   --no-adjudicate              Do not adjudicate drawn or lost positions early (slower, purer).
#   --recover                    Restart an engine that crashes or stalls and carry on, instead of
#                                ending the match. Off by default, so a dead engine is loud. Worth
#                                setting for a long screening run of an experimental net: a
#                                degenerate net (e.g. one whose evaluation is flat) makes the engine
#                                shuffle, which fastchess reports as a stall and which would
#                                otherwise throw away the whole run.
#   --own-book                   Let the engines use their built-in opening book. OFF by default, and
#                                it should stay off: Pawnstar's OwnBook defaults to TRUE and the book
#                                picks a WEIGHTED-RANDOM move, so with it on the opening moves come
#                                from pawnstar.book rather than from the net under test - diluting the
#                                measurement - and the games stop being reproducible. Measured here:
#                                with the book on, two identical engines gave asymmetric pair results
#                                at fixed depth; with it off, every pair scored exactly 1.
#   --allow-same-net             Permit candidate and baseline to be the same file. For the harness
#                                self-test, whose result must come out at 0 Elo.
#   --allow-busy-runner          Start even while a GitHub Actions job is running on this machine.
#                                Off by default: a local load this heavy has killed a job here by
#                                starving the runner's connection to GitHub.
#   --allow-unsound-net          Run even though print_net_header reports a net as not intact (a bad
#                                CRC, a truncated payload). Off by default: the engine would refuse it.
#   --dry-run                    Do the preflight and print the fastchess command, play nothing.
#   --print-header <net.bin>     Decode one net's header, print it, and exit. Needs no engine, no
#                                fastchess and no opening set. Exits 0 when the net is intact, 1 when
#                                it is not. This is the mode CI asserts against a known fixture.
#
# Output: the preflight report, then fastchess's own result block - Elo with its error bars, nElo,
# LOS, draw ratio and the W/L/D counts - then the games per minute this machine measured. All of it is
# tee'd to the log file, whose path is printed at the start and at the end.
#
set -uo pipefail

readonly kProbeDepth=4
readonly kProbeTimeoutSeconds=60
# The comment block above, as --help. One constant so an edit to the block is one edit here.
readonly kHelpLineRange='3,118p'

candidate_net=""
baseline_net=""
candidate_binary=""
baseline_binary=""
stamp_tool=""
search_depth=""
time_control=""
game_count=""
sprt_bounds=""
concurrency=""
hash_megabytes=16
openings_file=""
repository_directory="${PAWNSTAR_REPO:-$HOME/pawnstar}"
header_tool="${PAWNSTAR_PRINT_NET_HEADER:-}"
# Whether the decoder was NAMED (by --print-net-header or $PAWNSTAR_PRINT_NET_HEADER) rather than
# discovered. A named one is never second-guessed; a discovered one may be replaced below by the
# candidate build's own, whose shape verdict is about the engine that will play.
header_tool_was_named=0
[ -z "$header_tool" ] || header_tool_was_named=1
run_label=""
log_directory="$HOME/sprt/logs"
pgn_file=""
print_header_net=""
does_adjudicate=1
does_recover=0
does_use_own_book=0
does_allow_same_net=0
does_allow_busy_runner=0
does_allow_unsound_net=0
is_dry_run=0

die()
{
    printf '\npawnstar-match: ERROR: %s\n' "$*" >&2
    exit 1
}

note()
{
    printf 'pawnstar-match: %s\n' "$*"
}

while [ $# -gt 0 ]; do
    case "$1" in
        --candidate)            candidate_net="${2:?--candidate needs a path}"; shift 2 ;;
        --baseline)             baseline_net="${2:?--baseline needs a path}"; shift 2 ;;
        --candidate-binary)     candidate_binary="${2:?}"; shift 2 ;;
        --baseline-binary)      baseline_binary="${2:?}"; shift 2 ;;
        --stamp-raw-with)       stamp_tool="${2:?}"; shift 2 ;;
        --depth)                search_depth="${2:?}"; shift 2 ;;
        --time-control|--tc)    time_control="${2:?}"; shift 2 ;;
        --games)                game_count="${2:?}"; shift 2 ;;
        --sprt)                 sprt_bounds="${2:?}"; shift 2 ;;
        --concurrency)          concurrency="${2:?}"; shift 2 ;;
        --hash)                 hash_megabytes="${2:?}"; shift 2 ;;
        --openings)             openings_file="${2:?}"; shift 2 ;;
        --repo)                 repository_directory="${2:?}"; shift 2 ;;
        --print-net-header)     header_tool="${2:?--print-net-header needs a path}"
                                header_tool_was_named=1; shift 2 ;;
        --label)                run_label="${2:?}"; shift 2 ;;
        --log-dir)              log_directory="${2:?}"; shift 2 ;;
        --pgn)                  pgn_file="${2:?}"; shift 2 ;;
        --print-header)         print_header_net="${2:?--print-header needs a path}"; shift 2 ;;
        --no-adjudicate)        does_adjudicate=0; shift ;;
        --recover)              does_recover=1; shift ;;
        --own-book)             does_use_own_book=1; shift ;;
        --allow-same-net)       does_allow_same_net=1; shift ;;
        --allow-busy-runner)    does_allow_busy_runner=1; shift ;;
        --allow-unsound-net)    does_allow_unsound_net=1; shift ;;
        --dry-run)              is_dry_run=1; shift ;;
        -h|--help)              sed -n "$kHelpLineRange" "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *)                      die "unknown argument '$1' (--help for the list)" ;;
    esac
done

# ---------------------------------------------------------------------------------------------------
# The RNNUE1 header, read by the repository's one reviewed decoder.
#
# tools/print_net_header prints `key=value` lines on stdout - every key always present, in a fixed
# order - its human reading of any failure on stderr, and exits 0 when the file is intact, 1 when it is
# not and 2 when it cannot be read at all. It describes a net of ANY shape, including one the build that
# made it could not load, which is exactly the net a preflight most needs readable. The format is
# documented at the top of tools/net_header_report.h, where KeyValues() is its one definition.
# ---------------------------------------------------------------------------------------------------

# Find the print_net_header tool. Echoes its path, or nothing.
locate_header_tool()
{
    local candidate
    for candidate in "$header_tool" \
                     "$repository_directory/build/print_net_header" \
                     "$repository_directory/build-new-shape/print_net_header"; do
        if [ -n "$candidate" ] && [ -x "$candidate" ]; then
            printf '%s\n' "$candidate"
            return 0
        fi
    done
    command -v print_net_header 2>/dev/null || true
}

# The keys this script reads out of a report. Checked present once, up front, so no later read can come
# back empty and be used as if it were a value.
readonly kRequiredHeaderKeys="file_bytes header_complete magic format_version header_bytes
                              features_per_bucket king_buckets hidden_size output_buckets flags
                              activation qa qb eval_scale king_bucket_map_crc integrity
                              shape_matches_build"

# Decode one net into g_net_report, and validate that report.
#
# **Called at the TOP LEVEL, never inside a command substitution**, which is why it writes a global
# instead of echoing: `die` inside `$(...)` exits only the subshell, so the caller would carry on with an
# empty string where a header field belonged. A verification tool quietly substituting a blank for a
# field it could not read is the same class of defect as one printing the wrong field, and that defect is
# the reason this script was rewritten. Dying here, properly, is the point of the shape.
#
# @param net_file the net to decode. Exit status is print_net_header's own (0 intact, 1 not intact); a
# file it cannot read at all (its exit 2) is fatal.
g_net_report=""
decode_net_header()
{
    local net_file="$1" status=0 key
    g_net_report="$("$header_tool" "$net_file" 2>/dev/null)" || status=$?
    [ "$status" -lt 2 ] || die "print_net_header could not read '$net_file' (it exited $status)"
    for key in $kRequiredHeaderKeys; do
        printf '%s\n' "$g_net_report" | grep -q "^$key=" || die "print_net_header printed no '$key' for
         '$net_file': the tool and this script disagree about its output. Rebuild it from this checkout
         ('make tools'), or point --print-net-header at one built from it."
    done
    return "$status"
}

# One field out of a report. @param report the key=value text. @param key the key to read.
# Presence is guaranteed by decode_net_header, so an empty result here means the field's value really is
# empty (`shape_mismatches=` on a net of this build's shape), not that the key was missing.
net_header_field()
{
    printf '%s\n' "$1" | sed -n "s/^$2=//p" | head -1 | tr -d '\n'
}

# The architecture fields alone, as one line: what must match between a net and the binary that loads it,
# and between two nets played against each other by one binary. Deliberately NOT the whole report, which
# also carries the file's name, its length and its CRCs - two different nets of the same shape must
# compare equal here. These are the fields the engine itself checks (src/nnue.h, FieldCheck).
net_shape_signature()
{
    local report="$1" key signature=""
    for key in format_version header_bytes features_per_bucket king_buckets hidden_size output_buckets \
               flags activation qa qb eval_scale king_bucket_map_crc; do
        # No trailing separator: this line is compared as text, in CI among other places, and a trailing
        # space is whitespace nobody can see and some editors strip.
        signature="${signature:+$signature }$key=$(net_header_field "$report" "$key")"
    done
    printf '%s\n' "$signature"
}

# A net's header in the engine's own shorthand, so a mismatch reads the same here as in its message.
describe_net_header()
{
    local report="$1"
    printf 'magic=%s v%s/hdr%s/feat%s/kb%s/hidden%s/ob%s/flags%s/act%s/qa%s/qb%s/scale%s/mapcrc=%s' \
           "$(net_header_field "$report" magic)" \
           "$(net_header_field "$report" format_version)" \
           "$(net_header_field "$report" header_bytes)" \
           "$(net_header_field "$report" features_per_bucket)" \
           "$(net_header_field "$report" king_buckets)" \
           "$(net_header_field "$report" hidden_size)" \
           "$(net_header_field "$report" output_buckets)" \
           "$(net_header_field "$report" flags)" \
           "$(net_header_field "$report" activation)" \
           "$(net_header_field "$report" qa)" \
           "$(net_header_field "$report" qb)" \
           "$(net_header_field "$report" eval_scale)" \
           "$(net_header_field "$report" king_bucket_map_crc)"
}

# No decoder anywhere. A function because the match path looks once more, beside --candidate-binary,
# before it gives up - see "PREFER A DECODER FROM THE CANDIDATE'S OWN BUILD" below.
die_with_no_header_tool()
{
    die "no print_net_header found (looked at \$PAWNSTAR_PRINT_NET_HEADER, the candidate binary's own
         directory, $repository_directory/build/print_net_header,
         $repository_directory/build-new-shape/print_net_header and the PATH). Build it with
         'make tools' in the checkout, or say where it is with --print-net-header. This
         script no longer decodes a header itself: one format, one decoder."
}

header_tool="$(locate_header_tool)"

# ---------------------------------------------------------------------------------------------------
# --print-header: decode one net and stop. No engine, no fastchess, no opening set - so CI can assert
# the field values against a known fixture, which is the check that would have caught the offset bug
# described above before a match rather than after one.
# ---------------------------------------------------------------------------------------------------

if [ -n "$print_header_net" ]; then
    # This mode has no candidate binary to take a decoder from, so a decoder must already be in hand.
    [ -n "$header_tool" ] || die_with_no_header_tool
    [ -r "$print_header_net" ] || die "cannot read '$print_header_net'"
    decode_net_header "$print_header_net"
    print_header_report="$g_net_report"
    printf '%s\n' "$print_header_report"
    printf 'description=%s\n' "$(describe_net_header "$print_header_report")"
    printf 'shape_signature=%s\n' "$(net_shape_signature "$print_header_report")"
    [ "$(net_header_field "$print_header_report" integrity)" = ok ] || exit 1
    exit 0
fi

# ---------------------------------------------------------------------------------------------------
# Defaults that depend on the repository, and the sanity of what was asked for.
# ---------------------------------------------------------------------------------------------------

[ -n "$candidate_net" ] || die "--candidate <net.bin> is required (--help for the rest)"
[ -d "$repository_directory" ] || die "no pawnstar checkout at '$repository_directory' (--repo to say where)"

default_binary="$repository_directory/build/pawnstar"
: "${candidate_binary:=$default_binary}"
: "${baseline_binary:=$default_binary}"
: "${baseline_net:=$repository_directory/nnue/pawnstar-v12.bin}"
: "${openings_file:=$repository_directory/nnue/openings.epd}"

if [ -n "$search_depth" ] && [ -n "$time_control" ]; then
    die "--depth and --time-control are alternatives; give one"
fi
[ -n "$search_depth" ] || [ -n "$time_control" ] || search_depth=8

if [ -n "$game_count" ] && [ -n "$sprt_bounds" ]; then
    die "--games and --sprt are alternatives; give one"
fi
[ -n "$game_count" ] || [ -n "$sprt_bounds" ] || sprt_bounds="0:10"

for required_file in "$candidate_net" "$baseline_net" "$openings_file"; do
    [ -r "$required_file" ] || die "cannot read '$required_file'"
done
for required_binary in "$candidate_binary" "$baseline_binary"; do
    [ -x "$required_binary" ] || die "'$required_binary' is not an executable engine (build it with 'make RELEASE=1', or build-variant.sh for another architecture)"
done
if [ -n "$stamp_tool" ]; then
    [ -x "$stamp_tool" ] || die "'$stamp_tool' is not an executable stamp_net"
fi

# ---------------------------------------------------------------------------------------------------
# PREFER A DECODER FROM THE CANDIDATE'S OWN BUILD.
#
# print_net_header's `shape_matches_build` compares a net against the compile-time constants of
# whichever build of THAT TOOL is running - not against the engine that is about to play. On these
# runners the discovered decoder is ~/pawnstar/build/print_net_header, which is compiled at the old
# architecture (format_version 1, output_buckets 1, map CRC 0xCC6DDB5DAF0AB18D), so for any new-shape
# net it reports `shape_matches_build=no` and prints its shape note. Nothing is wrong when it does: this
# script gates on `integrity=ok` and on the depth-4 probe below, which runs the exact command fastchess
# will run and makes each binary load its own net for real. But a shape verdict about a build that is
# not in the match is a verdict nobody can act on, and on 2026-10-28 that note stopped a 4000-game setup
# while a human worked out whether it mattered. It did not.
#
# So when the decoder was discovered rather than named, and the candidate's build directory holds a
# print_net_header of its own, use that one: then `shape_matches_build` is a statement about the engine
# that will play. Deliberately the CANDIDATE's and not the baseline's - the candidate is the side that
# carries a new shape - and the preflight says below which of the two it got, because with two engines
# of different architectures no single decoder can speak for both.
# ---------------------------------------------------------------------------------------------------
candidate_build_directory="$(dirname "$candidate_binary")"
if [ "$header_tool_was_named" -eq 0 ] && [ -x "$candidate_build_directory/print_net_header" ]; then
    header_tool="$candidate_build_directory/print_net_header"
fi
[ -n "$header_tool" ] || die_with_no_header_tool
header_tool_is_candidates_own=0
if [ "$(dirname "$header_tool")" = "$candidate_build_directory" ]; then
    header_tool_is_candidates_own=1
fi

fastchess_binary="${FASTCHESS:-$HOME/sprt/bin/fastchess}"
[ -x "$fastchess_binary" ] || fastchess_binary="$(command -v fastchess || true)"
[ -n "$fastchess_binary" ] && [ -x "$fastchess_binary" ] \
    || die "no fastchess found (looked at \$FASTCHESS, ~/sprt/bin/fastchess and the PATH)"

if [ "$does_allow_same_net" -eq 0 ] \
   && [ "$(readlink -f "$candidate_net")" = "$(readlink -f "$baseline_net")" ]; then
    die "the candidate and the baseline are the same file - that measures nothing but the harness
         itself. Pass --allow-same-net if that is deliberately what you want (the self-test does)."
fi

core_count="$(nproc)"
if [ -z "$concurrency" ]; then
    concurrency=$(( core_count - 2 ))
    [ "$concurrency" -ge 1 ] || concurrency=1
fi

# ---------------------------------------------------------------------------------------------------
# Scratch files, and logging. Everything from here on is tee'd to the log.
# ---------------------------------------------------------------------------------------------------

scratch_directory="$(mktemp -d "${TMPDIR:-/tmp}/pawnstar_match.XXXXXX")"
cleanup()
{
    rm -rf "$scratch_directory"
}
trap cleanup EXIT

mkdir -p "$log_directory" || die "cannot create '$log_directory'"
[ -n "$run_label" ] || run_label="$(basename "$candidate_net" .bin)"
run_label="$(printf '%s' "$run_label" | tr -c 'A-Za-z0-9._-' '-')"
log_file="$log_directory/${run_label}-$(date +%Y%m%d-%H%M%S).log"
note "log: $log_file"
exec > >(tee -a "$log_file") 2>&1

printf '===================================================================================\n'
printf 'pawnstar-match  %s on %s\n' "$(date -Is)" "$(hostname)"
printf '===================================================================================\n'

if [ "$concurrency" -gt $(( core_count - 2 )) ]; then
    note "WARNING: concurrency $concurrency on $core_count cores leaves under 2 cores for the Actions runner."
fi

# ---------------------------------------------------------------------------------------------------
# Is a GitHub Actions job running on this machine? Its work matters more than ours.
# ---------------------------------------------------------------------------------------------------

running_job_count="$(ps -e -o args= | grep -c '[R]unner\.Worker' || true)"
if [ "$running_job_count" -gt 0 ]; then
    if [ "$does_allow_busy_runner" -eq 0 ]; then
        die "$running_job_count GitHub Actions job(s) are running on this machine. A local load this
         heavy has killed a job here before, by starving the runner's connection to GitHub. Wait for
         it, or pass --allow-busy-runner if you accept the risk."
    fi
    note "WARNING: $running_job_count Actions job(s) running and --allow-busy-runner was given."
else
    note "Actions runner: idle."
fi

# ---------------------------------------------------------------------------------------------------
# Stamp a raw bullet net, if asked.
# ---------------------------------------------------------------------------------------------------

if [ -n "$stamp_tool" ]; then
    stamped_candidate="$scratch_directory/$(basename "$candidate_net" .bin)-stamped.bin"
    printf '\n--- stamping --------------------------------------------------------------------\n'
    printf 'raw net   : %s\n' "$candidate_net"
    printf 'stamp_net : %s\n' "$stamp_tool"
    "$stamp_tool" "$candidate_net" "$stamped_candidate" \
        || die "stamp_net failed. It only accepts RAW (unstamped) bullet output - if the net is
         already stamped, drop --stamp-raw-with."
    printf 'stamped   : %s\n' "$stamped_candidate"
    printf 'NOTE: the header this wrote came from %s\n      own compiled constants. The preflight below is what proves it was the right one.\n' "$(basename "$stamp_tool")"
    candidate_net="$stamped_candidate"
fi

# ---------------------------------------------------------------------------------------------------
# The engine wrappers, and the preflight.
# ---------------------------------------------------------------------------------------------------

# Write the command fastchess will run for one side: the engine, given its own net at STARTUP through
# PAWNSTAR_EVALFILE and pinned to one search thread. The startup net is what lets a variant binary
# (whose on-disk and embedded nets are the stock ones it cannot load) survive to the UCI handshake at
# all. `exec` leaves no extra process behind, and /proc/self/exe still points at the real binary, so
# the engine's own resource location keeps working.
write_engine_wrapper()
{
    local wrapper_path="$1" engine_binary="$2" net_file="$3"
    {
        printf '#!/bin/sh\n'
        printf '# generated by pawnstar-match.sh - gives this engine its net at startup\n'
        printf 'exec env PAWNSTAR_EVALFILE=%q PAWNSTAR_THREADS=1 %q "$@"\n' "$net_file" "$engine_binary"
    } > "$wrapper_path"
    chmod +x "$wrapper_path"
}

# Run one side's exact command, load its net the way fastchess will, and make it search. Echoes "OK",
# or the reason to refuse. This is what catches a refused net, a startup exit and a crash on go.
#
# THE PATTERNS BELOW ARE WHAT THE ENGINE REALLY PRINTS, and they are not what this script looked for
# until 2026-10-28. It grepped for `is incompatible`, `is not a stamped` and `falling back to the
# embedded net`, and src/nnue.h prints none of the three. Every refusal goes through nnue::RefuseNet,
# which writes `NNUE: refusing net '<path>': <reason>` to stderr AND `info string NNUE: refusing net
# ...` to stdout - deliberately both, because a harness keeps stderr and a GUI reads the info line.
#
# The `falling back to the embedded net` grep was worse than dead. src/main.cpp's fatal message contains
# the words "Not falling back to the embedded net", so a refused STARTUP net matched that grep and this
# script reported the opposite of what had happened: that the engine had fallen back and played on, when
# in fact it had exited. The one fallback left is for a net file that is simply ABSENT (`NNUE: no net
# file at '<path>'; using the embedded net`), which must still be caught - a mistyped path would
# otherwise quietly measure the embedded net against itself.
probe_engine()
{
    local wrapper_path="$1" net_file="$2" probe_output="" refusal=""
    probe_output="$(printf 'uci\nsetoption name Threads value 1\nsetoption name OwnBook value false\nsetoption name EvalFile value %s\nisready\nposition startpos\ngo depth %d\nquit\n' \
                           "$net_file" "$kProbeDepth" \
                    | timeout "$kProbeTimeoutSeconds" "$wrapper_path" 2>&1)"
    # src/nnue.h RefuseNet. The reason names the field and both values, so it is quoted verbatim here
    # rather than paraphrased - a paraphrase is how this script came to describe messages that no
    # longer existed.
    refusal="$(printf '%s' "$probe_output" | grep -o "NNUE: refusing net .*" | head -1)"
    if [ -n "$refusal" ]; then
        printf 'the engine REFUSED this net: %s\n' "$refusal"
        return 1
    fi
    # src/input_handling.h: a net refused through `setoption name EvalFile` leaves the previously loaded
    # net playing. The match would then run to completion and credit every game to a net that never
    # played a move - the exact failure this preflight exists for.
    if printf '%s' "$probe_output" | grep -q 'NNUE: keeping the net already loaded'; then
        printf 'the engine REFUSED this net and kept another: %s\n' \
               "$(printf '%s' "$probe_output" | grep -o 'NNUE: keeping the net already loaded.*' | head -1)"
        return 1
    fi
    if printf '%s' "$probe_output" | grep -q 'NNUE: no net was loaded'; then
        printf 'the engine refused this net and has no net loaded at all.\n'
        return 1
    fi
    # src/main.cpp: a startup net that was read and then refused is fatal, and the engine exits.
    if printf '%s' "$probe_output" | grep -q 'FATAL: the NNUE net'; then
        printf 'the engine read this net, refused it and EXITED at startup rather than fall back. That
                      is a variant binary meeting a net of another architecture.\n'
        return 1
    fi
    if printf '%s' "$probe_output" | grep -q 'FATAL: no NNUE net at'; then
        printf 'there is no net at this path AND the copy embedded in the binary was refused too - the
                      build is corrupt.\n'
        return 1
    fi
    # src/main.cpp: the one remaining fallback, for a net file that is not there at all.
    if printf '%s' "$probe_output" | grep -q 'NNUE: no net file at'; then
        printf 'there is no net file at this path, so the engine used its EMBEDDED net - the match would
                      run happily and measure nothing.\n'
        return 1
    fi
    # src/nnue.h, the end of Load: `info string NNUE: loaded net '<path>'` on stdout and `NNUE: loaded
    # net '<path>'` on stderr, both on purpose. This probe reads both streams, so either satisfies it.
    if ! printf '%s' "$probe_output" | grep -qF "loaded net '$net_file'"; then
        printf 'the engine never reported loading this exact net (it may have loaded another, or none).\n'
        return 1
    fi
    if ! printf '%s' "$probe_output" | grep -q '^bestmove '; then
        printf 'the engine loaded the net but produced no bestmove at depth %d - it crashed, hung or died.\n' "$kProbeDepth"
        return 1
    fi
    printf 'OK\n'
    return 0
}

printf '\n--- preflight -------------------------------------------------------------------\n'
printf 'header reader : %s\n' "$header_tool"
# Whose build any `shape_matches_build` below is a verdict on - said once, here, rather than left for a
# reader to work out from a note that looks like a refusal.
if [ "$header_tool_is_candidates_own" -eq 1 ]; then
    printf '                built beside the candidate engine, so its shape verdict is about this match.\n'
else
    printf '                NOT from the candidate'\''s build directory (%s), so a `shape_matches_build=no`\n' \
           "$candidate_build_directory"
    printf '                below is about THIS DECODER'\''s build and says nothing about the engines playing.\n'
    printf '                This run is gated on `integrity=ok` and on the depth-4 load probe, not on that.\n'
fi

decode_net_header "$candidate_net"
candidate_report="$g_net_report"
decode_net_header "$baseline_net"
baseline_report="$g_net_report"
candidate_shape="$(net_shape_signature "$candidate_report")"
baseline_shape="$(net_shape_signature "$baseline_report")"
printf 'candidate net : %s\n                %s\n' "$candidate_net" "$(describe_net_header "$candidate_report")"
printf 'baseline net  : %s\n                %s\n' "$baseline_net" "$(describe_net_header "$baseline_report")"
printf 'candidate bin : %s\n' "$candidate_binary"
printf 'baseline bin  : %s\n' "$baseline_binary"

# THE SHAPE VERDICT, SAID WHERE A READER WILL SEE IT AND FRAMED BY WHOSE BUILD IT IS ABOUT. The shape
# information itself is useful and is kept - it is the whole point when the decoder IS the engine under
# test, as in the `new-shape` CI job - but on its own it reads as a refusal, and a note that reads like a
# refusal on a run that is fine is worse than no note. So it is printed with the one thing that decides
# whether it matters: whether the decoder speaks for the engine that will play.
for side in candidate baseline; do
    if [ "$side" = candidate ]; then
        side_report="$candidate_report"
    else
        side_report="$baseline_report"
    fi
    [ "$(net_header_field "$side_report" shape_matches_build)" = no ] || continue
    if [ "$header_tool_is_candidates_own" -eq 1 ] && [ "$side" = candidate ]; then
        printf 'shape         : the candidate net is NOT the shape the candidate build was compiled at, and\n'
        printf '                the header reader IS that build - so this one is about the match. The depth-4\n'
        printf '                probe below decides it: it makes that binary load that net and search.\n'
    else
        printf 'shape         : the %s net is not the shape the HEADER READER was compiled at. That is a fact\n' "$side"
        printf '                about the decoder above, not about this net or the engines: what this run is\n'
        printf '                gated on is `integrity=ok` and the depth-4 load probe below.\n'
    fi
done

# A net whose own CRCs or payload length do not check out is a net the engine will refuse, and the probe
# below would catch it - but it is worth saying as a corrupt file rather than as an architecture problem.
for side in candidate baseline; do
    if [ "$side" = candidate ]; then
        side_report="$candidate_report"; side_net="$candidate_net"
    else
        side_report="$baseline_report"; side_net="$baseline_net"
    fi
    if [ "$(net_header_field "$side_report" integrity)" != ok ]; then
        printf '\n%s net is NOT INTACT:\n' "$side"
        "$header_tool" "$side_net" > /dev/null
        [ "$does_allow_unsound_net" -eq 1 ] \
            || die "the $side net '$side_net' fails its own RNNUE1 integrity checks (above). The engine
         will refuse it. Re-stamp or re-copy it; pass --allow-unsound-net to run anyway."
        note "WARNING: the $side net is not intact and --allow-unsound-net was given."
    fi
done

if [ "$candidate_shape" != "$baseline_shape" ] && [ "$candidate_binary" = "$baseline_binary" ]; then
    die "the two nets are different architectures but both sides would run the SAME binary
         ($candidate_binary). One build loads one architecture only. Build the candidate's variant
         with build-variant.sh --hidden <n> --buckets <n> and pass its engine as --candidate-binary.
         candidate: $candidate_shape
         baseline : $baseline_shape"
fi

candidate_wrapper="$scratch_directory/engine-candidate.sh"
baseline_wrapper="$scratch_directory/engine-baseline.sh"
write_engine_wrapper "$candidate_wrapper" "$candidate_binary" "$candidate_net"
write_engine_wrapper "$baseline_wrapper" "$baseline_binary" "$baseline_net"

preflight_did_fail=0
for side in candidate baseline; do
    if [ "$side" = candidate ]; then
        side_wrapper="$candidate_wrapper"; side_binary="$candidate_binary"; side_net="$candidate_net"
    else
        side_wrapper="$baseline_wrapper"; side_binary="$baseline_binary"; side_net="$baseline_net"
    fi
    printf '\nprobing %-9s %s\n          with %s\n' "$side" "$side_binary" "$side_net"
    probe_result="$(probe_engine "$side_wrapper" "$side_net")"
    if [ "$probe_result" = OK ]; then
        printf '          -> loaded that exact net and searched to depth %d. OK.\n' "$kProbeDepth"
    else
        printf '          -> REFUSED: %s\n' "$probe_result"
        preflight_did_fail=1
    fi
done

if [ "$preflight_did_fail" -ne 0 ]; then
    die "architecture preflight failed - NO GAMES WERE PLAYED. A net's header must match its binary's
         compile-time nnue::kHiddenSize, nnue::kNumKingBuckets and nnue::kNumOutputBuckets exactly.
         Build the matching variant with build-variant.sh, stamp the raw net with THAT tree's
         stamp_net, and try again."
fi
printf '\npreflight passed: each engine loads its own net and searches with it.\n'

# ---------------------------------------------------------------------------------------------------
# The openings, and the fastchess command.
# ---------------------------------------------------------------------------------------------------

# fastchess hands every non-empty line of an EPD file to its FEN parser, so the '#' provenance header
# nnue/openings.epd carries would be read as a position. Strip comments into a scratch copy.
if grep -q '^[[:space:]]*#' "$openings_file"; then
    stripped_openings="$scratch_directory/openings.epd"
    grep -v '^[[:space:]]*#' "$openings_file" | grep -v '^[[:space:]]*$' > "$stripped_openings"
    note "stripped the comment header from $openings_file ($(wc -l < "$stripped_openings") openings)"
    openings_file="$stripped_openings"
fi

if [ -n "$time_control" ]; then
    clock_arguments=(tc="$time_control")
    clock_description="time control $time_control"
else
    clock_arguments=(depth="$search_depth")
    clock_description="fixed depth $search_depth"
fi

if [ -n "$sprt_bounds" ]; then
    elo0="${sprt_bounds%%:*}"
    elo1="${sprt_bounds##*:}"
    [ "$elo0" != "$sprt_bounds" ] || die "--sprt wants <elo0>:<elo1>, e.g. --sprt 0:10"
    # An SPRT stops when it decides, so the round cap is only an upper bound.
    stop_arguments=(-rounds 20000 -sprt elo0="$elo0" elo1="$elo1" alpha=0.05 beta=0.05)
    stop_description="SPRT elo0=$elo0 elo1=$elo1 alpha=beta=0.05 (capped at 40000 games)"
else
    round_count=$(( (game_count + 1) / 2 ))
    stop_arguments=(-rounds "$round_count")
    stop_description="$(( round_count * 2 )) games"
fi

# Pawnstar's OwnBook defaults to true and its book move is weighted-random: left on, the opening moves
# come from the book instead of the net under test, and the games are not reproducible.
if [ "$does_use_own_book" -eq 1 ]; then
    own_book_setting=true
else
    own_book_setting=false
fi

adjudication_arguments=()
if [ "$does_adjudicate" -eq 1 ]; then
    adjudication_arguments=(-draw movenumber=40 movecount=8 score=20 -resign movecount=3 score=600)
fi

recover_arguments=()
if [ "$does_recover" -eq 1 ]; then
    recover_arguments=(-recover)
fi

pgn_arguments=()
if [ -n "$pgn_file" ]; then
    pgn_arguments=(-pgnout "file=$pgn_file")
fi

fastchess_command=(
    "$fastchess_binary"
    -engine cmd="$candidate_wrapper" name=candidate option.EvalFile="$candidate_net"
    -engine cmd="$baseline_wrapper"  name=baseline  option.EvalFile="$baseline_net"
    -each proto=uci option.Threads=1 option.Hash="$hash_megabytes" option.OwnBook="$own_book_setting" "${clock_arguments[@]}"
    "${stop_arguments[@]}"
    -games 2 -repeat
    -openings "file=$openings_file" format=epd order=random
    "${adjudication_arguments[@]}"
    "${recover_arguments[@]}"
    "${pgn_arguments[@]}"
    -concurrency "$concurrency"
)

printf '\n--- match -----------------------------------------------------------------------\n'
printf 'machine       : %s, %s cores\n' "$(hostname)" "$core_count"
printf 'clock         : %s\n' "$clock_description"
printf 'stop rule     : %s\n' "$stop_description"
printf 'concurrency   : %s (leaving %s cores)\n' "$concurrency" "$(( core_count - concurrency ))"
printf 'hash          : %s MB per engine, Threads=1 per engine\n' "$hash_megabytes"
printf 'built-in book : OwnBook=%s\n' "$own_book_setting"
printf 'openings      : %s\n' "$openings_file"
printf 'fastchess     : %s\n' "$("$fastchess_binary" --version | head -1)"
printf '\ncommand:\n'
printf '  %q' "${fastchess_command[@]}"
printf '\n\n'

if [ "$is_dry_run" -eq 1 ]; then
    note "--dry-run: preflight done, no games played."
    exit 0
fi

# The nets must be distinguished ONLY per engine; let nothing leak in from this shell.
unset PAWNSTAR_EVALFILE PAWNSTAR_NNUE
export PAWNSTAR_THREADS=1

# Keep fastchess's own output in a file of its own as well as in the log, so the game count can be
# read back without racing the log's tee, which has not necessarily flushed when fastchess exits.
fastchess_output="$scratch_directory/result.txt"

started_at="$(date +%s)"
"${fastchess_command[@]}" 2>&1 | tee "$fastchess_output"
fastchess_status="${PIPESTATUS[0]}"
finished_at="$(date +%s)"
elapsed_seconds=$(( finished_at - started_at ))
[ "$elapsed_seconds" -gt 0 ] || elapsed_seconds=1

games_played="$(grep -oE 'Games: [0-9]+' "$fastchess_output" | tail -1 | grep -oE '[0-9]+' || true)"

printf '\n--- throughput ------------------------------------------------------------------\n'
printf 'wall time      : %s s (%s)\n' "$elapsed_seconds" "$(printf '%dm%02ds' $(( elapsed_seconds / 60 )) $(( elapsed_seconds % 60 )))"
if [ -n "$games_played" ] && [ "$games_played" -gt 0 ]; then
    printf 'games          : %s\n' "$games_played"
    printf 'games / minute : %s\n' "$(awk -v g="$games_played" -v s="$elapsed_seconds" 'BEGIN { printf "%.1f", g * 60 / s }')"
    printf 'at             : %s, concurrency %s, on %s\n' "$clock_description" "$concurrency" "$(hostname)"
else
    printf 'games          : could not be read back (fastchess exited %s)\n' "$fastchess_status"
fi
printf '\nlog: %s\n' "$log_file"

exit "$fastchess_status"

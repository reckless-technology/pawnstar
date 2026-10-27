#!/usr/bin/env bash
# Measure what fraction of an assembled NNUE training pool is distinct positions - the cheap check that a
# data campaign is not generating the same games over and over - and audit the campaign identities the
# shards' .meta sidecars record.
#
# WHY THIS EXISTS. The engine is deterministic at a fixed budget with its book off, so a self-play game is
# decided by the position it starts from. A campaign whose openings are too few, or whose workers draw from
# the same openings, therefore writes the same games repeatedly; tools/gen_data.cpp deduplicates POSITIONS
# PER SHARD, so a shard always looks healthy from the outside and the duplication only becomes visible once
# the shards are assembled. Shard sizes never show it. Measured in minutes here, it would otherwise cost a
# week of CPU and a net that trains to nothing.
#
# Usage: check_duplicates.sh <pool.txt | shard_dir> [min_unique_fraction] [records_per_shard]
#   pool.txt | shard_dir   a single assembled text pool, or a directory of shards (data_*.txt / seed*.txt,
#                          as tools/run_gendata.sh writes).
#   min_unique_fraction    fail below this fraction of distinct positions (default 0.80; 0 = report only).
#   records_per_shard      read only the first N records of each shard (0 = all of them). Left out, all of a
#                          pool up to 2 GB is read exactly, and a larger one is sampled at 200,000 records
#                          per shard - see SAMPLING below.
#
# WHAT IT COUNTS. Three numbers, because two of them would mislead on their own:
#
#   records              every line read.
#   after collapsing     consecutive identical lines counted once. tools/gen_data.cpp writes a record twice
#                        on purpose when the search and the static evaluation disagree (--disagreement-copies),
#                        and those copies are adjacent, so collapsing them measures diversity rather than the
#                        deliberate weighting.
#   distinct positions   distinct FEN (everything before the first " | "). The same position labelled by two
#                        different games counts once: it is one training input either way.
#
# The headline is distinct / collapsed. A healthy pool from the current defaults measures about 0.98 (1.0000
# at development scale); a campaign whose workers were replaying each other's games would land far below it.
# The 0.80 default gate sits well under the healthy value and far above any real degeneracy, so it fires on a
# broken campaign and not on an unlucky one. WATCH THE TREND, NOT ONE READING: distinct games transpose into
# each other, so a growing pool's unique fraction falls below 1.0 for entirely legitimate reasons, and the
# question is whether it is drifting down steadily as the pool grows or holding near its value.
#
# WHAT IT AUDITS. For a directory, the .meta sidecars are read too, and the campaign identities in them
# (global_worker_index / global_worker_count, which tools/run_gendata.sh passes) must be consistent: no two
# shards may share a global worker index, every shard must name the same campaign worker count, and every
# shard must name the same opening pool and pool size. Two machines given overlapping offsets is the one
# campaign mistake that silently produces duplicate games across the whole run, and the sidecars are where
# it is visible once the shards come home. A problem here fails the script however good the unique fraction
# looks - the fraction is measured over whatever records are read, and a sampled read can miss the overlap.
#
# SAMPLING. A duplicate here is a repeated GAME, which is a contiguous run of records inside each shard, so
# reading the first N records of EVERY shard keeps whole games intact and still sees every worker - which is
# where cross-worker duplication lives. Note what that measurement is: the distinct fraction of that
# sub-pool. The whole pool has more records and so more opportunity to collide, and its fraction can only be
# lower - the sampled number is an upper bound on how unique the pool is, which is the safe direction for a
# gate. Pass records_per_shard 0 to measure a pool exactly (memory: roughly 200 bytes per distinct position).
set -euo pipefail

POOL="${1:?pool.txt or shard_dir}"
MIN_UNIQUE="${2:-0.80}"
RECORDS_PER_SHARD="${3:-}"

# The shards to read, and the games they say they played (the .meta sidecars gen_data writes).
shards=()
metas=()
games_completed=0
if [ -d "$POOL" ]; then
    while IFS= read -r shard; do
        shards+=("$shard")
    done < <(find "$POOL" -maxdepth 1 -type f \( -name 'data_*.txt' -o -name 'seed*.txt' \) | sort)
    if [ "${#shards[@]}" -eq 0 ]; then
        echo "check_duplicates.sh: no data_*.txt or seed*.txt shards in '$POOL'" >&2
        exit 1
    fi
    for meta in "$POOL"/*.txt.meta; do
        [ -f "$meta" ] || continue
        metas+=("$meta")
        played="$(awk '/^games_completed /{print $2; exit}' "$meta")"
        games_completed=$((games_completed + ${played:-0}))
    done
elif [ -f "$POOL" ]; then
    shards=("$POOL")
else
    echo "check_duplicates.sh: '$POOL' is neither a file nor a directory" >&2
    exit 1
fi

# The campaign-identity audit, from the sidecars. One line per shard: the sidecar's name, the worker's
# global index and the campaign's worker count, the opening pool it used and how many positions that held.
# A field the sidecar does not carry comes through as "-" (a shard from before these were recorded).
partition_problems=0
if [ "${#metas[@]}" -gt 0 ]; then
    identities="$(
        for meta in "${metas[@]}"; do
            awk -v file="$(basename "$meta")" '
                /^global_worker_index /  { global_index = $2 }
                /^global_worker_count /  { global_count = $2 }
                /^openings /             { pool_file    = $2 }
                /^openings_count /       { pool_size    = $2 }
                END {
                    printf "%s %s %s %s %s\n", file,
                           (global_index == "" ? "-" : global_index), (global_count == "" ? "-" : global_count),
                           (pool_file == "" ? "-" : pool_file),       (pool_size == "" ? "-" : pool_size)
                }
            ' "$meta"
        done
    )"
    if ! printf '%s\n' "$identities" | awk '
        NF == 0 { next }
        {
            ++shards
            shard_file = $1; global_index = $2; global_count = $3; pool_file = $4; pool_size = $5
            if (global_index == "-" || global_count == "-") {
                ++unrecorded
                unrecorded_names = unrecorded_names " " shard_file
                next
            }
            if (global_index in owner_of_index) {
                printf "check_duplicates.sh: FAIL: %s and %s both claim campaign worker %s - their slices of\n",
                       owner_of_index[global_index], shard_file, global_index > "/dev/stderr"
                printf "check_duplicates.sh: the opening pool are the same, so they played the same games.\n" \
                       > "/dev/stderr"
                ++problems
            } else {
                owner_of_index[global_index] = shard_file
            }
            if (!(global_count in count_seen)) { count_seen[global_count] = shard_file; ++distinct_counts }
            if (!(pool_file " " pool_size in pool_seen)) {
                pool_seen[pool_file " " pool_size] = shard_file
                ++distinct_pools
            }
        }
        END {
            if (distinct_counts > 1) {
                printf "check_duplicates.sh: FAIL: the shards disagree about the campaign worker count, so the\n" \
                       > "/dev/stderr"
                printf "check_duplicates.sh: partition means nothing: " > "/dev/stderr"
                for (value in count_seen) { printf "%s (%s) ", value, count_seen[value] > "/dev/stderr" }
                printf "\n" > "/dev/stderr"
                ++problems
            }
            if (distinct_pools > 1) {
                printf "check_duplicates.sh: FAIL: the shards used different opening pools, so one index is not\n" \
                       > "/dev/stderr"
                printf "check_duplicates.sh: one position across them: " > "/dev/stderr"
                for (value in pool_seen) { printf "%s (%s) ", value, pool_seen[value] > "/dev/stderr" }
                printf "\n" > "/dev/stderr"
                ++problems
            }
            if (unrecorded > 0) {
                printf "check_duplicates.sh: WARNING: %d sidecar(s) record no campaign identity, so their\n",
                       unrecorded > "/dev/stderr"
                printf "check_duplicates.sh: partition cannot be audited:%s\n", unrecorded_names > "/dev/stderr"
            }
            if (problems == 0) {
                printf "check_duplicates.sh: %-36s %d shard(s), %d distinct campaign worker(s)\n",
                       "campaign identities", shards, shards - unrecorded
            }
            exit problems > 0 ? 1 : 0
        }
    '; then
        partition_problems=1
    fi
fi

# Bytes in the inputs, for the automatic sampling decision (a size, not a line count: reading every line
# twice to decide how to read them once would be the wrong way round).
total_bytes=0
for shard in "${shards[@]}"; do
    size="$(stat -c%s "$shard" 2>/dev/null || stat -f%z "$shard" 2>/dev/null || wc -c <"$shard")"
    total_bytes=$((total_bytes + size))
done
if [ -z "$RECORDS_PER_SHARD" ]; then
    if [ "$total_bytes" -gt $((2 * 1024 * 1024 * 1024)) ]; then
        RECORDS_PER_SHARD=200000
    else
        RECORDS_PER_SHARD=0
    fi
fi

echo "check_duplicates.sh: ${#shards[@]} shard(s), $((total_bytes / 1024 / 1024)) MiB in $POOL"
if [ "$RECORDS_PER_SHARD" -gt 0 ]; then
    echo "check_duplicates.sh: reading the first $RECORDS_PER_SHARD records of each shard (an upper bound on uniqueness)"
else
    echo "check_duplicates.sh: reading every record (exact)"
fi

# One pass. `previous` collapses the adjacent weighting copies; `seen` holds one entry per distinct position.
count_records() {
    awk '
        {
            ++records
            if ($0 != previous) { ++collapsed; previous = $0 }
            separator = index($0, " | ")
            key       = (separator > 0) ? substr($0, 1, separator - 1) : $0
            if (!(key in seen)) { seen[key] = 1; ++distinct }
        }
        END { printf "%d %d %d\n", records, collapsed, distinct }
    '
}
if [ "$RECORDS_PER_SHARD" -gt 0 ]; then
    counts="$(for shard in "${shards[@]}"; do head -n "$RECORDS_PER_SHARD" "$shard"; done | count_records)"
else
    counts="$(cat "${shards[@]}" | count_records)"
fi
records="$(echo "$counts" | cut -d' ' -f1)"
collapsed="$(echo "$counts" | cut -d' ' -f2)"
distinct="$(echo "$counts" | cut -d' ' -f3)"

if [ "$collapsed" -eq 0 ]; then
    echo "check_duplicates.sh: the pool is empty" >&2
    exit 1
fi
unique_fraction="$(awk -v d="$distinct" -v c="$collapsed" 'BEGIN{printf "%.4f", d / c}')"
printf 'check_duplicates.sh: %-36s %d\n' 'records' "$records"
printf 'check_duplicates.sh: %-36s %d\n' 'after collapsing weighting copies' "$collapsed"
printf 'check_duplicates.sh: %-36s %d\n' 'distinct positions' "$distinct"
awk -v u="$unique_fraction" \
    'BEGIN{printf "check_duplicates.sh: %-36s %.4f (%.2f %% duplicate)\n", "unique fraction", u, 100 * (1 - u)}'
if [ "$games_completed" -gt 0 ]; then
    # A repeated game contributes no new positions, so the distinct fraction of the records carries over to
    # the games: implied distinct games is the games the shards played times that fraction.
    awk -v g="$games_completed" -v u="$unique_fraction" \
        'BEGIN{printf "check_duplicates.sh: %-36s %d -> about %d distinct\n", "games played (from the .meta)", g, g * u}'
fi

if awk -v u="$unique_fraction" -v m="$MIN_UNIQUE" 'BEGIN{exit !(m > 0 && u < m)}'; then
    echo "check_duplicates.sh: FAIL: unique fraction $unique_fraction is under the $MIN_UNIQUE gate." >&2
    echo "check_duplicates.sh: the campaign is generating the same games repeatedly. Check that every worker" >&2
    echo "check_duplicates.sh: got --worker-index/--worker-count and --global-worker-offset/--global-worker-count" >&2
    echo "check_duplicates.sh: (tools/run_gendata.sh passes them), that the opening pool is large enough for the" >&2
    echo "check_duplicates.sh: games asked of it (tools/gen_openings.cpp), and that --random-plies is not 0. Do" >&2
    echo "check_duplicates.sh: not train on this pool." >&2
    exit 1
fi
if [ "$partition_problems" -ne 0 ]; then
    echo "check_duplicates.sh: FAIL: the campaign identities above collide or disagree, so this pool holds" >&2
    echo "check_duplicates.sh: duplicate games however its unique fraction reads. Fix the offsets (every" >&2
    echo "check_duplicates.sh: machine a distinct GLOBAL_WORKER_OFFSET into one GLOBAL_WORKER_COUNT, and the" >&2
    echo "check_duplicates.sh: identical pool file everywhere) and regenerate the colliding shards." >&2
    exit 1
fi
echo "check_duplicates.sh: OK (gate $MIN_UNIQUE)"

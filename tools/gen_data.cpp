/// @file gen_data.cpp Self-play training-data generator for the NNUE evaluation.
///
/// The engine plays itself with the loaded NNUE net and its own search, and every kept position is written
/// as one text record in the format bullet's `bullet-utils convert --from text` reads:
///
///     <fen> | <white_relative_eval_cp> | <wdl>
///
/// `wdl` is the eventual game result from **White's** perspective (1.0 win, 0.5 draw, 0.0 loss) and the
/// score is in centipawns, also from **White's** perspective. Both are White-relative, not side-to-move
/// relative: bullet's text parser mirrors the board for a black-to-move FEN and negates the score and the
/// result itself (`ChessBoard::from_str`, verified by converting a white-to-move and a black-to-move copy of
/// the start position with the same `| 123 | 1.0` and reading the two 32-byte records back: the second
/// came out `score=-123 result=0`). Writing side-to-move-relative values here would invert the label of
/// every black-to-move position.
///
/// **The search is the teacher, not the net.** The net only has to be good enough to play a coherent game;
/// the label is what the search backs up, which is why the two-stage mode below exists. The first self-play
/// bootstrap (2026-06, ~3.6M positions at a flat depth 6) lost to the handcrafted evaluation by 67 Elo, and
/// the lesson recorded then was that label quality, not volume, is the lever. Every option here spends more
/// search per position rather than producing more positions per second.
///
/// **Where the diversity comes from.** The engine is deterministic at a fixed budget, single-threaded, with
/// its book off, so a game is decided by the position it starts from - and only by that, across processes,
/// because the transposition table starts empty. (Within one process the table carries over from the
/// previous games, so a worker that starts twice from the same opening plays a near-copy rather than a
/// bit-identical game: ten games from one opening in one process still wrote 676 records with the dedup on,
/// not fifty and then nothing. That is not a diversity mechanism to lean on - the lines overlap heavily, and
/// two workers whose first game is from the same opening write the same game exactly.) So three things have
/// to compose, and a campaign that leans on only the first is capped near as many distinct games as it has
/// openings:
///
///  1. **The opening pool** (`--openings`): tens of thousands of balanced positions of our own play from
///     tools/gen_openings.cpp, or millions generated to a path of its own. The 306-position set that
///     tools/make_openings.sh can wring out of the TSCP book is a ceiling of that source, not a choice.
///  2. **A few random plies on top of it** (`--random-plies`), which multiply each opening by the number of
///     short legal lines out of it - about thirty to the power of the ply count. This is what makes the
///     number of distinct games unbounded rather than merely large: an opening pool alone cannot cover a
///     campaign, which at the ~71 records a game these defaults measure needs millions of games for 500M.
///  3. **A disjoint slice of the pool per worker, campaign-wide** (`--worker-index` / `--worker-count`
///     within one machine, `--global-worker-offset` / `--global-worker-count` across the whole campaign),
///     so that no two workers anywhere start from the same opening - which makes a duplicate game
///     impossible by construction rather than merely unlikely. Within one worker the openings are handed
///     out without replacement, in a seeded shuffle, so a pass over the slice uses each of them once.
///
/// **The partition is campaign-wide, not machine-wide, and that distinction is the whole of its value.** A
/// slice is the pool indices congruent to the worker's GLOBAL identity modulo the campaign's TOTAL worker
/// count. Taken modulo a machine's own worker count instead, a 24-worker box and a 12-worker box each have
/// a worker whose slice holds index 0, and only their differing seeds keep those two workers' games apart -
/// "unlikely" rather than "impossible", which is not a promise worth a week of rented CPU. Give every
/// machine the same pool file and a distinct, non-overlapping offset into one campaign-wide fan and the
/// slices are disjoint across the entire campaign. The arguments are checked for overlap and range before a
/// game is played, and the identity lands in the shard's .meta so that the assembled pool can be audited
/// afterwards too (tools/check_duplicates.sh does exactly that).
///
/// Determinism per seed survives all three: the shuffle, the slice and the random plies are all functions of
/// the seed and the worker's global identity, so a shard is reproducible and auditable. Nothing here reads a
/// clock or any other entropy. tools/check_duplicates.sh measures what the pool actually came out at.
///
/// Usage:  gen_data <out_file> <num_games> <seed> [options]
///
/// Positional arguments:
///   out_file   the shard to write (name it `data_<i>.txt` so tools/train_pipeline.sh's glob finds it).
///   num_games  self-play games this process plays.
///   seed       RNG seed, for the opening order and the random opening plies; distinct seeds give
///              distinct shards, and the same seed replays the same games.
///
/// Labelling options (each independently switchable, so they can be A/B tested one at a time):
///   --play-depth N        fixed depth for the game-playing search (default 8).
///   --play-nodes N        node budget for the game-playing search instead of a fixed depth. A budget is
///                         exact at one thread and, unlike a depth, costs the same on a quiet and on a
///                         tactical position, so throughput is predictable.
///   --label-depth N       fixed depth for the second-stage re-label (default: unset, see --label-nodes).
///   --label-nodes N       node budget for the second-stage re-label (default 220000, about depth 12 on this
///                         engine). Only one of --label-depth / --label-nodes applies; the last one wins.
///   --label-interval N    two-stage labelling: re-label and emit every Nth kept quiet position with the
///                         much larger label budget (1 = every one, 4 = every fourth). 0 (the default)
///                         is single-stage: every kept quiet position is emitted with the play search's own
///                         score. Two-stage decouples the cost of playing the game from the cost of the
///                         label, which is the whole point of the mode.
///   --pv-plies N          harvest the first N plies of the labelling search's principal variation as extra
///                         records, each carrying the backed-up root score (default 4, 0 = off, max 8).
///                         Up to N+1 labelled positions per search instead of one - measured about three
///                         times the records per deep search at N=4, since the quiet filter drops some.
///   --disagreement-cp N   emit a record --disagreement-copies times when |search score - static eval|
///                         exceeds N centipawns (default 150).
///   --disagreement-copies N  how many copies such a record gets (default 2; 1 = weighting off). This is a
///                         mild reweighting toward positions the net gets wrong. No position is ever
///                         dropped for agreeing - filtering on disagreement would skew the distribution the
///                         net is calibrated against.
///   --openings FILE       start each game from a position of this EPD / FEN opening set, instead of from the
///                         start position (nnue/openings-selfplay.epd is the committed one, 30,000 positions
///                         of our own play from tools/gen_openings.cpp). This also drops --random-plies to
///                         kDefaultRandomPliesWithOpenings unless --random-plies is given explicitly: eight
///                         random plies from the start position reach a position one side is already lost
///                         in, and the resign adjudication then ends the game four plies later.
///   --random-plies N      uniformly random opening plies per game, for diversity (default 8 from the start
///                         position, kDefaultRandomPliesWithOpenings on top of an opening; not recorded).
///   --worker-index I      this worker's index in its own machine's fan of --worker-count workers
///                         (default 0).
///   --worker-count N      how many workers this machine's fan has (default 1).
///   --global-worker-offset K  the campaign-wide index of this machine's first worker (default 0), so this
///                         worker's global identity is K plus its --worker-index. Every machine in one
///                         campaign needs a distinct, non-overlapping offset.
///   --global-worker-count N  how many workers the WHOLE campaign has, across every machine (default: the
///                         same as --worker-count, which is what a single-machine run wants). The opening
///                         pool is partitioned across the campaign - the worker whose global identity is G
///                         takes the openings whose index is congruent to G modulo this count - so no two
///                         workers anywhere can start a game from the same position, and therefore cannot
///                         generate the same game. A machine whose offset plus worker count would run past
///                         this count is refused rather than wrapping onto another machine's slices.
///                         tools/run_gendata.sh passes all four.
///   --no-dedup            keep repeated positions. By default a position whose Zobrist hash was already
///                         written to this shard is dropped: self-play repeats openings heavily.
///   --hash MB             transposition-table size per process (default: the engine's own 64 MB).
///   --net PATH            the labelling net (default: $PAWNSTAR_EVALFILE, else nnue/pawnstar-v20.bin).
///   --progress N          games between progress lines on stderr (default 50, 0 = silent).
///
/// Each run also writes `<out_file>.meta`, a provenance sidecar naming the engine version, the labelling
/// net and its SHA-256, every budget, the seed and the record count (§6 of nnue/README.md). It is a separate
/// file rather than a header line because `bullet-utils convert --from text` reads one record per line and
/// would try to parse a comment as a position.
///
/// The search runs single-threaded (this driver forces PAWNSTAR_THREADS=1), so a run is deterministic for a
/// given seed and set of options, and cheap per process; run one process per core to scale throughput
/// (tools/run_gendata.sh).
///
/// Build:  make tools   ->   build/gen_data

#include "chess_clock.h"
#include "constants.h"
#include "game.h"
#include "move.h"
#include "nnue.h"
#include "position.h"
#include "version.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iostream>
#include <memory>
#include <random>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

namespace
{
/// @brief A streambuf that discards everything, used to silence the engine's UCI info output.
class NullBuffer : public std::streambuf
{
  public:
    int overflow(int c) override
    {
        return c;
    }
};

/// @brief Positions above this |eval| (cp) are skipped as training targets (near-mate, unstable).
constexpr int kEvalCap = 3000;
/// @brief Adjudicate a win once one side's eval exceeds this (cp) for several consecutive plies.
constexpr int kResignThreshold = 1500;
/// @brief Consecutive decisive plies required to adjudicate.
constexpr int kResignPlies = 4;
/// @brief Hard cap on game length (plies) before adjudicating a draw, so self-play games terminate promptly
/// (the Game position history grows dynamically now, so this is a data-quality bound, not an overflow guard).
constexpr int kMaxPlies = 200;
/// @brief Upper bound on --pv-plies. The deeper into a principal variation a position lies, the less the
/// root score describes it (and the more likely the line is only the search's current guess), so the harvest
/// is capped rather than taking the whole line.
constexpr int kMaxPrincipalVariationPlies = 8;
/// @brief Default node budget for the second-stage re-label: about depth 12 on this engine (measured
/// single-threaded: depth 9 is 48.4k nodes/position, extrapolating to ~220k at depth 12).
constexpr uint64_t kDefaultLabelNodes = 220000;
/// @brief Random opening plies played on top of an opening-set position, unless --random-plies says
/// otherwise.
///
/// Two, measured over 480 games a setting at the campaign budgets (nnue/README.md §6 has the table). It
/// costs 15 % of the records a game yields against no random plies at all (70.8 against 83.8) and 5 % of the
/// throughput, and doubles the share of games already decided at their first label (12.3 % against 6.5 %) -
/// and it buys the thing the opening pool cannot: each opening becomes the ~900 two-ply legal lines out of
/// it, so the campaign's distinct-game count is no longer bounded by the pool's size. Three and four cost
/// 24 % and 27 % of the records for no measurable improvement in anything, and an even count keeps the
/// random line symmetric - each side moves the same number of times, rather than one getting a free move.
/// Eight, the default from the start position, is far past the knee: it reaches positions a side is already
/// lost in, and such a game yields about one record.
constexpr int kDefaultRandomPliesWithOpenings = 2;
/// @brief Sanity cap on a campaign's worker count, on a machine's fan and on its offset into the campaign.
/// A campaign of a million single-threaded workers is not a thing that exists, and the cap is also what
/// keeps the overlap arithmetic (offset + worker count) below the range of std::size_t rather than wrapping
/// to a small number and passing a check it should fail.
constexpr std::size_t kMaximumCampaignWorkers = 1000000;

/// @brief One search budget: either a fixed depth or a node limit, as the UCI `go depth` / `go nodes` forms.
struct SearchBudget
{
    bool     is_node_limited = false; ///< True to stop at node_limit, false to search to depth.
    int      depth           = 8;     ///< Target depth when is_node_limited is false.
    uint64_t node_limit      = 0;     ///< Node budget when is_node_limited is true.
};

/// @brief Everything the run was asked to do; also what the .meta sidecar reports.
struct Options
{
    std::string              out_file;                         ///< Shard path.
    int                      num_games = 0;                    ///< Self-play games to play.
    uint64_t                 seed      = 0;                    ///< Seed for the opening order and the random plies.
    SearchBudget             play_budget;                      ///< Budget for the game-playing search.
    SearchBudget             label_budget;                     ///< Budget for the second-stage re-label.
    int                      label_interval            = 0;    ///< 0 = single-stage; else re-label every Nth.
    int                      principal_variation_plies = 4;    ///< PV plies harvested per labelling search.
    int                      disagreement_centipawns   = 150;  ///< |search - static| above which to reweight.
    int                      disagreement_copies       = 2;    ///< Copies written for a disagreeing record.
    int                      random_plies              = 8;    ///< Random opening plies per game.
    bool                     does_deduplicate          = true; ///< Drop positions already written this shard.
    std::size_t              hash_megabytes            = 0;    ///< 0 = leave the engine's default size.
    std::string              net_path;                         ///< Labelling net.
    std::string              openings_file;                    ///< EPD opening set, or empty for the start position.
    std::vector<std::string> openings;                         ///< The parsed opening FENs.
    std::size_t              worker_index         = 0;         ///< Index in this machine's own fan.
    std::size_t              worker_count         = 1;         ///< Workers in this machine's own fan.
    std::size_t              global_worker_offset = 0;         ///< Campaign-wide index of this machine's first worker.
    std::size_t              global_worker_count  = 0;  ///< Campaign-wide worker total; 0 = the same as worker_count.
    int                      progress_games       = 50; ///< Games between stderr progress lines.
};

/// @brief What the run counted, reported on stderr and in the .meta sidecar.
///
/// The three failure counts are the degeneracy instruments. A campaign that generates millions of games
/// cannot be read game by game, so the shard has to say for itself how many of its games were worth playing:
/// one that abandoned or immediately decided most of them, or that wrote records for hardly any, has settings
/// wrong and the shard sizes alone would not show it.
struct Statistics
{
    uint64_t records_written              = 0; ///< Records written to the shard (copies counted separately).
    uint64_t games_completed              = 0; ///< Games that reached a result.
    uint64_t games_abandoned              = 0; ///< Games whose opening line left no legal move.
    uint64_t games_decided_at_first_label = 0; ///< Games whose first searched ply was already past resigning.
    uint64_t games_without_records        = 0; ///< Games that contributed no record at all (see the note).
};

/// @brief Hands this worker its openings without replacement.
///
/// Two jobs, and they are the whole answer to a campaign generating the same game twice:
///
///  * **Across every worker in the campaign**: the pool is partitioned, the worker whose global identity is
///    G taking the openings whose index is congruent to G modulo the campaign's worker count. The slices are
///    disjoint, so two workers cannot start a game from the same position - and since the engine is
///    deterministic, a game is decided by where it starts. A duplicate game across workers is therefore
///    impossible by construction, not merely unlikely. (Folding the worker index into the RNG stream instead
///    would have made a collision improbable, which is a weaker promise for a 1-2 week campaign and much
///    harder to argue.) The identity is global rather than an index within one machine's fan precisely so
///    that this holds across machines of different core counts: see the file header.
///  * **Within a worker**: the slice is handed out in a seeded shuffle and reshuffled at the end of each
///    pass, so a worker playing fewer games than its slice holds never repeats an opening at all, and one
///    playing more spreads the repeats evenly instead of drawing them from a uniform distribution with
///    replacement - which starts repeating after about the square root of the pool.
///
/// The shuffle is seeded from the run's seed and the worker's global identity together, so each worker walks
/// its own slice in its own order, and the whole thing is still a function of the seed: the shard replays
/// exactly.
class OpeningSchedule
{
  public:
    OpeningSchedule(const std::vector<std::string> &openings, std::size_t global_worker_index,
                    std::size_t global_worker_count, uint64_t seed);
    bool               IsEmpty() const;
    std::size_t        Size() const;
    const std::string &Next();

  private:
    const std::vector<std::string> *openings_;    ///< The whole pool (not owned).
    std::vector<std::size_t>        order_;       ///< This worker's slice, in the current pass's order.
    std::size_t                     next_ = 0;    ///< Position in order_ the next game takes.
    std::mt19937_64                 shuffle_rng_; ///< Reshuffles each pass; separate from the game RNG.
};

/// @brief Build the schedule for one worker.
/// @param openings The whole opening pool, which must outlive the schedule.
/// @param global_worker_index This worker's campaign-wide identity, below global_worker_count.
/// @param global_worker_count How many workers the whole campaign shares the pool between.
/// @param seed The run's seed; mixed with the global identity so each worker orders its slice differently.
inline OpeningSchedule::OpeningSchedule(const std::vector<std::string> &openings, std::size_t global_worker_index,
                                        std::size_t global_worker_count, uint64_t seed)
    : openings_{&openings}
{
    for (std::size_t index = global_worker_index; index < openings.size(); index += global_worker_count)
    {
        order_.push_back(index);
    }
    // The golden-ratio odd constant is the usual way to turn two numbers into one well-spread seed; what
    // matters here is only that two workers of one campaign get different orders and that the mapping is
    // fixed. The global identity goes in rather than the machine-local index, so that worker 0 of two
    // machines does not walk one order twice.
    shuffle_rng_.seed(seed + 0x9e3779b97f4a7c15ull * (uint64_t)(global_worker_index + 1));
    std::shuffle(order_.begin(), order_.end(), shuffle_rng_);
}

/// @brief Whether this worker has no openings at all (an empty pool, or more workers than openings).
inline bool OpeningSchedule::IsEmpty() const
{
    return order_.empty();
}

/// @brief How many openings this worker's slice holds.
inline std::size_t OpeningSchedule::Size() const
{
    return order_.size();
}

/// @brief The next game's opening, reshuffling when a pass over the slice is complete.
/// @return The FEN to start from.
inline const std::string &OpeningSchedule::Next()
{
    if (next_ >= order_.size())
    {
        std::shuffle(order_.begin(), order_.end(), shuffle_rng_);
        next_ = 0;
    }
    return (*openings_)[order_[next_++]];
}

/// @brief One text record, ready to write once the game's result is known.
struct Record
{
    std::string fen;         ///< The position, in FEN.
    int         white_score; ///< Label in centipawns, White's perspective.
    int         copies;      ///< How many times to write it (disagreement weighting).
};

/// @brief A kept quiet position from the playing phase, awaiting its label.
struct Candidate
{
    std::string       fen;                 ///< The position, in FEN.
    int               white_score;         ///< The play search's score, White's perspective.
    std::vector<Move> principal_variation; ///< The play search's PV (used when single-stage).
};

// ---------------------------------------------------------------------------
// SHA-256, for the provenance sidecar. A net's filename says nothing about its contents, and the lineage of
// a net trained on this data has to be auditable later: only a net two or more generations removed from the
// retired v12 may ship, which means knowing exactly which net labelled which shard. FIPS 180-4, no
// dependency - the engine links no crypto library and should not start now for one hash of one file.
// ---------------------------------------------------------------------------

/// @brief The SHA-256 round constants (FIPS 180-4 §4.2.2).
constexpr std::array<uint32_t, 64> kSha256RoundConstants{
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
    0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
    0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
    0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
    0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
    0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u, 0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u};

/// @brief Rotate a 32-bit word right. @param x Word. @param n Bits. @return The rotated word.
constexpr uint32_t RotateRight(uint32_t x, int n)
{
    return (x >> n) | (x << (32 - n));
}

/// @brief Compress one 64-byte block into the running state (FIPS 180-4 §6.2.2).
/// @param state The eight-word hash state, updated in place. @param block The 64 message bytes.
void Sha256CompressBlock(std::array<uint32_t, 8> &state, const unsigned char *block)
{
    std::array<uint32_t, 64> schedule{};
    for (int i = 0; i < 16; ++i)
    {
        schedule[(std::size_t)i] = ((uint32_t)block[i * 4] << 24) | ((uint32_t)block[i * 4 + 1] << 16) |
                                   ((uint32_t)block[i * 4 + 2] << 8) | (uint32_t)block[i * 4 + 3];
    }
    for (int i = 16; i < 64; ++i)
    {
        const uint32_t s0 = RotateRight(schedule[(std::size_t)i - 15], 7) ^
                            RotateRight(schedule[(std::size_t)i - 15], 18) ^ (schedule[(std::size_t)i - 15] >> 3);
        const uint32_t s1 = RotateRight(schedule[(std::size_t)i - 2], 17) ^
                            RotateRight(schedule[(std::size_t)i - 2], 19) ^ (schedule[(std::size_t)i - 2] >> 10);
        schedule[(std::size_t)i] = schedule[(std::size_t)i - 16] + s0 + schedule[(std::size_t)i - 7] + s1;
    }
    std::array<uint32_t, 8> working = state;
    for (int i = 0; i < 64; ++i)
    {
        const uint32_t s1    = RotateRight(working[4], 6) ^ RotateRight(working[4], 11) ^ RotateRight(working[4], 25);
        const uint32_t ch    = (working[4] & working[5]) ^ (~working[4] & working[6]);
        const uint32_t temp1 = working[7] + s1 + ch + kSha256RoundConstants[(std::size_t)i] + schedule[(std::size_t)i];
        const uint32_t s0    = RotateRight(working[0], 2) ^ RotateRight(working[0], 13) ^ RotateRight(working[0], 22);
        const uint32_t maj   = (working[0] & working[1]) ^ (working[0] & working[2]) ^ (working[1] & working[2]);
        const uint32_t temp2 = s0 + maj;
        working[7]           = working[6];
        working[6]           = working[5];
        working[5]           = working[4];
        working[4]           = working[3] + temp1;
        working[3]           = working[2];
        working[2]           = working[1];
        working[1]           = working[0];
        working[0]           = temp1 + temp2;
    }
    for (std::size_t i = 0; i < state.size(); ++i)
    {
        state[i] += working[i];
    }
}

/// @brief The SHA-256 of a file, as 64 lowercase hex digits.
/// @param path File to hash.
/// @return The digest, or an empty string if the file could not be read.
std::string Sha256OfFile(const std::string &path)
{
    std::ifstream in{path, std::ios::binary};
    if (!in)
    {
        return {};
    }
    std::array<uint32_t, 8>       state{0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
                                        0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u};
    std::array<unsigned char, 64> buffer{};
    uint64_t                      total_bytes = 0;
    std::streamsize               filled      = 0;
    while (in.read(reinterpret_cast<char *>(buffer.data()), 64) || in.gcount() > 0)
    {
        filled = in.gcount();
        total_bytes += (uint64_t)filled;
        if (filled < 64)
        {
            break; // the final, partial block: padded below
        }
        Sha256CompressBlock(state, buffer.data());
        filled = 0;
    }
    // Pad: a 0x80 byte, zeroes, then the message length in bits as a big-endian 64-bit word. A final block
    // with no room for the length (56 bytes or more of message) spills into one extra block.
    std::array<unsigned char, 128> tail{};
    std::memcpy(tail.data(), buffer.data(), (std::size_t)filled);
    tail[(std::size_t)filled]     = 0x80;
    const std::size_t tail_blocks = filled >= 56 ? 2 : 1;
    const uint64_t    bit_length  = total_bytes * 8;
    for (int i = 0; i < 8; ++i)
    {
        tail[tail_blocks * 64 - 1 - (std::size_t)i] = (unsigned char)((bit_length >> (8 * i)) & 0xff);
    }
    for (std::size_t block = 0; block < tail_blocks; ++block)
    {
        Sha256CompressBlock(state, tail.data() + block * 64);
    }
    std::string digest;
    digest.reserve(64);
    for (const uint32_t word : state)
    {
        for (int shift = 28; shift >= 0; shift -= 4)
        {
            digest.push_back("0123456789abcdef"[(word >> shift) & 0xf]);
        }
    }
    return digest;
}

// ---------------------------------------------------------------------------
// Self-play
// ---------------------------------------------------------------------------

/// @brief Convert a side-to-move-relative score to White's perspective, which is what a record carries.
/// @param side_to_move_score Score from the mover's point of view (the engine's negamax convention).
/// @param color_to_move Side to move in that position.
/// @return The same score from White's point of view.
constexpr int WhiteRelativeScore(int side_to_move_score, Color color_to_move)
{
    return color_to_move == kWhite ? side_to_move_score : -side_to_move_score;
}

/// @brief Arm the game for one search: clear the cancel flag and set the budget, as UCI `go depth N` /
/// `go nodes N` do.
///
/// Two engine contracts meet here, and both bite a caller that drives Game::SearchRootNode in a loop:
///
///  * **The clock must be set after SetPosition**, which resets it (to the 5-minute standard control).
///  * **The cancel flag must be cleared by whoever starts a search.** SearchRootNode deliberately does not
///    clear `is_cancel_pending_` - it *sets* it at the end of every search to wind up the Lazy SMP helpers,
///    and leaves it set; clearing it is the job of the caller that starts the next search (Game::StartThinking
///    for the UCI path, Game::SetPosition for `bench` and the tests, which set a position per search). A
///    tool that plays a whole game through one Game therefore has to clear it per move, or every search
///    after the first returns from its first cancellation check - which looks exactly like a working run,
///    just with one labelled position per game instead of eighty.
///
/// @param game Game to prepare. @param budget The budget to apply.
void PrepareSearch(Game &game, const SearchBudget &budget)
{
    game.is_cancel_pending_.store(false, std::memory_order_relaxed);
    if (budget.is_node_limited)
    {
        // A node-limited search runs with no clock and stops at the budget, exactly as handle_go does; the
        // limit is exact at one thread (Search polls node_count_ per node).
        game.time_control_.clock_type_ = ChessClock::kInfinite;
        game.time_control_.max_nodes_  = budget.node_limit;
    }
    else
    {
        game.time_control_.clock_type_ = ChessClock::kFixedDepth;
        game.time_control_.depth_      = budget.depth;
        game.time_control_.max_nodes_  = 0;
    }
}

/// @brief Whether the current position ends the game; if so set the White-relative result.
/// Position has no IsCheckmate/IsStalemate (UCI never declares a result), so the mate/stalemate split is
/// made here from the legal move count and the check flag, as the search's own terminal handling does.
/// @param game Game whose current position to test. @param white_result Set to 1.0 / 0.5 / 0.0 on a result.
/// @return true if the game is over.
bool IsGameOver(const Game &game, double &white_result)
{
    const Position &position = game.CurrentPosition();
    if (position.GenerateLegalMoves().size() == 0)
    {
        if (position.IsInCheck())
        {
            white_result = position.color_to_move_ == kWhite ? 0.0 : 1.0; // the side to move is mated
        }
        else
        {
            white_result = 0.5; // stalemate
        }
        return true;
    }
    if (position.IsDrawByMaterial() || position.reversible_move_count_ >= 100)
    {
        white_result = 0.5;
        return true;
    }
    // Threefold repetition over the game history. The search has its own (faster) stack-walking test on
    // SearchState; this driver holds the whole game in Game::positions_, so it counts outright.
    int repetitions = 0;
    for (const Position &earlier : game.positions_)
    {
        if (earlier.hash_ == position.hash_)
        {
            ++repetitions;
        }
    }
    if (repetitions >= 3)
    {
        white_result = 0.5;
        return true;
    }
    return false;
}

/// @brief Append one position's records, applying the disagreement weighting and the shard-wide dedup.
/// @param options The run's options. @param network The labelling net, for the static eval.
/// @param position The position to record. @param white_score Its label, White's perspective.
/// @param seen Zobrist hashes already written to this shard (updated); ignored when dedup is off.
/// @param records Output records.
void AppendRecord(const Options &options, const nnue::Network &network, const Position &position, int white_score,
                  std::unordered_set<zobrist_t> &seen, std::vector<Record> &records)
{
    if (options.does_deduplicate && !seen.insert(position.hash_).second)
    {
        return; // already in this shard. Note the Zobrist hash excludes the fifty-move clock, so two
                // positions differing only in that clock count as one.
    }
    // The static eval is the net's own opinion of this position, with no search and no fifty-move scaling
    // (Network::Evaluate, the `nnue` UCI diagnostic, not EvaluatePosition). Where it disagrees with what the
    // search backed up, the net is wrong and the position carries more signal, so it is written more than
    // once. Every position is still written at least once: filtering on agreement would bias the
    // distribution the evaluation is calibrated against.
    int copies = 1;
    if (options.disagreement_copies > 1)
    {
        const int static_white_score = WhiteRelativeScore(network.Evaluate(position), position.color_to_move_);
        if (std::abs(white_score - static_white_score) > options.disagreement_centipawns)
        {
            copies = options.disagreement_copies;
        }
    }
    records.push_back({position.ToString(), white_score, copies});
}

/// @brief Record a labelled position and the first few plies of the line the label came from.
///
/// The root score is what the search backed up through the whole principal variation, so every position
/// along that line shares it - in White's perspective it is literally the same number, which is why no
/// per-ply sign flip appears here: `white_score` is already White-relative, and White-relative scores do
/// not depend on whose turn it is. (A side-to-move-relative harvest would have to negate on every ply.)
/// That is what makes the harvest cheap: one deep search yields up to pv_plies + 1 labelled positions.
///
/// The line is replayed on a copy of the position, never on the Game or a SearchState, so neither the game
/// history nor any NNUE accumulator is touched - there is nothing to undo. Each move is checked against the
/// position's legal moves before it is played: a principal variation can be seeded from a transposition-table
/// move (see CopyVariation's callers in search_state.h), and a hash collision could put a move in it that is
/// not legal in the position it is reached at. The harvest stops at the first such move rather than
/// corrupting the line.
///
/// @param options The run's options. @param network The labelling net.
/// @param root The position that was searched. @param white_score Its label, White's perspective.
/// @param principal_variation The labelling search's PV (may be empty).
/// @param seen Zobrist hashes already written to this shard (updated).
/// @param records Output records.
void AppendLabelledLine(const Options &options, const nnue::Network &network, const Position &root, int white_score,
                        const std::vector<Move> &principal_variation, std::unordered_set<zobrist_t> &seen,
                        std::vector<Record> &records)
{
    AppendRecord(options, network, root, white_score, seen, records);
    Position  position = root;
    const int harvest  = std::min(options.principal_variation_plies, kMaxPrincipalVariationPlies);
    for (int ply = 0; ply < harvest; ++ply)
    {
        if ((std::size_t)ply >= principal_variation.size())
        {
            return; // the line ran out
        }
        const Move played   = principal_variation[(std::size_t)ply];
        MoveList   legal    = position.GenerateLegalMoves();
        bool       is_legal = false;
        for (const Move &candidate : legal)
        {
            if (candidate == played)
            {
                is_legal = true;
                break;
            }
        }
        if (!is_legal)
        {
            return; // not a move of this position (see the collision note above); stop here
        }
        position = position.MakeMove(played);
        // The same filter the root had to pass: a position in check or about to resolve a capture is not a
        // clean static-evaluation target. The move *out* of this position is the next PV move, so quietness
        // is judged on that; without one (the end of the line) the position is not harvested.
        if ((std::size_t)ply + 1 >= principal_variation.size())
        {
            return;
        }
        if (position.IsInCheck() || !principal_variation[(std::size_t)ply + 1].IsQuiet())
        {
            continue;
        }
        AppendRecord(options, network, position, white_score, seen, records);
    }
}

/// @brief Play one self-play game and write its records, with the final result baked into every one.
/// @param options The run's options.
/// @param game A Game with the labelling net loaded; reset to the start position here and reused.
/// @param openings This worker's opening schedule, advanced by this call; empty for the start position.
/// @param rng Opening-ply RNG, advanced by this call.
/// @param seen Zobrist hashes already written to this shard (updated).
/// @param out The shard stream.
/// @param statistics The run's counters, updated by this call.
void PlayGame(const Options &options, Game &game, OpeningSchedule &openings, std::mt19937_64 &rng,
              std::unordered_set<zobrist_t> &seen, std::ofstream &out, Statistics &statistics)
{
    // Start from an opening position if one was supplied, else from the standard start position. The opening
    // set is the first source of diversity and the random plies below are the second; see the file header.
    if (openings.IsEmpty())
    {
        game.SetPosition();
    }
    else
    {
        game.SetPosition(openings.Next());
    }
    game.use_own_book_ = false; // the engine's own judgement only, and the book's pick is random

    // Random opening plies for diversity (not recorded).
    for (int i = 0; i < options.random_plies; ++i)
    {
        MoveList moves = game.CurrentPosition().GenerateLegalMoves();
        if (moves.size() == 0)
        {
            ++statistics.games_abandoned;
            return; // dead opening line; abandon this game
        }
        game.PlayMove(moves[rng() % moves.size()]);
    }

    std::vector<Candidate> candidates;
    double                 white_result     = 0.5;
    int                    decisive_plies   = 0;
    int                    decisive_sign    = 0;
    bool                   has_first_search = false; // the first ply this game actually searched

    for (int ply = 0;; ++ply)
    {
        if (IsGameOver(game, white_result))
        {
            break;
        }
        if (ply >= kMaxPlies)
        {
            white_result = 0.5;
            break;
        }
        const Position &position = game.CurrentPosition();
        PrepareSearch(game, options.play_budget);
        const Move best        = game.SearchRootNode();
        const int  white_score = WhiteRelativeScore(best.score(), position.color_to_move_);
        // A root that returned without searching - a forced move, or a book move - carries no score (the
        // node count is the engine's own marker for that), so it is played but never labelled.
        const bool was_searched = game.last_search_node_count_ != 0;

        // Was the game already decided before it was played? A random opening line can reach a position one
        // side is lost in, and the resign adjudication then ends the game a few plies later having learnt
        // almost nothing. The count of those is how the random-ply setting is judged (the file header).
        if (was_searched && !has_first_search)
        {
            has_first_search = true;
            if (std::abs(white_score) > kResignThreshold)
            {
                ++statistics.games_decided_at_first_label;
            }
        }

        // Keep only quiet, in-bounds, not-in-check positions (clean eval targets for NNUE).
        if (was_searched && !position.IsInCheck() && best.IsQuiet() && std::abs(white_score) < kEvalCap)
        {
            candidates.push_back({position.ToString(), white_score, game.principal_variation_});
        }

        // Resign-style adjudication to end decided games early.
        if (was_searched)
        {
            const int sign = white_score > kResignThreshold ? 1 : (white_score < -kResignThreshold ? -1 : 0);
            if (sign != 0 && sign == decisive_sign)
            {
                if (++decisive_plies >= kResignPlies)
                {
                    white_result = sign > 0 ? 1.0 : 0.0;
                    break;
                }
            }
            else
            {
                decisive_sign  = sign;
                decisive_plies = sign != 0 ? 1 : 0;
            }
        }

        game.PlayMove(best);
    }

    // The game is over, so the Game is free for the second-stage labelling searches: each one resets it to a
    // candidate's FEN. Nothing below reads the game history any more.
    std::vector<Record> records;
    for (std::size_t i = 0; i < candidates.size(); ++i)
    {
        const Candidate &candidate = candidates[i];
        if (options.label_interval <= 0)
        {
            // Single-stage: the play search's own score is the label.
            AppendLabelledLine(options, game.nnue_network_, Position::FromString(candidate.fen), candidate.white_score,
                               candidate.principal_variation, seen, records);
            continue;
        }
        if (i % (std::size_t)options.label_interval != 0)
        {
            continue; // sampled out: played for free, never labelled
        }
        // Two-stage: re-search this position with the much larger label budget and use *that* score. The
        // filter is applied again to the deeper verdict, which can see that the position is not quiet after
        // all, or that it is past the eval cap.
        game.SetPosition(candidate.fen);
        PrepareSearch(game, options.label_budget);
        const Move      best     = game.SearchRootNode();
        const Position &position = game.CurrentPosition();
        if (game.last_search_node_count_ == 0)
        {
            continue;
        }
        const int white_score = WhiteRelativeScore(best.score(), position.color_to_move_);
        if (position.IsInCheck() || !best.IsQuiet() || std::abs(white_score) >= kEvalCap)
        {
            continue;
        }
        AppendLabelledLine(options, game.nnue_network_, position, white_score, game.principal_variation_, seen,
                           records);
    }

    const char *result_text = white_result > 0.75 ? "1.0" : (white_result < 0.25 ? "0.0" : "0.5");
    for (const Record &record : records)
    {
        for (int copy = 0; copy < record.copies; ++copy)
        {
            out << record.fen << " | " << record.white_score << " | " << result_text << '\n';
            ++statistics.records_written;
        }
    }
    ++statistics.games_completed;
    if (records.empty())
    {
        // Nothing at all from a completed game. A handful are ordinary (a game decided in four plies), but a
        // large share means the shard is replaying games it has already written, since the dedup drops every
        // position of a repeat - which is exactly the degeneracy the opening pool and the partition prevent.
        ++statistics.games_without_records;
    }
}

/// @brief Describe a budget for the .meta sidecar. @param budget The budget. @return "depth 8" / "nodes 220000".
std::string DescribeBudget(const SearchBudget &budget)
{
    return budget.is_node_limited ? "nodes " + std::to_string(budget.node_limit)
                                  : "depth " + std::to_string(budget.depth);
}

/// @brief Read an EPD / FEN opening set, one position per line.
///
/// Only the first four FEN fields are taken (board, side to move, castling rights, en passant square) and a
/// fresh "0 1" clock is appended, so a four-field EPD line, a six-field FEN and an EPD line carrying
/// opcodes after the position all read the same way - and a game never inherits a stale fifty-move clock
/// from the file. Blank lines and `#` comments are skipped, which is what lets the committed opening set
/// carry its provenance in its own header.
///
/// @param path The file to read.
/// @param openings Parsed FENs are appended here.
/// @return true if the file could be read (an empty but readable file is still an error for the caller).
bool LoadOpenings(const std::string &path, std::vector<std::string> &openings)
{
    std::ifstream in{path};
    if (!in)
    {
        return false;
    }
    for (std::string line; std::getline(in, line);)
    {
        // A file written on Windows (or checked out with core.autocrlf) ends each line "\r\n", and the "\r"
        // would otherwise stick to the last FEN field and stop the position parsing. .gitattributes keeps
        // the committed set LF; this covers a set from anywhere else.
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n'))
        {
            line.pop_back();
        }
        std::istringstream       fields{line};
        std::vector<std::string> tokens;
        for (std::string token; fields >> token && tokens.size() < 4;)
        {
            tokens.push_back(token);
        }
        if (tokens.empty() || tokens[0][0] == '#')
        {
            continue;
        }
        if (tokens.size() < 4)
        {
            std::cerr << "gen_data: skipping malformed opening '" << line << "'\n";
            continue;
        }
        const std::string fen      = tokens[0] + ' ' + tokens[1] + ' ' + tokens[2] + ' ' + tokens[3] + " 0 1";
        const Position    position = Position::FromString(fen);
        // Position::FromString answers an unparseable FEN with the start position rather than failing, so
        // compare the board back: a line that did not round-trip was junk, and silently self-playing the
        // start position a thousand times instead would be invisible in the output.
        if (position.ToString().substr(0, tokens[0].size() + 1) != tokens[0] + ' ' ||
            position.GenerateLegalMoves().size() == 0)
        {
            std::cerr << "gen_data: skipping unusable opening '" << line << "'\n";
            continue;
        }
        openings.push_back(fen);
    }
    return true;
}

/// @brief The current UTC time as an ISO-8601 second, for the .meta sidecar.
/// Written with strftime rather than chrono's formatters, which the oldest standard library this repo
/// builds against (CI's clang-18 toolchain) does not carry for a system_clock time point.
/// @return e.g. "2026-10-26T18:04:11Z", or an empty string if the clock could not be read.
std::string UtcTimestamp()
{
    const std::time_t now      = std::time(nullptr);
    const std::tm    *utc      = std::gmtime(&now);
    char              text[32] = {};
    if (utc == nullptr || std::strftime(text, sizeof(text), "%Y-%m-%dT%H:%M:%SZ", utc) == 0)
    {
        return {};
    }
    return text;
}

/// @brief Read a non-negative integer argument. @param text The token. @return Its value, or -1 if malformed.
int64_t ParseNonNegative(const char *text)
{
    char          *end   = nullptr;
    const uint64_t value = std::strtoull(text, &end, 10);
    if (end == text || *end != '\0' || value > (uint64_t)INT64_MAX)
    {
        return -1;
    }
    return (int64_t)value;
}
} // namespace

int main(int argc, char **argv)
{
    if (argc < 4)
    {
        std::cerr << "usage: " << argv[0] << " <out_file> <num_games> <seed> [options]\n"
                  << "       see the comment at the top of tools/gen_data.cpp for every option\n";
        return 1;
    }
    Options options;
    options.out_file  = argv[1];
    options.num_games = (int)ParseNonNegative(argv[2]);
    options.seed      = std::strtoull(argv[3], nullptr, 10);
    if (options.num_games < 0)
    {
        std::cerr << "gen_data: <num_games> must be a non-negative integer\n";
        return 1;
    }
    options.label_budget.is_node_limited = true;
    options.label_budget.node_limit      = kDefaultLabelNodes;

    // Every option takes exactly one value, so a missing value is an error rather than a silently ignored
    // flag: a mistyped campaign option must not quietly generate data under the wrong settings.
    bool is_random_plies_set = false;
    for (int i = 4; i < argc; ++i)
    {
        const std::string flag      = argv[i];
        const bool        has_value = i + 1 < argc;
        const int64_t     value     = has_value ? ParseNonNegative(argv[i + 1]) : -1;
        if (flag == "--no-dedup")
        {
            options.does_deduplicate = false;
            continue;
        }
        if (!has_value)
        {
            std::cerr << "gen_data: '" << flag << "' needs a value\n";
            return 1;
        }
        if (flag == "--net")
        {
            options.net_path = argv[++i];
            continue;
        }
        if (flag == "--openings")
        {
            options.openings_file = argv[++i];
            // A few random plies on top of an opening, not the eight a start-position run takes: enough to
            // make the number of distinct games unbounded, few enough that the game is still worth playing.
            if (!is_random_plies_set)
            {
                options.random_plies = kDefaultRandomPliesWithOpenings;
            }
            continue;
        }
        if (value < 0)
        {
            std::cerr << "gen_data: '" << flag << "' needs a non-negative integer, got '" << argv[i + 1] << "'\n";
            return 1;
        }
        ++i;
        if (flag == "--play-depth")
        {
            options.play_budget.is_node_limited = false;
            options.play_budget.depth           = (int)value;
        }
        else if (flag == "--play-nodes")
        {
            options.play_budget.is_node_limited = true;
            options.play_budget.node_limit      = (uint64_t)value;
        }
        else if (flag == "--label-depth")
        {
            options.label_budget.is_node_limited = false;
            options.label_budget.depth           = (int)value;
        }
        else if (flag == "--label-nodes")
        {
            options.label_budget.is_node_limited = true;
            options.label_budget.node_limit      = (uint64_t)value;
        }
        else if (flag == "--label-interval")
        {
            options.label_interval = (int)value;
        }
        else if (flag == "--pv-plies")
        {
            options.principal_variation_plies = (int)std::min<int64_t>(value, kMaxPrincipalVariationPlies);
        }
        else if (flag == "--disagreement-cp")
        {
            options.disagreement_centipawns = (int)value;
        }
        else if (flag == "--disagreement-copies")
        {
            options.disagreement_copies = (int)std::max<int64_t>(1, value);
        }
        else if (flag == "--random-plies")
        {
            options.random_plies = (int)value;
            is_random_plies_set  = true;
        }
        else if (flag == "--worker-index")
        {
            options.worker_index = (std::size_t)value;
        }
        else if (flag == "--worker-count")
        {
            options.worker_count = (std::size_t)value;
        }
        else if (flag == "--global-worker-offset")
        {
            options.global_worker_offset = (std::size_t)value;
        }
        else if (flag == "--global-worker-count")
        {
            options.global_worker_count = (std::size_t)value;
        }
        else if (flag == "--hash")
        {
            options.hash_megabytes = (std::size_t)value;
        }
        else if (flag == "--progress")
        {
            options.progress_games = (int)value;
        }
        else
        {
            std::cerr << "gen_data: unrecognised option '" << flag << "'\n";
            return 1;
        }
    }

    // Force a deterministic single-threaded search. Set before any Game is constructed, which is when the
    // default thread count is read.
#if defined(_WIN32)
    _putenv_s("PAWNSTAR_THREADS", "1");
#else
    setenv("PAWNSTAR_THREADS", "1", 1);
#endif

    if (options.net_path.empty())
    {
        const char *eval_file = std::getenv("PAWNSTAR_EVALFILE");
        options.net_path      = eval_file != nullptr ? eval_file : "nnue/pawnstar-v20.bin";
    }

    if (options.worker_count == 0 || options.worker_index >= options.worker_count)
    {
        std::cerr << "gen_data: --worker-index must be below --worker-count, and --worker-count at least 1\n";
        return 1;
    }
    if (options.worker_count > kMaximumCampaignWorkers || options.global_worker_count > kMaximumCampaignWorkers ||
        options.global_worker_offset > kMaximumCampaignWorkers)
    {
        std::cerr << "gen_data: --worker-count, --global-worker-count and --global-worker-offset must each be at "
                  << "most " << kMaximumCampaignWorkers << '\n';
        return 1;
    }
    // The campaign-wide fan. Left unset, it is this machine's own fan - which is what a single-machine run
    // wants, and keeps every command line written before the campaign arguments existed meaning exactly
    // what it meant then.
    const std::size_t global_worker_count =
        options.global_worker_count != 0 ? options.global_worker_count : options.worker_count;
    const std::size_t global_worker_index = options.global_worker_offset + options.worker_index;
    // THE GUARD THAT PROTECTS A WEEK OF RENTED CPU. An overlapping or out-of-range offset would silently
    // generate colliding data - two machines' workers sharing slices of the pool, and so playing the same
    // games - and every shard would look healthy until the assembled pool was measured at the end. Refuse it
    // loudly instead, before a single game is played.
    if (options.global_worker_offset + options.worker_count > global_worker_count)
    {
        std::cerr << "gen_data: FATAL: --global-worker-offset " << options.global_worker_offset
                  << " plus --worker-count " << options.worker_count << " runs past --global-worker-count "
                  << global_worker_count << ", so this\n"
                  << "gen_data: machine's workers would wrap onto another machine's slices of the opening pool "
                  << "and play the same games. Set\n"
                  << "gen_data: --global-worker-count to the campaign's total worker count and give every machine "
                  << "a distinct offset (the first\n"
                  << "gen_data: machine 0, each next one the previous machine's offset plus its worker count).\n";
        return 1;
    }

    if (!options.openings_file.empty())
    {
        if (!LoadOpenings(options.openings_file, options.openings) || options.openings.empty())
        {
            std::cerr << "gen_data: FATAL: no usable openings in '" << options.openings_file << "'\n";
            return 1;
        }
        std::cerr << "gen_data: " << options.openings.size() << " openings from " << options.openings_file << '\n';
    }
    // Built even for a start-position run, where it is empty and PlayGame takes the other branch.
    OpeningSchedule openings{options.openings, global_worker_index, global_worker_count, options.seed};
    if (!options.openings.empty() && openings.IsEmpty())
    {
        // More campaign workers than openings. Refusing is right: a worker with no slice would silently
        // self-play the start position for a week, and the shard would look like any other.
        std::cerr << "gen_data: FATAL: campaign worker " << global_worker_index << " of " << global_worker_count
                  << " got no openings from a pool of " << options.openings.size()
                  << "; generate a bigger pool (tools/gen_openings.cpp) or fan out fewer workers\n";
        return 1;
    }
    if (!openings.IsEmpty())
    {
        std::cerr << "gen_data: machine worker " << options.worker_index << '/' << options.worker_count
                  << ", campaign worker " << global_worker_index << '/' << global_worker_count << ", takes "
                  << openings.Size() << " of them\n";
    }

    // NNUE is the only evaluator, so self-play needs a loaded net. Load once and reuse across games. The
    // Game is heap-allocated: at eight king buckets its inline feature-weight array is ~12.6 MB and would
    // overflow the default stack as a local.
    auto game = std::make_unique<Game>();
    if (!game->nnue_network_.Load(options.net_path))
    {
        std::cerr << "gen_data: FATAL: could not load NNUE net '" << options.net_path
                  << "' (set PAWNSTAR_EVALFILE or pass --net). NNUE is the only evaluator.\n";
        return 1;
    }
    if (options.hash_megabytes != 0)
    {
        game->transposition_table_.Resize(options.hash_megabytes);
    }

    std::ofstream out{options.out_file};
    if (!out)
    {
        std::cerr << "gen_data: cannot write '" << options.out_file << "'\n";
        return 1;
    }

    // The search prints UCI info lines on std::cout; a data run has no use for them and they would swamp
    // the shard's own progress output. std::cerr is left alone, which is where this tool reports.
    NullBuffer      null_buffer;
    std::streambuf *old_cout = std::cout.rdbuf(&null_buffer);

    const auto                    started = std::chrono::steady_clock::now();
    std::unordered_set<zobrist_t> seen;
    std::mt19937_64               rng{options.seed};
    Statistics                    statistics;
    for (int g = 0; g < options.num_games; ++g)
    {
        PlayGame(options, *game, openings, rng, seen, out, statistics);
        if (options.progress_games > 0 && ((g + 1) % options.progress_games) == 0)
        {
            const double elapsed_seconds =
                std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
            std::cerr << "info string gen_data seed=" << options.seed << " game " << (g + 1) << '/' << options.num_games
                      << " records=" << statistics.records_written << " records/s="
                      << (elapsed_seconds > 0.0 ? (double)statistics.records_written / elapsed_seconds : 0.0) << '\n';
        }
    }
    out.flush();
    out.close();
    std::cout.rdbuf(old_cout);

    const double elapsed_seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();

    // The provenance sidecar. A separate file, not a header line in the shard: bullet's text converter reads
    // one record per line and would try to parse a comment as a position.
    std::ofstream meta{options.out_file + ".meta"};
    if (!meta)
    {
        std::cerr << "gen_data: cannot write '" << options.out_file << ".meta'\n";
        return 1;
    }
    meta << "# Pawnstar self-play training-data shard provenance (tools/gen_data.cpp).\n"
         << "shard " << options.out_file << '\n'
         << "engine_version " << VersionString() << '\n'
         << "labelling_net " << options.net_path << '\n'
         << "labelling_net_sha256 " << Sha256OfFile(options.net_path) << '\n'
         << "play_budget " << DescribeBudget(options.play_budget) << '\n'
         << "label_budget " << (options.label_interval > 0 ? DescribeBudget(options.label_budget) : "unused") << '\n'
         << "label_interval " << options.label_interval << '\n'
         << "principal_variation_plies " << std::min(options.principal_variation_plies, kMaxPrincipalVariationPlies)
         << '\n'
         << "disagreement_centipawns " << options.disagreement_centipawns << '\n'
         << "disagreement_copies " << options.disagreement_copies << '\n'
         << "random_plies " << options.random_plies << '\n'
         << "openings " << (options.openings_file.empty() ? "none" : options.openings_file) << '\n'
         << "openings_count " << options.openings.size() << '\n'
         << "openings_in_slice " << openings.Size() << '\n'
         << "worker_index " << options.worker_index << '\n'
         << "worker_count " << options.worker_count << '\n'
         << "global_worker_offset " << options.global_worker_offset << '\n'
         << "global_worker_index " << global_worker_index << '\n'
         << "global_worker_count " << global_worker_count << '\n'
         << "deduplicated " << (options.does_deduplicate ? "yes" : "no") << '\n'
         << "seed " << options.seed << '\n'
         << "games_requested " << options.num_games << '\n'
         << "games_completed " << statistics.games_completed << '\n'
         << "games_abandoned " << statistics.games_abandoned << '\n'
         << "games_decided_at_first_label " << statistics.games_decided_at_first_label << '\n'
         << "games_without_records " << statistics.games_without_records << '\n'
         << "records " << statistics.records_written << '\n'
         << "elapsed_seconds " << elapsed_seconds << '\n'
         << "generated_utc " << UtcTimestamp() << '\n';
    meta.flush();

    std::cerr << "gen_data: wrote " << statistics.records_written << " records from " << statistics.games_completed
              << " games in " << elapsed_seconds << "s ("
              << (elapsed_seconds > 0.0 ? (double)statistics.records_written / elapsed_seconds : 0.0)
              << " records/s) -> " << options.out_file << '\n';
    std::cerr << "gen_data: " << statistics.games_abandoned << " games abandoned in the opening, "
              << statistics.games_decided_at_first_label << " already decided at the first label, "
              << statistics.games_without_records << " with no records\n";
    return 0;
}

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
/// Usage:  gen_data <out_file> <num_games> <seed> [options]
///
/// Positional arguments:
///   out_file   the shard to write (name it `data_<i>.txt` so tools/train_pipeline.sh's glob finds it).
///   num_games  self-play games this process plays.
///   seed       RNG seed, for the opening choice and the random opening plies; distinct seeds give
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
///   --openings FILE       start each game from a random position of this EPD / FEN opening set, instead of
///                         from the start position (nnue/openings.epd is the committed one). This also sets
///                         --random-plies to 0 unless --random-plies is given explicitly: a balanced
///                         opening is a far better source of diversity than random plies, which often reach
///                         a position one side is already lost in - the resign adjudication then ends the
///                         game four plies later and the whole game yields almost nothing.
///   --random-plies N      uniformly random opening plies per game, for diversity (default 8; not recorded).
///   --no-dedup            keep repeated positions. By default a position whose Zobrist hash was already
///                         written to this shard is dropped: self-play repeats openings heavily.
///   --hash MB             transposition-table size per process (default: the engine's own 64 MB).
///   --net PATH            the labelling net (default: $PAWNSTAR_EVALFILE, else nnue/pawnstar-v12.bin).
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
    uint64_t                 seed      = 0;                    ///< RNG seed for the random opening plies.
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
    int                      progress_games = 50;              ///< Games between stderr progress lines.
};

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
/// @param rng Opening-ply RNG, advanced by this call.
/// @param seen Zobrist hashes already written to this shard (updated).
/// @param out The shard stream.
/// @param records_written Incremented by the number of records written.
/// @param games_completed Incremented when the game produced a result (not when the opening died).
void PlayGame(const Options &options, Game &game, std::mt19937_64 &rng, std::unordered_set<zobrist_t> &seen,
              std::ofstream &out, uint64_t &records_written, uint64_t &games_completed)
{
    // Start from an opening position if one was supplied, else from the standard start position. A balanced
    // opening set is the better source of diversity: eight uniformly random plies reach positions a side is
    // often already lost in, which the resign adjudication then ends in four more plies.
    if (options.openings.empty())
    {
        game.SetPosition();
    }
    else
    {
        game.SetPosition(options.openings[rng() % options.openings.size()]);
    }
    game.use_own_book_ = false; // the engine's own judgement only, and the book's pick is random

    // Random opening plies for diversity (not recorded).
    for (int i = 0; i < options.random_plies; ++i)
    {
        MoveList moves = game.CurrentPosition().GenerateLegalMoves();
        if (moves.size() == 0)
        {
            return; // dead opening line; abandon this game
        }
        game.PlayMove(moves[rng() % moves.size()]);
    }

    std::vector<Candidate> candidates;
    double                 white_result   = 0.5;
    int                    decisive_plies = 0;
    int                    decisive_sign  = 0;

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
            ++records_written;
        }
    }
    ++games_completed;
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
            // An opening set replaces the random plies rather than adding to them, unless the caller asks
            // for both: eight random plies on top of a balanced opening would undo the point of the set.
            if (!is_random_plies_set)
            {
                options.random_plies = 0;
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
        options.net_path      = eval_file != nullptr ? eval_file : "nnue/pawnstar-v12.bin";
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
    uint64_t                      records_written = 0;
    uint64_t                      games_completed = 0;
    for (int g = 0; g < options.num_games; ++g)
    {
        PlayGame(options, *game, rng, seen, out, records_written, games_completed);
        if (options.progress_games > 0 && ((g + 1) % options.progress_games) == 0)
        {
            const double elapsed_seconds =
                std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
            std::cerr << "info string gen_data seed=" << options.seed << " game " << (g + 1) << '/' << options.num_games
                      << " records=" << records_written
                      << " records/s=" << (elapsed_seconds > 0.0 ? (double)records_written / elapsed_seconds : 0.0)
                      << '\n';
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
         << "deduplicated " << (options.does_deduplicate ? "yes" : "no") << '\n'
         << "seed " << options.seed << '\n'
         << "games_requested " << options.num_games << '\n'
         << "games_completed " << games_completed << '\n'
         << "records " << records_written << '\n'
         << "elapsed_seconds " << elapsed_seconds << '\n'
         << "generated_utc " << UtcTimestamp() << '\n';
    meta.flush();

    std::cerr << "gen_data: wrote " << records_written << " records from " << games_completed << " games in "
              << elapsed_seconds << "s (" << (elapsed_seconds > 0.0 ? (double)records_written / elapsed_seconds : 0.0)
              << " records/s) -> " << options.out_file << '\n';
    return 0;
}

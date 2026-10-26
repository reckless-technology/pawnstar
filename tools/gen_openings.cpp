/// @file gen_openings.cpp Balanced opening-position generator, from Pawnstar's own play.
///
/// Writes an EPD opening set for self-play data generation (tools/gen_data.cpp `--openings`), built from
/// nothing but this engine's own move generation and its own network: short randomised lines are played from
/// the start position, and a position is kept when the static NNUE evaluation says it is roughly level. No
/// book, no third party's data, no provenance to carry.
///
/// The file is a valid SPRT opening file too (tools/run_sprt.sh `OPENINGS=`), but the book-derived set from
/// tools/make_openings.sh stays the match set: its positions are opening-book lines, and a match wants
/// openings a player would recognise rather than randomised ones. Training data wants the opposite - the
/// wider the distribution the labels cover, the better, which is what makes random lines right here.
///
/// **Why this exists beside tools/make_openings.sh.** That script harvests the TSCP book, whose 385
/// heavily-overlapping lines hold only about 420 distinct positions in any sensible ply window - about 306
/// of them balanced. A self-play campaign starting from a 306-position set is capped near 306 distinct
/// games, because the engine is deterministic at a fixed budget with the book off: the opening decides the
/// game (near, not exactly, because the transposition table carries over between the games of one process -
/// see tools/gen_data.cpp's header, which measures it; a repeat is a near-copy, not a mechanism to rely on).
/// Random play has no such ceiling, so the count is a parameter here (tens of thousands committed, millions
/// generated to a path of its own for a campaign) rather than a property of the source.
///
/// **One position per line played.** A line is played to a ply drawn uniformly from the window and only the
/// position it ends at is considered. Harvesting every ply of a window from one line, as the book script
/// does, would emit up to five positions sharing a prefix - near-duplicate starts, which is the very thing
/// a large pool is for. Lines are cheap (a few microseconds each), so independence is worth the extra ones.
///
/// **Balance is measured against the engine's own evaluation of the START POSITION**, not against zero: the
/// net scores the initial position at about +158 cp, so a zero-centred filter would reject normal positions
/// and keep the ones it mildly dislikes. The baseline is read from the net at run time, so this stays right
/// when the net changes. The static evaluation is used rather than a search: it costs microseconds instead
/// of milliseconds, and "is this roughly level" does not need search depth.
///
/// What that filter cannot see is a tactic. Measured against the book-derived set, whose openings are sound:
/// 6.5 % of self-play games from this pool are already past the resign threshold at their first labelling
/// search, where none of 3,040 games from the book set were (nnue/README.md §6). Such a game still yields
/// records, just fewer, and it costs about half of the 30 % drop in records a game the diversity design
/// pays for. A cheap tactical filter here - a shallow search, or rejecting a position whose mover has a
/// winning capture by static exchange evaluation - is the obvious next improvement, and would be a change
/// to this file alone.
///
/// Usage:  gen_openings <out_file> <num_positions> <seed> [options]
///
/// Positional arguments:
///   out_file       the EPD to write (nnue/openings-selfplay.epd is the committed one).
///   num_positions  how many balanced positions to keep. The generator stops at this many, or when the line
///                  budget runs out.
///   seed           RNG seed. The same seed, options and net reproduce the file byte for byte, which is the
///                  audit trail for a set that carries no external source.
///
/// Options:
///   --min-ply N    first ply of the window a kept position may be at (default 8).
///   --max-ply N    last ply of the window (default 12). Eight to twelve plies is far enough in for the
///                  lines to diverge and still an opening rather than a random middlegame.
///   --margin-cp N  keep a position only when its White-relative static evaluation is within this many
///                  centipawns of the start position's (default 125, as tools/make_openings.sh).
///   --max-lines N  stop after this many lines however few positions were kept (default: 64 per requested
///                  position, which is ample - the measured acceptance rate is about one line in four).
///   --net PATH     the net whose evaluation filters (default: $PAWNSTAR_EVALFILE, else
///                  nnue/pawnstar-v12.bin).
///   --progress N   positions between progress lines on stderr (default 5000, 0 = silent).
///
/// The output carries its provenance in '#' comment lines, as nnue/openings.epd does: the generator, the
/// engine version, the net and its baseline, the window, the margin, the seed and what was rejected.
/// tools/gen_data.cpp skips those lines and tools/run_sprt.sh strips them before fastchess sees the file.
///
/// Build:  make tools   ->   build/gen_openings

#include "constants.h"
#include "move.h"
#include "nnue.h"
#include "position.h"
#include "version.h"

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <random>
#include <string>
#include <unordered_set>
#include <vector>

namespace
{
/// @brief First ply of the default harvest window: deep enough that the lines have diverged.
constexpr int kDefaultMinimumPly = 8;
/// @brief Last ply of the default harvest window: still an opening rather than a random middlegame.
constexpr int kDefaultMaximumPly = 12;
/// @brief Default balance margin (cp) around the start position's own evaluation, as make_openings.sh uses.
constexpr int kDefaultMarginCentipawns = 125;
/// @brief Hard upper bound on --max-ply. Beyond this a "random opening" is a random middlegame, and the
/// static evaluation of one says little about whether a game from it is worth playing.
constexpr int kMaximumPlyLimit = 40;
/// @brief Lines played per requested position before giving up, when --max-lines is not given. The measured
/// acceptance rate is about one line in four, so this is ample headroom for a narrow margin or window.
constexpr uint64_t kDefaultLinesPerPosition = 64;
/// @brief Positions between stderr progress lines, unless --progress says otherwise.
constexpr int kDefaultProgressPositions = 5000;

/// @brief Everything the run was asked to do; also what the output's provenance header reports.
struct Options
{
    std::string out_file;                                       ///< EPD path to write.
    int         position_count    = 0;                          ///< Balanced positions wanted.
    uint64_t    seed              = 0;                          ///< RNG seed for the random lines.
    int         minimum_ply       = kDefaultMinimumPly;         ///< First ply of the harvest window.
    int         maximum_ply       = kDefaultMaximumPly;         ///< Last ply of the harvest window.
    int         margin_centipawns = kDefaultMarginCentipawns;   ///< Balance margin around the baseline.
    uint64_t    line_limit        = 0;                          ///< 0 = kDefaultLinesPerPosition per position.
    std::string net_path;                                       ///< Net whose evaluation filters.
    int         progress_positions = kDefaultProgressPositions; ///< Positions between progress lines.
};

/// @brief What the run rejected and why, for the provenance header: a set that keeps far fewer positions
/// than it was asked for should say which filter ate them rather than leaving it to be guessed.
struct Statistics
{
    uint64_t lines_played        = 0; ///< Random lines played from the start position.
    uint64_t lines_died          = 0; ///< Lines that reached a mate or stalemate before the target ply.
    uint64_t rejected_terminal   = 0; ///< Positions in check, or with no legal move, at the target ply.
    uint64_t rejected_unbalanced = 0; ///< Positions outside the balance margin.
    uint64_t rejected_duplicate  = 0; ///< Positions whose Zobrist key had already been kept.
};

/// @brief Convert a side-to-move-relative score to White's perspective, which the balance filter works in.
/// @param side_to_move_score Score from the mover's point of view (the engine's negamax convention).
/// @param color_to_move Side to move in that position.
/// @return The same score from White's point of view.
constexpr int WhiteRelativeScore(int side_to_move_score, Color color_to_move)
{
    return color_to_move == kWhite ? side_to_move_score : -side_to_move_score;
}

/// @brief The first four FEN fields of a position - board, side to move, castling rights, en passant square.
///
/// The clock fields are dropped deliberately: a consumer appends its own ("0 1" in gen_data's LoadOpenings),
/// so a game never inherits a stale fifty-move count from the file. This is the form nnue/openings.epd uses.
///
/// @param position The position to describe.
/// @return The four fields, space separated.
std::string FourFieldFen(const Position &position)
{
    const std::string fen    = position.ToString();
    std::size_t       cursor = 0;
    for (int field = 0; field < 4; ++field)
    {
        const std::size_t space = fen.find(' ', cursor);
        if (space == std::string::npos)
        {
            return fen; // fewer than four fields: hand back what there is rather than truncating blindly
        }
        cursor = space + 1;
    }
    return fen.substr(0, cursor - 1);
}

/// @brief Play one uniformly random legal line from the start position and report where it ended.
/// @param start The start position.
/// @param target_ply How many plies to play.
/// @param rng The line RNG, advanced by this call.
/// @param ended Set to the position reached, when the line survived.
/// @return true if the line reached target_ply, false if it ran into a mate or a stalemate first.
bool PlayRandomLine(const Position &start, int target_ply, std::mt19937_64 &rng, Position &ended)
{
    Position position = start;
    for (int ply = 0; ply < target_ply; ++ply)
    {
        const MoveList moves = position.GenerateLegalMoves();
        if (moves.size() == 0)
        {
            return false;
        }
        position = position.MakeMove(moves[rng() % moves.size()]);
    }
    ended = position;
    return true;
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
        std::cerr << "usage: " << argv[0] << " <out_file> <num_positions> <seed> [options]\n"
                  << "       see the comment at the top of tools/gen_openings.cpp for every option\n";
        return 1;
    }
    Options options;
    options.out_file       = argv[1];
    options.position_count = (int)ParseNonNegative(argv[2]);
    options.seed           = std::strtoull(argv[3], nullptr, 10);
    if (options.position_count <= 0)
    {
        std::cerr << "gen_openings: <num_positions> must be a positive integer\n";
        return 1;
    }

    // Every option takes exactly one value, so a missing value is an error rather than a silently ignored
    // flag: a mistyped generation option must not quietly produce a set under the wrong filter.
    for (int i = 4; i < argc; ++i)
    {
        const std::string flag = argv[i];
        if (i + 1 >= argc)
        {
            std::cerr << "gen_openings: '" << flag << "' needs a value\n";
            return 1;
        }
        if (flag == "--net")
        {
            options.net_path = argv[++i];
            continue;
        }
        const int64_t value = ParseNonNegative(argv[i + 1]);
        if (value < 0)
        {
            std::cerr << "gen_openings: '" << flag << "' needs a non-negative integer, got '" << argv[i + 1] << "'\n";
            return 1;
        }
        ++i;
        if (flag == "--min-ply")
        {
            options.minimum_ply = (int)value;
        }
        else if (flag == "--max-ply")
        {
            options.maximum_ply = (int)value;
        }
        else if (flag == "--margin-cp")
        {
            options.margin_centipawns = (int)value;
        }
        else if (flag == "--max-lines")
        {
            options.line_limit = (uint64_t)value;
        }
        else if (flag == "--progress")
        {
            options.progress_positions = (int)value;
        }
        else
        {
            std::cerr << "gen_openings: unrecognised option '" << flag << "'\n";
            return 1;
        }
    }
    if (options.minimum_ply < 1 || options.maximum_ply < options.minimum_ply || options.maximum_ply > kMaximumPlyLimit)
    {
        std::cerr << "gen_openings: need 1 <= --min-ply <= --max-ply <= " << kMaximumPlyLimit << '\n';
        return 1;
    }

    if (options.net_path.empty())
    {
        const char *eval_file = std::getenv("PAWNSTAR_EVALFILE");
        options.net_path      = eval_file != nullptr ? eval_file : "nnue/pawnstar-v12.bin";
    }
    // NNUE is the only evaluator, and the balance filter is the net's own opinion. The Network is
    // heap-allocated: at eight king buckets its inline feature-weight array is ~12.6 MB and would overflow
    // the default stack as a local.
    auto network = std::make_unique<nnue::Network>();
    if (!network->Load(options.net_path))
    {
        std::cerr << "gen_openings: FATAL: could not load NNUE net '" << options.net_path
                  << "' (set PAWNSTAR_EVALFILE or pass --net). NNUE is the only evaluator.\n";
        return 1;
    }

    const Position start    = Position::FromString("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    const int      baseline = WhiteRelativeScore(network->Evaluate(start), start.color_to_move_);
    std::cerr << "gen_openings: start-position baseline " << baseline << " cp; keeping positions within +/-"
              << options.margin_centipawns << " cp of it\n";

    const uint64_t line_limit =
        options.line_limit != 0 ? options.line_limit : (uint64_t)options.position_count * kDefaultLinesPerPosition;
    const auto                    started = std::chrono::steady_clock::now();
    std::mt19937_64               rng{options.seed};
    std::unordered_set<zobrist_t> seen;
    std::vector<std::string>      kept;
    Statistics                    statistics;
    kept.reserve((std::size_t)options.position_count);
    while ((int)kept.size() < options.position_count && statistics.lines_played < line_limit)
    {
        ++statistics.lines_played;
        // One position per line, at a ply drawn uniformly from the window: see the header - harvesting the
        // whole window from one line would emit positions sharing a prefix.
        const int target_ply =
            options.minimum_ply + (int)(rng() % (uint64_t)(options.maximum_ply - options.minimum_ply + 1));
        Position ended = start;
        if (!PlayRandomLine(start, target_ply, rng, ended))
        {
            ++statistics.lines_died;
            continue;
        }
        // A position in check is not a neutral start (the first move is forced or nearly so), and one with no
        // legal move is not a start at all. gen_data's LoadOpenings refuses the latter anyway.
        if (ended.IsInCheck() || ended.GenerateLegalMoves().size() == 0)
        {
            ++statistics.rejected_terminal;
            continue;
        }
        const int white_score = WhiteRelativeScore(network->Evaluate(ended), ended.color_to_move_);
        if (std::abs(white_score - baseline) > options.margin_centipawns)
        {
            ++statistics.rejected_unbalanced;
            continue;
        }
        // Dedup on the Zobrist key rather than on the FEN text: it is the position's identity, and it
        // excludes the clock fields the output drops anyway.
        if (!seen.insert(ended.hash_).second)
        {
            ++statistics.rejected_duplicate;
            continue;
        }
        kept.push_back(FourFieldFen(ended));
        if (options.progress_positions > 0 && (kept.size() % (std::size_t)options.progress_positions) == 0)
        {
            std::cerr << "info string gen_openings " << kept.size() << '/' << options.position_count
                      << " positions from " << statistics.lines_played << " lines\n";
        }
    }
    const double elapsed_seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();

    // Binary mode so the file is LF on every platform: Windows' text mode would write CRLF, and an opening
    // set is read field by field by tools that would otherwise find "\r" stuck to the last FEN field
    // (.gitattributes keeps the committed sets LF for the same reason).
    std::ofstream out{options.out_file, std::ios::binary};
    if (!out)
    {
        std::cerr << "gen_openings: cannot write '" << options.out_file << "'\n";
        return 1;
    }
    out << "# Pawnstar opening set, for self-play data generation (tools/gen_data.cpp --openings).\n"
        << "#\n"
        << "# Generated by tools/gen_openings.cpp (Pawnstar " << VersionString()
        << ") from THIS ENGINE'S OWN PLAY: no\n"
        << "# book, no third party's data. " << statistics.lines_played
        << " lines of uniformly random legal moves were played\n"
        << "# from the start position, each to a ply drawn uniformly from [" << options.minimum_ply << ", "
        << options.maximum_ply << "], and the position\n"
        << "# the line ended at was kept when it was not in check and the static NNUE evaluation of it was\n"
        << "# within " << options.margin_centipawns << " centipawns of the net's evaluation of the start position ("
        << baseline << " cp,\n"
        << "# White's perspective; net " << options.net_path << "). Duplicates were dropped on the Zobrist key. One\n"
        << "# position per line played, so no two openings here share a line - and the count is a parameter,\n"
        << "# not a ceiling of the source.\n"
        << "#\n"
        << "# seed " << options.seed << "; kept " << kept.size() << " of " << statistics.lines_played << " lines ("
        << statistics.lines_died << " died, " << statistics.rejected_terminal << " in check or terminal, "
        << statistics.rejected_unbalanced << "\n"
        << "# unbalanced, " << statistics.rejected_duplicate
        << " duplicate). The generator is deterministic: the same seed,\n"
        << "# options and net reproduce this file exactly, which is the audit trail for a set with no\n"
        << "# external source.\n"
        << "#\n"
        << "# A CAMPAIGN WANTS A BIGGER POOL THAN THIS. Generate one to a path of its own rather than\n"
        << "# committing it, and point gen_data at it:\n"
        << "#     build/gen_openings ~/pawnstar_nnue/openings-1m.epd 1000000 1\n"
        << "#\n"
        << "# One position per line, as the first four FEN fields. Lines beginning with '#' are comments:\n"
        << "# tools/gen_data.cpp skips them, and tools/run_sprt.sh strips them before handing the file to\n"
        << "# fastchess, whose EPD reader does not.\n";
    for (const std::string &fen : kept)
    {
        out << fen << '\n';
    }
    out.flush();
    if (!out)
    {
        std::cerr << "gen_openings: failed writing '" << options.out_file << "'\n";
        return 1;
    }

    std::cerr << "gen_openings: wrote " << kept.size() << " openings from " << statistics.lines_played << " lines in "
              << elapsed_seconds << "s -> " << options.out_file << '\n';
    if ((int)kept.size() < options.position_count)
    {
        std::cerr << "gen_openings: WARNING: wanted " << options.position_count << " positions but the line budget ("
                  << line_limit << ") ran out; raise --max-lines, widen --margin-cp or widen the ply window\n";
    }
    return 0;
}

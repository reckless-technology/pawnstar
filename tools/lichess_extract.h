#pragma once
/// @file lichess_extract.h The Lichess-PGN-to-training-shard extraction core, apart from its command line.
///
/// Everything here is the part worth pinning with tests: the SAN resolver, the PGN reader, the eval-comment
/// parser, the result mapping and the filter/emit loop. tools/lichess_extract.cpp is the command line, the
/// shard writer and the provenance sidecars around it; test/lichess_extract_test.cpp drives this header
/// directly with hand-written PGN, which is the only way to pin the rules below in process.
///
/// ## What the records are
///
/// One line per kept position, in the format bullet's `bullet-utils convert --from text` reads and the one
/// tools/gen_data.cpp writes:
///
///     <fen> | <white_relative_eval_cp> | <wdl>
///
/// **Both columns are White-relative, not side-to-move relative.** bulletformat's text parser mirrors the
/// board for a black-to-move FEN and then negates both (`bulletformat-1.8.0/src/chess.rs` ends
/// `ChessBoard::from_str` with `if stm == 1 { board.score = -board.score; board.result = 2 - board.result; }`),
/// so writing side-to-move-relative values inverts the label of every black-to-move position. Lichess's own
/// `[%eval]` is already White-relative, so the score passes through with **no sign flip** - verified against
/// the real file (a game where White's 20th move was a blunder has `20. g4?? { [%eval -3.87] }`, and the
/// game's own openings read `1. e4 { [%eval 0.18] }`).
///
/// The only `wdl` tokens the parser accepts are `1.0` `0.5` `0.0` `1` `1/2` `0`, so the PGN `Result` header
/// value has to be mapped: `1-0` -> `1.0`, `1/2-1/2` -> `0.5`, `0-1` -> `0.0`. A `*` (unfinished) game has
/// no label at all and is skipped.
///
/// ## The off-by-one that matters
///
/// In Lichess PGN the eval comment **follows** the move and is the evaluation of the position **after** that
/// move was played:
///
///     1. e4 { [%eval 0.18] [%clk 0:02:00] } 1... e5 { [%eval 0.22] [%clk 0:02:00] }
///
/// `0.18` belongs to the position after `e4`, with Black to move. So the FEN emitted with a given eval is
/// the position reached by playing the annotated move, never the position it was played from. Getting this
/// wrong shifts every label by one ply, which trains to something subtly worse while every number in the
/// yield report still looks right - which is why test/lichess_extract_test.cpp pins it against a FEN
/// written out by hand.
///
/// ## The filters, and why they are gen_data's
///
/// This data is to be compared head-to-head with the self-play pool on a matched-size pilot, so the filters
/// mirror tools/gen_data.cpp's as closely as the source allows - a difference in the filters would show up
/// as a difference in the data source:
///
///  * the side to move must not be in check (`Position::IsInCheck`);
///  * the move must be quiet (no capture, no promotion - `Move::IsQuiet`), see QuietMode below;
///  * `|white_score|` must be under kEvalCap (3000);
///  * a position whose Zobrist key is already in this shard is dropped (gen_data deduplicates per shard,
///    and tools/check_duplicates.sh is what measures the assembled pool);
///  * the first `skip_plies` plies of each game are dropped (default 8): opening theory is the same few
///    hundred positions in every game, and keeping them would fill the pool with duplicates that the
///    per-shard dedup cannot see across shards;
///  * nothing past ply `max_plies` (200) is kept, because gen_data adjudicates a draw there and so its pool
///    contains nothing later either;
///  * a game is dropped from the point where one side's eval has been past kResignThreshold (1500) for
///    kResignPlies (4) consecutive annotated plies, because gen_data's resign adjudication ends its games
///    there and so its pool contains nothing after that point.
///
/// The one thing gen_data does that is **not** mirrored is the disagreement reweighting
/// (`--disagreement-cp` / `--disagreement-copies`, which writes a record twice when the search and the net's
/// static eval disagree). It needs the labelling net's static eval, and this tool loads no net - it is an
/// extractor, not a labeller. It drops no position either way, so the two pools stay comparable in content;
/// only the weighting differs.

#include "constants.h"
#include "move.h"
#include "position.h"
#include "square.h"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <istream>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "sha256.h"

namespace lichess
{
/// @brief Positions above this |eval| (cp) are skipped as training targets (near-mate, unstable).
/// tools/gen_data.cpp's kEvalCap.
constexpr int kEvalCap = 3000;

/// @brief One side's eval past this (cp) for kResignPlies consecutive plies ends the harvest of that game.
/// tools/gen_data.cpp's kResignThreshold, where it adjudicates the self-play game a win.
constexpr int kResignThreshold = 1500;

/// @brief Consecutive decisive plies before the harvest stops. tools/gen_data.cpp's kResignPlies.
constexpr int kResignPlies = 4;

/// @brief Nothing past this ply of a game is kept. tools/gen_data.cpp's kMaxPlies, where it adjudicates a
/// draw, so its pool holds nothing later.
constexpr int kMaxPlies = 200;

/// @brief Plies dropped from the start of each game by default - the opening book every game shares.
constexpr int kDefaultSkipPlies = 8;

/// @brief The standard start position, which every standard Lichess game begins from.
inline constexpr std::string_view kStartPositionFen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

/// @brief Which move the quiet filter judges.
///
/// tools/gen_data.cpp judges quietness on the move **out** of the labelled position (`best.IsQuiet()`, the
/// search's best move there; its principal-variation harvest says so explicitly - "the move *out* of this
/// position is the next PV move, so quietness is judged on that"). Here the position is reached by the
/// annotated move, so there are two candidates and they are not the same filter: kPlayedMove asks whether
/// the move that *reached* the position was quiet (no half-resolved exchange behind it), kNextMove asks
/// whether the move played *from* it was quiet, which is gen_data's own rule.
///
/// **The default is kNextMove, and that is deliberate: do not "fix" it to kPlayedMove.** The whole purpose
/// of this data is a matched, head-to-head comparison against the self-play pool on an equal-sized pilot,
/// and a comparison is only about the data source if every filter either side of it is the same. gen_data
/// judges quietness on the move *out* of the position, so judging it here on the move *in* would compare
/// two different filters and attribute the difference to Lichess rather than to the rule that changed.
/// kPlayedMove is kept reachable (`--quiet-mode played`) because "the position was not reached by a
/// half-resolved exchange" is a defensible rule in its own right - it is simply not gen_data's, and so not
/// the one a matched pilot may use.
///
/// **It costs about 2% of the yield, so there is no volume argument either way.** Measured over 2.25 GB of
/// the 2026-09 file, the two modes run back to back with every other setting and the shard layout identical:
/// at min-elo 0 the yield goes 3,455,705 -> 3,379,566 records (**-2.20%**), and at min-elo 2000 it goes
/// 649,251 -> 637,833 (**-1.76%**).
///
/// The drop accounting balances exactly, which is worth recording so nobody re-derives it: at min-elo 0
/// `dropped_not_quiet` rises by 74,035 (1,236,464 -> 1,310,499), `dropped_duplicate` by 2,132 and
/// `dropped_eval_cap` falls by 28, and the emitted count falls by precisely their sum, 76,139. So the two
/// filters simply disagree about 2% of plies - the move into a position and the move out of it are both
/// quiet in the large majority of positions - and nothing subtler than that is going on.
///
/// Note when re-measuring that the dedup is **per shard**, so a comparison is only valid between runs that
/// produced the same number of shards: a run deduplicated over one shard drops more duplicates than the same
/// data split over two, and that difference is easily mistaken for a filter effect. Both figures above are
/// from runs at the default 2,000,000-record shard size (two shards at min-elo 0, one at min-elo 2000).
enum class QuietMode : uint8_t
{
    kPlayedMove, ///< The annotated move, the one that reached this position, must be quiet.
    kNextMove,   ///< The next move of the game, the one played from this position (gen_data's own rule).
    kBoth,       ///< Both of them must be quiet.
};

/// @brief Everything the run was asked to do; also what the .meta sidecars report.
struct Options
{
    int         min_elo         = 0;                    ///< Both players' Elo must be at least this.
    int         skip_plies      = kDefaultSkipPlies;    ///< Plies dropped from the start of each game.
    int         max_plies       = kMaxPlies;            ///< Nothing past this ply is kept (0 = no cap).
    int         eval_cap        = kEvalCap;             ///< |score| at or above this is dropped.
    int         resign_cp       = kResignThreshold;     ///< Adjudication threshold (0 = no adjudication).
    int         resign_plies    = kResignPlies;         ///< Consecutive decisive plies to adjudicate.
    int         mate_score_cp   = 0;                    ///< Mate evals: 0 skips them, else clamp to ±this.
    QuietMode   quiet_mode      = QuietMode::kNextMove; ///< Which move the quiet filter judges (see QuietMode).
    std::size_t shard_positions = 2000000;              ///< Target records per shard.
};

/// @brief What the run counted - the yield report, and the per-shard provenance.
///
/// Every drop has a reason of its own, because the reasons are what size the real run: a yield that is
/// mostly `dropped_no_eval` means the Elo filter is picking games Lichess did not analyse, while one that is
/// mostly `dropped_duplicate` means `skip_plies` is too low.
///
/// **The denominator of `games_with_evals` and `games_passing_elo` is not `games_seen`.** Both are counted
/// only for a game that is complete and standard, so the figure to divide by is
/// `games_seen - games_truncated - games_non_standard`. That is deliberate rather than an ordering slip, and
/// it is the only correct place to count them: a game cut off by a bounded prefix has its tag block but only
/// part of its movetext, so scanning it for `[%eval]` would undercount, and counting a game the harvest
/// refuses outright would make these two incomparable with `games_used` below them. On a whole month-file
/// the correction is one game in a million; it matters only when the prefix is small.
struct Statistics
{
    uint64_t games_seen              = 0; ///< Games read from the stream.
    uint64_t games_truncated         = 0; ///< Games whose movetext ran off the end of a bounded stream.
    uint64_t games_non_standard      = 0; ///< Games with a FEN / non-standard Variant tag.
    uint64_t games_without_result    = 0; ///< Games whose Result is `*` (unfinished), which has no label.
    uint64_t games_with_evals        = 0; ///< Games carrying at least one [%eval] comment (see note below).
    uint64_t games_passing_elo       = 0; ///< Games where both players are at or above --min-elo (ditto).
    uint64_t games_used              = 0; ///< Games actually walked (standard, finished, analysed, in Elo).
    uint64_t games_with_unparsed_san = 0; ///< Games abandoned part-way at a SAN token that did not resolve.
    uint64_t games_contributing      = 0; ///< Games that emitted at least one record.
    uint64_t plies_seen              = 0; ///< Plies replayed.
    uint64_t plies_with_eval         = 0; ///< Plies carrying an [%eval].
    uint64_t positions_emitted       = 0; ///< Records written.

    uint64_t plies_unparsed_san         = 0; ///< Plies whose SAN did not resolve to exactly one legal move.
    uint64_t dropped_skip_plies         = 0; ///< Inside the first --skip-plies plies.
    uint64_t dropped_max_plies          = 0; ///< Past --max-plies.
    uint64_t dropped_after_adjudication = 0; ///< After the resign adjudication fired.
    uint64_t dropped_no_eval            = 0; ///< No [%eval] on this ply.
    uint64_t dropped_mate_eval          = 0; ///< A `#N` mate eval, with mate evals being skipped.
    uint64_t dropped_in_check           = 0; ///< The side to move is in check.
    uint64_t dropped_not_quiet          = 0; ///< The move the quiet filter judges is a capture or promotion.
    uint64_t dropped_eval_cap           = 0; ///< |score| at or above the eval cap.
    uint64_t dropped_duplicate          = 0; ///< The Zobrist key is already in this shard.
};

/// @brief What one `[%eval ...]` comment said.
struct EvalLabel
{
    bool   has_eval  = false; ///< Whether the comment carried an [%eval] at all.
    bool   is_mate   = false; ///< Whether it was a `#N` / `#-N` mate announcement.
    double pawns     = 0.0;   ///< The eval in pawns, White-relative (a decimal eval only).
    int    mate_sign = 0;     ///< +1 when White mates, -1 when Black does (a mate eval only).
};

/// @brief One ply of a game as the PGN carries it: its SAN token and the comment that follows it.
struct PlyToken
{
    std::string san;     ///< The SAN move token, decorations and all.
    std::string comment; ///< Everything from the `{ ... }` comments attached after this move.
};

/// @brief One PGN game: its tag pairs, its plies and whether the movetext was complete.
struct PgnGame
{
    std::vector<std::pair<std::string, std::string>> tags;                ///< Tag pairs in file order.
    std::vector<PlyToken>                            plies;               ///< The movetext, one entry per ply.
    std::string                                      result_token;        ///< The movetext's own result token.
    bool                                             is_complete = false; ///< A result token terminated it.

    std::string_view Tag(std::string_view name) const;
    bool             HasTag(std::string_view name) const;
};

/// @brief The value of a tag, or an empty view if the game has no such tag.
/// @param name The tag name. @return The value.
inline std::string_view PgnGame::Tag(std::string_view name) const
{
    for (const auto &tag : tags)
    {
        if (tag.first == name)
        {
            return tag.second;
        }
    }

    return {};
}

/// @brief Whether the game carries a tag at all (a tag may legitimately be empty).
/// @param name The tag name. @return true if present.
inline bool PgnGame::HasTag(std::string_view name) const
{
    for (const auto &tag : tags)
    {
        if (tag.first == name)
        {
            return true;
        }
    }

    return false;
}

/// @brief Where the records go, and what already went there.
///
/// An interface rather than the shard writer itself for two reasons: the dedup is **per shard**, so only
/// whoever owns the shards knows what has been written to the current one; and the test needs to collect
/// the records in memory rather than on disk.
class RecordSink
{
  public:
    virtual ~RecordSink() = default;

    /// @brief Whether this position is new to the current shard, claiming it if so.
    /// @param hash The position's Zobrist key. @return true if it had not been written to this shard.
    virtual bool IsNewToShard(zobrist_t hash) = 0;

    /// @brief Write one record. @param fen The position. @param white_score Centipawns, White-relative.
    /// @param wdl_text One of "1.0" / "0.5" / "0.0".
    virtual void Write(const std::string &fen, int white_score, const char *wdl_text) = 0;

    /// @brief Called after each game that was walked, so a shard can rotate on a game boundary.
    virtual void EndGame() = 0;
};

// ---------------------------------------------------------------------------
// SAN
// ---------------------------------------------------------------------------

/// @brief Strip the decorations a SAN token carries: check and mate marks, the annotation glyphs Lichess
/// writes inline (`?!`, `??`, `!`), and a trailing `e.p.`.
/// @param token The raw token. @return The bare move text.
inline std::string StripSanDecorations(std::string_view token)
{
    std::string san{token};

    if (san.size() > 4 && san.compare(san.size() - 4, 4, "e.p.") == 0)
    {
        san.erase(san.size() - 4);
    }

    while (!san.empty() && (san.back() == '+' || san.back() == '#' || san.back() == '!' || san.back() == '?'))
    {
        san.pop_back();
    }

    return san;
}

/// @brief The piece a SAN letter names. @param letter One of KQRBN. @return The piece, or kNone.
inline Piece PieceFromSanLetter(char letter)
{
    switch (letter)
    {
    case 'K':
        return kKing;
    case 'Q':
        return kQueen;
    case 'R':
        return kRook;
    case 'B':
        return kBishop;
    case 'N':
        return kKnight;
    default:
        return kNone;
    }
}

/// @brief Resolve one SAN token against a position's legal moves.
///
/// The engine has no SAN reader of its own (UCI speaks coordinates), so the token is turned into a set of
/// constraints - moving piece, destination square, promotion piece, and whichever of the source file and
/// source rank the token disambiguates with - and matched against `Position::GenerateLegalMoves()`. That
/// way the engine's own move generator decides what is legal, including the cases a hand-rolled reader gets
/// wrong: an en-passant capture whose destination square is empty, a promotion capture, and a pinned piece
/// that cannot be the one the notation meant.
///
/// A token that matches no legal move, or more than one, is refused rather than guessed at - a second match
/// means the PGN was ambiguous, and either way a wrong move would silently corrupt every position after it.
///
/// Long algebraic (`e2-e4`, `e2e4`) resolves too, since the source square is simply a full disambiguation.
///
/// @param position The position the move is played from. @param token The SAN token, decorations and all.
/// @return The matching legal move, or Move::None().
inline Move ParseSanMove(const Position &position, std::string_view token)
{
    std::string san = StripSanDecorations(token);

    if (san.empty())
    {
        return Move::None();
    }

    const MoveList legal = position.GenerateLegalMoves();

    // Castling, which names no squares at all. The engine encodes it as the king's own move, so it is found
    // by its destination file: g for the short castling, c for the long one.
    if (san == "O-O" || san == "0-0" || san == "O-O-O" || san == "0-0-0")
    {
        const uint8_t destination_file = san.size() > 3 ? 2 : 6;

        for (const Move &candidate : legal)
        {
            if (candidate.type() == Move::kCastling && candidate.to().File() == destination_file)
            {
                return candidate;
            }
        }

        return Move::None();
    }

    // The promotion piece, written `e8=Q` by Lichess and `e8Q` by some other writers.
    Piece             promoted = kNone;
    const std::size_t equals   = san.find('=');

    if (equals != std::string::npos)
    {
        if (equals + 1 >= san.size())
        {
            return Move::None();
        }

        promoted = PieceFromSanLetter(san[equals + 1]);
        san.erase(equals);
    }
    else if (san.size() >= 3 && PieceFromSanLetter(san.back()) != kNone && san[san.size() - 2] >= '1' &&
             san[san.size() - 2] <= '8')
    {
        promoted = PieceFromSanLetter(san.back());
        san.pop_back();
    }

    if (promoted == kKing)
    {
        return Move::None(); // `e8=K` is not a move
    }

    // The moving piece, and then the destination square, which is always the last two characters.
    Piece       piece = kPawn;
    std::size_t index = 0;

    if (PieceFromSanLetter(san[0]) != kNone)
    {
        piece = PieceFromSanLetter(san[0]);
        index = 1;
    }

    if (san.size() < index + 2)
    {
        return Move::None();
    }

    const char destination_file = san[san.size() - 2];
    const char destination_rank = san[san.size() - 1];

    if (destination_file < 'a' || destination_file > 'h' || destination_rank < '1' || destination_rank > '8')
    {
        return Move::None();
    }

    const Square destination{destination_file - 'a', destination_rank - '1'};

    // Whatever lies between the piece letter and the destination is the disambiguation (and the capture
    // `x`, which says nothing the move list does not already know).
    int source_file = -1;
    int source_rank = -1;

    for (std::size_t i = index; i + 2 < san.size(); ++i)
    {
        const char c = san[i];

        if (c == 'x' || c == '-' || c == ':')
        {
            continue;
        }

        if (c >= 'a' && c <= 'h')
        {
            source_file = c - 'a';
        }
        else if (c >= '1' && c <= '8')
        {
            source_rank = c - '1';
        }
        else
        {
            return Move::None();
        }
    }

    Move found   = Move::None();
    int  matches = 0;

    for (const Move &candidate : legal)
    {
        // A castling move is written O-O / O-O-O and never as a king move: the king's two-square step is
        // not a king move the notation could otherwise mean.
        if (candidate.type() == Move::kCastling)
        {
            continue;
        }

        if (candidate.piece() != piece || (uint8_t)candidate.to() != (uint8_t)destination)
        {
            continue;
        }

        if (candidate.promoted() != promoted)
        {
            continue;
        }

        if (source_file >= 0 && candidate.from().File() != (uint8_t)source_file)
        {
            continue;
        }

        if (source_rank >= 0 && candidate.from().Rank() != (uint8_t)source_rank)
        {
            continue;
        }

        found = candidate;
        ++matches;
    }

    return matches == 1 ? found : Move::None();
}

// ---------------------------------------------------------------------------
// Comments, results and Elo
// ---------------------------------------------------------------------------

/// @brief Read the `[%eval ...]` out of a move's comment.
///
/// A comment may hold any of the Lichess annotations in any order - `{ [%eval 0.18] [%clk 0:02:00] }`,
/// `{ [%clk 0:02:00] }` with no eval at all, or a bare human comment - so everything but the eval is
/// ignored rather than parsed. Two eval forms exist: a decimal in pawns (`0.18`, `-3.87`, `24.15`) and a
/// mate announcement (`#5` for White mating, `#-3` for Black).
///
/// @param comment The comment text (without the braces).
/// @return What was found; `has_eval` false when there was no eval.
inline EvalLabel ParseEvalComment(std::string_view comment)
{
    EvalLabel         label;
    const std::size_t marker = comment.find("[%eval");

    if (marker == std::string_view::npos)
    {
        return label;
    }

    std::size_t i = marker + 6;

    while (i < comment.size() && (comment[i] == ' ' || comment[i] == '\t'))
    {
        ++i;
    }

    const std::size_t begin = i;

    while (i < comment.size() && comment[i] != ']' && comment[i] != ' ' && comment[i] != '\t')
    {
        ++i;
    }

    std::string value{comment.substr(begin, i - begin)};

    if (value.empty())
    {
        return label;
    }

    if (value[0] == '#')
    {
        // `#0` would be meaningless; anything else is a mate for whoever the sign says.
        if (value.size() < 2)
        {
            return label;
        }

        label.has_eval  = true;
        label.is_mate   = true;
        label.mate_sign = value[1] == '-' ? -1 : 1;
        return label;
    }

    char        *end   = nullptr;
    const double pawns = std::strtod(value.c_str(), &end);

    if (end == value.c_str() || end != value.c_str() + value.size())
    {
        return label; // not a number we understand; treat the ply as unannotated rather than mislabel it
    }

    label.has_eval = true;
    label.pawns    = pawns;
    return label;
}

/// @brief The centipawn label a parsed eval yields, if it yields one.
///
/// Lichess's eval is already White-relative and in pawns, so this is a scale and a round with no sign flip.
/// A mate eval has no centipawn value at all: with `mate_score_cp` 0 it is skipped (the default, and what
/// gen_data's own pool holds, since a mate score there is far past kEvalCap), and otherwise it is clamped to
/// ±`mate_score_cp` - which must be under the eval cap or the cap drops it again.
///
/// @param label The parsed eval. @param options The run's options.
/// @param white_score Set to the label in centipawns, White-relative.
/// @return true if the ply has a usable label.
inline bool ScoreFromLabel(const EvalLabel &label, const Options &options, int &white_score)
{
    if (!label.has_eval)
    {
        return false;
    }

    if (label.is_mate)
    {
        if (options.mate_score_cp <= 0)
        {
            return false;
        }

        white_score = label.mate_sign * options.mate_score_cp;
        return true;
    }

    white_score = (int)std::lround(label.pawns * 100.0);
    return true;
}

/// @brief The score the resign adjudication reads, which exists even for a ply whose label is skipped.
///
/// gen_data adjudicates on the play search's score at every searched ply, not only at the plies it records,
/// so a mate eval counts here as decisive for whoever is mating even when the record itself is dropped.
///
/// @param label The parsed eval. @return The score in centipawns, or 0 for an unannotated ply.
inline int AdjudicationScore(const EvalLabel &label)
{
    if (!label.has_eval)
    {
        return 0;
    }

    if (label.is_mate)
    {
        return label.mate_sign * kMaxEvaluation;
    }

    return (int)std::lround(label.pawns * 100.0);
}

/// @brief Map a PGN `Result` header value onto a bulletformat wdl token.
///
/// bulletformat accepts only `1.0` `0.5` `0.0` `1` `1/2` `0`; the PGN spelling `1/2-1/2` is **not** one of
/// them, so the mapping is not the identity and an unfinished `*` game has no token at all.
///
/// @param result The Result tag's value. @return "1.0" / "0.5" / "0.0", or nullptr if there is no result.
inline const char *ResultToWdlToken(std::string_view result)
{
    if (result == "1-0")
    {
        return "1.0";
    }

    if (result == "0-1")
    {
        return "0.0";
    }

    if (result == "1/2-1/2")
    {
        return "0.5";
    }

    return nullptr;
}

/// @brief Read an Elo tag.
///
/// Lichess writes a plain integer, but a provisional or missing rating can read `?`; such a game counts as
/// Elo 0, so it survives `--min-elo 0` and is dropped by any real threshold.
///
/// @param text The tag's value. @return The rating, or 0 if it is not a number.
inline int ParseElo(std::string_view text)
{
    std::string value{text};
    char       *end    = nullptr;
    const long  rating = std::strtol(value.c_str(), &end, 10);

    if (end == value.c_str() || rating < 0 || rating > 100000)
    {
        return 0;
    }

    return (int)rating;
}

// ---------------------------------------------------------------------------
// Reading the PGN
// ---------------------------------------------------------------------------

/// @brief A line reader over a byte stream that hashes every byte it hands out.
///
/// The input is normally a pipe (`curl ... | zstd -dc | lichess_extract ...`), so it is read once and never
/// seekable: the only chance to take the SHA-256 the provenance sidecar owes is as the bytes go past. The
/// hash is therefore of exactly the bytes this run consumed - which, for a deliberately bounded prefix of a
/// 29 GB monthly file, is the only thing that identifies the input at all.
///
/// **The hash covers bytes consumed, not bytes read.** The stream is read in 64 KB chunks, so at any moment
/// up to a chunk of readahead sits in `pending_` having been read but not yet handed out as a line. Hashing
/// on *read* would therefore make the digest depend on where the chunk boundary happened to fall, and would
/// cover up to 64 KB that no returned line - and so no emitted record - ever came from. That would defeat
/// the point of the sidecar, whose only value is being exact: a digest that covers a different prefix from
/// the one the records came from cannot be checked against anything. So the bytes are hashed at the moment
/// they leave `pending_`, newline included, and `bytes_consumed()` counts exactly those.
class ByteCountingLineReader
{
  public:
    explicit ByteCountingLineReader(std::istream &in);
    bool                  NextLine(std::string &line);
    uint64_t              bytes_consumed() const;
    std::string           Digest() const;
    const sha256::Hasher &hasher() const;

  private:
    std::istream  *in_;                ///< The stream (not owned).
    sha256::Hasher hasher_;            ///< Running hash of every byte handed out as part of a line.
    std::string    pending_;           ///< Bytes read but not yet returned as a line.
    std::size_t    scan_from_ = 0;     ///< Where the next newline search starts in pending_.
    bool           is_at_end_ = false; ///< The stream has reported end of file.
};

/// @brief Wrap a stream. @param in The stream, which must outlive the reader.
inline ByteCountingLineReader::ByteCountingLineReader(std::istream &in) : in_{&in}
{
}

/// @brief The next line, with any trailing CR and LF removed.
/// @param line Set to the line's text. @return false at end of input.
inline bool ByteCountingLineReader::NextLine(std::string &line)
{
    for (;;)
    {
        const std::size_t newline = pending_.find('\n', scan_from_);

        if (newline != std::string::npos)
        {
            line.assign(pending_, 0, newline);

            while (!line.empty() && line.back() == '\r')
            {
                line.pop_back();
            }

            // Hash what is being handed out, the line terminator included, before it leaves pending_: the
            // digest must cover the bytes the records came from and nothing else.
            hasher_.Update(pending_.data(), newline + 1);
            pending_.erase(0, newline + 1);
            scan_from_ = 0;
            return true;
        }

        if (is_at_end_)
        {
            if (pending_.empty())
            {
                return false;
            }

            line = pending_;

            while (!line.empty() && line.back() == '\r')
            {
                line.pop_back();
            }

            // A final line with no terminator: the whole remainder is consumed.
            hasher_.Update(pending_.data(), pending_.size());
            pending_.clear();
            scan_from_ = 0;
            return true;
        }

        scan_from_ = pending_.size();

        char chunk[65536];
        in_->read(chunk, (std::streamsize)sizeof(chunk));
        const std::streamsize got = in_->gcount();

        if (got <= 0)
        {
            is_at_end_ = true;
            continue;
        }

        // Not hashed here: this is readahead, and may not all be handed out. See the class comment.
        pending_.append(chunk, (std::size_t)got);
    }
}

/// @brief How many bytes have been handed out as lines, line terminators included.
/// Not the same as the number read from the stream, which runs up to one 64 KB chunk ahead.
/// @return The byte count.
inline uint64_t ByteCountingLineReader::bytes_consumed() const
{
    return hasher_.bytes_hashed();
}

/// @brief The SHA-256 of everything consumed so far. @return The 64-hex-digit digest.
inline std::string ByteCountingLineReader::Digest() const
{
    return hasher_.Digest();
}

/// @brief The running hasher, so a caller can snapshot it at a game boundary. @return The hasher.
inline const sha256::Hasher &ByteCountingLineReader::hasher() const
{
    return hasher_;
}

/// @brief Split a movetext into plies, attaching each comment to the move it follows.
///
/// Tolerant by design: PGN in the wild carries recursive variations `( ... )`, numeric annotation glyphs
/// `$1`, semicolon comments and move numbers glued to their move (`1.e4`), none of which Lichess writes but
/// all of which would otherwise be read as moves. Comments are concatenated onto the preceding ply, so a
/// move followed by two comments keeps both.
///
/// @param text The movetext, with its lines already joined by spaces.
/// @param game The game to fill: `plies`, `result_token` and `is_complete`.
inline void TokenizeMovetext(const std::string &text, PgnGame &game)
{
    std::size_t i = 0;

    while (i < text.size())
    {
        const char c = text[i];

        if (c == ' ' || c == '\t')
        {
            ++i;
            continue;
        }

        if (c == '{')
        {
            const std::size_t close = text.find('}', i + 1);
            const std::size_t end   = close == std::string::npos ? text.size() : close;

            if (!game.plies.empty())
            {
                game.plies.back().comment.append(text, i + 1, end - i - 1);
                game.plies.back().comment.push_back(' ');
            }

            i = end == text.size() ? end : end + 1;
            continue;
        }

        if (c == '(')
        {
            // A recursive variation: everything inside it is analysis, not the game.
            int depth = 0;

            while (i < text.size())
            {
                if (text[i] == '(')
                {
                    ++depth;
                }
                else if (text[i] == ')')
                {
                    --depth;
                }

                ++i;

                if (depth == 0)
                {
                    break;
                }
            }

            continue;
        }

        if (c == ';')
        {
            break; // a comment to the end of the line, and the lines are already joined
        }

        if (c == '$')
        {
            while (i < text.size() && text[i] != ' ' && text[i] != '\t')
            {
                ++i;
            }

            continue;
        }

        const std::size_t begin = i;

        while (i < text.size() && text[i] != ' ' && text[i] != '\t' && text[i] != '{' && text[i] != '(' &&
               text[i] != ';')
        {
            ++i;
        }

        std::string token = text.substr(begin, i - begin);

        if (token == "1-0" || token == "0-1" || token == "1/2-1/2" || token == "*")
        {
            game.result_token = token;
            game.is_complete  = true;
            continue;
        }

        // A move number: digits, then dots. It may have the move glued to it (`1.e4`), in which case what
        // follows the dots is the move.
        std::size_t digits = 0;

        while (digits < token.size() && token[digits] >= '0' && token[digits] <= '9')
        {
            ++digits;
        }

        if (digits > 0 && digits < token.size() && token[digits] == '.')
        {
            std::size_t after = digits;

            while (after < token.size() && token[after] == '.')
            {
                ++after;
            }

            token.erase(0, after);

            if (token.empty())
            {
                continue;
            }
        }
        else if (digits == token.size())
        {
            continue; // a bare number
        }

        game.plies.push_back({token, {}});
    }
}

/// @brief Read PGN games one at a time from a stream, tracking exactly which prefix they came from.
///
/// Two byte counts, and the difference is the point of the provenance sidecar. `bytes_consumed()` is
/// everything handed out as a line, which on a bounded prefix includes the trailing fragment of a game that
/// was cut off mid-movetext and dropped. `attributable_bytes()` and `Digest()` cover only the prefix ending
/// at the last **complete** game - which is exactly the input every emitted record came from. Quoting the
/// first in the sidecar would name a byte range containing a game that contributed nothing, so the sidecar
/// quotes the second and the two are reported separately.
class PgnReader
{
  public:
    explicit PgnReader(std::istream &in);
    bool        NextGame(PgnGame &game);
    uint64_t    bytes_consumed() const;
    uint64_t    attributable_bytes() const;
    std::string Digest() const;

  private:
    ByteCountingLineReader reader_;              ///< The line source, which also hashes and counts the bytes.
    sha256::Hasher         attributable_hasher_; ///< The hasher as of the end of the last complete game.
};

/// @brief Wrap a stream. @param in The stream, which must outlive the reader.
inline PgnReader::PgnReader(std::istream &in) : reader_{in}
{
}

/// @brief Read the next game.
///
/// A game is a block of `[Tag "value"]` lines, then its movetext up to the first blank line after it. The
/// final game of a **bounded** stream - a prefix of the monthly file, cut mid-game - has no result token and
/// is returned with `is_complete` false so the caller can drop it rather than harvest a game whose movetext
/// stops in the middle of a SAN token.
///
/// @param game Filled in with the game. @return false at end of input.
inline bool PgnReader::NextGame(PgnGame &game)
{
    game.tags.clear();
    game.plies.clear();
    game.result_token.clear();
    game.is_complete = false;

    std::string line;
    bool        has_content = false;

    // The tag block, after any blank lines between games.
    while (reader_.NextLine(line))
    {
        if (line.empty())
        {
            if (has_content)
            {
                break;
            }

            continue;
        }

        if (line[0] != '[')
        {
            break; // movetext with no tag block; fall through to the movetext loop with this line in hand
        }

        has_content                = true;
        const std::size_t name_end = line.find(' ');
        const std::size_t open     = line.find('"');
        const std::size_t close    = line.rfind('"');

        if (name_end == std::string::npos || open == std::string::npos || close <= open)
        {
            continue;
        }

        game.tags.emplace_back(line.substr(1, name_end - 1), line.substr(open + 1, close - open - 1));
    }

    // The movetext, which is `line` plus every line up to the next blank one.
    std::string movetext;

    if (!line.empty() && line[0] != '[')
    {
        movetext    = line;
        has_content = true;
    }

    while (reader_.NextLine(line))
    {
        if (line.empty())
        {
            break;
        }

        if (!movetext.empty())
        {
            movetext.push_back(' ');
        }

        movetext += line;
        has_content = true;
    }

    if (!has_content)
    {
        return false;
    }

    TokenizeMovetext(movetext, game);

    // A game whose movetext ended in a result token is wholly within the bytes consumed so far (the blank
    // line that ended it has been read too), so this is the point at which the attributable prefix grows.
    // A truncated final game never reaches here with is_complete set, and so never widens the digest.
    if (game.is_complete)
    {
        attributable_hasher_ = reader_.hasher();
    }

    return true;
}

/// @brief How many bytes have been handed out as lines, the trailing truncated game included.
/// @return The byte count.
inline uint64_t PgnReader::bytes_consumed() const
{
    return reader_.bytes_consumed();
}

/// @brief How many bytes the complete games came from - the prefix Digest() covers. @return The byte count.
inline uint64_t PgnReader::attributable_bytes() const
{
    return attributable_hasher_.bytes_hashed();
}

/// @brief The SHA-256 of the prefix ending at the last complete game. @return The 64-hex-digit digest.
inline std::string PgnReader::Digest() const
{
    return attributable_hasher_.Digest();
}

// ---------------------------------------------------------------------------
// Extraction
// ---------------------------------------------------------------------------

/// @brief Whether the quiet filter accepts this ply.
/// @param options The run's options. @param played The annotated move, which reached the position.
/// @param next The move played from the position, or Move::None() if the game ended there.
/// @return true if the position passes.
inline bool PassesQuietFilter(const Options &options, const Move &played, const Move &next)
{
    const bool is_played_quiet = played.IsQuiet();
    // The end of the game has no move out of it. gen_data's PV harvest refuses such a position outright
    // ("without one (the end of the line) the position is not harvested"), so a mode that reads the next
    // move refuses it too.
    const bool is_next_quiet = !(next == Move::None()) && next.IsQuiet();

    switch (options.quiet_mode)
    {
    case QuietMode::kNextMove:
        return is_next_quiet;
    case QuietMode::kBoth:
        return is_played_quiet && is_next_quiet;
    case QuietMode::kPlayedMove:
    default:
        return is_played_quiet;
    }
}

/// @brief Walk one game and emit every position that passes the filters.
///
/// The record emitted for ply *i* is the position **after** that ply's move, carrying that ply's eval: the
/// comment follows the move and describes what it reached.
///
/// @param game The parsed game. @param options The run's options. @param sink Where records go.
/// @param statistics The run's counters, updated by this call.
inline void ExtractGame(const PgnGame &game, const Options &options, RecordSink &sink, Statistics &statistics)
{
    ++statistics.games_seen;

    if (!game.is_complete)
    {
        ++statistics.games_truncated;
        return;
    }

    const std::string_view variant = game.Tag("Variant");

    if (game.HasTag("FEN") || (!variant.empty() && variant != "Standard"))
    {
        ++statistics.games_non_standard;
        return;
    }

    // Both counts are wanted whatever else the game fails, because they are what sizes a real run: how much
    // of the file carries evals at all, and how much of it survives the Elo threshold.
    bool has_any_eval = false;

    for (const PlyToken &ply : game.plies)
    {
        if (ply.comment.find("[%eval") != std::string::npos)
        {
            has_any_eval = true;
            break;
        }
    }

    if (has_any_eval)
    {
        ++statistics.games_with_evals;
    }

    const bool passes_elo =
        ParseElo(game.Tag("WhiteElo")) >= options.min_elo && ParseElo(game.Tag("BlackElo")) >= options.min_elo;

    if (passes_elo)
    {
        ++statistics.games_passing_elo;
    }

    const char *wdl_text = ResultToWdlToken(game.Tag("Result"));

    if (wdl_text == nullptr)
    {
        ++statistics.games_without_result;
        return;
    }

    if (!has_any_eval || !passes_elo)
    {
        return;
    }

    ++statistics.games_used;

    // Replay the whole game first. The quiet filter may need the move played *out* of a position, and
    // keeping the positions makes that a lookup rather than a second parse.
    std::vector<Move>     moves;
    std::vector<Position> positions;
    Position              position = Position::FromString(kStartPositionFen);

    for (const PlyToken &ply : game.plies)
    {
        ++statistics.plies_seen;
        const Move move = ParseSanMove(position, ply.san);

        if (move == Move::None())
        {
            // The game is abandoned from here: every position after an unresolved move would be wrong.
            // What was already emitted stands - those positions were reached by moves that did resolve.
            ++statistics.plies_unparsed_san;
            ++statistics.games_with_unparsed_san;
            break;
        }

        position = position.MakeMove(move);
        moves.push_back(move);
        positions.push_back(position);
    }

    int  decisive_sign   = 0;
    int  decisive_plies  = 0;
    bool is_adjudicated  = false;
    bool has_contributed = false;

    for (std::size_t ply = 0; ply < moves.size(); ++ply)
    {
        const EvalLabel label = ParseEvalComment(game.plies[ply].comment);

        if (label.has_eval)
        {
            ++statistics.plies_with_eval;
        }

        int white_score = 0;

        if (is_adjudicated)
        {
            ++statistics.dropped_after_adjudication;
        }
        else if ((int)ply < options.skip_plies)
        {
            ++statistics.dropped_skip_plies;
        }
        else if (options.max_plies > 0 && (int)ply >= options.max_plies)
        {
            ++statistics.dropped_max_plies;
        }
        else if (!label.has_eval)
        {
            ++statistics.dropped_no_eval;
        }
        else if (label.is_mate && options.mate_score_cp <= 0)
        {
            ++statistics.dropped_mate_eval;
        }
        else if (!ScoreFromLabel(label, options, white_score))
        {
            ++statistics.dropped_no_eval;
        }
        else if (positions[ply].IsInCheck())
        {
            ++statistics.dropped_in_check;
        }
        else if (!PassesQuietFilter(options, moves[ply], ply + 1 < moves.size() ? moves[ply + 1] : Move::None()))
        {
            ++statistics.dropped_not_quiet;
        }
        else if (std::abs(white_score) >= options.eval_cap)
        {
            ++statistics.dropped_eval_cap;
        }
        else if (!sink.IsNewToShard(positions[ply].hash_))
        {
            ++statistics.dropped_duplicate;
        }
        else
        {
            sink.Write(positions[ply].ToString(), white_score, wdl_text);
            ++statistics.positions_emitted;
            has_contributed = true;
        }

        // The adjudication is updated after the record, as gen_data's is: the ply that completes the run of
        // decisive scores is itself recorded, and the harvest stops after it.
        if (label.has_eval && options.resign_cp > 0)
        {
            const int score = AdjudicationScore(label);
            const int sign  = score > options.resign_cp ? 1 : (score < -options.resign_cp ? -1 : 0);

            if (sign != 0 && sign == decisive_sign)
            {
                if (++decisive_plies >= options.resign_plies)
                {
                    is_adjudicated = true;
                }
            }
            else
            {
                decisive_sign  = sign;
                decisive_plies = sign != 0 ? 1 : 0;
            }
        }
    }

    if (has_contributed)
    {
        ++statistics.games_contributing;
    }

    sink.EndGame();
}

/// @brief Walk a whole PGN string - the form the tests use, where the input is a literal rather than a pipe.
/// @param text The PGN. @param options The run's options. @param sink Where records go.
/// @param statistics The run's counters, updated by this call.
inline void ExtractText(const std::string &text, const Options &options, RecordSink &sink, Statistics &statistics)
{
    std::istringstream in{text};
    PgnReader          reader{in};
    PgnGame            game;

    while (reader.NextGame(game))
    {
        ExtractGame(game, options, sink, statistics);
    }
}
} // namespace lichess

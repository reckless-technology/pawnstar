#pragma once
/// @file lc0_extract.h The Leela-Chess-Zero-v6-to-training-shard extraction core, apart from its command line.
///
/// Everything here is the part worth pinning with tests: the v6 record layout, the plane decoder, the
/// value-head-to-centipawn mapping, the result mapping and the filter/emit loop. tools/lc0_extract.cpp is
/// the command line, the shard writer and the provenance sidecars around it; test/lc0_extract_test.cpp
/// drives this header directly with records built from plane words taken out of the real archive, which is
/// the only way to pin the rules below without a multi-gigabyte download.
///
/// ## What the records are
///
/// One line per kept position, in the format bullet's `bullet-utils convert --from text` reads and the one
/// tools/gen_data.cpp and tools/lichess_extract.cpp write:
///
///     <fen> | <white_relative_eval_cp> | <wdl>
///
/// **Both columns are White-relative, not side-to-move relative.** bulletformat's text parser mirrors the
/// board for a black-to-move FEN and then negates both (`bulletformat-1.8.0/src/chess.rs` ends
/// `ChessBoard::from_str` with `if stm == 1 { board.score = -board.score; board.result = 2 - board.result; }`),
/// so writing side-to-move-relative values inverts the label of every black-to-move position. Lc0's record
/// is side-to-move relative throughout - `best_q`, `root_q` and `result_q` all are - so **every column here
/// is sign-flipped for a black-to-move record**, which is the opposite of what lichess_extract has to do
/// with Lichess's already-White-relative `[%eval]`.
///
/// The only `wdl` tokens the parser accepts are `1.0` `0.5` `0.0` `1` `1/2` `0`; `1.0` / `0.5` / `0.0` are
/// what this writes.
///
/// ## Where the data comes from, and its licence
///
/// `https://storage.lczero.org/files/training_data/test80/` publishes one `.tar` per hour, each holding a
/// few thousand gzip members, each member one self-play game as a run of fixed-size v6 records. Every tar
/// carries a `LICENSE` member reading, in full:
///
/// > This collection of training data for Leela Chess Zero is made available under the Open Database
/// > License: http://opendatacommons.org/licenses/odbl/1.0/. Any rights in individual contents of the
/// > database are licensed under the Database Contents License:
/// > http://opendatacommons.org/licenses/dbcl/1.0/
///
/// **ODbL is a share-alike licence on the database, not on what is learnt from it.** A net trained on these
/// positions is a *Produced Work* under ODbL §1.0, which obliges a notice naming the source and its licence
/// and does not oblige the net to be ODbL. That is the basis on which this source was accepted
/// (2026-10-27), and it is why every shard's `.meta` carries a ready-made `attribution` line: the notice
/// has to be writable from the sidecar, without going back to the web.
///
/// ## The v6 record, and where its layout is from
///
/// The authority is lc0's own `struct V6TrainingData` in `src/trainingdata/trainingdata_v6.h`, which is
/// `#pragma pack(1)` and ends `static_assert(sizeof(V6TrainingData) == 8356, "Wrong struct size")`. The
/// offsets in kRecord* below are that declaration order; kRecordBytesV6 is that assertion, and the reader
/// refuses a stream whose length is not a multiple of it. Every scalar is little-endian, which is what the
/// writer's `std::ofstream::write` of the packed struct produces on every machine lc0 is built for.
///
/// Four fields are read and the rest ignored:
///
///  * `planes[104]` - 8 history positions of 13 bitboards (ours P N B R Q K, theirs p n b r q k, then a
///    repetition flag), from `EncodePositionForNN` in `src/neural/encoder.cc`. Only the first 13 are the
///    position being labelled; planes 13..18 are read for one thing only, recovering en passant (below).
///  * `castling_us_ooo` / `castling_us_oo` / `castling_them_ooo` / `castling_them_oo` - 0 or 1 for a
///    non-Chess960 input format (`trainingdata.cc` sends a rook-file mask only when `Is960CastlingFormat`).
///  * `side_to_move_or_enpassant` - for input format 1 this is `position.IsBlackToMove() ? 1 : 0`, nothing
///    else. The en-passant reading of this byte belongs to the canonical formats, which this refuses.
///  * `rule50_count`, the FEN half-move clock, and `best_q` / `best_d` / `root_q` / `result_q` / `result_d`
///    / `visits` / `plies_left`, which are the label, the filters and the game boundary.
///
/// ## The two orientations, and the bit order
///
/// lc0 keeps every position from the mover's own point of view: `Position::GetBoard()` returns `us_board_`,
/// which has been vertically mirrored (`BitBoard::Mirror()` is `ReverseBytesInBytes`) once per ply, so
/// "ours" is always the side to move and always on the low ranks. So a **black-to-move record's planes are
/// rank-flipped and colour-swapped**, and undoing that is the whole of the orientation work:
/// `real_rank = is_black_to_move ? 7 - row : row`, and plane 0..5 are Black's pieces when Black is to move.
///
/// On top of that, the writer stores each plane **bit-reversed within every byte**
/// (`trainingdata.cc`: `plane = ReverseBitsInBytes(planes[plane_idx++].mask)`), so a plane's byte prints
/// like a rank of a board, a-file first. Internally lc0's square is `row * 8 + col` with col 0 = the a
/// file, so a stored bit at index `i` is `row = i / 8`, `col = 7 - i % 8`. Getting this one wrong mirrors
/// every position left-to-right, which leaves a legal-looking FEN carrying another position's label.
///
/// ## En passant, which the classical format does not store
///
/// Input format 1 has nowhere to put an en-passant square: `side_to_move_or_enpassant` holds the side to
/// move, and the planes hold placements only. lc0's net infers it from the history, and so does this: the
/// previous position is planes 13..25, in *its* own orientation (one rank-flip away) with ours/theirs
/// swapped, so planes[13] holds the opponent's pawns as they were before their move. Rank-flip that into
/// the current orientation, diff it against planes[6], and a single pawn that went from row 6 to row 4 on
/// one file is a double push whose en-passant square is row 5 of that file.
///
/// Dropping the square instead would be the quiet kind of wrong: the FEN would stay legal, the label would
/// still be lc0's, and the position the label belongs to would be a different one from the position in the
/// shard whenever the capture was available.
///
/// ## Chess960 games are in this archive and must be thrown away
///
/// The test80 tars are not all standard chess. Measured over one tar (1,574 games), 54 games began from a
/// shuffled back rank - about 3.5%. Input format 1 sends castling as a plain 0/1, so for those games the
/// rook files are simply **not in the record**, and decoding one as standard chess produces FENs like
/// `b2rk3/... w KQ - 1 16`, with rights for rooks that are not on a1/h1. Two filters catch them:
///
///  * a game whose first record is not the standard start position is dropped whole (`kStartPositionFen`);
///  * any record whose castling flags do not match a standard-chess king and rook placement is dropped on
///    its own, which also catches a game whose first record was never seen.
///
/// This is not hygiene for its own sake. Feeding one of those illegal FENs to `build/pawnstar.exe` through
/// `position fen` killed the engine mid-run (reproducible: the same FEN is harmless on its own and fatal
/// after nine prior searches have filled the transposition table), so a shard full of them would also be a
/// pile of positions no tool in this repo can read back.
///
/// ## The value head, and the centipawn mapping
///
/// Lc0 has no centipawn score. It has `best_q` = w - l for the search's best move and `best_d` = the draw
/// probability, both from the side to move's point of view, both from the root of an MCTS search of a few
/// hundred visits. The expected score follows exactly, with no modelling assumption:
///
///     expected = w + d / 2 = (1 + q) / 2            since w + d + l = 1 and q = w - l
///
/// tools/bullet/pawnstar.rs trains against `sigmoid(score / eval_scale)` with `eval_scale = 400`, so the
/// score that *means* that expected result is that sigmoid's inverse, and nothing else:
///
///     score = 400 * logit((1 + q) / 2) = 400 * ln((1 + q) / (1 - q))
///
/// Any other curve would hand the trainer a target its own loss cannot reach, which is why 400 is the
/// default and why changing it is a decision and not a tuning knob.
///
/// **What this does not claim** is that lc0's expected score at its own strength is the same number of
/// centipawns as anybody else's. It is not: measured against this repo's v12 net searching the same
/// positions to depth 8, the mapped scores run about 1.5x hotter than the engine's own search scores
/// (errors-in-variables slope 0.661 over 1,000 records of a real shard, 0.689 over a ply-stratified 1,180).
/// Lc0's self-play is draw-heavy, so a position it calls 0.75 expected is one a sharper evaluator would
/// call less than +400.
///
/// So there are two settings and they answer different questions. **`--eval-scale` rescales the score
/// column linearly** (the mapping is `scale * logit`, so halving the scale halves every score), and is the
/// knob for putting this pool on the same numeric scale as a differently-labelled one - `--eval-scale 276`
/// is what the 0.69 slope above implies. **`--q-scale` shrinks `q` before the mapping**, which is not a
/// linear rescale: it compresses the saturated ends far more than the middle, and so is the knob for an
/// over-confident value head rather than for a scale mismatch. Both default to no change, and the default
/// is the self-consistent mapping: the trade is between a label that means exactly what the trainer's loss
/// targets and a label that sits on the same scale as its neighbours in a blended pool. Pick one knowingly;
/// do not reach for `--q-scale` to fix a slope.
///
/// The inverse diverges at the ends, and the ends are populated: 9.2% of the records in one tar map past
/// ±3000 cp. kEvalCap (3000) drops them, which is gen_data's and lichess_extract's rule and costs nothing
/// real - `sigmoid(3000 / 400)` is 0.9994, so the target is already saturated and carries no gradient.
///
/// ## The filters, and why they are gen_data's
///
/// This data is to be compared head-to-head with the other pools on a matched-size pilot, so the filters
/// mirror tools/gen_data.cpp's as closely as the source allows:
///
///  * the side to move must not be in check (`Position::IsInCheck`);
///  * the move played out of the position must be quiet, which is gen_data's own rule (see QuietMode in
///    tools/lichess_extract.h). Lc0 stores the move as an index into its 1858-wide policy space, which
///    would need lc0's move table to decode - but it does not have to be decoded, because the next record
///    of the game *is* the position the move reached: a drop in the total piece count is a capture and a
///    drop in the total pawn count is a promotion. The last record of a game has no successor and so no
///    judgeable move, and is dropped;
///  * `|white_score|` must be under kEvalCap (3000);
///  * a position whose Zobrist key is already in this shard is dropped (gen_data deduplicates per shard,
///    and tools/check_duplicates.sh is what measures the assembled pool);
///  * the first `skip_plies` plies of each game are dropped (default 8). Lc0 self-play randomises its
///    opening by sampling from the visit distribution with temperature, so these plies are both shared
///    between games and labelled by a search whose root was deliberately disturbed;
///  * nothing past ply `max_plies` (200) is kept, because gen_data adjudicates a draw there;
///  * a record with fewer than `min_visits` visits is dropped. Lc0's own smart pruning can end a search
///    almost immediately when one move is forced, and `best_q` is then one network evaluation rather than a
///    search result.
///
/// There is no resign adjudication to mirror: lc0 adjudicates its own games and marks the records it did so
/// for (`invariance_info` bit 5), so the archive already contains nothing past the point gen_data would
/// have stopped.
///
/// ## Why the game boundary is inferred rather than read, and why it takes two signals
///
/// A tar's members are gzip streams, and a gzip decompressor reads a concatenation of members as one
/// stream, which is what makes `tar -xOf … | gzip -dc | lc0_extract` the whole pipeline. What that loses is
/// the member boundary, and the member is the game. So the reader rebuilds it from two independent signals,
/// and **a new game starts when either one fires**:
///
///  * **an empty history.** lc0 stops filling history where the game started, so a record whose planes
///    13..24 are all zero is ply 0. Measured over 173,480 records of one tar this was exact - true at every
///    one of 1,574 ply 0s and at no other record - but it is a property of that run's
///    `fill_empty_history` setting, so it cannot be the only signal.
///  * **the `plies_left` ladder.** `V6TrainingDataArray::Write` fills it descending by exactly 1.0 per
///    record of a game, so a step that is not -1 is a boundary. This holds whatever the history setting,
///    but it can *miss* a boundary: the next game's first record may happen to carry the value the ladder
///    was about to reach. Over the same tar it did, seven times, merging seven games into their
///    predecessors - which also dragged seven games' worth of Chess960 records past the whole-game filter
///    and into the per-record castling guard. That is why neither signal is trusted alone.
///
/// `boundary_disagreements` counts the records the two disagree about, because a boundary off by a game
/// mis-numbers every ply of it and so misapplies skip_plies. It should be 0; it is reported loudly.

#include "constants.h"
#include "position.h"
#include "sha256.h"
#include "square.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <istream>
#include <string>
#include <string_view>
#include <vector>

namespace lc0
{
// ---------------------------------------------------------------------------
// The v6 record layout (lc0 src/trainingdata/trainingdata_v6.h)
// ---------------------------------------------------------------------------

/// @brief Size of one v6 training record. lc0's own static_assert on `sizeof(V6TrainingData)`.
constexpr std::size_t kRecordBytesV6 = 8356;

/// @brief Size of one v7 training record. lc0's declaration is `struct V7TrainingData : V6TrainingData`,
/// which appends `d_st`, `opp_played_idx`, `next_played_idx` and `reserved[8]` - forty bytes - under
/// `#pragma pack(1)`, and asserts `sizeof(V7TrainingData) == 8396`. Because v7 *inherits* v6 rather than
/// restating it, **every v6 field keeps its offset and its meaning**, `best_q` among them, so the offsets
/// below serve both versions and the value this tool writes means the same thing for each.
constexpr std::size_t kRecordBytesV7 = 8396;

/// @brief The largest record read, which sizes the lookahead buffer.
constexpr std::size_t kRecordBytesMax = kRecordBytesV7;

/// @brief History positions in a record's plane block, and bitboards per position (lc0's kMoveHistory and
/// kPlanesPerBoard).
constexpr std::size_t kMoveHistory    = 8;
constexpr std::size_t kPlanesPerBoard = 13;

/// @brief The record versions this reads. v5 and earlier put the result where `dummy` now is, so they are
/// still refused; v7 only adds trailing fields this tool never looks at.
constexpr uint32_t kTrainingDataVersionV6 = 6;
constexpr uint32_t kTrainingDataVersionV7 = 7;

/// @brief Bytes in one record of @p version, or 0 if this tool does not read that version.
///
/// The version is the first four bytes of every record, so a stream's stride is derived from the data
/// rather than assumed. A version this returns 0 for must stop the read: striding a v7 stream by 8356
/// bytes - or a v6 one by 8396 - would not fail, it would decode plausible-looking garbage for an entire
/// tar, which is a fault nothing downstream could detect.
constexpr std::size_t RecordBytesForVersion(uint32_t version)
{
    switch (version)
    {
    case kTrainingDataVersionV6:
        return kRecordBytesV6;
    case kTrainingDataVersionV7:
        return kRecordBytesV7;
    default:
        return 0;
    }
}

/// @brief The only input format this reads: lc0's INPUT_CLASSICAL_112_PLANE. The canonical formats
/// transform the board (mirror / flip / transpose, recorded in `invariance_info`) and move the en-passant
/// square into `side_to_move_or_enpassant`, so decoding one as this would be silently wrong.
constexpr uint32_t kInputFormatClassical = 1;

// Byte offsets of every field read, in the packed struct's declaration order.
constexpr std::size_t kRecordOffsetVersion         = 0;
constexpr std::size_t kRecordOffsetInputFormat     = 4;
constexpr std::size_t kRecordOffsetProbabilities   = 8;
constexpr std::size_t kRecordOffsetPlanes          = kRecordOffsetProbabilities + 4 * 1858;
constexpr std::size_t kRecordOffsetCastlingUsOoo   = kRecordOffsetPlanes + 8 * kMoveHistory * kPlanesPerBoard;
constexpr std::size_t kRecordOffsetCastlingUsOo    = kRecordOffsetCastlingUsOoo + 1;
constexpr std::size_t kRecordOffsetCastlingThemOoo = kRecordOffsetCastlingUsOo + 1;
constexpr std::size_t kRecordOffsetCastlingThemOo  = kRecordOffsetCastlingThemOoo + 1;
constexpr std::size_t kRecordOffsetSideToMove      = kRecordOffsetCastlingThemOo + 1;
constexpr std::size_t kRecordOffsetRule50Count     = kRecordOffsetSideToMove + 1;
constexpr std::size_t kRecordOffsetInvarianceInfo  = kRecordOffsetRule50Count + 1;
constexpr std::size_t kRecordOffsetDummy           = kRecordOffsetInvarianceInfo + 1;
constexpr std::size_t kRecordOffsetRootQ           = kRecordOffsetDummy + 1;
constexpr std::size_t kRecordOffsetBestQ           = kRecordOffsetRootQ + 4;
constexpr std::size_t kRecordOffsetRootD           = kRecordOffsetBestQ + 4;
constexpr std::size_t kRecordOffsetBestD           = kRecordOffsetRootD + 4;
constexpr std::size_t kRecordOffsetRootM           = kRecordOffsetBestD + 4;
constexpr std::size_t kRecordOffsetBestM           = kRecordOffsetRootM + 4;
constexpr std::size_t kRecordOffsetPliesLeft       = kRecordOffsetBestM + 4;
constexpr std::size_t kRecordOffsetResultQ         = kRecordOffsetPliesLeft + 4;
constexpr std::size_t kRecordOffsetResultD         = kRecordOffsetResultQ + 4;
constexpr std::size_t kRecordOffsetPlayedQ         = kRecordOffsetResultD + 4;
constexpr std::size_t kRecordOffsetPlayedD         = kRecordOffsetPlayedQ + 4;
constexpr std::size_t kRecordOffsetPlayedM         = kRecordOffsetPlayedD + 4;
constexpr std::size_t kRecordOffsetOrigQ           = kRecordOffsetPlayedM + 4;
constexpr std::size_t kRecordOffsetOrigD           = kRecordOffsetOrigQ + 4;
constexpr std::size_t kRecordOffsetOrigM           = kRecordOffsetOrigD + 4;
constexpr std::size_t kRecordOffsetVisits          = kRecordOffsetOrigM + 4;
constexpr std::size_t kRecordOffsetPlayedIndex     = kRecordOffsetVisits + 4;
constexpr std::size_t kRecordOffsetBestIndex       = kRecordOffsetPlayedIndex + 2;
constexpr std::size_t kRecordOffsetPolicyKld       = kRecordOffsetBestIndex + 2;
// q_st, lc0's "Q standard deviation". Nothing here reads it, and nothing should without checking the
// stream first: lc0's own comment says it "was uint32_t reserved in original v6", and measured on
// 2026-10-28 it is **exactly zero in every test91 record** - the main run never populates it. The
// experimental BT4 forks (cleanvisits-bt4tf13g-v7op1, k2hybrid-bt4tf13g-v7op1) do write something
// there, but it alternates sign between consecutive plies, so whatever it is it is not a standard
// deviation. The offset is right; the contents are not dependable across runs.
constexpr std::size_t kRecordOffsetQStandardDeviation = kRecordOffsetPolicyKld + 4;

static_assert(kRecordOffsetQStandardDeviation + 4 == kRecordBytesV6,
              "the field offsets must account for every byte of lc0's V6TrainingData");
static_assert(kRecordBytesV7 > kRecordBytesV6, "v7 only ever appends to the v6 layout");

/// @brief Bit 5 of `invariance_info`: lc0 adjudicated the game rather than playing it out.
constexpr uint8_t kInvarianceGameAdjudicated = 1u << 5;

/// @brief Bit 4 of `invariance_info`: the game hit lc0's maximum length and was called a draw.
constexpr uint8_t kInvarianceMaxLengthExceeded = 1u << 4;

// ---------------------------------------------------------------------------
// The filters
// ---------------------------------------------------------------------------

/// @brief Positions above this |eval| (cp) are skipped as training targets. tools/gen_data.cpp's kEvalCap.
constexpr int kEvalCap = 3000;

/// @brief The sigmoid scale tools/bullet/pawnstar.rs trains against, and so the one this inverts.
constexpr double kEvalScale = 400.0;

/// @brief Nothing past this ply of a game is kept. tools/gen_data.cpp's kMaxPlies.
constexpr int kMaxPlies = 200;

/// @brief Plies dropped from the start of each game by default - lc0's temperature-sampled opening.
constexpr int kDefaultSkipPlies = 8;

/// @brief Fewest root visits a record's search may have had. Below this, `best_q` is barely a search.
constexpr uint32_t kDefaultMinVisits = 100;

/// @brief The standard start position, which every standard lc0 self-play game begins from.
inline constexpr std::string_view kStartPositionFen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

/// @brief Its placement field alone - what a game's first record is checked against to refuse Chess960.
inline constexpr std::string_view kStartPlacement = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR";

/// @brief Which of lc0's two value estimates becomes the label.
///
/// `best_q` is the backed-up value of the search's best move and is the one lczero-training uses as its own
/// value target; `root_q` is the visit-weighted average over every root move, which includes the bad ones
/// and is pulled toward the mean by the Dirichlet noise lc0 adds at the root. kBestQ is the default for
/// that reason, and kRootQ is reachable so that the choice can be A/B tested rather than argued about.
enum class ValueHead : uint8_t
{
    kBestQ, ///< `best_q`, the value of the search's best move (lczero-training's own target).
    kRootQ, ///< `root_q`, the visit-weighted average over the root's moves.
};

/// @brief Everything the run was asked to do; also what the .meta sidecars report.
struct Options
{
    int      skip_plies         = kDefaultSkipPlies; ///< Plies dropped from the start of each game.
    int      max_plies          = kMaxPlies;         ///< Nothing past this ply is kept (0 = no cap).
    int      eval_cap           = kEvalCap;          ///< |score| at or above this is dropped.
    double   eval_scale         = kEvalScale;        ///< The trainer's sigmoid scale, which this inverts.
    double   q_scale            = 1.0;               ///< `q` is multiplied by this before the mapping.
    uint32_t min_visits         = kDefaultMinVisits; ///< Fewest root visits a kept record may have had.
    bool     does_require_quiet = true;              ///< The move played out of the position must be quiet.
    /// @brief A game whose first record is not the standard start position is dropped whole.
    ///
    /// On by default, because that is how the archive's Chess960 games are caught before any of their
    /// records can reach a shard (see the file comment). It is switchable only so that
    /// test/lc0_extract_test.cpp can drive the per-position rules from a hand-written endgame rather than
    /// having to spell out forty plies of a game to reach one; a harvest run leaves it alone.
    bool        does_require_standard_start = true;
    bool        does_skip_adjudicated       = false;             ///< Drop records of a game lc0 adjudicated.
    ValueHead   value_head                  = ValueHead::kBestQ; ///< Which value estimate becomes the label.
    std::size_t shard_positions             = 2000000;           ///< Target records per shard.
};

/// @brief What the run counted - the yield report, and the per-shard provenance.
///
/// Every drop has a reason of its own, because the reasons are what size the real run: a yield that is
/// mostly `dropped_eval_cap` means the pool is all decided positions, while one that is mostly
/// `dropped_not_quiet` means the games are being harvested in their tactical middlegames.
struct Statistics
{
    uint64_t records_seen                 = 0; ///< Records read off the stream.
    uint64_t records_bad_version          = 0; ///< Records whose `version` was not 6.
    uint64_t records_bad_format           = 0; ///< Records whose `input_format` was not 1.
    uint64_t games_seen                   = 0; ///< Game boundaries found.
    uint64_t games_non_standard           = 0; ///< Games whose first record was not the standard start position.
    uint64_t games_used                   = 0; ///< Games walked.
    uint64_t games_contributing           = 0; ///< Games that wrote at least one record.
    uint64_t games_boundary_disagreements = 0; ///< Records the two boundary signals disagreed about.

    uint64_t plies_seen = 0; ///< Plies of a walked game considered.

    uint64_t dropped_skip_plies   = 0; ///< Dropped by --skip-plies.
    uint64_t dropped_max_plies    = 0; ///< Dropped by --max-plies.
    uint64_t dropped_min_visits   = 0; ///< Dropped by --min-visits.
    uint64_t dropped_adjudicated  = 0; ///< Dropped by --skip-adjudicated.
    uint64_t dropped_castling     = 0; ///< Dropped as not a standard-chess castling placement (Chess960).
    uint64_t dropped_unreadable   = 0; ///< Dropped because the FEN did not round-trip through Position.
    uint64_t dropped_in_check     = 0; ///< Dropped because the side to move was in check.
    uint64_t dropped_not_quiet    = 0; ///< Dropped because the move out of the position was not quiet.
    uint64_t dropped_last_of_game = 0; ///< Dropped because no successor record could judge the move.
    uint64_t dropped_eval_cap     = 0; ///< Dropped by --eval-cap.
    uint64_t dropped_duplicate    = 0; ///< Dropped as already in this shard.
};

// ---------------------------------------------------------------------------
// Reading one record
// ---------------------------------------------------------------------------

/// @brief A borrowed view over one 8356-byte v6 record, reading its fields little-endian on request.
///
/// A view rather than a decoded struct because a record is 8.1 KB of which 112 bytes are ever read: the
/// 1858 policy probabilities and seven of the eight history positions are nine tenths of the stream and are
/// never wanted. It borrows, so the bytes must outlive it.
class RecordView
{
  public:
    /// @brief Wrap a record. @param bytes One whole record, which must outlive this view.
    explicit RecordView(const unsigned char *bytes) : bytes_{bytes}
    {
    }

    uint32_t version() const
    {
        return ReadUint32(kRecordOffsetVersion);
    }

    uint32_t input_format() const
    {
        return ReadUint32(kRecordOffsetInputFormat);
    }

    /// @brief One of the 104 bitboards. @param index 0..103, index 0..12 being the position being labelled.
    /// @return The plane, bit-reversed within each byte exactly as the writer stored it.
    uint64_t plane(std::size_t index) const
    {
        return ReadUint64(kRecordOffsetPlanes + 8 * index);
    }

    bool is_black_to_move() const
    {
        return bytes_[kRecordOffsetSideToMove] != 0;
    }

    bool can_us_castle_queenside() const
    {
        return bytes_[kRecordOffsetCastlingUsOoo] != 0;
    }

    bool can_us_castle_kingside() const
    {
        return bytes_[kRecordOffsetCastlingUsOo] != 0;
    }

    bool can_them_castle_queenside() const
    {
        return bytes_[kRecordOffsetCastlingThemOoo] != 0;
    }

    bool can_them_castle_kingside() const
    {
        return bytes_[kRecordOffsetCastlingThemOo] != 0;
    }

    uint8_t rule50_count() const
    {
        return bytes_[kRecordOffsetRule50Count];
    }

    uint8_t invariance_info() const
    {
        return bytes_[kRecordOffsetInvarianceInfo];
    }

    bool is_game_adjudicated() const
    {
        return (invariance_info() & kInvarianceGameAdjudicated) != 0;
    }

    float root_q() const
    {
        return ReadFloat(kRecordOffsetRootQ);
    }

    float best_q() const
    {
        return ReadFloat(kRecordOffsetBestQ);
    }

    float best_d() const
    {
        return ReadFloat(kRecordOffsetBestD);
    }

    float plies_left() const
    {
        return ReadFloat(kRecordOffsetPliesLeft);
    }

    float result_q() const
    {
        return ReadFloat(kRecordOffsetResultQ);
    }

    float result_d() const
    {
        return ReadFloat(kRecordOffsetResultD);
    }

    uint32_t visits() const
    {
        return ReadUint32(kRecordOffsetVisits);
    }

    /// @brief Whether the history slot behind the labelled position holds a board at all.
    /// A record with none is ply 0 of a game: lc0 stops filling history where the game started.
    bool has_previous_position() const
    {
        for (std::size_t index = kPlanesPerBoard; index < kPlanesPerBoard + 12; ++index)
        {
            if (plane(index) != 0)
            {
                return true;
            }
        }

        return false;
    }

  private:
    uint32_t ReadUint32(std::size_t offset) const
    {
        return (uint32_t)bytes_[offset] | ((uint32_t)bytes_[offset + 1] << 8) | ((uint32_t)bytes_[offset + 2] << 16) |
               ((uint32_t)bytes_[offset + 3] << 24);
    }

    uint64_t ReadUint64(std::size_t offset) const
    {
        uint64_t value = 0;

        for (std::size_t byte = 0; byte < 8; ++byte)
        {
            value |= (uint64_t)bytes_[offset + byte] << (8 * byte);
        }

        return value;
    }

    float ReadFloat(std::size_t offset) const
    {
        const uint32_t bits  = ReadUint32(offset);
        float          value = 0.0f;
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    }

    const unsigned char *bytes_ = nullptr;
};

// ---------------------------------------------------------------------------
// Decoding the planes into a position
// ---------------------------------------------------------------------------

/// @brief A record's position, as far as a FEN and the filters need it.
struct DecodedPosition
{
    std::string fen;                            ///< The full six-field FEN.
    int         piece_count            = 0;     ///< Pieces of both colours on the board.
    int         pawn_count             = 0;     ///< Pawns of both colours on the board.
    bool        is_castling_consistent = true;  ///< Every castling flag matches a standard king/rook placement.
    bool        is_standard_start      = false; ///< The placement is the standard chess start placement.
};

/// @brief Flip a plane's ranks, which is how a history position's own orientation is undone.
/// lc0's `BitBoard::Mirror()` is a byte reversal, and a byte of a plane is a rank.
/// @param plane The plane. @return It, with rank 1 and rank 8 swapped.
inline uint64_t FlipPlaneRanks(uint64_t plane)
{
    return std::byteswap(plane);
}

/// @brief Where a stored plane's bit sits on the real board.
/// @param bit_index 0..63, as stored. @param is_black_to_move Whether the planes are Black's orientation.
/// @return The real square index, a1 = 0, h8 = 63.
inline int PlaneBitToSquare(int bit_index, bool is_black_to_move)
{
    const int row  = bit_index / 8;
    const int file = 7 - bit_index % 8; // the writer reversed the bits within every byte
    const int rank = is_black_to_move ? 7 - row : row;
    return rank * 8 + file;
}

/// @brief Turn a record's first 13 planes, castling flags and clocks into a FEN.
///
/// @param record The record. @param ply Which ply of its game this is, for the FEN's move number.
/// @return The decoded position. `is_castling_consistent` false means a Chess960 record (see the file
///         comment); the FEN is still filled in, so that a caller can report what it refused.
inline DecodedPosition DecodePosition(const RecordView &record, int ply)
{
    const bool            is_black_to_move = record.is_black_to_move();
    std::array<char, 64>  squares{};
    static constexpr char kWhitePieces[6] = {'P', 'N', 'B', 'R', 'Q', 'K'};
    static constexpr char kBlackPieces[6] = {'p', 'n', 'b', 'r', 'q', 'k'};
    DecodedPosition       decoded;

    squares.fill('\0');

    for (std::size_t plane_index = 0; plane_index < 12; ++plane_index)
    {
        const bool is_ours        = plane_index < 6;
        const bool is_white_piece = is_ours != is_black_to_move;
        const char piece          = is_white_piece ? kWhitePieces[plane_index % 6] : kBlackPieces[plane_index % 6];
        uint64_t   plane          = record.plane(plane_index);

        while (plane != 0)
        {
            const int bit = std::countr_zero(plane);
            plane &= plane - 1;
            squares[(std::size_t)PlaneBitToSquare(bit, is_black_to_move)] = piece;
            ++decoded.piece_count;

            if (piece == 'P' || piece == 'p')
            {
                ++decoded.pawn_count;
            }
        }
    }

    std::string placement;

    for (int rank = 7; rank >= 0; --rank)
    {
        int empty_run = 0;

        for (int file = 0; file < 8; ++file)
        {
            const char piece = squares[(std::size_t)(rank * 8 + file)];

            if (piece == '\0')
            {
                ++empty_run;
                continue;
            }

            if (empty_run > 0)
            {
                placement += (char)('0' + empty_run);
                empty_run = 0;
            }

            placement += piece;
        }

        if (empty_run > 0)
        {
            placement += (char)('0' + empty_run);
        }

        if (rank > 0)
        {
            placement += '/';
        }
    }

    // "Us" is the side to move, so which of White and Black owns each castling flag depends on that.
    const bool can_white_castle_kingside =
        is_black_to_move ? record.can_them_castle_kingside() : record.can_us_castle_kingside();
    const bool can_white_castle_queenside =
        is_black_to_move ? record.can_them_castle_queenside() : record.can_us_castle_queenside();
    const bool can_black_castle_kingside =
        is_black_to_move ? record.can_us_castle_kingside() : record.can_them_castle_kingside();
    const bool can_black_castle_queenside =
        is_black_to_move ? record.can_us_castle_queenside() : record.can_them_castle_queenside();

    std::string castling;

    if (can_white_castle_kingside)
    {
        castling += 'K';
    }

    if (can_white_castle_queenside)
    {
        castling += 'Q';
    }

    if (can_black_castle_kingside)
    {
        castling += 'k';
    }

    if (can_black_castle_queenside)
    {
        castling += 'q';
    }

    if (castling.empty())
    {
        castling = "-";
    }

    // Standard chess puts the king on e1/e8 and the rooks on a1/h1 and a8/h8; a Chess960 record keeps its
    // rights with the rooks somewhere else, and its rook files are not in the record to be read.
    const auto is_piece_at = [&squares](int file, int rank, char piece) {
        return squares[(std::size_t)(rank * 8 + file)] == piece;
    };

    decoded.is_castling_consistent =
        (!can_white_castle_kingside || (is_piece_at(4, 0, 'K') && is_piece_at(7, 0, 'R'))) &&
        (!can_white_castle_queenside || (is_piece_at(4, 0, 'K') && is_piece_at(0, 0, 'R'))) &&
        (!can_black_castle_kingside || (is_piece_at(4, 7, 'k') && is_piece_at(7, 7, 'r'))) &&
        (!can_black_castle_queenside || (is_piece_at(4, 7, 'k') && is_piece_at(0, 7, 'r')));

    // En passant, recovered from the previous position (see the file comment).
    std::string en_passant = "-";

    if (record.has_previous_position())
    {
        const uint64_t their_pawns_before = FlipPlaneRanks(record.plane(kPlanesPerBoard + 0));
        const uint64_t their_pawns_now    = record.plane(6);
        const uint64_t vacated            = their_pawns_before & ~their_pawns_now;
        const uint64_t occupied           = their_pawns_now & ~their_pawns_before;

        if (std::popcount(vacated) == 1 && std::popcount(occupied) == 1)
        {
            const int from_bit  = std::countr_zero(vacated);
            const int to_bit    = std::countr_zero(occupied);
            const int from_row  = from_bit / 8;
            const int to_row    = to_bit / 8;
            const int from_file = 7 - from_bit % 8;
            const int to_file   = 7 - to_bit % 8;

            // In the mover's own orientation their pawns start on row 6 and advance downward, so a double
            // push is row 6 to row 4 and the square passed over is row 5.
            if (from_row == 6 && to_row == 4 && from_file == to_file)
            {
                const int rank = is_black_to_move ? 7 - 5 : 5;
                en_passant     = std::string{(char)('a' + to_file)} + (char)('1' + rank);
            }
        }
    }

    decoded.is_standard_start = placement == kStartPlacement;

    decoded.fen = placement + ' ' + (is_black_to_move ? 'b' : 'w') + ' ' + castling + ' ' + en_passant + ' ' +
                  std::to_string((int)record.rule50_count()) + ' ' + std::to_string(ply / 2 + 1);
    return decoded;
}

// ---------------------------------------------------------------------------
// The label
// ---------------------------------------------------------------------------

/// @brief Turn one of lc0's value estimates into the centipawn score the trainer's sigmoid would map back.
///
/// @param q `w - l` from the side to move's point of view, in [-1, 1].
/// @param options The run's eval_scale and q_scale.
/// @return The score in centipawns, side-to-move relative, clamped so that a saturated `q` cannot produce
///         an infinity: the caller's eval cap is what decides whether to keep it.
inline int CentipawnsFromValue(double q, const Options &options)
{
    constexpr double kSaturationGuard = 1.0 - 1e-9;
    const double     scaled           = std::clamp(q * options.q_scale, -kSaturationGuard, kSaturationGuard);
    const double     score            = options.eval_scale * std::log((1.0 + scaled) / (1.0 - scaled));
    return (int)std::lround(std::clamp(score, -1.0e6, 1.0e6));
}

/// @brief Which value estimate a record offers for the chosen head. @param record The record.
/// @param head Which head. @return The value, side-to-move relative.
inline double ValueOf(const RecordView &record, ValueHead head)
{
    return head == ValueHead::kRootQ ? (double)record.root_q() : (double)record.best_q();
}

/// @brief The game's result as a bulletformat wdl token, White-relative.
///
/// lc0 stores it side-to-move relative: `result_d` is 1 for a draw, and otherwise `result_q` is +1 when the
/// side to move won. @param record The record. @return "1.0", "0.5" or "0.0" - never a PGN spelling.
inline const char *WhiteRelativeResultText(const RecordView &record)
{
    if (record.result_d() > 0.5f)
    {
        return "0.5";
    }

    const bool did_side_to_move_win = record.result_q() > 0.0f;
    const bool did_white_win        = did_side_to_move_win != record.is_black_to_move();
    return did_white_win ? "1.0" : "0.0";
}

// ---------------------------------------------------------------------------
// The emit loop
// ---------------------------------------------------------------------------

/// @brief Where kept records go. The shard writer implements it; the test collects into a vector.
///
/// The dedup lives behind IsNewToShard rather than here because only whoever owns the shards knows what is
/// in the current one, which is gen_data's rule and the one tools/check_duplicates.sh measures against.
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

/// @brief One game's records, as the reader hands them over: the raw bytes, back to back.
///
/// A game is held whole rather than streamed because the quiet filter needs each position's successor, and
/// a game is a few hundred records - 2 MB at the very worst, against the 8 GB a tar's worth would be.
using GameRecords = std::vector<unsigned char>;

// ---------------------------------------------------------------------------
// Reading the stream, and rebuilding the game boundary
// ---------------------------------------------------------------------------

/// @brief Reads fixed-size v6 records off a stream and hands them back a game at a time, hashing as it goes.
///
/// The stream is a pipe (`tar -xOf … | gzip -dc | lc0_extract`), so it can never be re-read and the digest
/// taken as the bytes go past is the only evidence of what was read - the same reason
/// tools/lichess_extract.cpp hashes its PGN inline. The digest covers exactly the records handed out as
/// part of a game, never the one-record lookahead sitting in the reader, so a shard's byte range and its
/// digest describe the same bytes its records came from.
///
/// The boundary itself is inferred from two signals, because a gzip concatenation has no member boundary
/// left to read: see "Why the game boundary is inferred rather than read, and why it takes two signals" in
/// the file comment.
class RecordReader
{
  public:
    /// @brief Read from @p input, which must outlive the reader.
    /// @param does_digest_input Whether to SHA-256 every byte handed out. It is the only evidence of what a
    ///        pipe held, but it is also the whole per-byte cost of a run: measured over one tar, 44.3 s of
    ///        a 52.7 s conversion was read plus hash, against 8.4 s for all the decoding and filtering. A
    ///        harvest that records the *tar's* digest instead (`--source-sha256`, one pass over a gigabyte
    ///        rather than forty) loses nothing auditable and runs five times faster.
    explicit RecordReader(std::istream &input, bool does_digest_input = true)
        : input_{&input}, does_digest_input_{does_digest_input}
    {
    }

    /// @brief Hand back the next game's records. @param game Filled with the game, back to back.
    /// @return false once the stream is spent (or malformed, which also sets has_failed()).
    bool NextGame(GameRecords &game);

    /// @brief One record's size for this stream, from the version its records carry.
    /// @return The size in bytes, or 0 before the first record has been read.
    std::size_t record_bytes() const
    {
        return record_bytes_;
    }

    /// @brief Bytes pulled off the stream, lookahead included. @return The count.
    uint64_t bytes_consumed() const
    {
        return bytes_consumed_;
    }

    /// @brief Bytes of records handed out in a game - the prefix Digest() covers. @return The count.
    uint64_t attributable_bytes() const
    {
        return attributable_bytes_;
    }

    /// @brief SHA-256 of the handed-out prefix. @return Its lower-case hex, or "notcomputed" if the digest
    /// was switched off.
    std::string Digest() const
    {
        return does_digest_input_ ? hasher_.Digest() : std::string{"notcomputed"};
    }

    /// @brief Whether the stream was not a whole number of records. @return true if so.
    bool has_failed() const
    {
        return has_failed_;
    }

    /// @brief Records the two independent boundary signals disagreed about. @return The count.
    uint64_t boundary_disagreements() const
    {
        return boundary_disagreements_;
    }

  private:
    bool ReadRecord();
    void AppendHeld(GameRecords &game);

    std::istream                              *input_ = nullptr;
    std::array<unsigned char, kRecordBytesMax> held_{}; ///< The lookahead record, the next game's first.
    std::size_t    record_bytes_           = 0;         ///< One record's size, settled by the first record's version.
    bool           has_held_record_        = false;
    bool           has_failed_             = false;
    bool           is_spent_               = false;
    float          held_plies_left_        = 0.0f;
    uint64_t       bytes_consumed_         = 0;
    uint64_t       attributable_bytes_     = 0;
    uint64_t       boundary_disagreements_ = 0;
    bool           does_digest_input_      = true;
    sha256::Hasher hasher_;
};

/// @brief Pull one record into held_. @return true if a whole record was read.
inline bool RecordReader::ReadRecord()
{
    if (is_spent_)
    {
        return false;
    }

    // Every record carries its version in its first four bytes, so the stride is read out of the data
    // rather than assumed. Doing it per record, not just for the first, is what makes a stream whose
    // records are mixed fail here instead of mis-striding: a v7 stream walked in 8356-byte steps would
    // not error, it would decode plausible-looking garbage for a whole tar, and nothing downstream
    // could tell. The 32-bytes-per-position check on the output is the same idea one stage later.
    input_->read((char *)held_.data(), (std::streamsize)sizeof(uint32_t));

    if (input_->gcount() == 0)
    {
        is_spent_ = true;
        return false;
    }

    bytes_consumed_ += (uint64_t)input_->gcount();

    if (input_->gcount() != (std::streamsize)sizeof(uint32_t))
    {
        has_failed_ = true;
        is_spent_   = true;
        return false;
    }

    const uint32_t    version      = RecordView{held_.data()}.version();
    const std::size_t record_bytes = RecordBytesForVersion(version);

    if (record_bytes == 0 || (record_bytes_ != 0 && record_bytes != record_bytes_))
    {
        // An unreadable version, or a stream that changed version part way through. Either way the
        // stride is no longer known, so stop rather than guess at it.
        has_failed_ = true;
        is_spent_   = true;
        return false;
    }

    record_bytes_ = record_bytes;

    input_->read((char *)held_.data() + sizeof(uint32_t), (std::streamsize)(record_bytes_ - sizeof(uint32_t)));
    const std::streamsize read = input_->gcount();
    bytes_consumed_ += (uint64_t)read;

    if (read != (std::streamsize)(record_bytes_ - sizeof(uint32_t)))
    {
        // A truncated tail is the shape a rate-limited download or a half-written tar takes, so it is a
        // failure and not an end of input: a run that swallowed it would report a short yield as success.
        has_failed_ = true;
        is_spent_   = true;
        return false;
    }

    has_held_record_ = true;
    held_plies_left_ = RecordView{held_.data()}.plies_left();
    return true;
}

/// @brief Move the held record into @p game, hashing it as part of the attributable prefix.
inline void RecordReader::AppendHeld(GameRecords &game)
{
    game.insert(game.end(), held_.begin(), held_.begin() + (std::ptrdiff_t)record_bytes_);

    if (does_digest_input_)
    {
        hasher_.Update(held_.data(), record_bytes_);
    }

    attributable_bytes_ += record_bytes_;
    has_held_record_ = false;
}

inline bool RecordReader::NextGame(GameRecords &game)
{
    game.clear();

    if (!has_held_record_ && !ReadRecord())
    {
        return false;
    }

    // This record opens a game, so the empty-history signal should say so too.
    if (RecordView{held_.data()}.has_previous_position())
    {
        ++boundary_disagreements_;
    }

    float previous_plies_left = held_plies_left_;
    AppendHeld(game);

    while (ReadRecord())
    {
        const bool does_ladder_continue =
            held_plies_left_ < previous_plies_left - 0.5f && held_plies_left_ > previous_plies_left - 1.5f;
        const bool has_history = RecordView{held_.data()}.has_previous_position();

        if (does_ladder_continue != has_history)
        {
            ++boundary_disagreements_;
        }

        // Either signal is enough to end the game: the ladder can run on into the next game by
        // coincidence, and the history signal depends on how the run was configured.
        if (!does_ladder_continue || !has_history)
        {
            return true; // held_ is the next game's first record, and stays held
        }

        previous_plies_left = held_plies_left_;
        AppendHeld(game);
    }

    return true;
}

/// @brief Walk one game and write every position that passes the filters.
///
/// @param game The game's records, back to back.
/// @param record_bytes One record's size, taken from the stream's own version; @p game must be an exact
///        multiple of it.
/// @param options The filters. @param sink Where kept records go. @param statistics Counters, added to.
inline void ExtractGame(const GameRecords &game, std::size_t record_bytes, const Options &options, RecordSink &sink,
                        Statistics &statistics)
{
    if (record_bytes == 0 || game.size() % record_bytes != 0)
    {
        // Not a whole number of records. Refuse it rather than stride past the end of the last one: a
        // mis-stride does not fail, it decodes plausible garbage, and nothing downstream could tell.
        return;
    }

    const std::size_t record_count = game.size() / record_bytes;

    if (record_count == 0)
    {
        return;
    }

    ++statistics.games_seen;

    const RecordView first{game.data()};

    if (RecordBytesForVersion(first.version()) != record_bytes)
    {
        statistics.records_bad_version += record_count;
        return;
    }

    if (first.input_format() != kInputFormatClassical)
    {
        statistics.records_bad_format += record_count;
        return;
    }

    // A game that did not start from the standard position is Chess960, whose rook files input format 1
    // does not carry (see the file comment). Dropped whole, so no part of it reaches a shard.
    if (options.does_require_standard_start && !DecodePosition(first, 0).is_standard_start)
    {
        ++statistics.games_non_standard;
        return;
    }

    ++statistics.games_used;
    bool has_game_contributed = false;

    for (std::size_t index = 0; index < record_count; ++index)
    {
        const RecordView record{game.data() + index * record_bytes};
        const int        ply = (int)index;
        ++statistics.plies_seen;

        if (ply < options.skip_plies)
        {
            ++statistics.dropped_skip_plies;
            continue;
        }

        if (options.max_plies > 0 && ply >= options.max_plies)
        {
            ++statistics.dropped_max_plies;
            continue;
        }

        if (record.visits() < options.min_visits)
        {
            ++statistics.dropped_min_visits;
            continue;
        }

        if (options.does_skip_adjudicated && record.is_game_adjudicated())
        {
            ++statistics.dropped_adjudicated;
            continue;
        }

        // The move out of this position is judged on the next record, which is the position it reached, so
        // the last ply of a game can never be judged and is not kept. See the file comment. Only the quiet
        // filter needs the successor, so with the filter off the last ply is as usable as any other.
        if (options.does_require_quiet && index + 1 >= record_count)
        {
            ++statistics.dropped_last_of_game;
            continue;
        }

        const DecodedPosition decoded = DecodePosition(record, ply);

        if (!decoded.is_castling_consistent)
        {
            ++statistics.dropped_castling;
            continue;
        }

        // Position::FromString answers an unparseable FEN with the start position rather than an error, so
        // the round trip is the only way to know it was read as written - and a decode bug that produced a
        // legal-looking FEN of the wrong position would otherwise be invisible.
        const Position position = Position::FromString(decoded.fen);

        if (position.ToString() != decoded.fen)
        {
            ++statistics.dropped_unreadable;
            continue;
        }

        if (position.IsInCheck())
        {
            ++statistics.dropped_in_check;
            continue;
        }

        if (options.does_require_quiet && index + 1 < record_count)
        {
            const RecordView      successor{game.data() + (index + 1) * record_bytes};
            const DecodedPosition after         = DecodePosition(successor, ply + 1);
            const bool            was_capture   = after.piece_count < decoded.piece_count;
            const bool            was_promotion = after.pawn_count < decoded.pawn_count;

            if (was_capture || was_promotion)
            {
                ++statistics.dropped_not_quiet;
                continue;
            }
        }

        const int side_to_move_score = CentipawnsFromValue(ValueOf(record, options.value_head), options);
        const int white_score        = record.is_black_to_move() ? -side_to_move_score : side_to_move_score;

        if (options.eval_cap > 0 && std::abs(white_score) >= options.eval_cap)
        {
            ++statistics.dropped_eval_cap;
            continue;
        }

        if (!sink.IsNewToShard(position.hash_))
        {
            ++statistics.dropped_duplicate;
            continue;
        }

        sink.Write(decoded.fen, white_score, WhiteRelativeResultText(record));
        has_game_contributed = true;
    }

    if (has_game_contributed)
    {
        ++statistics.games_contributing;
    }

    sink.EndGame();
}
} // namespace lc0

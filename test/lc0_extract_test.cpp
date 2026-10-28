/// @file lc0_extract_test.cpp Unit tests of the Leela-Chess-Zero v6 extraction rules (tools/lc0_extract.h).
///
/// The extractor reads a gigabyte-per-hour archive it can never re-read, writes shards nothing downstream
/// validates semantically, and every one of its rules fails *quietly*. Four of them would poison a training
/// run while every number in the yield report still looked right:
///
///  * **the bit order inside a plane byte.** lc0's writer reverses the bits of every byte
///    (`ReverseBitsInBytes`), so a stored bit's file is `7 - bit % 8`. Reading it the other way round
///    mirrors every position left-to-right and leaves a perfectly legal FEN carrying another position's
///    label.
///  * **the orientation.** lc0 keeps every position from the mover's point of view, so a black-to-move
///    record's planes are rank-flipped *and* colour-swapped. Missing that turns Black's position into a
///    different White one.
///  * **the White-relative sign.** `best_q` and `result_q` are side-to-move relative and bulletformat's
///    text parser flips both columns itself for a black-to-move FEN, so the extractor must flip them once -
///    exactly once. A missing flip inverts the label of half the data.
///  * **Chess960.** About 3.5% of the archive's games begin from a shuffled back rank, and input format 1
///    does not carry the rook files, so decoding one as standard chess invents castling rights for rooks
///    that are not there. The result is an illegal FEN that killed `build/pawnstar.exe` mid-run.
///
/// **The first test is the anchor, and it is not self-referential.** Its records are built from plane words
/// copied verbatim out of a real `training-run1-test80-20250725-1917.tar` member, with the FENs written out
/// by hand - so the bit order, the orientation, the castling mapping and the en-passant recovery are all
/// checked against the real archive rather than against this file's own encoder. The encoder below exists
/// only so that the *other* tests can state a position as a FEN, and it is trusted only because the anchor
/// test pins the convention it implements.
///
/// Usage:  test_lc0_extract   (no arguments, no resources)

#include "lc0_extract.h"

#include "test_report.h"

#include <array>
#include <cmath>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

namespace
{
void Check(bool condition, const std::string &message)
{
    test_report::Check(condition, message);
}

/// @brief One emitted record, as the shard would carry it.
struct Record
{
    std::string fen;
    int         white_score = 0;
    std::string wdl;
};

/// @brief Collects records in memory, deduplicating per "shard" exactly as the real writer does.
class CollectingSink final : public lc0::RecordSink
{
  public:
    bool IsNewToShard(zobrist_t hash) override
    {
        return seen_.insert(hash).second;
    }

    void Write(const std::string &fen, int white_score, const char *wdl_text) override
    {
        records.push_back(Record{fen, white_score, wdl_text});
    }

    void EndGame() override
    {
        ++games_ended;
    }

    std::vector<Record> records;
    int                 games_ended = 0;

  private:
    std::unordered_set<zobrist_t> seen_;
};

// ---------------------------------------------------------------------------
// Building a record
// ---------------------------------------------------------------------------

/// @brief Everything a built record needs that is not its planes.
struct RecordFields
{
    bool     is_black_to_move          = false;
    bool     can_us_castle_queenside   = false;
    bool     can_us_castle_kingside    = false;
    bool     can_them_castle_queenside = false;
    bool     can_them_castle_kingside  = false;
    uint8_t  rule50_count              = 0;
    uint8_t  invariance_info           = 0;
    float    best_q                    = 0.0f;
    float    root_q                    = 0.0f;
    float    result_q                  = 0.0f;
    float    result_d                  = 1.0f;
    uint32_t visits                    = 800;
    float    plies_left                = 40.0f;
};

void WriteUint32(std::vector<unsigned char> &bytes, std::size_t offset, uint32_t value)
{
    for (std::size_t byte = 0; byte < 4; ++byte)
    {
        bytes[offset + byte] = (unsigned char)((value >> (8 * byte)) & 0xffu);
    }
}

void WriteUint64(std::vector<unsigned char> &bytes, std::size_t offset, uint64_t value)
{
    for (std::size_t byte = 0; byte < 8; ++byte)
    {
        bytes[offset + byte] = (unsigned char)((value >> (8 * byte)) & 0xffu);
    }
}

void WriteFloat(std::vector<unsigned char> &bytes, std::size_t offset, float value)
{
    uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    WriteUint32(bytes, offset, bits);
}

/// @brief Build one record. @param planes The plane words, as stored (index 0 is the labelled position's
/// first bitboard). @param fields Everything else.
/// @param version The version to stamp: 6 by default, or 7 for lc0's `V7TrainingData`, which inherits the
///        v6 layout and appends forty bytes this tool never reads. Those trailing bytes are left zero,
///        which is all the tests need: the point of a v7 record here is that nothing the tool *does* read
///        has moved.
/// @return One whole record, sized for @p version.
std::vector<unsigned char> BuildRecord(const std::vector<uint64_t> &planes, const RecordFields &fields,
                                       uint32_t version = lc0::kTrainingDataVersionV6)
{
    std::vector<unsigned char> bytes(lc0::RecordBytesForVersion(version), 0);
    WriteUint32(bytes, lc0::kRecordOffsetVersion, version);
    WriteUint32(bytes, lc0::kRecordOffsetInputFormat, lc0::kInputFormatClassical);

    for (std::size_t index = 0; index < planes.size(); ++index)
    {
        WriteUint64(bytes, lc0::kRecordOffsetPlanes + 8 * index, planes[index]);
    }

    bytes[lc0::kRecordOffsetCastlingUsOoo]   = fields.can_us_castle_queenside ? 1 : 0;
    bytes[lc0::kRecordOffsetCastlingUsOo]    = fields.can_us_castle_kingside ? 1 : 0;
    bytes[lc0::kRecordOffsetCastlingThemOoo] = fields.can_them_castle_queenside ? 1 : 0;
    bytes[lc0::kRecordOffsetCastlingThemOo]  = fields.can_them_castle_kingside ? 1 : 0;
    bytes[lc0::kRecordOffsetSideToMove]      = fields.is_black_to_move ? 1 : 0;
    bytes[lc0::kRecordOffsetRule50Count]     = fields.rule50_count;
    bytes[lc0::kRecordOffsetInvarianceInfo]  = fields.invariance_info;
    WriteFloat(bytes, lc0::kRecordOffsetRootQ, fields.root_q);
    WriteFloat(bytes, lc0::kRecordOffsetBestQ, fields.best_q);
    WriteFloat(bytes, lc0::kRecordOffsetPliesLeft, fields.plies_left);
    WriteFloat(bytes, lc0::kRecordOffsetResultQ, fields.result_q);
    WriteFloat(bytes, lc0::kRecordOffsetResultD, fields.result_d);
    WriteUint32(bytes, lc0::kRecordOffsetVisits, fields.visits);
    return bytes;
}

/// @brief Encode a FEN's placement the way lc0's writer would: the mover's orientation, bits reversed
/// within every byte. Trusted only because PlaneDecodingAgainstTheRealArchive pins the convention.
/// @param fen Any FEN; only its placement and side-to-move fields are read.
/// @return The 13 planes of the labelled position.
std::vector<uint64_t> PlanesFromFen(const std::string &fen)
{
    const std::size_t     space         = fen.find(' ');
    const std::string     placement     = fen.substr(0, space);
    const bool            is_black_move = fen[space + 1] == 'b';
    std::vector<uint64_t> planes(lc0::kPlanesPerBoard, 0);
    int                   rank = 7;
    int                   file = 0;

    for (const char symbol : placement)
    {
        if (symbol == '/')
        {
            --rank;
            file = 0;
            continue;
        }

        if (symbol >= '1' && symbol <= '8')
        {
            file += symbol - '0';
            continue;
        }

        const bool        is_white_piece = symbol >= 'A' && symbol <= 'Z';
        const char        upper          = is_white_piece ? symbol : (char)(symbol - 'a' + 'A');
        const std::string order          = "PNBRQK";
        const std::size_t piece_index    = order.find(upper);
        const bool        is_ours        = is_white_piece != is_black_move;
        const int         row            = is_black_move ? 7 - rank : rank;
        const int         bit            = row * 8 + (7 - file);
        planes[(is_ours ? 0 : 6) + piece_index] |= 1ull << bit;
        ++file;
    }

    return planes;
}

/// @brief Build a one-record-per-position game out of FENs, with each position's label.
/// @param fens The positions, in ply order. @param fields Per-position fields; its planes come from the FEN.
/// @return The game's records back to back, with a descending plies_left ladder.
lc0::GameRecords BuildGame(const std::vector<std::string> &fens, std::vector<RecordFields> fields,
                           uint32_t version = lc0::kTrainingDataVersionV6)
{
    lc0::GameRecords game;

    for (std::size_t index = 0; index < fens.size(); ++index)
    {
        RecordFields entry     = fields.size() > index ? fields[index] : RecordFields{};
        entry.is_black_to_move = fens[index].find(" b ") != std::string::npos;
        entry.plies_left       = (float)(fens.size() - index);

        const std::size_t space           = fens[index].find(' ');
        const std::string rest            = fens[index].substr(space + 1);
        const std::string rights          = rest.substr(2, rest.find(' ', 2) - 2);
        const bool        is_black        = entry.is_black_to_move;
        const bool        white_kingside  = rights.find('K') != std::string::npos;
        const bool        white_queenside = rights.find('Q') != std::string::npos;
        const bool        black_kingside  = rights.find('k') != std::string::npos;
        const bool        black_queenside = rights.find('q') != std::string::npos;
        entry.can_us_castle_kingside      = is_black ? black_kingside : white_kingside;
        entry.can_us_castle_queenside     = is_black ? black_queenside : white_queenside;
        entry.can_them_castle_kingside    = is_black ? white_kingside : black_kingside;
        entry.can_them_castle_queenside   = is_black ? white_queenside : black_queenside;

        std::vector<uint64_t> planes = PlanesFromFen(fens[index]);
        planes.resize(lc0::kMoveHistory * lc0::kPlanesPerBoard, 0);

        if (index > 0)
        {
            // A non-empty history slot, so the record is not mistaken for ply 0. Its contents matter only
            // to the en-passant recovery, which these tests do not exercise (the anchor test does).
            const std::vector<uint64_t> previous = PlanesFromFen(fens[index - 1]);

            for (std::size_t slot = 0; slot < lc0::kPlanesPerBoard; ++slot)
            {
                planes[lc0::kPlanesPerBoard + slot] = previous[slot];
            }
        }

        const std::vector<unsigned char> record = BuildRecord(planes, entry, version);
        game.insert(game.end(), record.begin(), record.end());
    }

    return game;
}

// ---------------------------------------------------------------------------
// The anchor: plane words out of the real archive
// ---------------------------------------------------------------------------

/// @brief Ply 0 of `training.1723199869.gz`, verbatim. The start position, White to move, no history.
const std::vector<uint64_t> kRealStartPlanes{
    0x000000000000ff00ull, 0x0000000000000042ull, 0x0000000000000024ull, 0x0000000000000081ull, 0x0000000000000010ull,
    0x0000000000000008ull, 0x00ff000000000000ull, 0x4200000000000000ull, 0x2400000000000000ull, 0x8100000000000000ull,
    0x1000000000000000ull, 0x0800000000000000ull, 0x0000000000000000ull};

/// @brief Ply 1 of the same game: Black to move after 1.e4, with the previous position in planes 13..25.
/// Both the rank flip (the planes are Black's orientation) and the en-passant recovery ride on this one.
const std::vector<uint64_t> kRealAfterE4Planes{
    0x000000000000ff00ull, 0x0000000000000042ull, 0x0000000000000024ull, 0x0000000000000081ull, 0x0000000000000010ull,
    0x0000000000000008ull, 0x00f7000800000000ull, 0x4200000000000000ull, 0x2400000000000000ull, 0x8100000000000000ull,
    0x1000000000000000ull, 0x0800000000000000ull, 0x0000000000000000ull,
    // history step 1: the start position, in its own (White's) orientation
    0x000000000000ff00ull, 0x0000000000000042ull, 0x0000000000000024ull, 0x0000000000000081ull, 0x0000000000000010ull,
    0x0000000000000008ull, 0x00ff000000000000ull, 0x4200000000000000ull, 0x2400000000000000ull, 0x8100000000000000ull,
    0x1000000000000000ull, 0x0800000000000000ull, 0x0000000000000000ull};

/// @brief Ply 27 of `training.1723199878.gz`: Black to move, and the only castling flag set is
/// `castling_them_oo` - White's kingside right, which must come out as the FEN's `K` and nothing else.
const std::vector<uint64_t> kRealMidgamePlanes{
    0x0000000028006700ull, 0x0000000000040000ull, 0x0000000000000020ull, 0x0000000000000084ull, 0x0000001000000000ull,
    0x0000000000000002ull, 0x0026c12800000000ull, 0x0200008000000000ull, 0x0000000000000000ull, 0x1100000000000000ull,
    0x0000040000000000ull, 0x0800000000000000ull, 0x0000000000000000ull};

void PlaneDecodingAgainstTheRealArchive()
{
    {
        const std::vector<unsigned char> bytes =
            BuildRecord(kRealStartPlanes, RecordFields{.can_us_castle_queenside   = true,
                                                       .can_us_castle_kingside    = true,
                                                       .can_them_castle_queenside = true,
                                                       .can_them_castle_kingside  = true});
        const lc0::DecodedPosition decoded = lc0::DecodePosition(lc0::RecordView{bytes.data()}, 0);
        Check(decoded.fen == "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
              "the real archive's ply-0 planes decode to the start position: " + decoded.fen);
        Check(decoded.is_standard_start, "and are recognised as a standard start");
        Check(decoded.piece_count == 32 && decoded.pawn_count == 16, "with 32 pieces, 16 of them pawns");
        Check(decoded.is_castling_consistent, "and all four rights are consistent with the placement");
    }

    {
        const std::vector<unsigned char> bytes =
            BuildRecord(kRealAfterE4Planes, RecordFields{.is_black_to_move          = true,
                                                         .can_us_castle_queenside   = true,
                                                         .can_us_castle_kingside    = true,
                                                         .can_them_castle_queenside = true,
                                                         .can_them_castle_kingside  = true});
        const lc0::DecodedPosition decoded = lc0::DecodePosition(lc0::RecordView{bytes.data()}, 1);
        Check(decoded.fen == "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1",
              "a black-to-move record is un-flipped and its en passant recovered from the history: " + decoded.fen);
    }

    {
        const std::vector<unsigned char> bytes =
            BuildRecord(kRealMidgamePlanes,
                        RecordFields{.is_black_to_move = true, .can_them_castle_kingside = true, .rule50_count = 2});
        const lc0::DecodedPosition decoded = lc0::DecodePosition(lc0::RecordView{bytes.data()}, 27);
        Check(decoded.fen == "r1b2rk1/1pp2ppp/5n2/2p1p3/N1PqP3/PP3Q1P/2P2PP1/3RK1NR b K - 2 14",
              "`them` is White when Black is to move, so them_oo is the FEN's K: " + decoded.fen);
        Check(decoded.is_castling_consistent, "and that right matches White's king on e1 and rook on h1");
        Check(!decoded.is_standard_start, "a midgame placement is not a standard start");
    }

    // The encoder the other tests use must reproduce the same words, or they are testing something else.
    const std::vector<uint64_t> encoded = PlanesFromFen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    bool                        does_encoder_agree = true;

    for (std::size_t index = 0; index < lc0::kPlanesPerBoard; ++index)
    {
        does_encoder_agree = does_encoder_agree && encoded[index] == kRealStartPlanes[index];
    }

    Check(does_encoder_agree, "this test's own encoder reproduces the archive's plane words exactly");
}

// ---------------------------------------------------------------------------
// The value-head to centipawn mapping
// ---------------------------------------------------------------------------

void CentipawnMappingIsTheTrainersInverse()
{
    const lc0::Options options;

    Check(lc0::CentipawnsFromValue(0.0, options) == 0, "q = 0 is a dead-level 0 cp");
    Check(lc0::CentipawnsFromValue(0.5, options) == 439, "q = 0.5 (expected score 0.75) is 400*ln(3) = 439 cp, got " +
                                                             std::to_string(lc0::CentipawnsFromValue(0.5, options)));
    Check(lc0::CentipawnsFromValue(-0.5, options) == -439, "and it is odd in q");

    // The whole justification of the curve: pushing the score back through the trainer's own sigmoid must
    // return lc0's expected score. Anything else hands the trainer a target its loss cannot reach.
    bool does_round_trip = true;

    for (const double q : {-0.95, -0.6, -0.25, 0.0, 0.1, 0.45, 0.8, 0.97})
    {
        const double score    = (double)lc0::CentipawnsFromValue(q, options);
        const double sigmoid  = 1.0 / (1.0 + std::exp(-score / lc0::kEvalScale));
        const double expected = (1.0 + q) / 2.0;
        does_round_trip       = does_round_trip && std::abs(sigmoid - expected) < 2.0e-3;
    }

    Check(does_round_trip, "sigmoid(score / 400) returns (1 + q) / 2 across the range");

    // The ends do not produce an infinity or a NaN; the eval cap is what decides whether to keep them.
    Check(lc0::CentipawnsFromValue(1.0, options) > 3000 && lc0::CentipawnsFromValue(-1.0, options) < -3000,
          "a saturated q maps past the cap rather than to an infinity");

    lc0::Options halved = options;
    halved.q_scale      = 0.5;
    Check(lc0::CentipawnsFromValue(0.5, halved) == lc0::CentipawnsFromValue(0.25, options),
          "--q-scale rescales q before the mapping, not the score after it");
}

// ---------------------------------------------------------------------------
// The White-relative sign, both columns
// ---------------------------------------------------------------------------

void WhiteRelativeColumns()
{
    const std::string white_to_move = "4k3/8/8/8/8/8/4P3/4K3 w - - 0 20";
    const std::string black_to_move = "4k3/4p3/8/8/8/8/8/4K3 b - - 0 20";

    {
        CollectingSink  sink;
        lc0::Statistics statistics;
        lc0::Options    options;
        options.skip_plies                  = 0;
        options.does_require_standard_start = false;
        options.does_require_quiet          = false;
        // Two records so the first has a successor; the first is the one under test.
        std::vector<RecordFields> fields(2);
        fields[0].best_q   = 0.5f;
        fields[0].result_q = 1.0f;
        fields[0].result_d = 0.0f;
        fields[1].best_q   = -0.5f;
        fields[1].result_q = -1.0f;
        fields[1].result_d = 0.0f;
        lc0::ExtractGame(BuildGame({white_to_move, black_to_move}, fields), lc0::kRecordBytesV6, options, sink,
                         statistics);

        Check(sink.records.size() == 2, "both plies of the two-record game were emitted");

        if (sink.records.size() == 2)
        {
            Check(sink.records[0].white_score == 439 && sink.records[0].wdl == "1.0",
                  "a white-to-move record passes through unflipped");
            Check(sink.records[1].white_score == 439 && sink.records[1].wdl == "1.0",
                  "a black-to-move record is flipped once: q = -0.5 for Black is +439 for White, and "
                  "result_q = -1 for Black is a White win, got " +
                      std::to_string(sink.records[1].white_score) + " | " + sink.records[1].wdl);
        }
    }

    {
        CollectingSink  sink;
        lc0::Statistics statistics;
        lc0::Options    options;
        options.skip_plies                  = 0;
        options.does_require_standard_start = false;
        options.does_require_quiet          = false;
        std::vector<RecordFields> fields(2);
        fields[0].result_d = 1.0f;
        fields[1].result_d = 1.0f;
        lc0::ExtractGame(BuildGame({white_to_move, black_to_move}, fields), lc0::kRecordBytesV6, options, sink,
                         statistics);

        Check(sink.records.size() == 2 && sink.records[0].wdl == "0.5" && sink.records[1].wdl == "0.5",
              "result_d = 1 is a draw for either side to move, spelled 0.5 and never 1/2-1/2");
    }
}

// ---------------------------------------------------------------------------
// Chess960, which is in the archive and must not reach a shard
// ---------------------------------------------------------------------------

void Chess960IsRefused()
{
    {
        CollectingSink  sink;
        lc0::Statistics statistics;
        lc0::Options    options;
        options.skip_plies          = 0;
        const lc0::GameRecords game = BuildGame({"brnqknrb/pppppppp/8/8/8/8/PPPPPPPP/BRNQKNRB w KQkq - 0 1",
                                                 "brnqknrb/pppppppp/8/8/8/8/PPPPPPPP/BRNQKNRB b KQkq - 1 1"},
                                                {});
        lc0::ExtractGame(game, lc0::kRecordBytesV6, options, sink, statistics);

        Check(sink.records.empty() && statistics.games_non_standard == 1,
              "a game that did not start from the standard position is dropped whole");
        Check(statistics.games_used == 0, "and is not even walked");
    }

    {
        // The per-record guard, for a game whose first record was never seen: rights for a rook that moved.
        const std::vector<unsigned char> bytes =
            BuildRecord(PlanesFromFen("4k3/8/8/8/8/8/8/2R1K2R w KQ - 0 20"),
                        RecordFields{.can_us_castle_queenside = true, .can_us_castle_kingside = true});
        const lc0::DecodedPosition decoded = lc0::DecodePosition(lc0::RecordView{bytes.data()}, 20);
        Check(!decoded.is_castling_consistent,
              "a queenside right with no rook on a1 is refused as not standard chess: " + decoded.fen);
    }

    {
        const std::vector<unsigned char> bytes =
            BuildRecord(PlanesFromFen("4k3/8/8/8/8/8/8/R3K2R w KQ - 0 20"),
                        RecordFields{.can_us_castle_queenside = true, .can_us_castle_kingside = true});
        const lc0::DecodedPosition decoded = lc0::DecodePosition(lc0::RecordView{bytes.data()}, 20);
        Check(decoded.is_castling_consistent, "while a standard king and both rooks are accepted");
    }
}

// ---------------------------------------------------------------------------
// The quiet filter, which reads the next record instead of lc0's move index
// ---------------------------------------------------------------------------

void QuietFilterReadsTheSuccessor()
{
    const auto YieldOf = [](const std::vector<std::string> &fens, lc0::Statistics &statistics) {
        CollectingSink sink;
        lc0::Options   options;
        options.skip_plies                  = 0;
        options.does_require_standard_start = false;
        lc0::ExtractGame(BuildGame(fens, {}), lc0::kRecordBytesV6, options, sink, statistics);
        return sink.records.size();
    };

    {
        lc0::Statistics statistics;
        // Ply 0 -> ply 1 loses Black's pawn: the move out of ply 0 was a capture.
        const std::size_t yield = YieldOf({"4k3/5p2/8/8/8/8/4R3/4K3 w - - 0 20", "4k3/5R2/8/8/8/8/8/4K3 b - - 0 20",
                                           "4k3/5R2/8/8/8/8/8/4K3 w - - 1 21"},
                                          statistics);
        Check(yield == 1 && statistics.dropped_not_quiet == 1 && statistics.dropped_last_of_game == 1,
              "a capture out of a position drops it, leaving the middle ply of the three");
    }

    {
        lc0::Statistics statistics;
        // Ply 0 -> ply 1 turns a pawn into a queen with no change in piece count: a promotion.
        // Promotion to a knight, so that the successor neither loses a piece nor gives check.
        const std::size_t yield = YieldOf({"4k3/1P6/8/8/8/8/8/4K3 w - - 0 20", "1N2k3/8/8/8/8/8/8/4K3 b - - 0 20",
                                           "1N2k3/8/8/8/8/8/8/3K4 w - - 1 21"},
                                          statistics);
        Check(statistics.dropped_not_quiet == 1,
              "a promotion out of a position drops it too, although the piece count did not change");
        Check(yield >= 1, "and the rest of the game survives");
    }

    {
        lc0::Statistics   statistics;
        const std::size_t yield = YieldOf({"4k3/8/8/8/8/8/4P3/4K3 w - - 0 20", "4k3/8/8/8/4P3/8/8/4K3 b - - 0 20",
                                           "4k3/8/8/8/4P3/8/8/4K3 w - - 1 21"},
                                          statistics);
        Check(yield == 2 && statistics.dropped_not_quiet == 0 && statistics.dropped_last_of_game == 1,
              "quiet moves are kept, and the last ply of a game is dropped for want of a successor");
    }
}

// ---------------------------------------------------------------------------
// The rest of the filters, and the counters that size the next run
// ---------------------------------------------------------------------------

void FiltersAndCounters()
{
    const std::vector<std::string> fens{
        "4k3/8/8/8/8/8/4P3/4K3 w - - 0 20", "4k3/8/8/8/8/4P3/8/4K3 b - - 0 20", "4k3/8/8/8/8/4P3/8/3K4 w - - 1 21",
        "4k3/8/8/8/4P3/8/8/3K4 b - - 0 21", "4k3/8/8/8/4P3/8/8/2K5 w - - 1 22", "4k3/8/8/4P3/8/8/8/2K5 b - - 0 22",
    };

    {
        CollectingSink  sink;
        lc0::Statistics statistics;
        lc0::Options    options;
        options.skip_plies                  = 2;
        options.does_require_standard_start = false;
        lc0::ExtractGame(BuildGame(fens, {}), lc0::kRecordBytesV6, options, sink, statistics);
        Check(statistics.dropped_skip_plies == 2 && sink.records.size() == 3,
              "--skip-plies drops the opening plies, and the last of six is dropped for want of a successor");
        Check(sink.games_ended == 1 && statistics.games_contributing == 1, "the game ended once and contributed");
    }

    {
        CollectingSink  sink;
        lc0::Statistics statistics;
        lc0::Options    options;
        options.skip_plies                  = 0;
        options.does_require_standard_start = false;
        options.max_plies                   = 3;
        lc0::ExtractGame(BuildGame(fens, {}), lc0::kRecordBytesV6, options, sink, statistics);
        Check(statistics.dropped_max_plies == 3 && sink.records.size() == 3, "--max-plies drops the tail");
    }

    {
        CollectingSink  sink;
        lc0::Statistics statistics;
        lc0::Options    options;
        options.skip_plies                  = 0;
        options.does_require_standard_start = false;
        std::vector<RecordFields> fields(fens.size());

        for (RecordFields &entry : fields)
        {
            entry.visits = 10;
        }

        fields[0].visits = 800;
        lc0::ExtractGame(BuildGame(fens, fields), lc0::kRecordBytesV6, options, sink, statistics);
        Check(statistics.dropped_min_visits == 5 && sink.records.size() == 1,
              "--min-visits drops a record whose search barely ran");
    }

    {
        CollectingSink  sink;
        lc0::Statistics statistics;
        lc0::Options    options;
        options.skip_plies                  = 0;
        options.does_require_standard_start = false;
        std::vector<RecordFields> fields(fens.size());

        for (RecordFields &entry : fields)
        {
            entry.best_q = 0.9995f; // past 3000 cp
        }

        lc0::ExtractGame(BuildGame(fens, fields), lc0::kRecordBytesV6, options, sink, statistics);
        Check(statistics.dropped_eval_cap == 5 && sink.records.empty(),
              "--eval-cap drops a saturated label rather than writing a 6000 cp score");
    }

    {
        CollectingSink  sink;
        lc0::Statistics statistics;
        lc0::Options    options;
        options.skip_plies                  = 0;
        options.does_require_standard_start = false;
        options.does_skip_adjudicated       = true;
        std::vector<RecordFields> fields(fens.size());

        for (RecordFields &entry : fields)
        {
            entry.invariance_info = lc0::kInvarianceGameAdjudicated;
        }

        lc0::ExtractGame(BuildGame(fens, fields), lc0::kRecordBytesV6, options, sink, statistics);
        Check(statistics.dropped_adjudicated == 6 && sink.records.empty(),
              "--skip-adjudicated drops a game lc0 called rather than played out");
    }

    {
        // A repeated position inside one game is written once, which is gen_data's per-shard rule.
        CollectingSink  sink;
        lc0::Statistics statistics;
        lc0::Options    options;
        options.skip_plies                  = 0;
        options.does_require_standard_start = false;
        lc0::ExtractGame(BuildGame({"4k3/8/8/8/8/8/8/4K2R w - - 4 20", "4k3/8/8/8/8/8/8/4K1R1 b - - 5 20",
                                    "4k3/8/8/8/8/8/8/4K2R w - - 4 20", "4k3/8/8/8/8/8/8/4K1R1 b - - 5 20"},
                                   {}),
                         lc0::kRecordBytesV6, options, sink, statistics);
        Check(statistics.dropped_duplicate == 1 && sink.records.size() == 2,
              "a position already in the shard is dropped, counted as a duplicate");
    }

    {
        // In check: not a training target, and gen_data's own rule.
        CollectingSink  sink;
        lc0::Statistics statistics;
        lc0::Options    options;
        options.skip_plies                  = 0;
        options.does_require_standard_start = false;
        lc0::ExtractGame(BuildGame({"4k3/4R3/8/8/8/8/8/4K3 b - - 4 20", "5k2/4R3/8/8/8/8/8/4K3 w - - 5 21"}, {}),
                         lc0::kRecordBytesV6, options, sink, statistics);
        Check(statistics.dropped_in_check == 1 && sink.records.empty(),
              "a side to move in check is dropped, counted as in_check");
    }

    {
        // A record whose version or input format is not the one this reads takes its whole game with it.
        CollectingSink             sink;
        lc0::Statistics            statistics;
        lc0::Options               options;
        std::vector<unsigned char> bytes =
            BuildRecord(PlanesFromFen(std::string{lc0::kStartPositionFen}), RecordFields{});
        WriteUint32(bytes, lc0::kRecordOffsetInputFormat, 132);
        lc0::GameRecords game{bytes.begin(), bytes.end()};
        lc0::ExtractGame(game, lc0::kRecordBytesV6, options, sink, statistics);
        Check(statistics.records_bad_format == 1 && sink.records.empty(),
              "a canonical-format record is refused rather than decoded as classical");
    }
}

// ---------------------------------------------------------------------------
// The reader: the game boundary, and the provenance digest
// ---------------------------------------------------------------------------

void ReaderAndProvenance()
{
    const std::vector<std::string> first{"4k3/8/8/8/8/8/4P3/4K3 w - - 0 20", "4k3/8/8/8/8/4P3/8/4K3 b - - 0 20",
                                         "4k3/8/8/8/8/4P3/8/3K4 w - - 1 21"};
    const std::vector<std::string> second{"4k3/8/8/8/4P3/8/8/3K4 b - - 0 21", "4k3/8/8/4P3/8/8/8/3K4 w - - 0 22"};
    const lc0::GameRecords         game_one = BuildGame(first, {});
    const lc0::GameRecords         game_two = BuildGame(second, {});

    std::string stream_text;
    stream_text.append((const char *)game_one.data(), game_one.size());
    stream_text.append((const char *)game_two.data(), game_two.size());

    {
        std::istringstream stream{stream_text, std::ios::binary};
        lc0::RecordReader  reader{stream};
        lc0::GameRecords   game;

        Check(reader.NextGame(game) && game.size() == 3 * lc0::kRecordBytesV6,
              "the plies_left ladder splits the first game off at three records");
        Check(reader.attributable_bytes() == 3 * lc0::kRecordBytesV6,
              "and the attributable prefix covers those three and not the lookahead");
        Check(reader.NextGame(game) && game.size() == 2 * lc0::kRecordBytesV6, "the second game is two records");
        Check(!reader.NextGame(game), "and then the stream is spent");
        Check(reader.attributable_bytes() == stream_text.size() && !reader.has_failed(),
              "the whole stream was attributed, with no failure");
        Check(reader.boundary_disagreements() == 0, "the two boundary signals agreed about every record");
        Check(reader.Digest().size() == 64, "the digest is a 64-character hex SHA-256");
    }

    {
        // A stream that ends mid-record is a truncated download, not an end of input: it must be loud.
        std::istringstream stream{stream_text.substr(0, stream_text.size() - 100), std::ios::binary};
        lc0::RecordReader  reader{stream};
        lc0::GameRecords   game;

        while (reader.NextGame(game))
        {
        }

        Check(reader.has_failed(), "a stream that is not a whole number of records is a failure, not an end");
    }

    {
        // The digest is of the bytes, so a one-byte change in a record must change it.
        std::string altered                    = stream_text;
        altered[lc0::kRecordOffsetRule50Count] = (char)(altered[lc0::kRecordOffsetRule50Count] + 1);
        std::istringstream clean{stream_text, std::ios::binary};
        std::istringstream dirty{altered, std::ios::binary};
        lc0::RecordReader  clean_reader{clean};
        lc0::RecordReader  dirty_reader{dirty};
        lc0::GameRecords   game;

        while (clean_reader.NextGame(game))
        {
        }

        while (dirty_reader.NextGame(game))
        {
        }

        Check(clean_reader.Digest() != dirty_reader.Digest(), "the digest follows the bytes it hashed");
    }
}

/// @brief v7 must produce byte-identical output to v6, and a stream that lies about its version must fail.
///
/// This is the headline check for v7 support, and it is deliberately an equality rather than a list of
/// field comparisons. lc0's `struct V7TrainingData : V6TrainingData` appends forty bytes and moves nothing,
/// so the *only* correct outcome is that the same position, stamped v7, converts to exactly the same text.
/// A field-by-field test would pass just as happily with an offset misread by four bytes; this one cannot.
void VersionSevenMatchesVersionSix()
{
    const std::vector<std::string> fens = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1",
        "rnbqkbnr/pp1ppppp/8/2p5/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 0 2",
    };
    const std::vector<RecordFields> fields = {
        RecordFields{.best_q = 0.25f, .result_q = 1.0f, .result_d = 0.0f, .visits = 800, .plies_left = 42.0f},
        RecordFields{.best_q = -0.10f, .result_q = -1.0f, .result_d = 0.0f, .visits = 750, .plies_left = 41.0f},
        RecordFields{.best_q = 0.33f, .result_q = 1.0f, .result_d = 0.0f, .visits = 900, .plies_left = 40.0f},
    };

    lc0::Options options;
    options.skip_plies                  = 0;
    options.does_require_standard_start = false;

    CollectingSink  sink_v6;
    lc0::Statistics statistics_v6;
    lc0::ExtractGame(BuildGame(fens, fields, lc0::kTrainingDataVersionV6), lc0::kRecordBytesV6, options, sink_v6,
                     statistics_v6);

    CollectingSink  sink_v7;
    lc0::Statistics statistics_v7;
    lc0::ExtractGame(BuildGame(fens, fields, lc0::kTrainingDataVersionV7), lc0::kRecordBytesV7, options, sink_v7,
                     statistics_v7);

    Check(!sink_v6.records.empty(), "the v6 game produced records to compare against");
    bool do_records_match = sink_v7.records.size() == sink_v6.records.size();

    for (std::size_t index = 0; do_records_match && index < sink_v6.records.size(); ++index)
    {
        do_records_match = sink_v7.records[index].fen == sink_v6.records[index].fen &&
                           sink_v7.records[index].white_score == sink_v6.records[index].white_score &&
                           sink_v7.records[index].wdl == sink_v6.records[index].wdl;
    }

    Check(do_records_match, "a v7 record converts byte-identically to the same v6 record");
    Check(statistics_v7.plies_seen == statistics_v6.plies_seen && statistics_v7.games_used == statistics_v6.games_used,
          "v7 and v6 agree on every counter");

    // The sizes really do differ, so the equality above is not two identical inputs comparing equal.
    Check(BuildGame(fens, fields, lc0::kTrainingDataVersionV7).size() ==
              BuildGame(fens, fields, lc0::kTrainingDataVersionV6).size() + 3 * 40,
          "the v7 game is forty bytes per record larger, so the comparison was of different inputs");

    // Striding a v7 game as though it were v6 must produce nothing rather than plausible garbage.
    CollectingSink  sink_mixed;
    lc0::Statistics statistics_mixed;
    lc0::ExtractGame(BuildGame(fens, fields, lc0::kTrainingDataVersionV7), lc0::kRecordBytesV6, options, sink_mixed,
                     statistics_mixed);
    Check(sink_mixed.records.empty(), "a v7 game strided as v6 is refused, not mis-decoded");

    Check(lc0::RecordBytesForVersion(5) == 0 && lc0::RecordBytesForVersion(8) == 0,
          "a version this tool cannot read has no stride");
    Check(lc0::RecordBytesForVersion(lc0::kTrainingDataVersionV6) == 8356 &&
              lc0::RecordBytesForVersion(lc0::kTrainingDataVersionV7) == 8396,
          "the two readable versions carry lc0's own struct sizes");
}
} // namespace

int main()
{
    PlaneDecodingAgainstTheRealArchive();
    CentipawnMappingIsTheTrainersInverse();
    WhiteRelativeColumns();
    Chess960IsRefused();
    QuietFilterReadsTheSuccessor();
    FiltersAndCounters();
    ReaderAndProvenance();
    VersionSevenMatchesVersionSix();
    return test_report::Summary("LC0 EXTRACT: plane decoding against the real archive, the centipawn mapping, "
                                "White-relative columns, Chess960 refusal, filters, reader provenance, "
                                "v7 byte-identical to v6");
}

/// @file primitives_test.cpp Unit tests of the engine's building blocks that the search never calls at run time.
///
/// Some of the engine's small pieces run only inside the compiler (the constexpr generators behind the attack tables,
/// evaluated at compile time), only from diagnostics (Bitboard::ToString, the table's UsageStats) or only on inputs
/// a game rarely meets (insufficient material with no pieces at all, castling text with stray characters). Each is
/// checked here against what it is for: the generators, run at run time, rebuild exactly the tables the engine
/// uses; the text forms read back; the material rules call the classic draws and only those.
///
/// It also owns the two checks the shared NNUE net container rests on, and runs them first: CRC-64/XZ's
/// published check value, and the RNNUE1 header this build writes. Neither needs a net file, so unlike the
/// NNUE suites these run in every checkout.
///
/// Usage:  test_primitives   (no arguments, no resources)

// position.h first: it brings in bitboard.h, castling_rights.h, generated_data.h and move.h in the order they need.
#include "position.h"

#include "crc64.h"
#include "history_table.h"
#include "nnue.h"
#include "test_report.h"
#include "transposition_table.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <format>
#include <fstream>
#include <iostream>
#include <iterator>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace
{
bool Check(bool condition, const std::string &message)
{
    return test_report::Check(condition, message);
}

/// @brief The legal move named @p name in @p position, or Move::None().
Move FindMove(const Position &position, const std::string &name)
{
    for (const Move &move : position.GenerateLegalMoves())
    {
        if (move.ToString() == name)
        {
            return move;
        }
    }
    return Move::None();
}

void BitboardBasics()
{
    const Bitboard e4{4, 3};
    Check(e4 == Bitboard{Square{"e4"}} && e4 != Bitboard{Square{"d4"}}, "Bitboard(file, rank) is that square");
    Check(e4.ToString() == "00000000\n00000000\n00000000\n00000000\n00001000\n00000000\n00000000\n00000000\n",
          "Bitboard::ToString draws rank 8 first, file a on the left");
    const Bitboard pair = Bitboard{Square{"b2"}} | Bitboard{Square{"g7"}};
    Check(pair.Lsb() == Square{"b2"} && pair.Msb() == Square{"g7"}, "Bitboard::Lsb / Msb");
    Check(Bitboard{Square{"h4"}}.ShiftEast().IsEmpty() && Bitboard{Square{"g4"}}.ShiftEast() == Bitboard{Square{"h4"}},
          "Bitboard::ShiftEast drops the h file");
    Check(Bitboard{Square{"a4"}}.ShiftWest().IsEmpty() && Bitboard{Square{"b4"}}.ShiftWest() == Bitboard{Square{"a4"}},
          "Bitboard::ShiftWest drops the a file");
    int count = 0;
    for (auto it = pair.begin(); !(it == pair.end()); ++it)
    {
        ++count;
    }
    Check(count == 2, "Bitboard iterates its two squares to the end sentinel");
}

void GeneratedTables()
{
    using namespace gendata_detail;

    // The tables are built at compile time; run the same generators at run time and compare.
    struct Generator
    {
        BBFn                            function_;
        const std::array<Bitboard, 64> *table_;
        const char                     *name_;
    };

    Generator generators[] = {
        {KnightAttacks, &kKnightAttacks, "knight"},
        {KingAttacks, &kKingAttacks, "king"},
        {BishopAttacksOnEmptyBoard, &kBishopAttacks, "bishop (empty board)"},
        {RookAttacksOnEmptyBoard, &kRookAttacks, "rook (empty board)"},
    };
    for (Generator &generator : generators)
    {
        std::array<Bitboard, 64> rebuilt = MakeBitboards(generator.function_);
        Check(rebuilt == *generator.table_,
              std::format("the {} attack table rebuilt at run time matches the engine's", generator.name_));
    }
    int on_board = 0;
    for (int x = -2; x < 10; ++x)
    {
        for (int y = -2; y < 10; ++y)
        {
            if (IsInBoard(x, y))
            {
                ++on_board;
                if (SqBB(x, y) != SqBB(static_cast<uint8_t>(x + 8 * y)) || FileOf(static_cast<uint8_t>(x + 8 * y)) != x)
                {
                    on_board = -1000;
                }
            }
        }
    }
    Check(on_board == 64, "IsInBoard admits exactly the 64 squares, which SqBB and FileOf agree on");
}

void CastlingText()
{
    Check(CastlingRights::FromFen("KQkq").ToFenString() == "KQkq", "castling rights KQkq read back");
    Check(CastlingRights::FromFen("Qk").ToFenString() == "Qk", "castling rights Qk read back");
    Check(CastlingRights::FromFen("-").ToFenString() == "-", "no castling rights read back as '-'");
    Check(CastlingRights::FromFen("KxZq").ToFenString() == "Kq", "stray characters in the castling field are skipped");
}

/// @brief The castling field of a FEN against what the board can support (issue #30).
/// A right is the engine's only record that its king and rook are home - the move generator tests the
/// rights, the squares between and the squares the king crosses, and never looks for the rook - so a FEN
/// asserting one the board cannot support put a position into the search that no game can reach: the
/// generator emitted e1c1 and MakeMove moved a rook from an empty a1. Every such right is dropped at the
/// parse, which is where the invariant the generator relies on is established.
void CastlingRightsAgainstTheBoard()
{
    struct Case
    {
        const char *fen;            ///< The FEN handed to FromString.
        const char *expect_rights;  ///< The castling field it must read back as.
        int         castling_moves; ///< How many castling moves the side to move must then have.
        const char *what;           ///< What the case is about.
    };

    // The first is the position from issue #30, out of Leela's test80 archive: a Chess960 game read as
    // standard chess, so the rights say KQ while White's rooks stand on c1 and g1.
    static constexpr std::array<Case, 7> kCases{{
        {"b2rk3/p2p1nr1/1p2n1p1/1Np2p1p/2P2P1P/1P1BP1P1/P4N2/2R1K1R1 w KQ - 1 16", "-", 0,
         "issue #30: KQ with White's rooks on c1 and g1"},
        {"4k3/8/8/8/8/8/8/4K3 w KQkq - 0 1", "-", 0, "every right claimed with no rook on the board"},
        {"r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1", "KQkq", 2, "every right supported, and both White castlings legal"},
        {"r3k3/8/8/8/8/8/8/R3K2R w KQkq - 0 1", "KQq", 2, "Black's kingside right dropped, with no rook on h8"},
        {"4k3/8/8/8/8/8/8/rn2K2R w KQ - 0 1", "K", 1,
         "a rook of the other colour on a1 supports nothing, and the knight keeps it off the king"},
        {"4k3/8/8/8/8/8/8/R2K3R w KQ - 0 1", "-", 0, "a king off its home square supports neither right"},
        {"r3k2r/8/8/8/8/8/8/R3K1NR w KQkq - 0 1", "KQkq", 1,
         "a right stands while a piece between king and rook blocks the move"},
    }};
    for (const Case &c : kCases)
    {
        const Position    position = Position::FromString(c.fen);
        const std::string fen      = position.ToString();
        // The castling field is the third of a FEN, and ToString writes all six.
        std::istringstream fields{fen};
        std::string        board, color_to_move, rights;
        fields >> board >> color_to_move >> rights;
        Check(rights == c.expect_rights,
              std::format("{}: rights read back as {}, expected {}", c.what, rights, c.expect_rights));
        int castling_moves = 0;
        for (const Move &move : position.GenerateLegalMoves())
        {
            castling_moves += move.type() == Move::Type::kCastling;
        }
        Check(castling_moves == c.castling_moves,
              std::format("{}: {} castling moves generated, expected {}", c.what, castling_moves, c.castling_moves));
    }
    // The board is what decides, so the same board with the rights spelled out and with them dropped parses
    // to the same position, hash included - a position the search can be handed either way.
    const Position from_bad_fen =
        Position::FromString("b2rk3/p2p1nr1/1p2n1p1/1Np2p1p/2P2P1P/1P1BP1P1/P4N2/2R1K1R1 w KQ - 1 16");
    const Position from_clean_fen =
        Position::FromString("b2rk3/p2p1nr1/1p2n1p1/1Np2p1p/2P2P1P/1P1BP1P1/P4N2/2R1K1R1 w - - 1 16");
    Check(from_bad_fen.hash_ == from_clean_fen.hash_, "the dropped rights leave the Zobrist hash of the real position");
    Check(from_bad_fen.ToString() == from_clean_fen.ToString(), "both FENs read back as the same position");
}

void MaterialDraws()
{
    struct Case
    {
        const char *fen_;
        bool        is_draw_;
        const char *what_;
    };

    constexpr Case kCases[] = {
        {"8/8/4k3/8/8/4K3/8/8 w - - 0 1", true, "king against king"},
        {"8/8/4k3/8/8/4KB2/8/8 w - - 0 1", true, "king and bishop against king"},
        {"8/8/4k3/8/8/4KN2/8/8 b - - 0 1", true, "king and knight against king"},
        {"8/8/4k3/8/8/4KR2/8/8 w - - 0 1", false, "king and rook against king"},
        {"8/8/4kp2/8/8/4K3/8/8 w - - 0 1", false, "king and pawn against king"},
        {"8/8/4k1b1/8/8/4KB2/8/8 w - - 0 1", true, "bishops on the same colour (f3, g6)"},
        {"8/8/4kb2/8/8/4KB2/8/8 w - - 0 1", false, "bishops on opposite colours (f3, f6)"},
        {"8/8/4kn2/8/8/4KN2/8/8 w - - 0 1", false, "a knight each"},
        {"8/8/4kn2/8/8/3BKN2/8/8 w - - 0 1", false, "five pieces"},
    };
    for (const Case &c : kCases)
    {
        Check(Position::FromString(c.fen_).IsDrawByMaterial() == c.is_draw_,
              std::format("material: {} is {}a draw", c.what_, c.is_draw_ ? "" : "not "));
    }
    const Position empty{};
    Check(empty.IsDrawByMaterial(), "material: an empty board (not a game) is reported and called a draw");
}

void MovesAndText()
{
    const Position start = Position::FromString("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    std::set<Move> ordered;
    for (const Move &move : start.GenerateLegalMoves())
    {
        ordered.insert(move);
    }
    Check(ordered.size() == 20, "Move::operator< orders the twenty opening moves as distinct");
    std::unordered_set<Move, Move> hashed(ordered.begin(), ordered.end());
    Check(hashed.size() == 20, "Move's hash keeps the twenty opening moves distinct");

    const Position promoting = Position::FromString("4k3/1P6/8/8/8/8/8/4K3 w - - 0 1");
    for (const char *name : {"b7b8q", "b7b8r", "b7b8b", "b7b8n"})
    {
        Check(FindMove(promoting, name) != Move::None(), std::format("promotion {} is generated and named", name));
    }
    for (const char *fen : {"r3k2r/pp1n1ppp/8/2pP4/8/8/PPP2PPP/R3K2R w KQkq c6 0 12", "8/8/8/8/8/8/8/K6k b - - 99 80"})
    {
        Check(Position::FromString(fen).ToString() == fen, std::format("FEN round trip: {}", fen));
    }
}

void HistoryAndTable()
{
    Check(HistoryBonus(-3) == 1 && HistoryBonus(5) == 25 && HistoryBonus(1000) == kMaxHistoryBonus,
          "HistoryBonus: depth squared, at least 1, clamped to the maximum");
    HistoryTable   history;
    const Position start = Position::FromString("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    const Move     e2e4  = FindMove(start, "e2e4");
    history.RecordGoodMove(kWhite, e2e4);
    history.RecordGoodMove(kWhite, e2e4);
    Check(history.MaxCount() == history.GetCount(kWhite, e2e4) && history.MaxCount() > 0,
          "HistoryTable::MaxCount is the largest count");
    history.Reset();
    Check(history.MaxCount() == 0 && history.GetCount(kWhite, e2e4) == 0, "HistoryTable::Reset zeroes every count");

    TranspositionTable table{0};
    Check(table.UsageStats().first == 0 && table.HashfullPermille() == 0, "a 0 MB table is clamped to 1 MB, empty");
    table.RecordTransposition(Transposition{start.hash_, e2e4, 17, 6, Transposition::NodeType::kPv});
    const auto found = table.FindTransposition(start.hash_);
    Check(found.has_value() && found->move_ == e2e4 && found->score() == 17 && found->depth() == 6 && found->age() == 0,
          "a recorded transposition is found with its move, score, depth and age");
    Check(table.UsageStats().first == 1, "UsageStats counts the one entry");
    table.Age();
    table.RecordTransposition(Transposition{start.hash_ ^ 1, e2e4, 3, 2, Transposition::NodeType::kPv});
    const auto aged = table.FindTransposition(start.hash_ ^ 1);
    Check(aged.has_value() && aged->age() == 1, "an entry recorded after Age carries the new generation");
}

/// @brief CRC-64/XZ, the checksum the shared NNUE net container is built on. The published check value
/// comes first, before anything else this suite does: every Reckless engine asserts that same value, so an
/// implementation that fails it will reject every net the other four accept. crc64.h also asserts it at
/// compile time, but a `static_assert` proves only the constant-evaluated path — this proves the one the
/// loader actually runs, over bytes it reads at run time.
void Crc64()
{
    const std::string_view check_input = "123456789";
    const std::uint64_t    checksum =
        crc64::Checksum(reinterpret_cast<const std::uint8_t *>(check_input.data()), check_input.size());
    Check(checksum == 0x995DC9BBDF1939FAULL,
          std::format("CRC-64/XZ check value: CRC of \"123456789\" is {:#018x}, must be 0x995dc9bbdf1939fa", checksum));
    Check(crc64::Checksum(nullptr, 0) == 0ULL, "CRC-64/XZ of an empty range is zero");

    // Fed in pieces, a running register must reach the same answer as one call — that is what lets a
    // stamping tool checksum a 12 MB payload without holding a second copy of it.
    std::uint64_t running = crc64::kInitialAndFinalXor;
    for (const char character : check_input)
    {
        const auto byte = static_cast<std::uint8_t>(character);
        running         = crc64::Update(running, &byte, 1);
    }
    Check((running ^ crc64::kInitialAndFinalXor) == checksum, "CRC-64/XZ: byte-at-a-time equals all at once");

    // A single changed byte must change the checksum; that is the whole job.
    const std::string_view altered = "123456780";
    Check(crc64::Checksum(reinterpret_cast<const std::uint8_t *>(altered.data()), altered.size()) != checksum,
          "CRC-64/XZ: one differing byte gives a different checksum");
}

/// @brief The RNNUE1 header this build writes: the exact 64-byte layout, and the architecture fields and
/// map digest taken from the compile-time constants. A header whose fields moved would be read wrongly by
/// the other four engines, so the offsets are pinned here as well as by the static_asserts in nnue.h.
void NetHeaderLayout()
{
    const nnue::NetHeader header = nnue::ExpectedNetHeader();
    Check(sizeof(nnue::NetHeader) == 64, "RNNUE1 header is 64 bytes");
    Check(std::memcmp(header.magic_, "RNNUE1\0\0", 8) == 0, "RNNUE1 header magic is \"RNNUE1\" and two zero bytes");
    // format_version is 2 with mirroring on, 1 with it off: the mirrored map changes what a bucket means,
    // not only how many there are, and the container's rule is that the map's contents are part of the
    // architecture. king_bucket_map_crc would catch a mix-up anyway, but the version says plainly that the
    // two are different formats rather than two nets of one format.
    Check(header.format_version_ == (nnue::kIsKingMirroringEnabled ? 2 : 1),
          std::format("RNNUE1 header: format_version {} with king mirroring {}", header.format_version_,
                      nnue::kIsKingMirroringEnabled ? "on" : "off"));
    Check(header.header_bytes_ == 64, "RNNUE1 header: header_bytes is 64");
    Check(header.features_per_bucket_ == nnue::kInputSize && header.king_buckets_ == nnue::kNumKingBuckets &&
              header.hidden_size_ == nnue::kHiddenSize && header.output_buckets_ == nnue::kNumOutputBuckets,
          "RNNUE1 header: the shape fields are this build's constants");
    Check(header.qa_ == nnue::kQA && header.qb_ == nnue::kQB && header.eval_scale_ == nnue::kScale,
          "RNNUE1 header: qa, qb and eval_scale are this build's constants");
    Check(header.activation_ == nnue::kActivationScreelu, "RNNUE1 header: activation is SCReLU");
    Check(((header.flags_ & nnue::kNetFlagKingMirroring) != 0) == nnue::kIsKingMirroringEnabled,
          "RNNUE1 header: the mirroring flag matches this build");
    Check((header.flags_ & static_cast<std::uint8_t>(~nnue::kNetFlagKingMirroring)) == 0,
          "RNNUE1 header: no flag bit but bit 0 is ever set");
    Check(header.payload_bytes_ == nnue::kNetPayloadBytes, "RNNUE1 header: payload_bytes is this build's payload size");

    // The map digest must be the CRC of this build's own 64 map entries as one uint8 each, computed here
    // from the map rather than taken from the same consteval function the header used.
    std::array<std::uint8_t, 64> entries{};
    for (std::size_t square = 0; square < entries.size(); ++square)
    {
        entries[square] = static_cast<std::uint8_t>(nnue::kKingBucketMap[square]);
    }
    Check(header.king_bucket_map_crc_ == crc64::Checksum(entries.data(), entries.size()),
          std::format("RNNUE1 header: king_bucket_map_crc {:#018x} is the CRC-64 of the 64 map entries",
                      header.king_bucket_map_crc_));

    // The two CRCs a stamping tool fills, over a payload short enough to write out here.
    const std::array<std::uint8_t, 4> payload{1, 2, 3, 4};
    nnue::NetHeader                   stamped = header;
    nnue::FinaliseNetHeader(stamped, payload.data(), payload.size());
    Check(stamped.payload_crc_ == crc64::Checksum(payload.data(), payload.size()),
          "RNNUE1 header: FinaliseNetHeader writes the payload's CRC");
    Check(stamped.header_crc_ ==
              crc64::Checksum(reinterpret_cast<const std::uint8_t *>(&stamped), nnue::kNetHeaderCrcBytes),
          "RNNUE1 header: FinaliseNetHeader writes the CRC of the header's first 56 bytes");
}

/// @brief The king-bucket map the **trainer** uses must be the same 32 numbers the engine does.
///
/// `king_bucket_map_crc` in the net header binds the map to the *engine build* that stamped a net. What it
/// cannot see is the trainer, which computes nothing from the engine's header — so a map that drifts
/// between tools/bullet/pawnstar.rs and src/nnue.h trains one network and runs another, and the digest
/// only reports it after the training run has been spent. That is the half of the risk nnue/README.md §2
/// called the honest limitation of the digest, and this closes it: read `KING_BUCKETS_HALF` out of the .rs
/// file and compare it, number for number, with `kKingBucketMapHalf`.
///
/// Reading the Rust source as text is crude, and that is the point — it needs no Rust toolchain, no build
/// step and no generated file to keep in step, and it fails loudly the moment somebody edits one array and
/// not the other. The engine's 64-entry map is *generated* from its 32, by the same arithmetic bullet uses
/// on the trainer's 32, so proving the two 32-entry maps equal proves the two 64-entry maps equal.
void TrainerKingBucketMap()
{
    const std::string path = "tools/bullet/pawnstar.rs";
    std::ifstream     trainer(path);
    if (!trainer)
    {
        Check(false, std::format("cannot open {} to compare its king-bucket map with the engine's (run this "
                                 "suite from the repository root)",
                                 path));
        return;
    }
    const std::string source((std::istreambuf_iterator<char>(trainer)), std::istreambuf_iterator<char>());

    const std::string declaration = "const KING_BUCKETS_HALF: [usize; 32] = [";
    const std::size_t opening     = source.find(declaration);
    if (opening == std::string::npos)
    {
        Check(false, std::format("{} has no `{}` — if the trainer's map was renamed or reshaped, this check and "
                                 "nnue.h's kKingBucketMapHalf must follow it",
                                 path, declaration));
        return;
    }
    const std::size_t closing = source.find("];", opening);
    if (closing == std::string::npos)
    {
        Check(false, std::format("{}: KING_BUCKETS_HALF is never closed with `];`", path));
        return;
    }

    std::vector<int>  entries;
    std::string       digits;
    const std::string body = source.substr(opening + declaration.size(), closing - opening - declaration.size());
    for (const char character : body + ",")
    {
        if (character >= '0' && character <= '9')
        {
            digits += character;
        }
        else if (!digits.empty())
        {
            entries.push_back(std::stoi(digits));
            digits.clear();
        }
    }

    if (entries.size() != nnue::kKingBucketMapHalf.size())
    {
        Check(false, std::format("{}: KING_BUCKETS_HALF has {} entries, the engine's kKingBucketMapHalf has {}", path,
                                 entries.size(), nnue::kKingBucketMapHalf.size()));
        return;
    }
    // One check for the whole array, naming every entry that differs. A per-entry check would print
    // thirty-two passing lines for one property and bury the one that mattered.
    std::string differences;
    for (std::size_t index = 0; index < entries.size(); ++index)
    {
        if (entries[index] != nnue::kKingBucketMapHalf[index])
        {
            differences += std::format("{}[{}] trainer {} vs engine {}", differences.empty() ? "" : ", ", index,
                                       entries[index], nnue::kKingBucketMapHalf[index]);
        }
    }
    Check(differences.empty(),
          differences.empty()
              ? std::format("{}: KING_BUCKETS_HALF matches the engine's kKingBucketMapHalf, all {} entries", path,
                            entries.size())
              : std::format("{}: KING_BUCKETS_HALF differs from the engine's kKingBucketMapHalf at {} — the "
                            "trainer would train one network and the engine run another",
                            path, differences));

    // And the generated 64-entry map really is bullet's expansion of those 32.
    bool expansion_matches = true;
    for (std::size_t square = 0; square < 64; ++square)
    {
        const std::size_t file          = square % 8;
        const std::size_t mirrored_file = file < 4 ? file : 7 - file;
        expansion_matches &=
            nnue::ExpandMirroredKingBucketMap()[square] == nnue::kKingBucketMapHalf[(square / 8) * 4 + mirrored_file];
    }
    Check(expansion_matches, "the engine's 64-entry mirrored map is bullet's expansion of the 32-entry one");
}

} // namespace

int main()
{
    Crc64();
    NetHeaderLayout();
    TrainerKingBucketMap();
    BitboardBasics();
    GeneratedTables();
    CastlingText();
    CastlingRightsAgainstTheBoard();
    MaterialDraws();
    MovesAndText();
    HistoryAndTable();
    return test_report::Summary(
        "PRIMITIVES: CRC-64, the net header, bitboards, generated tables, castling, material, moves, tables");
}

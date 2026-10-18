/// @file primitives_test.cpp Unit tests of the engine's building blocks that the search never calls at run time.
///
/// Some of the engine's small pieces run only inside the compiler (the constexpr generators behind the attack tables,
/// evaluated at compile time), only from diagnostics (Bitboard::ToString, the table's UsageStats) or only on inputs
/// a game rarely meets (insufficient material with no pieces at all, castling text with stray characters). Each is
/// checked here against what it is for: the generators, run at run time, rebuild exactly the tables the engine
/// uses; the text forms read back; the material rules call the classic draws and only those.
///
/// Usage:  test_primitives   (no arguments, no resources)

// position.h first: it brings in bitboard.h, castling_rights.h, generated_data.h and move.h in the order they need.
#include "position.h"

#include "history_table.h"
#include "test_report.h"
#include "transposition_table.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <format>
#include <iostream>
#include <set>
#include <string>
#include <unordered_set>

namespace
{
void Check(bool condition, const std::string &message)
{
    test_report::Check(condition, message);
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
} // namespace

int main()
{
    BitboardBasics();
    GeneratedTables();
    CastlingText();
    MaterialDraws();
    MovesAndText();
    HistoryAndTable();
    return test_report::Summary("PRIMITIVES: bitboards, generated tables, castling, material, moves, tables");
}

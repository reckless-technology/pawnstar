/// @file opening_book_test.cpp Unit tests for the opening book, focused on the '?' (questionable) move suffix.
///
/// A questionable move (written "f2f4?" in the book) must be recognised and replayed so the rest of the line
/// parses, but must never be returned by GetMove. The move that follows it in the line must still be recorded
/// and selectable. These tests build a tiny book in a temp file and assert exactly that.

#include "move.h"
#include "opening_book.h"
#include "position.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <ranges>
#include <string>

namespace
{
constexpr const char *kStartFen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

int g_failures = 0;

void Check(bool condition, const std::string &message)
{
    if (!condition)
    {
        std::cout << "FAIL: " << message << "\n";
        ++g_failures;
    }
}

/// @brief Advance a position by a coordinate move (e.g. "e2e4"), matching the parser's lookup.
Position Play(const Position &position, const std::string &coord)
{
    auto moves = position.GenerateLegalMoves();
    auto i     = std::ranges::find_if(moves, [&](const Move &m) { return m.ToString() == coord; });
    if (i == moves.end())
    {
        std::cout << "FAIL: test bug - illegal move " << coord << "\n";
        ++g_failures;
        return position;
    }
    return position.MakeMove(*i);
}

/// @brief Hash of the position reached by replaying a sequence of coordinate moves from the start.
zobrist_t HashAfter(std::initializer_list<std::string> moves)
{
    Position p = Position::FromString(kStartFen);
    for (const auto &m : moves)
    {
        p = Play(p, m);
    }
    return p.Hash();
}

std::string WriteTempBook(const std::string &contents)
{
    std::filesystem::path path = std::filesystem::temp_directory_path() / "pawnstar_book_test.book";
    std::ofstream         out{path};
    out << contents;
    out.close();
    return path.string();
}
} // namespace

int main()
{
    // Line: 1.e4 e5 2.f4? (King's Gambit, marked questionable) exf4 3.Nf3
    const std::string book_text = "e2e4 e7e5 f2f4? e5f4 g1f3\n";
    const std::string book_path = WriteTempBook(book_text);

    OpeningBook book;
    Check(book.Initialize(book_path), "book with a '?' move should parse successfully");

    // Playable moves along the line are selectable.
    Check(book.GetMove(HashAfter({})) != Move::None(), "e2e4 should be a playable book move from the start");
    Check(book.GetMove(HashAfter({"e2e4"})) != Move::None(), "e7e5 should be playable after e2e4");

    // The questionable move f2f4 must never be returned, even over many draws.
    const zobrist_t after_e4_e5 = HashAfter({"e2e4", "e7e5"});
    bool            ever_played = false;
    for (int n = 0; n < 1000; ++n)
    {
        if (book.GetMove(after_e4_e5) != Move::None())
        {
            ever_played = true;
            break;
        }
    }
    Check(!ever_played, "questionable move f2f4? must never be returned by GetMove");

    // The line continued past the '?' move: the reply exf4 is recorded and selectable.
    const Move reply = book.GetMove(HashAfter({"e2e4", "e7e5", "f2f4"}));
    Check(reply != Move::None() && reply.ToString() == "e5f4", "exf4 should be playable after the questionable f2f4");

    std::remove(book_path.c_str());

    if (g_failures == 0)
    {
        std::cout << "OPENING BOOK PASS\n";
        return 0;
    }
    std::cout << "FAIL: " << g_failures << " opening-book assertion(s) failed\n";
    return 1;
}

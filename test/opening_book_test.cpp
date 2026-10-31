/// @file opening_book_test.cpp Unit tests for the opening book.
///
/// A questionable move (written "f2f4?" in the book) must be recognised and replayed so the rest of the line
/// parses, but must never be returned by GetMove. The move that follows it in the line must still be recorded
/// and selectable. These tests build a tiny book in a temp file and assert exactly that. Then the loading rules
/// (a missing file, the in-memory form the engine's embedded copy uses, move numbers, comments, an illegal move),
/// the weighting of GetMove by how many lines pass through a position, the bookmoves display and Free, and the
/// shipped pawnstar.book from its file and from memory (run from the repository root, as ctest does).

#include "move.h"
#include "opening_book.h"
#include "position.h"
#include "test_report.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <ranges>
#include <sstream>
#include <string>

namespace
{
constexpr const char *kStartFen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

void Check(bool condition, const std::string &message)
{
    test_report::Check(condition, message);
}

/// @brief Advance a position by a coordinate move (e.g. "e2e4"), matching the parser's lookup.
Position Play(const Position &position, const std::string &coord)
{
    auto moves = position.GenerateLegalMoves();
    auto i     = std::ranges::find_if(moves, [&](const Move &m) { return m.ToString() == coord; });
    if (i == moves.end())
    {
        test_report::Check(false, "opening book: test bug - illegal move " + coord);
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
    return p.hash_;
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

    // Loading: a missing file is refused; the in-memory form (the engine's embedded fallback) reads the same text,
    // skipping move numbers, blank lines and comments, and an illegal move refuses the whole book.
    OpeningBook missing;
    Check(!missing.Initialize("no/such/directory/pawnstar.book"), "a missing book file is refused");
    Check(missing.GetMove(HashAfter({})) == Move::None(), "an empty book has no move for the start");

    const std::string annotated = "# a comment line\n"
                                  "\n"
                                  "1. d2d4 d7d5 2. c2c4 # the Queen's Gambit, with a trailing comment\n"
                                  "1. d2d4 g8f6\n";
    OpeningBook       from_memory;
    Check(from_memory.InitializeFromMemory(annotated.data(), annotated.size()),
          "a book read from memory, with move numbers, blank lines and comments");
    const Move reply_to_d4 = from_memory.GetMove(HashAfter({"d2d4"}));
    Check(reply_to_d4.ToString() == "d7d5" || reply_to_d4.ToString() == "g8f6", "both replies to d2d4 are in the book");
    Check(from_memory.GetMove(HashAfter({"d2d4", "d7d5", "c2c4"})) == Move::None(),
          "the line stops where its comment starts");
    Check(from_memory.GetMove(HashAfter({"e2e4"})) == Move::None(), "a position outside the book has no move");

    const std::string illegal = "e2e4 e7e5\ne2e4 e2e4\n";
    OpeningBook       broken;
    Check(!broken.InitializeFromMemory(illegal.data(), illegal.size()), "a book with an illegal move is refused");

    // Weighting: a move's chance is its share of the lines through the position (three e2e4 lines, one d2d4).
    const std::string weighted = "e2e4\ne2e4 e7e5\ne2e4 c7c5\nd2d4\n";
    OpeningBook       weighted_book;
    Check(weighted_book.InitializeFromMemory(weighted.data(), weighted.size()), "a weighted book loads");
    int e4_count = 0, d4_count = 0;
    for (int draw = 0; draw < 4000; ++draw)
    {
        const std::string move = weighted_book.GetMove(HashAfter({})).ToString();
        e4_count += move == "e2e4";
        d4_count += move == "d2d4";
    }
    Check(e4_count + d4_count == 4000 && d4_count > 600 && e4_count > 2 * d4_count,
          "GetMove draws e2e4 about three times as often as d2d4");

    // Display: the moves and their counts, questionable ones flagged; nothing for a position outside the book.
    std::ostringstream captured;
    std::streambuf    *saved = std::cout.rdbuf(captured.rdbuf());
    weighted_book.DisplayAvailableMoves(Position::FromString(kStartFen));
    weighted_book.DisplayAvailableMoves(Play(Position::FromString(kStartFen), "a2a3"));
    std::cout.rdbuf(saved);
    Check(captured.str().starts_with("MOVE   COUNT\n") && captured.str().find("e2e4       3") != std::string::npos &&
              captured.str().find("d2d4       1") != std::string::npos,
          "DisplayAvailableMoves: each move with its count, and nothing outside the book");

    weighted_book.Free();
    Check(weighted_book.GetMove(HashAfter({})) == Move::None(), "Free empties the book");

    // The shipped book loads from its file and, as the engine's embedded copy is loaded, from memory.
    std::ifstream shipped_file{"pawnstar.book", std::ios::binary};
    if (shipped_file)
    {
        const std::string shipped{std::istreambuf_iterator<char>{shipped_file}, std::istreambuf_iterator<char>{}};
        OpeningBook       shipped_book;
        OpeningBook       shipped_from_memory;
        Check(shipped_book.Initialize("pawnstar.book"), "the shipped pawnstar.book loads from its file");
        Check(shipped_from_memory.InitializeFromMemory(shipped.data(), shipped.size()),
              "the shipped pawnstar.book loads from memory, as the embedded copy does");
        Check(shipped_book.GetMove(HashAfter({})) != Move::None() &&
                  shipped_from_memory.GetMove(HashAfter({})) != Move::None(),
              "the shipped book has moves for the start position");

        // The book's own provenance header, which is the reason this book exists at all. The book it replaced
        // was derived from TSCP's book.txt and was asserted to be public domain in about twenty places in this
        // repository; TSCP is Copyright 1997 Tom Kerrigan under an informal non-commercial permission, which
        // GPLv3 cannot take, so the claim was false and the material had to go. This book is built from the
        // Lichess open database, whose CC0 1.0 dedication makes the same claim true. Pinned here because a
        // false provenance claim is exactly the kind of defect that survives a rewrite of the prose around it:
        // the file itself has to say where it came from, and must not say the old thing.
        Check(shipped.find("Lichess open database") != std::string::npos &&
                  shipped.find("https://database.lichess.org/") != std::string::npos &&
                  shipped.find("CC0 1.0") != std::string::npos &&
                  shipped.find("https://creativecommons.org/publicdomain/zero/1.0/") != std::string::npos,
              "the shipped book names its Lichess source and the CC0 dedication");
        Check(shipped.find("TSCP") == std::string::npos && shipped.find("Kerrigan") == std::string::npos,
              "the shipped book makes no claim about TSCP");
        // CC0 waives attribution, so the header must not imply an obligation that does not exist - claiming a
        // duty nobody owes is the mirror image of claiming a permission nobody gave.
        Check(shipped.find("asks for no attribution") != std::string::npos,
              "the shipped book says the Lichess credit is a courtesy, not an obligation");
        // Every line below the header parses, which the Initialize above already proves - but ParseLineOfPlay
        // stops at the first bad line and reports failure, so count the lines too: a book truncated to its
        // header would load perfectly and play nothing, and only the start-position check above would catch
        // it. This pins the width that was the other half of the point: 385 lines replayed each opening about
        // 13 times in a long match, which inflated every confidence interval the matches produced.
        std::size_t        shipped_lines = 0;
        std::istringstream shipped_text{shipped};

        for (std::string line; std::getline(shipped_text, line);)
        {
            if (!line.empty() && line.front() != '#')
            {
                ++shipped_lines;
            }
        }

        Check(shipped_lines > 385, "the shipped book is wider than the 385-line book it replaced");

        // The book has been through tools/filter_book: a move scoring far enough below its position's best
        // carries a '?', which is replayed but never chosen. Pinned for the same reason as the line count
        // above - an unfiltered book loads perfectly and plays every move the database offered, including the
        // ones this engine evaluates as losing a pawn, and nothing else here would notice. Regenerating the
        // book means make_book THEN filter_book; this is what catches forgetting the second step.
        Check(shipped.find('?') != std::string::npos, "the shipped book carries questionable-move marks");

        // Where every move a position had is marked, filter_book adds the engine's own choice there, unmarked.
        // Those are the only moves in the file that are not the database's, so the file has to say so: the
        // header's CC0 claim is about the database's lines, and a reader must not take it to cover these.
        Check(shipped.find("NOT THE DATABASE'S") != std::string::npos,
              "the shipped book distinguishes the engine's own moves from the database's");

        // And the invariant that addition exists for: no position the book reaches is left with nothing to
        // play. A '?' move is not selectable, so a position whose every move is marked would return
        // Move::None and drop the engine out of book - silently, and exactly where an opponent had just
        // played something unusual. Replay every line and require a selectable move at every position the
        // line continues from; the last position of a line is a leaf and is allowed to have none.
        std::istringstream lines_text{shipped};
        std::size_t        dead_ends = 0;

        for (std::string line; std::getline(lines_text, line);)
        {
            if (line.empty() || line.front() == '#')
            {
                continue;
            }
            Position                 position = Position::FromString(kStartFen);
            std::vector<std::string> moves;
            std::istringstream       tokens{line};

            for (std::string token; tokens >> token;)
            {
                if (token.front() == '#')
                {
                    break;
                }
                moves.push_back(token.back() == '?' ? token.substr(0, token.size() - 1) : token);
            }
            for (std::size_t i = 0; i + 1 < moves.size(); ++i) // every position but the line's last
            {
                position = Play(position, moves[i]);
                if (shipped_book.GetMove(position.hash_) == Move::None())
                {
                    ++dead_ends;
                }
            }
        }

        Check(dead_ends == 0, "every position the shipped book continues from has a move it may actually play");
    }

    return test_report::Summary("OPENING BOOK: loading, weighting, display and '?' (questionable) moves");
}

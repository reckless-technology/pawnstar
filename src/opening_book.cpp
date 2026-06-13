#include <cctype>
#include <chrono>
#include <format>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "debug_hashtable.h"
#include "game.h"
#include "opening_book.h"
#include "position.h"
#include "transposition_table.h"

using std::string;

/// @brief Constructor. Seeds the PRNG.
OpeningBook::OpeningBook() : prng_{static_cast<uint32_t>(std::chrono::system_clock::now().time_since_epoch().count())}
{
}

/// @brief Parse the opening book file and create the opening book from it.
/// @param filename Filename of book file.
/// @return true on success.
bool OpeningBook::Initialize(std::string_view filename)
{
    std::ifstream file{string(filename)};
    if (!file)
    {
        return false;
    }
    const bool is_ok = InitializeFromStream(file);
    file.close();
    return is_ok;
}

/// @brief Pseudo randomly select from available book moves
/// @param hash the position hash
/// @return Move selected from book, or Move::None if no move found
Move OpeningBook::GetMove(zobrist_t hash)
{
    if (book_.count(hash))
    {
        const auto &moves = book_[hash];
        return moves[prng_() % moves.size()];
    }
    return Move::None();
}

/// @brief Free the opening book
void OpeningBook::Free()
{
    book_.clear();
}

/// @brief Print available book moves for a position
/// @param position Position to consider for book moves
void OpeningBook::DisplayAvailableMoves(const Position &position)
{
    if (!book_.count(position.Hash()))
    {
        return;
    }
    // Create an associative container, indexed by move, of how many times that move appears for this position.
    std::unordered_map<Move, int, Move> move_counts;
    for (const auto &move : book_[position.Hash()])
    {
        ++move_counts[move];
    }
    std::cout << "MOVE   COUNT\n";
    for (const auto &[move, freq] : move_counts)
    {
        std::cout << std::format("{:<8} {:3}\n", move.ToString(), freq);
    }
}

/// @brief Parse a single line of play and add it to the book. '#' denotes a comment and the rest of the line
/// will be ignored. Move numbers are ignored.
/// @param line The line of play
/// @return true on success
bool OpeningBook::ParseLineOfPlay(std::string_view line)
{
    Position          position{Position::FromString("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1")};
    std::stringstream ss;
    ss << line;
    while (ss)
    {
        string move_string;
        ss >> move_string;
        if (move_string.length() == 0 || isdigit(move_string[0]))
        {
            continue; // Ignore move numbers or blanks.
        }
        if (move_string[0] == '#')
        {
            return true; // Done with this line.
        }
        const zobrist_t hash  = position.Hash();
        auto            moves = position.GenerateLegalMoves();
        auto            i =
            std::ranges::find_if(moves, [&move_string](const Move &move) { return move.ToString() == move_string; });
        if (i != moves.end())
        {
            book_[hash].push_back(*i);
            position = position.MakeMove(*i);
        }
        else
        {
            std::cout << std::format("Error found in book line {} move {}.\n", line, move_string);
            return false;
        }
    }
    return true;
}

/// @brief Parse a multiline stringstream containing opening book lines of play and create the opening book from it.
/// Each line is a sequence of moves in coordinate notation (e.g. "e2e4 e7e5 g1f3").
/// @param ss stringstream to be parsed, with one line of play per new line.
/// @return true on success
bool OpeningBook::InitializeFromStream(std::istream &ss)
{
    while (ss)
    {
        string line_of_play;
        std::getline(ss, line_of_play);
        if (!ParseLineOfPlay(line_of_play))
        {
            return false;
        }
    }
    return true;
}
#pragma once
/// @file opening_book.h Opening book storage and move selection.
#include <cstdint>
#include <map>
#include <random>
#include <sstream>
#include <string_view>
#include <vector>

#include "generated_data.h"
#include "move.h"

class Position;

/// @brief The opening book is stored as a map, indexed by the Zobrist hash of the position, containing a vector of
/// Moves which are available from that position. The same move may be in the moves vector multiple times if it appears
/// in several lines of play, i.e. the frequency of occurence of a move in the list is proportional to the liklihood
/// that we want to play it.
///
/// A move written with a trailing '?' suffix in the book file (e.g. "f2f4?") is *questionable*: it is a legal move
/// that the engine recognises and replays so that the rest of the line is still parsed, but which the engine will
/// never itself select. This lets a line teach a response to a dubious move (be ready for it as the defender)
/// without ever initiating it. Questionable moves are kept in a separate map for display only; GetMove never
/// returns them.
class OpeningBook
{
  public:
    /// @brief Construct an empty opening book.
    OpeningBook();

    /// @brief Load the opening book from a file (see definition for details).
    bool Initialize(std::string_view filename);

    /// @brief Load the opening book from an in-memory buffer (the embedded fallback; see definition).
    /// @param data Pointer to the book text. @param size Number of bytes (the text is not null-terminated).
    /// @return true on success.
    bool InitializeFromMemory(const char *data, std::size_t size);

    /// @brief Print the book moves available from a position (see definition for details).
    void DisplayAvailableMoves(const Position &position);

    /// @brief Pick a book move for a position, weighted by frequency (see definition for details).
    Move GetMove(zobrist_t hash);

    /// @brief Release the memory held by the book.
    void Free();

  private:
    /// @brief Load book entries from an input stream (see definition for details).
    bool InitializeFromStream(std::istream &ss);
    /// @brief Parse a single line of play and add its moves to the book (see definition for details).
    bool                                   ParseLineOfPlay(std::string_view line);
    std::map<zobrist_t, std::vector<Move>> book_;         ///< Map of position hash to playable moves (with repeats).
    std::map<zobrist_t, std::vector<Move>> questionable_; ///< Recognised-but-never-played '?' moves (display only).
    std::mt19937                           prng_;         ///< PRNG used to randomly select among book moves.
};

// ---- Definitions (moved from opening_book.cpp for header-only build) ----
#include "debug_hashtable.h"
#include "position.h"
#include "transposition_table.h"
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
using std::string;

/// @brief Constructor. Seeds the PRNG.
inline OpeningBook::OpeningBook()
    : prng_{static_cast<uint32_t>(std::chrono::system_clock::now().time_since_epoch().count())}
{
}

inline bool OpeningBook::Initialize(std::string_view filename)
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

/// @brief Parse the opening book from an in-memory buffer (the copy embedded in the binary, used when the
/// on-disk book cannot be loaded). The buffer is the raw book text and is not null-terminated.
/// @param data Pointer to the book text. @param size Number of bytes.
/// @return true on success.
inline bool OpeningBook::InitializeFromMemory(const char *data, std::size_t size)
{
    std::istringstream ss{std::string(data, size)};
    return InitializeFromStream(ss);
}

/// @brief Pseudo randomly select from available book moves
/// @param hash the position hash
/// @return Move selected from book, or Move::None if no move found
inline Move OpeningBook::GetMove(zobrist_t hash)
{
    if (book_.count(hash))
    {
        const auto &moves = book_[hash];
        return moves[prng_() % moves.size()];
    }
    return Move::None();
}

/// @brief Free the opening book
inline void OpeningBook::Free()
{
    book_.clear();
    questionable_.clear();
}

/// @brief Print available book moves for a position
/// @param position Position to consider for book moves
inline void OpeningBook::DisplayAvailableMoves(const Position &position)
{
    if (!book_.count(position.hash_))
    {
        return;
    }
    // Create an associative container, indexed by move, of how many times that move appears for this position.
    std::unordered_map<Move, int, Move> move_counts;
    for (const auto &move : book_[position.hash_])
    {
        ++move_counts[move];
    }
    std::cout << "MOVE   COUNT\n";
    for (const auto &[move, freq] : move_counts)
    {
        std::cout << std::format("{:<8} {:3}\n", move.ToString(), freq);
    }
    // Questionable moves are recognised but never played; show them flagged with '?' for diagnostics.
    if (questionable_.count(position.hash_))
    {
        std::unordered_map<Move, int, Move> q_counts;
        for (const auto &move : questionable_[position.hash_])
        {
            ++q_counts[move];
        }
        for (const auto &[move, freq] : q_counts)
        {
            std::cout << std::format("{:<8} {:3}  (questionable, never played)\n", move.ToString() + "?", freq);
        }
    }
}

/// @brief Parse a single line of play and add it to the book. '#' denotes a comment and the rest of the line
/// will be ignored. Move numbers are ignored. A move with a trailing '?' (e.g. "f2f4?") is *questionable*: it is
/// replayed so the rest of the line still parses, but recorded in questionable_ rather than book_, so GetMove
/// never returns it.
/// @param line The line of play
/// @return true on success
inline bool OpeningBook::ParseLineOfPlay(std::string_view line)
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
        // A trailing '?' marks a questionable move: recognise and replay it, but never select it.
        const bool questionable = move_string.back() == '?';
        if (questionable)
        {
            move_string.pop_back();
        }
        const zobrist_t hash  = position.hash_;
        auto            moves = position.GenerateLegalMoves();
        auto            i =
            std::ranges::find_if(moves, [&move_string](const Move &move) { return move.ToString() == move_string; });
        if (i != moves.end())
        {
            (questionable ? questionable_ : book_)[hash].push_back(*i);
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
inline bool OpeningBook::InitializeFromStream(std::istream &ss)
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
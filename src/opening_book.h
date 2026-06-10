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
class OpeningBook
{
  public:
    /// @brief Construct an empty opening book.
    OpeningBook();
    /// @brief Load the opening book from a file (see definition for details).
    bool Initialize(std::string_view filename);
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
    bool ParseLineOfPlay(std::string_view line);
    /// @brief Parse PGN game text and add its moves to the book (see definition for details).
    bool                                   ParsePgn(std::istream &is);
    std::map<zobrist_t, std::vector<Move>> book_; ///< Map of position hash to available moves (with repeats).
    std::mt19937                           prng_; ///< PRNG used to randomly select among book moves.
};

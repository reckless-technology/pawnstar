#pragma once
#include <cstdint>
#include <map>
#include <random>
#include <sstream>
#include <string_view>
#include <vector>

#include "move.h"

class Position;

/// @brief The opening book is stored as a map, indexed by the Zobrist hash of the position, containing a vector of
/// Moves which are available from that position. The same move may be in the moves vector multiple times if it appears
/// in several lines of play, i.e. the frequency of occurence of a move in the list is proportional to the liklihood
/// that we want to play it.
class OpeningBook
{
  public:
    OpeningBook();
    bool Initialize(std::string_view filename);
    void DisplayAvailableMoves(const Position &position);
    Move GetMove(uint64_t hash);
    void Free();

  private:
    bool                                  InitializeFromStream(std::istream &ss);
    bool                                  ParseLineOfPlay(std::string_view line);
    std::map<uint64_t, std::vector<Move>> book_;
    std::mt19937                          prng_;
};

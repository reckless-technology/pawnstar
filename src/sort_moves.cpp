
#include <algorithm>
#include <array>
#include <cstring>

#include "debug_hashtable.h"
#include "move.h"
#include "search.h"
#include "sort_moves.h"
#include "static_exchange_evaluation.h"

/// @brief Array of good move counts indexed by move from and to squares (12 bits), for a single ply.
typedef std::array<uint32_t, 64 * 64> PlyCounts;

/// @brief History table: good move counts arrays for each ply of search.
static std::array<PlyCounts, MAX_PLY> history_table;

/// @brief Record a good move in the history table.
/// @param ply Current search ply.
/// @param move The move to record.
void RecordGoodMove(int ply, Move move)
{
    ++history_table[ply][move.from_to_mask()];
}

/// @brief Clear the history table.
/// This is typically done before starting each root node search.
void ResetHistoryTable()
{
    std::ranges::for_each(history_table, [](PlyCounts &x) { x.fill(0); });
}

/// @brief Raw material values for MVV/LVA provisional scoring of moves.
constexpr std::array<int, 7> piece_values{0, 100, 300, 300, 500, 900, 10000};

/// @brief Assign provisional scores to each move and then sort them best first.
/// Use MVV/LVA plus history table counts to score moves. This is primitive, but way faster than static exchange
/// evaluation, and seems to work pretty well in practice.
/// @param game Game being searched
/// @param moves List of legal moves to evaluate.
/// @param ply Current search ply.
void ScoreAndSortMoves(Game &game, MoveList &moves, int ply)
{
    const PlyCounts &counts   = history_table[ply];
    const Position  &position = game.CurrentPosition();
    for (Move &move : moves)
    {
        move.AssignScore(piece_values[position.PieceAt(move.to())] * 10000 -
                         piece_values[position.PieceAt(move.from())] * 1000 + counts[move.from_to_mask()]);
    }
    SortMoves<false>(moves);
}

/// @brief Get the maximum count in the history move table.
/// @return Count of the most popular history table move.
uint32_t MaxHistoryCount()
{
    uint32_t result = 0;
    for (const auto &ply : history_table)
    {
        const auto ply_max = *std::ranges::max_element(ply);
        result             = std::max(result, ply_max);
    }
    return result;
}
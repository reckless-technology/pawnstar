/// @file functions to search for the best move.
#pragma once

#include "constants.h"
#include "game.h"
#include "move.h"
#include "stack_list.h"

/// @brief A variation (typically principal variation) is a line of play or series of moves.
typedef StackList<Move, MAX_PLY> Variation;

Move                 SearchRootNode(Game &game);
int                  Search(Game &game, int depth, int ply, int alpha, int beta, Variation &parent_pv);
std::pair<int, bool> SearchSingleMove(Game &game, int depth, int ply, int alpha, int beta, Move move, Variation &pv,
                                      int move_index);
int                  SearchQuiescent(Game &game, int depth, int ply, int alpha, int beta);

/// @brief When the PV changes we need to copy the new PV up the tree recursively.
/// @param dst Destination PV.
/// @param src Source PV.
/// @param best_move New best move at this position.
constexpr void CopyVariation(Variation &dst, const Variation &src, const Move &best_move)
{
    dst.clear();
    dst.push_back(best_move);
    for (const auto &move : src)
    {
        dst.push_back(move);
    }
}

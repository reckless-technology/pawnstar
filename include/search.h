/// @file functions to search for the best move.
#pragma once

#include "constants.h"
#include "game.h"
#include "move.h"
#include "search_state.h"
#include "stack_list.h"

/// @brief A variation (typically principal variation) is a line of play or series of moves.
using Variation = StackList<Move, kMaxPly>;

struct SingleMoveResult
{
    int  score;       ///< Alpha-beta score for the move.
    bool is_checking; ///< True if the move gives check.
};

Move             SearchRootNode(Game &game);
int              Search(SearchState &state, int depth, int ply, int alpha, int beta, Variation &parent_pv);
SingleMoveResult SearchSingleMove(SearchState &state, int depth, int ply, int alpha, int beta, Move move,
                                  Variation &pv, int move_index);
int              SearchQuiescent(SearchState &state, int depth, int ply, int alpha, int beta);

/// @brief When the PV changes we need to copy the new PV up the tree recursively.
/// @param dst Destination PV.
/// @param src Source PV.
/// @param best_move New best move at this position.
constexpr void CopyVariation(Variation &dst, const Variation &src, const Move &best_move)
{
    dst.clear();
    dst.push_back(best_move);
    std::copy(src.begin(), src.end(), std::back_inserter(dst));
}

/// @file search.h functions to search for the best move.
#pragma once

#include "constants.h"
#include "game.h"
#include "move.h"
#include "search_state.h"
#include "stack_list.h"

/// @brief A variation (typically principal variation) is a line of play or series of moves.
using Variation = StackList<Move, kMaxPly>;

/// @brief Result of searching a single move: its score and whether it gives check.
struct SingleMoveResult
{
    int  score;       ///< Alpha-beta score for the move.
    bool is_checking; ///< True if the move gives check.
};

/// @brief Search the root node via iterative deepening and return the best move (see definition for details).
Move SearchRootNode(Game &game);
/// @brief Fail-soft negamax alpha-beta search (see definition for details).
int Search(SearchState &state, int depth, int ply, int alpha, int beta, Variation &parent_pv);
/// @brief Search a single move from the current node (see definition for details).
SingleMoveResult SearchSingleMove(SearchState &state, int depth, int ply, int alpha, int beta, Move move, Variation &pv,
                                  int move_index);
/// @brief Quiescence search over captures (see definition for details).
int SearchQuiescent(SearchState &state, int depth, int ply, int alpha, int beta);

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

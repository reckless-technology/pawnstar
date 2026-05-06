/// @file functions to search for the best move.
#pragma once

#include "constants.h"
#include "game.h"
#include "move.h"
#include "stack_list.h"

/// @brief A variation (typically principal variation) is a line of play or series of moves.
typedef StackList<Move, kMaxPly> Variation;

struct SingleMoveResult
{
    int  score;
    bool is_checking;
};

Move             SearchRootNode(Game &game);
int              Search(Game &game, int depth, int ply, int alpha, int beta, Variation &parent_pv);
SingleMoveResult SearchSingleMove(Game &game, int depth, int ply, int alpha, int beta, Move move, Variation &pv,
                                  int move_index);
int              SearchQuiescent(Game &game, int depth, int ply, int alpha, int beta);

/// @brief When the kPv changes we need to copy the new kPv up the tree recursively.
/// @param dst Destination kPv.
/// @param src Source kPv.
/// @param best_move New best move at this position.
constexpr void CopyVariation(Variation &dst, const Variation &src, const Move &best_move)
{
    dst.clear();
    dst.push_back(best_move);
    std::copy(src.begin(), src.end(), std::back_inserter(dst));
}

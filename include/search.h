#pragma once

#include "game.h"
#include "move.h"

typedef std::vector<std::string> Variation;

Move SearchRootNode(Game &game);
int  Search(Game &game, int depth, int ply, int alpha, int beta, Variation &parent_pv);
int  SearchSingleMove(Game &game, int depth, int ply, int alpha, int beta, Move move, Variation &pv, int move_index);
int  SearchQuiescent(Game &game, int depth, int ply, int alpha, int beta);

constexpr void CopyVariation(Variation &dst, const Variation &src, const std::string &best_move)
{
    dst.clear();
    dst.push_back(best_move);
    for (const auto &move : src)
    {
        dst.push_back(move);
    }
}

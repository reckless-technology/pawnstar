#pragma once

#include "game.h"
#include "move.h"

int  Search(Game &game, int depth, int ply, int alpha, int beta, Variation &parent_pv);
int  SearchSingleMove(Game &game, int depth, int ply, int alpha, int beta, Move move, Variation &pv, int move_index);
int  SearchQuiescent(Game &game, int depth, int ply, int alpha, int beta);
Move SearchRootNode(Game &game);

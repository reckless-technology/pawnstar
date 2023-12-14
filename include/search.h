#pragma once

#include "move.h"
#include "game.h"

int     Search          (Game& game, int depth, int ply, int alpha, int beta, Variation& parent_pv);
int     SearchQuiescent (Game& game, int depth, int ply, int alpha, int beta);
int     SearchSingleMove(Game& game, int depth, int ply, int alpha, int beta, Move move, Variation& pv, int move_index);
Move    SearchRootNode  (Game& game);

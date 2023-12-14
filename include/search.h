#pragma once

#include "move.h"

struct Position;

int     Search          (const Position& position, int depth, int ply, int alpha, int beta, volatile bool& cancel, Variation& parent_pv);
int     SearchQuiescent (const Position& position, int depth, int ply, int alpha, int beta, volatile bool& cancel);
int     SearchSingleMove(const Position& position, int depth, int ply, int alpha, int beta, volatile bool& cancel, Move move, Variation& pv, int move_index);
Move    SearchRootNode  (const Position& position);
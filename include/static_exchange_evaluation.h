#pragma once
#include "move.h"

class Position;

int EvaluateStaticExchange(const Position &src_position, Move move, int &is_checking);
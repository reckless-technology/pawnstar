#pragma once
#include "move.h"

class Position;

std::pair<int, bool> EvaluateStaticExchange(const Position &src_position, Move move);
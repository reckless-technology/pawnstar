#pragma once
/// @file static_exchange_evaluation.h Static exchange evaluation (SEE) for capture move ordering and pruning.
#include "move.h"

class Position;

/// @brief Compute the static exchange evaluation of a capture, resolving the full capture sequence
/// (see definition for details).
std::pair<int, bool> EvaluateStaticExchange(const Position &src_position, Move move);
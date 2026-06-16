#pragma once
/// @file evaluation.h Position evaluation.
///
/// Pawnstar evaluates exclusively with its NNUE network (see nnue.h / nnue.cpp). The handcrafted
/// evaluation (material, piece-square tables, pawn structure, king safety, mobility) was removed once
/// NNUE became the only evaluator — a loaded net is required to search (main.cpp loads one at startup
/// and exits if it cannot).

class SearchState;

/// @brief Evaluate the current position from the side-to-move's perspective using the NNUE network.
/// @param state Per-thread search state providing the current position, accumulator and draw-detection.
/// @return Score in centipawns from the side-to-move's perspective (kDrawScore for a detected draw).
int EvaluatePosition(const SearchState &state);

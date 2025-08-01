#pragma once
/// @file Program configuration constants.

#include <cstdint>

constexpr int HASHTABLE_MEGABYTES         = 64;        ///< default transposition table size in MB
constexpr int Q_HASHTABLE_MB              = 8;         ///< quiescent hash table default size
constexpr int MAX_MOVES_PER_POSITION      = 256;       ///< maximum possible number of moves for a chess position
constexpr int BETA                        = 11000;     ///< greater than any possible evaluation score
constexpr int ALPHA                       = -11000;    ///< smaller than any possible evaluation score
constexpr int CHECKMATED_SCORE            = -10000;    ///< score for losing the game
constexpr int WIN_THRESHOLD               = 9000;      ///< score threshold for winning at any ply
constexpr int LOSE_THRESHOLD              = -9000;     ///< score threshold for losing at any ply
constexpr int DRAW_SCORE                  = 0;         ///< score for achieving a draw
constexpr int MAX_PLY                     = 64;        ///< terminate the search at this ply no matter what
constexpr int RANK_FLIP                   = 0x38;      ///< used as an XOR mask to flip the board horizontally
constexpr int SEARCH_CANCELLED_SCORE      = -12345678; ///< illegal value returned when search is cancelled
constexpr int SCORE_INSTABILITY_THRESHOLD = 50;        ///< window of score variability
constexpr int START_DEPTH                 = 3;         ///< depth to do full width search
constexpr int MEGABYTE                    = 1 << 20;
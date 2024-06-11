#pragma once

#ifndef DEBUGX
#define DEBUGX 1
#endif

#ifndef DO_NULL_MOVE_PRUNING
#define DO_NULL_MOVE_PRUNING 1
#endif

#ifndef DO_LATE_MOVE_REDUCTION
#define DO_LATE_MOVE_REDUCTION 1
#endif

constexpr int HASHTABLE_MEGABYTES         = 512;       ///< default transposition table size in MB
constexpr int MAX_MOVES_PER_POSITION      = 256;       ///< maximum possible number of pseudo-legal moves for a chess position
constexpr int BETA                        = 11000;     ///< greater than any possible evaluation score including checkmate
constexpr int ALPHA                       = -11000;    ///< smaller than any possible evaluation score including being checkmated
constexpr int CHECKMATED_SCORE            = -10000;    ///< score for losing at this ply
constexpr int WIN_THRESHOLD               = 9000;      ///< score threshold for winning at any ply
constexpr int LOSE_THRESHOLD              = -9000;     ///< score threshold for losing at any ply
constexpr int DRAW_SCORE                  = 0;         ///< score for achieving a draw
constexpr int MAX_PLY                     = 64;        ///< terminate the search at this ply no matter what
constexpr int MAX_GAME_LENGTH             = 512;       ///< maximum number of positions we can store in a game
constexpr int RANK_FLIP                   = 0x38;      ///< used as XOR mask to mirror or "flip" the board horizontally (x,y) => (x,7-y)
constexpr int SEARCH_CANCELLED_SCORE      = -12345678; ///< you should never see this score in any search result
constexpr int MOVED_INTO_CHECK_SCORE      = -23456789; ///< returned when a pseudo-legal move placed us into check
constexpr int SCORE_INSTABILITY_THRESHOLD = 50;        ///< deviation of score this amount between iterations triggers deeper search
constexpr int DEBUG_DICT_SIZE             = 4999;      ///< number of entries in the debug counts hashtable (should be prime)
constexpr int STARTING_SEARCH_DEPTH       = 3;         ///< depth to do full width alpha beta pre-search for move ordering at the root node
constexpr int MEGABYTE                    = 0x100000;
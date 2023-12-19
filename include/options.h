#pragma once

#ifndef DEBUGX
#define DEBUGX 1
#endif 

#ifndef DO_TEST_HASH_DURING_PERFT
#define DO_TEST_HASH_DURING_PERFT 0
#endif

#ifndef DO_NULL_MOVE_PRUNING
#define DO_NULL_MOVE_PRUNING 1
#endif

#ifndef DO_LATE_MOVE_REDUCTION
#define DO_LATE_MOVE_REDUCTION 0
#endif

#ifndef DO_FUTILITY_PRUNING
#define DO_FUTILITY_PRUNING 0
#endif

const int HASHTABLE_MEGABYTES           =      1024; ///< default transposition table size in MB
const int MAX_MOVES_PER_POSITION        =       256; ///< maximum possible number of pseudo-legal moves for a chess position
const int BETA                          =     11000; ///< greater than any possible evaluation score including checkmate
const int ALPHA                         =    -11000; ///< smaller than any possible evaluation score including being checkmated
const int CHECKMATED_SCORE              =    -10000; ///< score for losing at this ply
const int WIN_THRESHOLD                 =      9000; ///< score threshold for winning at any ply
const int LOSE_THRESHOLD                =     -9000; ///< score threshold for losing at any ply
const int DRAW_SCORE                    =         0; ///< score for achieving a draw
const int MAX_PLY                       =        64; ///< terminate the search at this ply no matter what
const int MAX_GAME_LENGTH               =       512; ///< maximum number of positions we can store in a game
const int RANK_FLIP                     =      0x38; ///< used as XOR mask to mirror or "flip" the board horizontally (x,y) => (x,7-y)
const int SEARCH_CANCELLED_SCORE        = -12345678; ///< you should never see this score in any search result - returned only when cancel flag was set
const int MOVED_INTO_CHECK_SCORE        = -23456789; ///< returned when a pseudo-legal move placed us into check
const int SCORE_INSTABILITY_THRESHOLD   =        50; ///< deviation of score this amount between iterations triggers deeper search
const int DEBUG_DICT_SIZE               =      4999; ///< number of entries in the debug counts hashtable (should be prime)
const int STARTING_SEARCH_DEPTH         =         3; ///< depth to do full width alpha beta pre-search for move ordering at the root node
const int MEGABYTE                      =  0x100000;
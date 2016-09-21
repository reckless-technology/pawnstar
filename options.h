#pragma once
/******************************************************************************
Determine whether to enable the debugging counts dictionaries
*******************************************************************************/
#ifndef DEBUGX
#define DEBUGX 1
#endif 
#if DEBUGX && !defined(EVAL_DEBUGX)
#define EVAL_DEBUGX 0
#endif
/******************************************************************************
Determine whether to use the intrinsic population count instruction on native 
64-bit platforms (not available on many older CPUs)
*******************************************************************************/
#ifndef USE_INTRINSIC_POPCNT
#define USE_INTRINSIC_POPCNT 1
#endif
/******************************************************************************
Use experimental "more sophisticated" eval (probably not a great idea)
*******************************************************************************/
#ifndef DO_EVALUATION_FULL
#define DO_EVALUATION_FULL 1
#endif
/******************************************************************************
Whether to test the Zobrist hash value at every node of a perft move gen test
*******************************************************************************/
#ifndef DO_TEST_HASH_DURING_PERFT
#define DO_TEST_HASH_DURING_PERFT 0
#endif
/******************************************************************************
Whether to enable magic bitboard attacks for bishops and rooks (marginally 
faster on machines with a large L1 cache)
*******************************************************************************/
#ifndef DO_MAGIC_BITBOARDS
#define DO_MAGIC_BITBOARDS 1
#endif
/******************************************************************************
Whether to enable null move pruning
*******************************************************************************/
#ifndef DO_NULL_MOVE_PRUNING
#define DO_NULL_MOVE_PRUNING 1
#endif
/******************************************************************************
Whether to enable extension on recapture of same value piece
*******************************************************************************/
#ifndef DO_RECAPTURE_EXTENSION
#define DO_RECAPTURE_EXTENSION 0
#endif
/******************************************************************************
Whether to enable extension on pawn push to 7th rank
*******************************************************************************/
#ifndef DO_PUSH_TO_SEVENTH_RANK_EXTENSION
#define DO_PUSH_TO_SEVENTH_RANK_EXTENSION 0
#endif
/******************************************************************************
Whether to enable futility pruning at frontier nodes
*******************************************************************************/
#ifndef DO_FUTILITY_PRUNING
#define DO_FUTILITY_PRUNING 0
#endif
/******************************************************************************
Whether to enable late move reductions
*******************************************************************************/
#ifndef DO_LATE_MOVE_REDUCTION
#define DO_LATE_MOVE_REDUCTION 0
#endif
/******************************************************************************
Whether to skip moves in quiescence search which have a negative static
exchange evaluation (SEE)
*******************************************************************************/
#ifndef DO_QUIESCENCE_STATIC_EXCHANGE_EVAL
#define DO_QUIESCENCE_STATIC_EXCHANGE_EVAL 1
#endif
/******************************************************************************
Global constants
*******************************************************************************/
#define HASHTABLE_MEGABYTES                 256 // default transposition table size in MB
#define STRING_BUF_LEN                     1024 // default line buffer size
#define MAX_MOVES_PER_POSITION              128 // maximum possible number of pseudo-legal moves for any chess position
#define BETA                              11000 // greater than any possible evaluation score including checkmate
#define ALPHA                            -11000 // smaller than any possible evaluation score including being checkmated
#define CHECKMATED_SCORE                 -10000 // score for losing at this ply
#define WIN_THRESHOLD                      9000 // score threshold for winning at any ply
#define LOSE_THRESHOLD                    -9000 // score threshold for losing at any ply
#define DRAW_SCORE                            0 // contempt factor - pawnstar won't play for a draw at scores above this
#define MAX_PLY                              64 // terminate the search at this ply no matter what
#define MAX_GAME_LENGTH                     512 // maximum number of positions we can store in a game
#define RANK_FLIP                          0x38 // used as XOR mask to mirror or "flip" the board horizontally (x,y) => (x,7-y)
#define ILLEGAL_SCORE                  12345678 // you should never see this score in any search result - returned only when cancel flag was set
#define MOVED_INTO_CHECK_SCORE        -23456789 // returned when a pseudo-legal move placed us into check
#define SCORE_INSTABILITY_THRESHOLD          50 // deviation of score this amount between iterations triggers deeper search
#define PV_TABLE_SIZE                      4999 // number of entries in the principal variation hashtable (should be prime)
#define DEBUG_DICT_SIZE                    4999 // number of entries in the debug counts hashtable (should be prime)
#define EVAL_HASHTABLE_SIZE                4999 // number of entries in the evaluation hashtable (should be prime)
#define SMALL_HASTABLE_SIZE                4999 // number of transpositions in the small auxiliary (cached = fast) TT
#define FUTILITY_CUTOFF_THRESHOLD          1200 // Prune frontier nodes where eval is this much below alpha
#define STARTING_SEARCH_DEPTH                 3 // depth to do full width alpha beta pre-search for move ordering at the root node
#define NUM_ROOT_MOVES_BEFORE_PVS             1 // number of moves to search with full width alpha beta window at the root node
#define MEGABYTE                       0x100000
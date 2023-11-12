#pragma once
/*
Global header file - included by each source file
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "options.h"
#include "bitboard_constants.h"
#include "macros.h"
#include "types.h"
#include "function_prototypes.h"
#include "inline_functions.h"

#if _MSC_VER
#define strtok_r strtok_s
#endif

extern const char* const    OPENING_BOOK_MOVES;
extern const Sets           SETS[64];
extern const PawnSets       PAWN_SETS[2][64];
extern const bitboard       INTERVENING_SQUARES[64][64];
extern const uint64_t       CASTLING_RIGHTS_HASHES[16];
extern const uint64_t       EN_PASSANT_HASHES[8];
extern const uint64_t       PIECE_SQUARE_HASHES[2][6][64];
extern Game                 the_game;


#if DEBUGX
#define DEBUG_STATEMENT(x)  x
#define INCREMENT(x)        DebugXIncrement(x)
#define INCREMENT_IF(b,x)   DebugXIncrementIf(b,x)
#else
#define DEBUG_STATEMENT(x)
#define INCREMENT(x)
#define INCREMENT_IF(b,x)
#endif

#if EVAL_DEBUGX
#define EVAL_INCREMENT(x)   DebugXIncrement(x)
#else
#define EVAL_INCREMENT(x)
#endif

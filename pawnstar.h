#pragma once
/******************************************************************************
Global header file - included by each source file
*******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "options.h"
#include "bitboard_constants.h"
#include "macros.h"
#include "types.h"
#include "function_prototypes.h"

extern const MagicMoveEntry ROOK_MAGICS[64];
extern const MagicMoveEntry BISHOP_MAGICS[64];
extern const bitboard       NORTH_OF[64];
extern const bitboard       NORTHEAST_OF[64];
extern const bitboard       EAST_OF[64];
extern const bitboard       SOUTHEAST_OF[64];
extern const bitboard       SOUTH_OF[64];
extern const bitboard       SOUTHWEST_OF[64];
extern const bitboard       WEST_OF[64];
extern const bitboard       NORTHWEST_OF[64];
extern const bitboard       PAWN_ATTACKS_WHITE[64];
extern const bitboard       PAWN_ATTACKS_BLACK[64];
extern const bitboard       KNIGHT_ATTACKS[64];
extern const bitboard       BISHOP_ATTACKS[64];
extern const bitboard       ROOK_ATTACKS[64];
extern const bitboard       QUEEN_ATTACKS[64];
extern const bitboard       KING_ATTACKS[64];
extern const bitboard       BISHOP_ADJACENT[64];
extern const bitboard       KING_PAWN_SHIELD_WHITE[64];
extern const bitboard       KING_PAWN_SHIELD_BLACK[64];
extern const bitboard       KING_PAWN_SHIELD_WHITE_2[64];
extern const bitboard       KING_PAWN_SHIELD_BLACK_2[64];
extern const bitboard       INTERVENING_SQUARES[64][64];            // indexed by [to][from] square locations
extern const signed char    NORTH_FROM[64][8];
extern const signed char    NORTHEAST_FROM[64][8];
extern const signed char    EAST_FROM[64][8];
extern const signed char    SOUTHEAST_FROM[64][8];
extern const signed char    SOUTH_FROM[64][8];
extern const signed char    SOUTHWEST_FROM[64][8];
extern const signed char    WEST_FROM[64][8];
extern const signed char    NORTHWEST_FROM[64][8];
extern const uchar          DIRECTIONS[64][64];                     // indexed by [to][from] square locations 
extern const uint64         PIECE_SQUARE_HASHES[2][8][64];          // indexed by [color][piece][location]
extern const uint64         CASTLING_RIGHTS_HASHES[16];             // indexed by 4 bit castling rights
extern const uint64         EN_PASSANT_HASHES[8];                   // indexed by en passant file (if any)
extern Context              globals[1];
extern const char* const    OPENING_BOOK_MOVES;

#if DEBUGX
#define DEBUG_STATEMENT(x)      x
#define INCREMENT(x)            DebugXIncrement(x)
#define INCREMENT_IF(b,x)       DebugXIncrementIf(b,x)
#else
#define DEBUG_STATEMENT(x)
#define INCREMENT(x)
#define INCREMENT_IF(b,x)
#endif

#if EVAL_DEBUGX
#define EVAL_INCREMENT(x)       DebugXIncrement(x)
#else
#define EVAL_INCREMENT(x)
#endif

#include "inline_functions.h"

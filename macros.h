#pragma once
#include "bitboard_constants.h"
/*
Function like macros

Moves are contained within the least significant 22 bits of an integer

  Bits      Interpretation
  
 0 -  5     to (destination square index)
 6 - 11     from (source square index)
12 - 14     moving piece type
15 - 17     captured piece type in the case of capture moves
18 - 20     promoted piece type in the case of pawn promotions
21 - 21     special flag (castling or en passant capture move)

A value of 0 terminates a move list
*/
#define CONSTRUCT_PROMOTION_MOVE(from, to, captured, promoted) \
                                        ((to) | ((from) << 6) | (PAWN << 12) | \
                                        ((captured) << 15) | ((promoted) << 18))
#define CONSTRUCT_MOVE(from, to, piece, captured) \
                                        ((to) | ((from) << 6) | ((piece) << 12) | \
                                        ((captured) << 15))
#define CONSTRUCT_NON_CAPTURE_MOVE(from, to, piece) \
                                        ((to) | ((from) << 6) | ((piece) << 12))
#define CONSTRUCT_SPECIAL_MOVE(from, to, piece, captured) \
                                        ((to) | ((from) << 6) | ((piece) << 12) | \
                                        ((captured) << 15) | (1 << 21))
#define MOVE_TO(m)                      ( (m)        & 0x3F)
#define MOVE_FROM(m)                    (((m) >>  6) & 0x3F)
#define MOVE_PIECE(m)                   (((m) >> 12) & 0x07)
#define MOVE_CAPTURED(m)                (((m) >> 15) & 0x07)
#define MOVE_PROMOTED(m)                (((m) >> 18) & 0x07)
#define MOVE_IS_SPECIAL(m)              (((m) >> 21))
#define FILE_OF(locn)                   ((locn) & 7)
#define RANK_OF(locn)                   ((locn) >> 3)
#define FILE_CHAR(locn)                 ('a' + FILE_OF(locn))
#define RANK_CHAR(locn)                 ('1' + RANK_OF(locn))
#define BITBOARD(locn)                  (1ull << (locn))
#define BITBOARD_XY(x,y)                (1ull << ((x) + 8 * (y)))
#define SHIFT_NORTH(b)                  ((b) << 8)
#define SHIFT_NORTHEAST(b)              (((b) & MASK_EAST_1) << 9)
#define SHIFT_EAST(b)                   (((b) & MASK_EAST_1) << 1)
#define SHIFT_SOUTHEAST(b)              (((b) & MASK_EAST_1) >> 7)
#define SHIFT_SOUTH(b)                  ((b) >> 8)
#define SHIFT_SOUTHWEST(b)              (((b) & MASK_WEST_1) >> 9)
#define SHIFT_WEST(b)                   (((b) & MASK_WEST_1) >> 1)
#define SHIFT_NORTHWEST(b)              (((b) & MASK_WEST_1) << 7)
#define ENEMY(color)                    (!(color))
#define COLOR_TO_MOVE(position)         (((position)->state_flags & IS_BLACK_TO_MOVE) ? BLACK : WHITE)
#define COLOR_NOT_TO_MOVE(position)     (((position)->state_flags & IS_BLACK_TO_MOVE) ? WHITE : BLACK)
#define IS_ENDGAME(position)            (PopCount(position->occupied_squares) < 8)

#ifndef min
#define min(a,b)                        ((a) < (b) ? (a) : (b))
#endif
#ifndef max
#define max(a,b)                        ((a) > (b) ? (a) : (b))
#endif

#pragma once
#include "bitboard_constants.h"
/*
Function like macros
*/

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
#define KING_LOCATION(position, color)  (Lsb(position->kings & position->pieces_of_color[color]))
#define IS_IN_CHECK(position, color)    (IsAttacked(position, KING_LOCATION(position, color), ENEMY(color)))

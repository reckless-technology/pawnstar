#pragma once
#include "types.h"
#include "bitboard_constants.h"
/*
Inline functions - called many times per move; need to be FAST.
*/
#if _MSC_VER
#define INLINE static __forceinline
#elif defined (__GNUC__)
#define INLINE static inline __attribute__((always_inline))
#else
#error you must define force inline for your compiler
#endif
/*
Find the index of the least significant bit set in a bitboard, also known as
bit scan forward, or trailing zero count - bitboard must be non zero.
On Intel architectures we use the compiler intrinsic BSF instruction.
*/
#if _MSC_VER /* Microsoft C compiler? */
#include <intrin.h>
INLINE int Lsb(bitboard x)
{
    unsigned long index;
    _BitScanForward64(&index, x);
    return (int)index;
}
INLINE int Msb(bitboard x)
{
    unsigned long index;
    _BitScanReverse64(&index, x);
    return (int)index;
}
INLINE int PopCount(bitboard x)
{
    return (int)__popcnt64(x);
}
#elif defined (__GNUC__)
INLINE int Lsb(bitboard x)
{
    return (int)__builtin_ctzll(x);
}
INLINE int Msb(bitboard x)
{
    return 63 - (int)__builtin_clzll(x);
}
INLINE int PopCount(bitboard x)
{
    return (int)__builtin_popcountll(x);
}
#else
#error you must define bitscan forward, reverse and popcount for your compiler
#endif

/*
Find the index of and clear the least significant bit set in a bitboard
*/
INLINE int FindAndClearLsb(bitboard* x)
{
    const int y = Lsb(*x);
    *x &= (*x - 1);
    return y;
}

/*
Find the first location of a move in a zero terminated list of moves.
This is analagous to 'strchr' but using 32-bit words instead of bytes.
*/
INLINE const int* FindMove(const int moves[], int move)
{
    while (*moves)
    {
        if (*moves == move)
        {
            return moves;
        }
        ++moves;
    }
    return NULL;
}
/*
King attack fill
*/
INLINE bitboard KingFill(bitboard b)
{
    bitboard x = SHIFT_WEST(b) | SHIFT_EAST(b);
    x |= SHIFT_NORTH(x) | SHIFT_SOUTH(x);
    return x | SHIFT_NORTH(b) | SHIFT_SOUTH(b);
}
/*
Bitboard vector fill functions:
These functions shift in the specified direction by 1, 2 and 4 squares 
successively, masking off the appropriate edge files to avoid wraparound in the
larger shifts, and ORing in the results, which "fills" the bitboard to the edge
of the board in the specified direction in 3 successive shift-or operations.
*/
INLINE bitboard FillNorth(bitboard b)
{
    b |= b <<  8;
    b |= b << 16;
    b |= b << 32;
    return b;
}
INLINE bitboard FillNorthEast(bitboard b)
{
    b |= (b & MASK_EAST_1) <<  9;
    b |= (b & MASK_EAST_2) << 18;
    b |= (b & MASK_EAST_4) << 36;
    return b;
}
INLINE bitboard FillEast(bitboard b)
{
    b |= (b & MASK_EAST_1) << 1;
    b |= (b & MASK_EAST_2) << 2;
    b |= (b & MASK_EAST_4) << 4;
    return b;
}
INLINE bitboard FillSouthEast(bitboard b)
{
    b |= (b & MASK_EAST_1) >>  7;
    b |= (b & MASK_EAST_2) >> 14;
    b |= (b & MASK_EAST_4) >> 28;
    return b;
}
INLINE bitboard FillSouth(bitboard b)
{
    b |= b >>  8;
    b |= b >> 16;
    b |= b >> 32;
    return b;
}
INLINE bitboard FillSouthWest(bitboard b)
{
    b |= (b & MASK_WEST_1) >>  9;
    b |= (b & MASK_WEST_2) >> 18;
    b |= (b & MASK_WEST_4) >> 36;
    return b;
}
INLINE bitboard FillWest(bitboard b)
{
    b |= (b & MASK_WEST_1) >> 1;
    b |= (b & MASK_WEST_2) >> 2;
    b |= (b & MASK_WEST_4) >> 4;
    return b;
}
INLINE bitboard FillNorthWest(bitboard b)
{
    b |= (b & MASK_WEST_1) <<  7;
    b |= (b & MASK_WEST_2) << 14;
    b |= (b & MASK_WEST_4) << 28;
    return b;
}
INLINE bitboard FillNorthAndSouth(bitboard b)
{
    b |= (b <<  8) | (b >>  8);
    b |= (b << 16) | (b >> 16);
    b |= (b << 32) | (b >> 32);
    return b;
}

/*
Propagate a principal variation up to the parent node, prepending a 
"best move" before copying the child PV.
*/
INLINE void 
CopyVariation(Variation*       dst_variation, 
              const Variation* src_variation, 
              int              best_move)
{
    if (dst_variation)
    {
        int* dst = dst_variation->moves;
        *dst++ = best_move;
        if (src_variation)
        {
            const int* src = src_variation->moves;
            while (*src)
            {
                *dst++ = *src++;
            }
        }
        *dst = 0;
    }
}

INLINE int
PieceAt(const Position* position, 
        int             location)
{
    const bitboard square = BITBOARD(location);
    return 
        (square & position->pawns)   ? PAWN   :
        (square & position->knights) ? KNIGHT :
        (square & position->bishops) ? BISHOP :
        (square & position->rooks)   ? ROOK   :
        (square & position->queens)  ? QUEEN  :
        (square & position->kings)   ? KING   : NO_PIECE;

}

INLINE int
ColorAt(const Position* position,
        int             location)
{
    const bitboard square = BITBOARD(location);
    return 
        (square & position->white_pieces) ? WHITE :
        (square & position->black_pieces) ? BLACK : NEITHER_COLOR;
}

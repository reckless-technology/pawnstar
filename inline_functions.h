#pragma once
#include "types.h"
#include "bitboard_constants.h"
/*
Inline functions - called many times per move; need to be FAST.
*/
#if _MSC_VER
#define INLINE static __forceinline
#else
#define INLINE static inline
#endif
/*
Find the index of the least significant bit set in a bitboard, also known as
bit scan forward, or trailing zero count - bitboard must be non zero.
On Intel architectures we use the compiler intrinsic BSF instruction.
*/
#if _MSC_VER /* Microsoft C compiler? */
#include <intrin.h>
#if _M_X64   /* 64 bit platform? */
INLINE int Lsb(bitboard x)
{
    unsigned long index;
    _BitScanForward64(&index, x);
    return (int)index;
}
#else /* 32 bit platform */
INLINE int Lsb(bitboard x)
{
    unsigned long index;
    if ((unsigned long)x)
    {
        _BitScanForward(&index, (unsigned long)x);
        return (int)index;
    }
    _BitScanForward(&index, (unsigned long)(x >> 32));
    return (int)(index + 32);
}
#endif /* _M_X64 */
#elif defined (__GNUC__)
INLINE int Lsb(bitboard x)
{
    return (int)__builtin_ctzll(x);
}
#else
#error Your compiler does not appear to support bit scan. This is required for pawnstar.
#endif /* _MSC_VER */
/*
Population count (Hamming weight)
NB: needs to be FAST
Only recent CPUs support intrinsic popcnt, so this is optional, and otherwise
we just loop counting the bits set.
*/
#if USE_INTRINSIC_POPCNT
#if _MSC_VER
#if _M_X64
INLINE int PopCount(bitboard x)
{
    return (int)__popcnt64(x);
}
#else
INLINE int PopCount(bitboard x)
{
    return __popcnt((unsigned int)x) + __popcnt((unsigned int)(x >> 32));
}
#endif /* _M_X64 */
#elif defined (__GNUC__)
INLINE int PopCount(bitboard x)
{
    return (int)__builtin_popcountll(x);
}
#endif /* _MSC_VER */
#else
INLINE int PopCount(bitboard x)
{
    int count = 0;
    while (x)
    {
        ++count;
        x &= x - 1; /* clear the LSB of x */
    }
    return count;
}
#endif /* USE_INTRINSIC_POPCNT */
/*
Find the index of and clear the least significant bit set in a bitboard
*/
INLINE int FindAndClearLsb(bitboard* x)
{
    const bitboard y = *x;
    *x = y & (y - 1);
    return Lsb(y);
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
CopyVariation(Variation*       dst, 
              const Variation* src, 
              int              move)
{
    if (dst)
    {
        dst->moves[0] = move;
        if (src)
        {
            memcpy(&dst->moves[1], src->moves, src->num_moves * sizeof(int));
            dst->num_moves = src->num_moves + 1;
        }
        else
        {
            dst->num_moves = 1;
        }
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

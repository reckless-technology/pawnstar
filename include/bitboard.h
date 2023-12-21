#pragma once
#include <cstdint>

#include "move.h"
/**
 * @file Functions and macros to use Bitboards.
 * 
 * A Bitboard is a set-wise interpretation of a 64-bit unsigned integer, with 
 * each bit mapping to a square on the chessboard. 
 * 
 * If the bit is 1, then the corresponding square is a member of that set, for example:
 * 
 * The set of squares occupied by pawns
 * The set of squares occupied by a black piece
 * The set of squares to which a knight on e4 may move
 * The set of squares attacked by black queens
 * 
 * Bit  0 maps to square a1 (LSB)
 * Bit  7 maps to square h1
 * Bit 56 maps to square a8
 * Bit 63 maps to square h8 (MSB)
 * 
 * This is commonly referred to as LERF (little endian rank file mapping). 
 * If you treat the board as a 2D array, with square (0,0) being a1 and square 
 * (7,7) being h8, then the Bitboard for a given square is just 
 * 
 * (1ull << (x + 8 * y))
 */

typedef uint64_t Bitboard;

const Bitboard RANK_1 = 0x00000000000000FFull;
const Bitboard RANK_2 = 0x000000000000FF00ull;
const Bitboard RANK_3 = 0x0000000000FF0000ull;
const Bitboard RANK_4 = 0x00000000FF000000ull;
const Bitboard RANK_5 = 0x000000FF00000000ull;
const Bitboard RANK_6 = 0x0000FF0000000000ull;
const Bitboard RANK_7 = 0x00FF000000000000ull;
const Bitboard RANK_8 = 0xFF00000000000000ull;

const Bitboard FILE_A = 0x0101010101010101ull;
const Bitboard FILE_B = 0x0202020202020202ull;
const Bitboard FILE_C = 0x0404040404040404ull;
const Bitboard FILE_D = 0x0808080808080808ull;
const Bitboard FILE_E = 0x1010101010101010ull;
const Bitboard FILE_F = 0x2020202020202020ull;
const Bitboard FILE_G = 0x4040404040404040ull;
const Bitboard FILE_H = 0x8080808080808080ull;

/* Used to mask files to prevent wrap when bit shifting. */
const Bitboard MASK_EAST_1 = 0x7F7F7F7F7F7F7F7Full;
const Bitboard MASK_EAST_2 = 0x3F3F3F3F3F3F3F3Full;
const Bitboard MASK_EAST_4 = 0x0F0F0F0F0F0F0F0Full;
const Bitboard MASK_WEST_1 = 0xFEFEFEFEFEFEFEFEull;
const Bitboard MASK_WEST_2 = 0xFCFCFCFCFCFCFCFCull;
const Bitboard MASK_WEST_4 = 0xF0F0F0F0F0F0F0F0ull;

const Bitboard ALL_SQUARES     = 0xFFFFFFFFFFFFFFFFull;
const Bitboard NO_SQUARES      = 0x0000000000000000ull;
const Bitboard WHITE_SQUARES   = 0x55AA55AA55AA55AAull;
const Bitboard BLACK_SQUARES   = 0xAA55AA55AA55AA55ull;
const Bitboard CORNER_SQUARES  = 0x8100000000000081ull;
const Bitboard BORDER_SQUARES  = 0xFF818181818181FFull;
const Bitboard CTR_16_SQUARES  = 0x00003C3C3C3C0000ull;
const Bitboard CTR_4_SQUARES   = 0x0000001818000000ull;
const Bitboard BLACK_MOVE_HASH = 0xB92B78FCCF92F8CDull;

constexpr Bitboard BITBOARD(Square location)    { return 1ull << location;                  }
constexpr Bitboard BITBOARD(int x, int y)       { return 1ull << (x + 8 * y);               }
constexpr Square   Lsb(Bitboard x)              { return (Square)__builtin_ctzll(x);        }
constexpr Square   Msb(Bitboard x)              { return (Square)(63 - __builtin_clzll(x)); }
constexpr uint8_t  PopCount(Bitboard x)         { return (uint8_t)__builtin_popcountll(x);  }
constexpr Bitboard ShiftNorth(Bitboard b)       { return  b << 8;                           }
constexpr Bitboard ShiftNortheast(Bitboard b)   { return (b & MASK_EAST_1) << 9;            }
constexpr Bitboard ShiftEast(Bitboard b)        { return (b & MASK_EAST_1) << 1;            }
constexpr Bitboard ShiftSoutheast(Bitboard b)   { return (b & MASK_EAST_1) >> 7;            }
constexpr Bitboard ShiftSouth(Bitboard b)       { return  b >> 8;                           }
constexpr Bitboard ShiftSouthwest(Bitboard b)   { return (b & MASK_WEST_1) >> 9;            }
constexpr Bitboard ShiftWest(Bitboard b)        { return (b & MASK_WEST_1) >> 1;            }
constexpr Bitboard ShiftNorthwest(Bitboard b)   { return (b & MASK_WEST_1) << 7;            }

constexpr Square 
FindAndClearLsb(Bitboard& x)
{
    const Square lsb = (Square)__builtin_ctzll(x);
    x &= (x - 1);
    return lsb;
}

constexpr Bitboard 
KnightFill(Bitboard b)
{
    const Bitboard west1     = ShiftWest(b);
    const Bitboard west2     = ShiftWest(west1);
    const Bitboard east1     = ShiftEast(b);
    const Bitboard east2     = ShiftEast(east1);
    const Bitboard eastwest1 = west1 | east1;
    const Bitboard eastwest2 = west2 | east2;
    return (eastwest1 << 16) | (eastwest1 >> 16) | (eastwest2 << 8) | (eastwest2 >> 8);
}

constexpr Bitboard 
KingFill(Bitboard b)
{
    return
        ShiftNorth(b)      |
        ShiftNortheast(b)  |
        ShiftEast(b)       |
        ShiftSoutheast(b)  |
        ShiftSouth(b)      |
        ShiftSouthwest(b)  |
        ShiftWest(b)       |
        ShiftNorthwest(b);
}

constexpr Bitboard 
FillNorth(Bitboard b)
{
    b |= b <<  8;
    b |= b << 16;
    b |= b << 32;
    return b;
}

constexpr Bitboard 
FillNorthEast(Bitboard b)
{
    b |= (b & MASK_EAST_1) <<  9;
    b |= (b & MASK_EAST_2) << 18;
    b |= (b & MASK_EAST_4) << 36;
    return b;
}

constexpr Bitboard 
FillEast(Bitboard b)
{
    b |= (b & MASK_EAST_1) << 1;
    b |= (b & MASK_EAST_2) << 2;
    b |= (b & MASK_EAST_4) << 4;
    return b;
}

constexpr Bitboard 
FillSouthEast(Bitboard b)
{
    b |= (b & MASK_EAST_1) >>  7;
    b |= (b & MASK_EAST_2) >> 14;
    b |= (b & MASK_EAST_4) >> 28;
    return b;
}

constexpr Bitboard 
FillSouth(Bitboard b)
{
    b |= b >>  8;
    b |= b >> 16;
    b |= b >> 32;
    return b;
}

constexpr Bitboard 
FillSouthWest(Bitboard b)
{
    b |= (b & MASK_WEST_1) >>  9;
    b |= (b & MASK_WEST_2) >> 18;
    b |= (b & MASK_WEST_4) >> 36;
    return b;
}

constexpr Bitboard 
FillWest(Bitboard b)
{
    b |= (b & MASK_WEST_1) >> 1;
    b |= (b & MASK_WEST_2) >> 2;
    b |= (b & MASK_WEST_4) >> 4;
    return b;
}

constexpr Bitboard 
FillNorthWest(Bitboard b)
{
    b |= (b & MASK_WEST_1) <<  7;
    b |= (b & MASK_WEST_2) << 14;
    b |= (b & MASK_WEST_4) << 28;
    return b;
}

constexpr Bitboard 
FillNorthAndSouth(Bitboard b)
{
    b |= (b <<  8) | (b >>  8);
    b |= (b << 16) | (b >> 16);
    b |= (b << 32) | (b >> 32);
    return b;
}
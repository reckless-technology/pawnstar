#pragma once
/// @file Constants and functions for using Bitboards.
///
/// A Bitboard is a set-wise interpretation of a 64-bit unsigned integer, with each bit mapping to a square on the
/// chessboard.
///
/// For example:
///
/// The set of squares occupied by pawns
/// The set of squares occupied by a black piece
/// The set of squares to which a knight on e4 may move
/// The set of squares attacked by black queens
/// The set of squares containing a piece checking the white king
///
/// If the bit is 1, then the corresponding square is a member of that set.
///
/// Bit  0 maps to square a1 (LSB)
/// Bit  7 maps to square h1
/// Bit 56 maps to square a8
/// Bit 63 maps to square h8 (MSB)
///
/// This is commonly referred to as LERF (little endian rank file mapping).
///
/// If you treat the board as a 2D array, with square (0,0) being a1 and square (7,7) being h8, then the Bitboard for a
/// given square is just
///
/// (1 << (x + 8 * y))

#include <cstdint>

#include "move.h"

typedef uint64_t Bitboard;

// Useful Bitboard constant values.
constexpr Bitboard NO_SQUARES    = 0;
constexpr Bitboard ALL_SQUARES   = ~NO_SQUARES;
constexpr Bitboard RANK_1        = 0xFFull;
constexpr Bitboard RANK_2        = RANK_1 << 8;
constexpr Bitboard RANK_3        = RANK_2 << 8;
constexpr Bitboard RANK_4        = RANK_3 << 8;
constexpr Bitboard RANK_5        = RANK_4 << 8;
constexpr Bitboard RANK_6        = RANK_5 << 8;
constexpr Bitboard RANK_7        = RANK_6 << 8;
constexpr Bitboard RANK_8        = RANK_7 << 8;
constexpr Bitboard FILE_A        = 0x0101010101010101ull;
constexpr Bitboard FILE_B        = FILE_A << 1;
constexpr Bitboard FILE_C        = FILE_B << 1;
constexpr Bitboard FILE_D        = FILE_C << 1;
constexpr Bitboard FILE_E        = FILE_D << 1;
constexpr Bitboard FILE_F        = FILE_E << 1;
constexpr Bitboard FILE_G        = FILE_F << 1;
constexpr Bitboard FILE_H        = FILE_G << 1;
constexpr Bitboard NOT_FILE_H    = ~FILE_H; ///< Used when shifting east to avoid wraparound.
constexpr Bitboard NOT_FILE_A    = ~FILE_A; ///< Used when shifting west to avoid wraparound.
constexpr Bitboard WHITE_SQUARES = 0x55AA55AA55AA55AAull;
constexpr Bitboard BLACK_SQUARES = ~WHITE_SQUARES;

/// @brief Convert a location into a Bitboard.
/// @param location square index.
/// @return Bitboard of square.
constexpr Bitboard BITBOARD(Square location)
{
    return 1ull << location;
}
/// @brief Convert a location into a Bitboard
/// @param x file index
/// @param y rank index
/// @return Bitboard of square
constexpr Bitboard BITBOARD(int x, int y)
{
    return 1ull << (x + 8 * y);
}
/// @brief Find the least significant bit set.
/// @param x Value to search.
/// @return Index of LSB in x.
constexpr Square Lsb(Bitboard x)
{
    return (Square)__builtin_ctzll(x);
}
/// @brief Find the most significant bit set.
/// @param x Value to search.
/// @return Index of MSB in x.
constexpr Square Msb(Bitboard x)
{
    return (Square)(63 - __builtin_clzll(x));
}
/// @brief Find the least significant bit set and clear it.
/// @param x Value to scan.
/// @return Index of bit which was cleared.
constexpr Square FindAndClearLsb(Bitboard &x)
{
    const Square lsb = (Square)__builtin_ctzll(x);
    x &= (x - 1);
    return lsb;
}
/// @brief Find the population count.
/// @param x Value to scan.
/// @return Number of bits set in x.
constexpr int PopCount(Bitboard x)
{
    return __builtin_popcountll(x);
}

/// Functions to shift bitboards by one square in the specified direction. Compass rose directions are from White's
/// perspective.
constexpr Bitboard ShiftNorth(Bitboard b)
{
    return b << 8;
}
constexpr Bitboard ShiftNortheast(Bitboard b)
{
    return (b & NOT_FILE_H) << 9;
}
constexpr Bitboard ShiftEast(Bitboard b)
{
    return (b & NOT_FILE_H) << 1;
}
constexpr Bitboard ShiftSoutheast(Bitboard b)
{
    return (b & NOT_FILE_H) >> 7;
}
constexpr Bitboard ShiftSouth(Bitboard b)
{
    return b >> 8;
}
constexpr Bitboard ShiftSouthwest(Bitboard b)
{
    return (b & NOT_FILE_A) >> 9;
}
constexpr Bitboard ShiftWest(Bitboard b)
{
    return (b & NOT_FILE_A) >> 1;
}
constexpr Bitboard ShiftNorthwest(Bitboard b)
{
    return (b & NOT_FILE_A) << 7;
}
#pragma once
/// @file Sliding piece bitboard attacks. Uses precomputed magic bitboards to lookup sliding piece attacks without loops
/// or branches.

#include "generated_data.h"

#ifndef USE_MAGIC_BITBOARDS
#define USE_MAGIC_BITBOARDS 1
#endif

#if USE_MAGIC_BITBOARDS

/// @brief Bishop sliding attacks.
/// @param occupied_squares Squares with a piece on them.
/// @param s Bishop location.
/// @return Set of squares attacked by a bishop on location.
constexpr Bitboard BishopAttacks(Bitboard occupied_squares, Square s)
{
    const MagicBitboard &m = BISHOP_MAGICS[s];
    return m.attacks[m.indices[((uint64_t)(occupied_squares & m.occupancy_mask) * m.magic) >> m.shift]];
}

/// @brief Rook sliding attacks.
/// @param occupied_squares Squares with a piece on them.
/// @param s Rook location.
/// @return Set of squares attacked by a rook on location.
constexpr Bitboard RookAttacks(Bitboard occupied_squares, Square s)
{
    const MagicBitboard &m = ROOK_MAGICS[s];
    return m.attacks[m.indices[((uint64_t)(occupied_squares & m.occupancy_mask) * m.magic) >> m.shift]];
}

#else

// Alternate sliding piece attacks using traditional approach instead of magic bitboards. This is slower, and only used
// for regression testing purposes.

constexpr Square LsbX(Bitboard b)
{
    return (b | 0x8000000000000000).Lsb();
}

constexpr Square MsbX(Bitboard b)
{
    return (b | 1).Msb();
}

constexpr Bitboard BishopAttacks(Bitboard occupied_squares, Square location)
{
    const Sets &s      = SETS[location];
    Bitboard    result = s.bishop_attacks;
    result ^= SETS[LsbX(s.northeast & occupied_squares)].northeast;
    result ^= SETS[LsbX(s.northwest & occupied_squares)].northwest;
    result ^= SETS[MsbX(s.southwest & occupied_squares)].southwest;
    result ^= SETS[MsbX(s.southeast & occupied_squares)].southeast;
    return result;
}

constexpr Bitboard RookAttacks(Bitboard occupied_squares, Square location)
{
    const Sets &s      = SETS[location];
    Bitboard    result = s.rook_attacks;
    result ^= SETS[LsbX(s.north & occupied_squares)].north;
    result ^= SETS[MsbX(s.south & occupied_squares)].south;
    result ^= SETS[LsbX(s.east & occupied_squares)].east;
    result ^= SETS[MsbX(s.west & occupied_squares)].west;
    return result;
}

#endif

/// @brief Queen sliding attacks.
/// @param occupied_squares Squares with a piece on them.
/// @param location Queen location.
/// @return Set of squares attacked by a queen on location.
constexpr Bitboard QueenAttacks(Bitboard occupied_squares, Square s)
{
    return BishopAttacks(occupied_squares, s) | RookAttacks(occupied_squares, s);
}

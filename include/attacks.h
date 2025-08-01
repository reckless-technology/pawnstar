#pragma once
/// @file Sliding piece bitboard attacks. Uses precomputed pext bitboards to lookup sliding piece attacks without loops
/// or branches.

#include "generated_data.h"

#include <immintrin.h>

#ifndef USE_PEXT_BITBOARDS
#define USE_PEXT_BITBOARDS 1
#endif

#if USE_PEXT_BITBOARDS

/// @brief Bishop sliding attacks.
/// @param occupied_squares Squares with a piece on them.
/// @param s Bishop location.
/// @return Set of squares attacked by a bishop on location.
constexpr Bitboard BishopAttacks(Bitboard occupied_squares, Square s)
{
    const PextBitboard &p = BISHOP_PEXTS[s];
    return p.attacks[p.indices[_pext_u64((uint64_t)occupied_squares, (uint64_t)p.occupancy_mask)]];
}

/// @brief Rook sliding attacks.
/// @param occupied_squares Squares with a piece on them.
/// @param s Rook location.
/// @return Set of squares attacked by a rook on location.
constexpr Bitboard RookAttacks(Bitboard occupied_squares, Square s)
{
    const PextBitboard &p = ROOK_PEXTS[s];
    return p.attacks[p.indices[_pext_u64((uint64_t)occupied_squares, (uint64_t)p.occupancy_mask)]];
}

#else

// Alternate sliding piece attacks using traditional approach instead of pext bitboards. This is slower, and only used
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
    Bitboard result = BISHOP_ATTACKS[location];
    result ^= NORTHEAST[LsbX(NORTHEAST[location] & occupied_squares)];
    result ^= NORTHWEST[LsbX(NORTHWEST[location] & occupied_squares)];
    result ^= SOUTHWEST[MsbX(SOUTHWEST[location] & occupied_squares)];
    result ^= SOUTHEAST[MsbX(SOUTHEAST[location] & occupied_squares)];
    return result;
}

constexpr Bitboard RookAttacks(Bitboard occupied_squares, Square location)
{
    Bitboard result = ROOK_ATTACKS[location];
    result ^= NORTH[LsbX(NORTH[location] & occupied_squares)];
    result ^= SOUTH[MsbX(SOUTH[location] & occupied_squares)];
    result ^= EAST[LsbX(EAST[location] & occupied_squares)];
    result ^= WEST[MsbX(WEST[location] & occupied_squares)];
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

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
/// @param location Bishop location.
/// @return Set of squares attacked by a bishop on location.
constexpr Bitboard BishopAttacks(Bitboard occupied_squares, Square location)
{
    const MagicMoveEntry &m = BISHOP_MAGICS[location];
    return m.attacks[m.indices[(uint64_t)(((occupied_squares & m.occupancy_mask) * m.magic) >> m.shift)]];
}

/// @brief Rook sliding attacks.
/// @param occupied_squares Squares with a piece on them.
/// @param location Rook location.
/// @return Set of squares attacked by a rook on location.
constexpr Bitboard RookAttacks(Bitboard occupied_squares, Square location)
{
    const MagicMoveEntry &m = ROOK_MAGICS[location];
    return m.attacks[m.indices[(uint64_t)(((occupied_squares & m.occupancy_mask) * m.magic) >> m.shift)]];
}

#else

/// Alternate sliding piece attacks using loops instead of magic bitboards.
/// Slower, only used for regression testing purposes.

typedef Bitboard (*BitboardFn)(Bitboard);

template <BitboardFn fn> constexpr Bitboard RayAttacks(Bitboard occupied_squares, Square location)
{
    Bitboard result = NO_SQUARES;
    for (Bitboard b = fn(Bitboard(location)); b != NO_SQUARES; b = fn(b))
    {
        result |= b;
        if ((b & occupied_squares) != NO_SQUARES)
        {
            break;
        }
    }
    return result;
}

constexpr Bitboard BishopAttacks(Bitboard occupied_squares, Square location)
{
    return RayAttacks<ShiftNortheast>(occupied_squares, location) |
           RayAttacks<ShiftNorthwest>(occupied_squares, location) |
           RayAttacks<ShiftSoutheast>(occupied_squares, location) |
           RayAttacks<ShiftSouthwest>(occupied_squares, location);
}

constexpr Bitboard RookAttacks(Bitboard occupied_squares, Square location)
{
    return RayAttacks<ShiftNorth>(occupied_squares, location) | RayAttacks<ShiftEast>(occupied_squares, location) |
           RayAttacks<ShiftSouth>(occupied_squares, location) | RayAttacks<ShiftWest>(occupied_squares, location);
}

#endif

/// @brief Queen sliding attacks.
/// @param occupied_squares Squares with a piece on them.
/// @param location Queen location.
/// @return Set of squares attacked by a queen on location.
constexpr Bitboard QueenAttacks(Bitboard occupied_squares, Square location)
{
    return BishopAttacks(occupied_squares, location) | RookAttacks(occupied_squares, location);
}

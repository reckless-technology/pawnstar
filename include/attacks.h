#pragma once
/// @file Sliding piece bitboard attacks. Uses precomputed magic bitboards to lookup sliding piece attacks without loops
/// or branches.

#include "generated_data.h"

/// @brief Bishop sliding attacks.
/// @param occupied_squares Squares with a piece on them.
/// @param location Bishop location.
/// @return Set of squares attacked by a bishop on location.
constexpr Bitboard BishopAttacks(Bitboard occupied_squares, Square location)
{
    const MagicMoveEntry &m = BISHOP_MAGICS[location];
    return m.attacks[m.indices[((occupied_squares & m.occupancy_mask) * m.magic) >> m.shift]];
}

/// @brief Rook sliding attacks.
/// @param occupied_squares Squares with a piece on them.
/// @param location Rook location.
/// @return Set of squares attacked by a rook on location.
constexpr Bitboard RookAttacks(Bitboard occupied_squares, Square location)
{
    const MagicMoveEntry &m = ROOK_MAGICS[location];
    return m.attacks[m.indices[((occupied_squares & m.occupancy_mask) * m.magic) >> m.shift]];
}

/// @brief Queen sliding attacks.
/// @param occupied_squares Squares with a piece on them.
/// @param location Queen location.
/// @return Set of squares attacked by a queen on location.
constexpr Bitboard QueenAttacks(Bitboard occupied_squares, Square location)
{
    return BishopAttacks(occupied_squares, location) | RookAttacks(occupied_squares, location);
}

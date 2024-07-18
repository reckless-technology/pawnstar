#pragma once
/**
 * @file Sliding piece magic bitboard attacks.
 */

#include "generated_data.h"

constexpr Bitboard BishopAttacks(Bitboard occupied_squares, Square location)
{
    const MagicMoveEntry &m = BISHOP_MAGICS[location];
    return m.attacks[m.indices[((occupied_squares & m.occupancy_mask) * m.magic) >> m.shift]];
}

constexpr Bitboard RookAttacks(Bitboard occupied_squares, Square location)
{
    const MagicMoveEntry &m = ROOK_MAGICS[location];
    return m.attacks[m.indices[((occupied_squares & m.occupancy_mask) * m.magic) >> m.shift]];
}

constexpr Bitboard QueenAttacks(Bitboard occupied_squares, Square location)
{
    return BishopAttacks(occupied_squares, location) | RookAttacks(occupied_squares, location);
}

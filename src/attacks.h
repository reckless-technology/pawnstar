#pragma once
/// @file attacks.h Sliding piece bitboard attacks. Uses precomputed pext bitboards to lookup sliding piece attacks
/// without loops or branches.

#include "constants.h"
#include "generated_data.h"

#include <immintrin.h>

// Use the hardware PEXT path when the compiler targets BMI2 (the default `-mbmi2` build); otherwise fall
// back to the slower traditional sliding-attack generator below.
#if defined(__BMI2__)

#if __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winvalid-constexpr"
#endif

/// @brief Bishop sliding attacks.
/// @param occupied_squares Squares with a piece on them.
/// @param s Bishop location.
/// @return Set of squares attacked by a bishop on location.
constexpr Bitboard BishopAttacks(Bitboard occupied_squares, Square s)
{
    const PextBitboard &p = kBishopPexts[s];
    return p.attacks[p.indices[_pext_u64((uint64_t)occupied_squares, (uint64_t)p.occupancy_mask)]];
}

/// @brief Rook sliding attacks.
/// @param occupied_squares Squares with a piece on them.
/// @param s Rook location.
/// @return Set of squares attacked by a rook on location.
constexpr Bitboard RookAttacks(Bitboard occupied_squares, Square s)
{
    const PextBitboard &p = kRookPexts[s];
    return p.attacks[p.indices[_pext_u64((uint64_t)occupied_squares, (uint64_t)p.occupancy_mask)]];
}

#if __GNUC__
#pragma GCC diagnostic pop
#endif

#else

// Alternate sliding piece attacks using traditional approach instead of pext bitboards. This is slower, and only used
// for regression testing purposes.

/// @brief Least significant bit, with the MSB forced set so an empty board still yields a valid square.
/// @param b Bitboard to scan.
/// @return Square index of the least significant set bit.
constexpr Square LsbX(Bitboard b)
{
    return (b | 0x8000000000000000).Lsb();
}

/// @brief Most significant bit, with the LSB forced set so an empty board still yields a valid square.
/// @param b Bitboard to scan.
/// @return Square index of the most significant set bit.
constexpr Square MsbX(Bitboard b)
{
    return (b | 1).Msb();
}

/// @brief Bishop sliding attacks (traditional ray-scan implementation).
/// @param occupied_squares Squares with a piece on them.
/// @param location Bishop location.
/// @return Set of squares attacked by a bishop on location.
constexpr Bitboard BishopAttacks(Bitboard occupied_squares, Square location)
{
    Bitboard result = kBishopAttacks[location];
    result ^= kNortheast[LsbX(kNortheast[location] & occupied_squares)];
    result ^= kNorthwest[LsbX(kNorthwest[location] & occupied_squares)];
    result ^= kSouthwest[MsbX(kSouthwest[location] & occupied_squares)];
    result ^= kSoutheast[MsbX(kSoutheast[location] & occupied_squares)];
    return result;
}

/// @brief Rook sliding attacks (traditional ray-scan implementation).
/// @param occupied_squares Squares with a piece on them.
/// @param location Rook location.
/// @return Set of squares attacked by a rook on location.
constexpr Bitboard RookAttacks(Bitboard occupied_squares, Square location)
{
    Bitboard result = kRookAttacks[location];
    result ^= kNorth[LsbX(kNorth[location] & occupied_squares)];
    result ^= kSouth[MsbX(kSouth[location] & occupied_squares)];
    result ^= kEast[LsbX(kEast[location] & occupied_squares)];
    result ^= kWest[MsbX(kWest[location] & occupied_squares)];
    return result;
}

#endif

/// @brief Queen sliding attacks.
/// @param occupied_squares Squares with a piece on them.
/// @param s Queen location.
/// @return Set of squares attacked by a queen on location.
constexpr Bitboard QueenAttacks(Bitboard occupied_squares, Square s)
{
    return BishopAttacks(occupied_squares, s) | RookAttacks(occupied_squares, s);
}

/// @brief Pointer to an attack-generation function taking occupancy and a square.
using AttackFn = Bitboard (*)(Bitboard occupied_squares, Square locn);
// clang-format off
/// @brief Attack functions for every non-pawn piece type, including the king.
constexpr std::array<std::pair<Piece, AttackFn>, 5> piece_attackers
{{
    { kKnight,   [](Bitboard, Square s){ return kKnightAttacks[s]; } }, 
    { kBishop,   BishopAttacks                                       }, 
    { kRook,     RookAttacks                                         }, 
    { kQueen,    QueenAttacks                                        },
    { kKing,     [](Bitboard, Square s){ return kKingAttacks[s]; }   }
}};

/// @brief Attack functions for every non-pawn piece type, excluding the king.
constexpr std::array<std::pair<Piece, AttackFn>, 4> piece_attackers_except_king
{{
    { kKnight,   [](Bitboard, Square s){ return kKnightAttacks[s]; } },
    { kBishop,   BishopAttacks                                       }, 
    { kRook,     RookAttacks                                         }, 
    { kQueen,    QueenAttacks                                        }
}};
// clang-format on

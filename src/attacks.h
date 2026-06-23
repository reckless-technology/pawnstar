#pragma once
/// @file attacks.h Sliding piece bitboard attacks. Uses precomputed pext bitboards to lookup sliding piece attacks
/// without loops or branches.

#include "constants.h"
#include "generated_data.h"

#include <immintrin.h>

// Sliding attacks use the hardware PEXT instruction (the build targets BMI2 via `-mbmi2`). The functions
// are marked constexpr for use in constexpr contexts (e.g. AttacksTo); since `_pext_u64` is not itself a
// constant expression, suppress the resulting -Winvalid-constexpr diagnostic.
static_assert(__BMI2__, "Pawnstar requires a BMI2 (-mbmi2) build for PEXT sliding attacks");

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
    return p.attacks_[p.indices_[_pext_u64((uint64_t)occupied_squares, (uint64_t)p.occupancy_mask_)]];
}

/// @brief Rook sliding attacks.
/// @param occupied_squares Squares with a piece on them.
/// @param s Rook location.
/// @return Set of squares attacked by a rook on location.
constexpr Bitboard RookAttacks(Bitboard occupied_squares, Square s)
{
    const PextBitboard &p = kRookPexts[s];
    return p.attacks_[p.indices_[_pext_u64((uint64_t)occupied_squares, (uint64_t)p.occupancy_mask_)]];
}

#if __GNUC__
#pragma GCC diagnostic pop
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

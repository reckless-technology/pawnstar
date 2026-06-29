#pragma once
/// @file attacks.h Sliding piece bitboard attacks. Uses precomputed magic bitboards to look up sliding
/// piece attacks without loops or branches.

#include "constants.h"
#include "generated_data.h"

// Sliding attacks use magic bitboards: a masked occupancy times a precomputed magic, shifted down to a
// hash slot (hash = (occupancy & mask) * magic >> shift); the slot's 1-byte indices_ entry then selects
// the de-duplicated attack set in attacks_. This is a plain 64-bit multiply — portable and fast on every
// x86-64 CPU (unlike pext, which is microcoded/slow on AMD Zen 1/2). The functions are marked constexpr
// for use in constexpr contexts (e.g. AttacksTo); since they read a runtime-initialised global table they
// are never actually constant-evaluated, so suppress the -Winvalid-constexpr diagnostic.
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
    const MagicBitboard &m = kBishopMagics[s];
    return m.attacks_[m.indices_[(((uint64_t)occupied_squares & (uint64_t)m.occupancy_mask_) * m.magic_) >> m.shift_]];
}

/// @brief Rook sliding attacks.
/// @param occupied_squares Squares with a piece on them.
/// @param s Rook location.
/// @return Set of squares attacked by a rook on location.
constexpr Bitboard RookAttacks(Bitboard occupied_squares, Square s)
{
    const MagicBitboard &m = kRookMagics[s];
    return m.attacks_[m.indices_[(((uint64_t)occupied_squares & (uint64_t)m.occupancy_mask_) * m.magic_) >> m.shift_]];
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

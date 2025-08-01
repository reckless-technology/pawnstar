#pragma once
#include "bitboard.h"
#include <array>
#include <cstdint>

using zobrist_t = uint64_t;

/// @file Declares data which is generated at compile time by the program "generate_constants.cpp" and contains various
/// precomputed constants that are used by pawnstar to speed up positional hashing, move generation, attack detection
/// and pawn structure analysis.

///@brief An entry in the pext bitboard move generator array.
/// Each entry contains information to generate sliding moves for either a bishop or a rook, for one square.
/// Uses an extra indirection via indices, since indices are 1 byte each and attack sets are 8 bytes each. This saves a
/// lot of space in the (already very large) rook bitboard tables, since for example for rooks there are 12 bits
/// in the occupancy mask (4096 indices) but typically only around 100 actual move sets. The pext bitboard sets are
/// generated at compile time.
struct PextBitboard
{
    Bitboard        occupancy_mask; ///< Occupancy mask (excludes final target square).
    const Bitboard *attacks;        ///< Discrete attack vectors (move sets).
    const uint8_t  *indices;        ///< Indices into the discrete attack vector array.
};

extern const std::array<Bitboard, 64>     NORTH;
extern const std::array<Bitboard, 64>     NORTHEAST;
extern const std::array<Bitboard, 64>     EAST;
extern const std::array<Bitboard, 64>     SOUTHEAST;
extern const std::array<Bitboard, 64>     SOUTH;
extern const std::array<Bitboard, 64>     SOUTHWEST;
extern const std::array<Bitboard, 64>     WEST;
extern const std::array<Bitboard, 64>     NORTHWEST;
extern const std::array<Bitboard, 64>     PAWN_ATTACKS_WHITE;
extern const std::array<Bitboard, 64>     PAWN_ATTACKS_BLACK;
extern const std::array<Bitboard, 64>     KNIGHT_ATTACKS;
extern const std::array<Bitboard, 64>     BISHOP_ATTACKS;
extern const std::array<Bitboard, 64>     ROOK_ATTACKS;
extern const std::array<Bitboard, 64>     QUEEN_ATTACKS;
extern const std::array<Bitboard, 64>     KING_ATTACKS;
extern const std::array<Bitboard, 64>     KING_ATTACKS_2;
extern const std::array<Bitboard, 64>     KING_PAWN_SHELTER_WHITE;
extern const std::array<Bitboard, 64>     KING_PAWN_SHELTER_BLACK;
extern const std::array<Bitboard, 64>     PASSED_PAWN_MASK_WHITE;
extern const std::array<Bitboard, 64>     PASSED_PAWN_MASK_BLACK;
extern const std::array<Bitboard, 64>     ISOLATED_PAWN_MASK_WHITE;
extern const std::array<Bitboard, 64>     ISOLATED_PAWN_MASK_BLACK;
extern const std::array<Bitboard, 64>     SUPPORTED_PAWN_MASK_WHITE;
extern const std::array<Bitboard, 64>     SUPPORTED_PAWN_MASK_BLACK;
extern const std::array<Bitboard, 64>     DOUBLED_PAWN_MASK_WHITE;
extern const std::array<Bitboard, 64>     DOUBLED_PAWN_MASK_BLACK;
extern const std::array<zobrist_t, 16>    CASTLING_RIGHTS_HASHES;
extern const std::array<zobrist_t, 64>    EN_PASSANT_HASHES;
extern const std::array<PextBitboard, 64> BISHOP_PEXTS;
extern const std::array<PextBitboard, 64> ROOK_PEXTS;
extern const Bitboard                     INTERVENING_SQUARES[64][64];
extern const zobrist_t                    PIECE_SQUARE_HASHES[2][6][64];
constexpr zobrist_t                       BLACK_MOVE_HASH = 0x28AB74D640E50602;
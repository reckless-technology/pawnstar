#pragma once
#include "bitboard.h"
#include <array>
#include <cstdint>
#include <vector>

using zobrist_t = uint64_t; ///< Zobrist hash value type.

/// @file generated_data.h Declares precomputed lookup tables (attack/ray bitboards, pext sliding-attack
/// tables, intervening-squares masks and Zobrist hashes) used by pawnstar for hashing, move generation and
/// attack detection. The tables are defined in generated_data.cpp and computed once at program startup
/// (dynamic initialisation), so there is no build-time code-generation step.

///@brief An entry in the pext bitboard move generator array.
/// Each entry contains information to generate sliding moves for either a bishop or a rook, for one square.
/// Uses an extra indirection via indices, since indices are 1 byte each and attack sets are 8 bytes each. This saves a
/// lot of space in the (already very large) rook bitboard tables, since for example for rooks there are 12 bits
/// in the occupancy mask (4096 indices) but typically only around 100 actual move sets. The pext bitboard sets are
/// computed once at program startup (see generated_data.cpp); each entry owns its attack/index storage.
struct PextBitboard
{
    Bitboard              occupancy_mask; ///< Occupancy mask (excludes final target square).
    std::vector<Bitboard> attacks;        ///< Discrete attack vectors (move sets).
    std::vector<uint8_t>  indices;        ///< Indices into the discrete attack vector array.
};

extern const std::array<Bitboard, 64>     kEastWest;               ///< East|west rays from each square (king's rank, en passant).
extern const std::array<Bitboard, 64>     kPawnAttacksWhite;       ///< Squares attacked by a white pawn on each square.
extern const std::array<Bitboard, 64>     kPawnAttacksBlack;       ///< Squares attacked by a black pawn on each square.
extern const std::array<Bitboard, 64>     kKnightAttacks;          ///< Squares attacked by a knight on each square.
extern const std::array<Bitboard, 64>     kBishopAttacks;          ///< Bishop attacks on an empty board per square.
extern const std::array<Bitboard, 64>     kRookAttacks;            ///< Rook attacks on an empty board per square.
extern const std::array<Bitboard, 64>     kQueenAttacks;           ///< Queen attacks on an empty board per square.
extern const std::array<Bitboard, 64>     kKingAttacks;            ///< Squares attacked by a king on each square.
extern const std::array<zobrist_t, 16>    kCastlingRightsHashes;   ///< Zobrist hash per castling-rights value.
extern const std::array<zobrist_t, 64>    kEnPassantHashes;        ///< Zobrist hash per en passant square.
extern const std::array<PextBitboard, 64> kBishopPexts;            ///< Bishop pext lookup tables per square.
extern const std::array<PextBitboard, 64> kRookPexts;              ///< Rook pext lookup tables per square.
constexpr zobrist_t                       kBlackMoveHash = 0x28AB74D640E50602; ///< Zobrist hash for black to move.

/// @brief Meta template - syntactic sugar for multi dim std::array types.
/// Refer to https://cpptruths.blogspot.com/2011/10/multi-dimensional-arrays-in-c11.html
/// @tparam T Type of object to store in array.
/// @tparam I Size of first dimension.
/// @tparam ...J Remaining dimensions.
template <class T, size_t I, size_t... J> struct MultiDimArray
{
    using Nested = typename MultiDimArray<T, J...>::type; ///< Array type for the remaining dimensions.
    using type   = std::array<Nested, I>;                 ///< Array type for this and all nested dimensions.
};

/// @brief Specialization for a single dimension MultiDimArray.
/// @tparam T Type of object to store in array.
/// @tparam I Number of elements.
template <class T, size_t I> struct MultiDimArray<T, I>
{
    using type = std::array<T, I>; ///< One-dimensional array type.
};

extern const MultiDimArray<Bitboard, 64, 64>::type    kInterveningSquares; ///< Squares strictly between two squares.
extern const MultiDimArray<zobrist_t, 2, 6, 64>::type kPieceSquareHashes;  ///< Zobrist hash per color, piece, square.

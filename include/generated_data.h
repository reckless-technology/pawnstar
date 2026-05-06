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

extern const std::array<Bitboard, 64>     kNorth;
extern const std::array<Bitboard, 64>     kNortheast;
extern const std::array<Bitboard, 64>     kEast;
extern const std::array<Bitboard, 64>     kSoutheast;
extern const std::array<Bitboard, 64>     kSouth;
extern const std::array<Bitboard, 64>     kSouthwest;
extern const std::array<Bitboard, 64>     kWest;
extern const std::array<Bitboard, 64>     kNorthwest;
extern const std::array<Bitboard, 64>     kPawnAttacksWhite;
extern const std::array<Bitboard, 64>     kPawnAttacksBlack;
extern const std::array<Bitboard, 64>     kKnightAttacks;
extern const std::array<Bitboard, 64>     kBishopAttacks;
extern const std::array<Bitboard, 64>     kRookAttacks;
extern const std::array<Bitboard, 64>     kQueenAttacks;
extern const std::array<Bitboard, 64>     kKingAttacks;
extern const std::array<Bitboard, 64>     kKingAttacks2;
extern const std::array<Bitboard, 64>     kKingPawnShelterWhite;
extern const std::array<Bitboard, 64>     kKingPawnShelterBlack;
extern const std::array<Bitboard, 64>     kPassedPawnMaskWhite;
extern const std::array<Bitboard, 64>     kPassedPawnMaskBlack;
extern const std::array<Bitboard, 64>     kIsolatedPawnMaskWhite;
extern const std::array<Bitboard, 64>     kIsolatedPawnMaskBlack;
extern const std::array<Bitboard, 64>     kSupportedPawnMaskWhite;
extern const std::array<Bitboard, 64>     kSupportedPawnMaskBlack;
extern const std::array<Bitboard, 64>     kDoubledPawnMaskWhite;
extern const std::array<Bitboard, 64>     kDoubledPawnMaskBlack;
extern const std::array<zobrist_t, 16>    kCastlingRightsHashes;
extern const std::array<zobrist_t, 64>    kEnPassantHashes;
extern const std::array<PextBitboard, 64> kBishopPexts;
extern const std::array<PextBitboard, 64> kRookPexts;
constexpr zobrist_t                       kBlackMoveHash = 0x28AB74D640E50602; ///< Just a big random number.

/// @brief Meta template - syntactic sugar for multi dim std::array types.
/// Refer to https://cpptruths.blogspot.com/2011/10/multi-dimensional-arrays-in-c11.html
/// @tparam T Type of object to store in array.
/// @tparam I Size of first dimension.
/// @tparam ...J Remaining dimensions.
template <class T, size_t I, size_t... J> struct MultiDimArray
{
    using Nested = typename MultiDimArray<T, J...>::type;
    using type   = std::array<Nested, I>;
};
/// @brief Specialization for a single dimension MultiDimArray.
/// @tparam T Type of object to store in array.
/// @tparam I Number of elements.
template <class T, size_t I> struct MultiDimArray<T, I>
{
    using type = std::array<T, I>;
};

extern const MultiDimArray<Bitboard, 64, 64>::type    kInterveningSquares;
extern const MultiDimArray<zobrist_t, 2, 6, 64>::type kPieceSquareHashes;

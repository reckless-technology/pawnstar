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

constexpr zobrist_t kBlackMoveHash = 0x28AB74D640E50602; ///< Zobrist hash for black to move.

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

// ---- Definitions moved from generated_data.cpp (header-only) ----
#include <algorithm>
#include <array>
#include <immintrin.h>
#include <random>
#include <set>
#include <vector>

namespace gendata_detail
{
/// @brief Compass-rose directions (north = forward for white), indices into kShiftFunctions.
enum Direction
{
    kNorth,
    kNortheast,
    kEast,
    kSoutheast,
    kSouth,
    kSouthwest,
    kWest,
    kNorthwest
};

constexpr uint64_t kFileA = 0x0101010101010101; ///< The a-file (LERF mapping).
constexpr uint64_t kFileH = kFileA << 7;        ///< The h-file.

// clang-format off
constexpr uint8_t  FileOf(uint8_t sq)         { return sq & 7;                       } ///< Square index -> file (0..7).
constexpr uint8_t  RankOf(uint8_t sq)         { return sq >> 3;                      } ///< Square index -> rank (0..7).
constexpr uint64_t SqBB(uint8_t sq)           { return 1ull << sq;                   } ///< Square index -> single-square Bitboard.
constexpr uint64_t SqBB(int x, int y)         { return 1ull << (x + 8 * y);          } ///< (file, rank) -> single-square Bitboard.
constexpr bool     IsInBoard(int x, int y)    { return x == (x & 7) && y == (y & 7); } ///< Is (file, rank) on the board?
constexpr uint64_t ShiftNorth(uint64_t b)     { return b << 8;                       } ///< Shift a Bitboard one square north.
constexpr uint64_t ShiftNortheast(uint64_t b) { return (b & ~kFileH) << 9;           } ///< Shift a Bitboard one square northeast.
constexpr uint64_t ShiftEast(uint64_t b)      { return (b & ~kFileH) << 1;           } ///< Shift a Bitboard one square east.
constexpr uint64_t ShiftSoutheast(uint64_t b) { return (b & ~kFileH) >> 7;           } ///< Shift a Bitboard one square southeast.
constexpr uint64_t ShiftSouth(uint64_t b)     { return b >> 8;                       } ///< Shift a Bitboard one square south.
constexpr uint64_t ShiftSouthwest(uint64_t b) { return (b & ~kFileA) >> 9;           } ///< Shift a Bitboard one square southwest.
constexpr uint64_t ShiftWest(uint64_t b)      { return (b & ~kFileA) >> 1;           } ///< Shift a Bitboard one square west.
constexpr uint64_t ShiftNorthwest(uint64_t b) { return (b & ~kFileA) << 7;           } ///< Shift a Bitboard one square northwest.

// clang-format on

using ShiftFn = uint64_t (*)(uint64_t);
constexpr std::array<ShiftFn, 8> kShiftFunctions{ShiftNorth, ShiftNortheast, ShiftEast, ShiftSoutheast,
                                                 ShiftSouth, ShiftSouthwest, ShiftWest, ShiftNorthwest};

/// @brief Ray from a square in one direction (excludes the source square).
constexpr uint64_t RayFrom(uint8_t sq, Direction direction)
{
    uint64_t   result = 0;
    const auto fn     = kShiftFunctions[direction];
    for (uint64_t b = fn(SqBB(sq)); b != 0; b = fn(b))
    {
        result |= b;
    }
    return result;
}

constexpr uint64_t KnightAttacks(uint8_t sq)
{
    constexpr std::array<std::pair<int, int>, 8> kKnightVectors{
        {{-2, -1}, {-2, 1}, {-1, -2}, {-1, 2}, {1, -2}, {1, 2}, {2, -1}, {2, 1}}};
    uint64_t result = 0;
    for (const auto &[dx, dy] : kKnightVectors)
    {
        const int x = FileOf(sq) + dx;
        const int y = RankOf(sq) + dy;
        if (IsInBoard(x, y))
        {
            result |= SqBB(x, y);
        }
    }
    return result;
}

constexpr uint64_t BishopAttacksOnEmptyBoard(uint8_t sq)
{
    return RayFrom(sq, kNortheast) | RayFrom(sq, kNorthwest) | RayFrom(sq, kSoutheast) | RayFrom(sq, kSouthwest);
}

constexpr uint64_t RookAttacksOnEmptyBoard(uint8_t sq)
{
    return RayFrom(sq, kNorth) | RayFrom(sq, kSouth) | RayFrom(sq, kEast) | RayFrom(sq, kWest);
}

constexpr uint64_t KingAttacks(uint8_t sq)
{
    const uint64_t b = SqBB(sq);
    return ShiftNorth(b) | ShiftNortheast(b) | ShiftEast(b) | ShiftSoutheast(b) | ShiftSouth(b) | ShiftSouthwest(b) |
           ShiftWest(b) | ShiftNorthwest(b);
}

/// @brief Squares strictly between two colinear squares (0 if not colinear).
constexpr uint64_t InterveningSquares(uint8_t from, uint8_t to)
{
    constexpr std::array<Direction, 8> kDirections{kNorth, kNortheast, kEast, kSoutheast,
                                                   kSouth, kSouthwest, kWest, kNorthwest};
    for (auto dir : kDirections)
    {
        const uint64_t ray = RayFrom(from, dir);
        if (ray & SqBB(to))
        {
            return ray ^ RayFrom(to, dir) ^ SqBB(to);
        }
    }
    return 0;
}

/// @brief Occupancy mask for one direction (excludes the final edge square, which never blocks).
constexpr uint64_t RayOccupancyMask(uint8_t sq, Direction direction)
{
    uint64_t   result      = 0;
    uint64_t   last_square = 0;
    const auto fn          = kShiftFunctions[direction];
    for (uint64_t b = fn(SqBB(sq)); b != 0; b = fn(b))
    {
        result |= b;
        last_square = b;
    }
    return result ^ last_square;
}

constexpr uint64_t BishopOccupancyMask(uint8_t sq)
{
    return RayOccupancyMask(sq, kNortheast) | RayOccupancyMask(sq, kNorthwest) | RayOccupancyMask(sq, kSoutheast) |
           RayOccupancyMask(sq, kSouthwest);
}

constexpr uint64_t RookOccupancyMask(uint8_t sq)
{
    return RayOccupancyMask(sq, kNorth) | RayOccupancyMask(sq, kSouth) | RayOccupancyMask(sq, kEast) |
           RayOccupancyMask(sq, kWest);
}

/// @brief Sliding move targets in one direction, stopping at (and including) the first occupied square.
constexpr uint64_t RayAttacks(uint64_t occupied_squares, uint8_t sq, Direction direction)
{
    uint64_t   result = 0;
    const auto fn     = kShiftFunctions[direction];
    for (uint64_t b = fn(SqBB(sq)); b != 0; b = fn(b))
    {
        result |= b;
        if (b & occupied_squares)
        {
            break;
        }
    }
    return result;
}

constexpr uint64_t BishopAttacks(uint64_t occupied_squares, uint8_t sq)
{
    return RayAttacks(occupied_squares, sq, kNortheast) | RayAttacks(occupied_squares, sq, kSoutheast) |
           RayAttacks(occupied_squares, sq, kSouthwest) | RayAttacks(occupied_squares, sq, kNorthwest);
}

constexpr uint64_t RookAttacks(uint64_t occupied_squares, uint8_t sq)
{
    return RayAttacks(occupied_squares, sq, kNorth) | RayAttacks(occupied_squares, sq, kEast) |
           RayAttacks(occupied_squares, sq, kSouth) | RayAttacks(occupied_squares, sq, kWest);
}

/// @brief Enumerate every subset of a bit mask (Hacker's Delight carry-rippler).
constexpr std::vector<uint64_t> EnumerateMaskCombinations(uint64_t mask)
{
    std::vector<uint64_t> result;
    uint64_t              n = 0;
    do
    {
        result.push_back(n);
        n = (n - 1) & mask;
    } while (n);
    return result;
}

using BBFn   = uint64_t (*)(uint8_t);
using MaskFn = uint64_t (*)(uint8_t);
using AttFn  = uint64_t (*)(uint64_t, uint8_t);

/// @brief Build a 64-entry Bitboard table from a per-square generator.
constexpr std::array<Bitboard, 64> MakeBitboards(BBFn fn)
{
    std::array<Bitboard, 64> table{};
    for (uint8_t sq = 0; sq != 64; ++sq)
    {
        table[sq] = Bitboard{fn(sq)};
    }
    return table;
}

/// @brief Build one square's pext entry: the de-duplicated attack sets and per-occupancy indices into them.
inline PextBitboard ComputePext(uint8_t sq, MaskFn mask_fn, AttFn attack_fn)
{
    PextBitboard   entry;
    const uint64_t mask               = mask_fn(sq);
    entry.occupancy_mask              = Bitboard{mask};
    const auto            occupancies = EnumerateMaskCombinations(mask);
    std::vector<uint64_t> dense(occupancies.size(), 0);
    for (auto occupancy : occupancies)
    {
        dense[_pext_u64(occupancy, mask)] = attack_fn(occupancy, sq);
    }
    // Compress to unique attack sets (8 bytes each) plus 1-byte indices, to shrink the (large) tables.
    const std::set<uint64_t>    unique_set{dense.begin(), dense.end()};
    const std::vector<uint64_t> unique_attacks{unique_set.begin(), unique_set.end()};
    for (auto attack : dense)
    {
        const auto it = std::find(unique_attacks.begin(), unique_attacks.end(), attack);
        entry.indices.push_back(static_cast<uint8_t>(it - unique_attacks.begin()));
    }
    for (auto attack : unique_attacks)
    {
        entry.attacks.push_back(Bitboard{attack});
    }
    return entry;
}

/// @brief Build the 64-square pext table for a sliding piece.
inline std::array<PextBitboard, 64> MakePexts(MaskFn mask_fn, AttFn attack_fn)
{
    std::array<PextBitboard, 64> table{};
    for (uint8_t sq = 0; sq != 64; ++sq)
    {
        table[sq] = ComputePext(sq, mask_fn, attack_fn);
    }
    return table;
}

constexpr MultiDimArray<Bitboard, 64, 64>::type MakeInterveningSquares()
{
    MultiDimArray<Bitboard, 64, 64>::type table{};
    for (uint8_t from = 0; from != 64; ++from)
    {
        for (uint8_t to = 0; to != 64; ++to)
        {
            table[from][to] = Bitboard{InterveningSquares(from, to)};
        }
    }
    return table;
}

/// @brief The Zobrist tables are filled from this single fixed-seed PRNG. The three table globals below
/// are defined in the historical draw order (piece-square, then castling rights, then en passant); within
/// this translation unit they are dynamically initialised in definition order, so the PRNG is drawn in
/// that order and the values match the old generated file. No table is stored twice.
inline std::mt19937_64 g_zobrist_prng{0xAA55AA55AA55AA55ull};

inline MultiDimArray<zobrist_t, 2, 6, 64>::type MakePieceSquareHashes()
{
    MultiDimArray<zobrist_t, 2, 6, 64>::type table{};
    for (int color = 0; color != 2; ++color)
    {
        for (int piece = 0; piece != 6; ++piece) // pawn..king
        {
            for (uint8_t sq = 0; sq != 64; ++sq)
            {
                table[color][piece][sq] = g_zobrist_prng();
            }
        }
    }
    return table;
}

inline std::array<zobrist_t, 16> MakeCastlingRightsHashes()
{
    std::array<zobrist_t, 16> table{};
    for (uint8_t i = 0; i != 16; ++i)
    {
        table[i] = g_zobrist_prng();
    }
    return table;
}

inline std::array<zobrist_t, 64> MakeEnPassantHashes()
{
    std::array<zobrist_t, 64> table{};
    for (uint8_t i = 0; i != 64; ++i)
    {
        const uint8_t rank = RankOf(i);
        table[i]           = (rank == 2 || rank == 5) ? g_zobrist_prng() : 0;
    }
    return table;
}

// ── The precomputed tables, computed once at program startup (dynamic initialisation). ──────────────
// clang-format off
inline const std::array<Bitboard, 64>     kEastWest            =    MakeBitboards([](uint8_t sq) { return RayFrom(sq, kEast) | RayFrom(sq, kWest); });
inline const std::array<Bitboard, 64>     kPawnAttacksWhite    =    MakeBitboards([](uint8_t sq) { return ShiftNorthwest(SqBB(sq)) | ShiftNortheast(SqBB(sq)); });
inline const std::array<Bitboard, 64>     kPawnAttacksBlack    =    MakeBitboards([](uint8_t sq) { return ShiftSouthwest(SqBB(sq)) | ShiftSoutheast(SqBB(sq)); });
inline const std::array<Bitboard, 64>     kKnightAttacks       = MakeBitboards(KnightAttacks);
inline const std::array<Bitboard, 64>     kBishopAttacks       = MakeBitboards(BishopAttacksOnEmptyBoard);
inline const std::array<Bitboard, 64>     kRookAttacks         = MakeBitboards(RookAttacksOnEmptyBoard);
inline const std::array<Bitboard, 64>     kKingAttacks         = MakeBitboards(KingAttacks);
inline const std::array<PextBitboard, 64> kBishopPexts         = MakePexts(BishopOccupancyMask, BishopAttacks);
inline const std::array<PextBitboard, 64> kRookPexts           = MakePexts(RookOccupancyMask, RookAttacks);

inline const MultiDimArray<Bitboard, 64, 64>::type    kInterveningSquares      = MakeInterveningSquares();
// The zobrist tables below MUST stay in this order: they share the global g_zobrist_prng, so reordering them
// changes every drawn key (and breaks book / TT / repetition compatibility).
inline const MultiDimArray<zobrist_t, 2, 6, 64>::type kPieceSquareHashes       = MakePieceSquareHashes();
inline const std::array<zobrist_t, 16>                kCastlingRightsHashes    = MakeCastlingRightsHashes();
inline const std::array<zobrist_t, 64>                kEnPassantHashes         = MakeEnPassantHashes();
// clang-format on

} // namespace gendata_detail

using gendata_detail::kBishopAttacks;
using gendata_detail::kBishopPexts;
using gendata_detail::kCastlingRightsHashes;
using gendata_detail::kEastWest;
using gendata_detail::kEnPassantHashes;
using gendata_detail::kInterveningSquares;
using gendata_detail::kKingAttacks;
using gendata_detail::kKnightAttacks;
using gendata_detail::kPawnAttacksBlack;
using gendata_detail::kPawnAttacksWhite;
using gendata_detail::kPieceSquareHashes;
using gendata_detail::kRookAttacks;
using gendata_detail::kRookPexts;
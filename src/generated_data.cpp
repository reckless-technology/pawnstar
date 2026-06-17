/// @file generated_data.cpp Definitions of the precomputed lookup tables declared in generated_data.h.
///
/// These tables were once emitted as C++ literals by a separate build-time program
/// (generate_constants.cpp) that the Makefile compiled and ran to produce this file. They are now computed
/// once at program startup — as the dynamic initialisation of these `const` globals — which removes the
/// codegen build step entirely. The computation (and the Zobrist PRNG draw order) is identical to the old
/// generator, so the tables are byte-for-byte the same as the previously generated file.
///
/// All globals live in one translation unit and are defined in dependency order, so their dynamic
/// initialisation runs deterministically (no static-init-order issues): the compute helpers below are pure
/// functions of a square, and the three Zobrist tables are filled from a single fixed-seed PRNG in order.

#include "generated_data.h"

#include <algorithm>
#include <array>
#include <immintrin.h>
#include <random>
#include <set>
#include <vector>

namespace
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

constexpr uint8_t  FileOf(uint8_t sq) { return sq & 7; }
constexpr uint8_t  RankOf(uint8_t sq) { return sq >> 3; }
constexpr uint64_t SqBB(uint8_t sq) { return 1ull << sq; }
constexpr uint64_t SqBB(int x, int y) { return 1ull << (x + 8 * y); }
constexpr bool     IsInBoard(int x, int y) { return x == (x & 7) && y == (y & 7); }

constexpr uint64_t ShiftNorth(uint64_t b) { return b << 8; }
constexpr uint64_t ShiftNortheast(uint64_t b) { return (b & ~kFileH) << 9; }
constexpr uint64_t ShiftEast(uint64_t b) { return (b & ~kFileH) << 1; }
constexpr uint64_t ShiftSoutheast(uint64_t b) { return (b & ~kFileH) >> 7; }
constexpr uint64_t ShiftSouth(uint64_t b) { return b >> 8; }
constexpr uint64_t ShiftSouthwest(uint64_t b) { return (b & ~kFileA) >> 9; }
constexpr uint64_t ShiftWest(uint64_t b) { return (b & ~kFileA) >> 1; }
constexpr uint64_t ShiftNorthwest(uint64_t b) { return (b & ~kFileA) << 7; }

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
constexpr uint64_t QueenAttacksOnEmptyBoard(uint8_t sq)
{
    return BishopAttacksOnEmptyBoard(sq) | RookAttacksOnEmptyBoard(sq);
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
    uint64_t   last_square  = 0;
    const auto fn           = kShiftFunctions[direction];
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
PextBitboard ComputePext(uint8_t sq, MaskFn mask_fn, AttFn attack_fn)
{
    PextBitboard   entry;
    const uint64_t mask = mask_fn(sq);
    entry.occupancy_mask = Bitboard{mask};
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
std::array<PextBitboard, 64> MakePexts(MaskFn mask_fn, AttFn attack_fn)
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

/// @brief All Zobrist tables, filled from a single fixed-seed PRNG in the historical draw order
/// (piece-square, then castling rights, then en passant) so the values match the old generated file.
struct ZobristTables
{
    MultiDimArray<zobrist_t, 2, 6, 64>::type piece_square{};
    std::array<zobrist_t, 16>               castling{};
    std::array<zobrist_t, 64>               en_passant{};
};

ZobristTables MakeZobrist()
{
    std::mt19937_64 prng{0xAA55AA55AA55AA55ull};
    ZobristTables   z;
    for (int color = 0; color != 2; ++color)
    {
        for (int piece = 0; piece != 6; ++piece) // pawn..king
        {
            for (uint8_t sq = 0; sq != 64; ++sq)
            {
                z.piece_square[color][piece][sq] = prng();
            }
        }
    }
    for (uint8_t i = 0; i != 16; ++i)
    {
        z.castling[i] = prng();
    }
    for (uint8_t i = 0; i != 64; ++i)
    {
        const uint8_t rank = RankOf(i);
        z.en_passant[i]    = (rank == 2 || rank == 5) ? prng() : 0;
    }
    return z;
}

const ZobristTables g_zobrist = MakeZobrist();
} // namespace

// ── The precomputed tables, computed once at program startup (dynamic initialisation). ──────────────
const std::array<Bitboard, 64> kEastWest =
    MakeBitboards([](uint8_t sq) { return RayFrom(sq, kEast) | RayFrom(sq, kWest); });
const std::array<Bitboard, 64> kPawnAttacksWhite =
    MakeBitboards([](uint8_t sq) { return ShiftNorthwest(SqBB(sq)) | ShiftNortheast(SqBB(sq)); });
const std::array<Bitboard, 64> kPawnAttacksBlack =
    MakeBitboards([](uint8_t sq) { return ShiftSouthwest(SqBB(sq)) | ShiftSoutheast(SqBB(sq)); });
const std::array<Bitboard, 64> kKnightAttacks = MakeBitboards(KnightAttacks);
const std::array<Bitboard, 64> kBishopAttacks = MakeBitboards(BishopAttacksOnEmptyBoard);
const std::array<Bitboard, 64> kRookAttacks   = MakeBitboards(RookAttacksOnEmptyBoard);
const std::array<Bitboard, 64> kQueenAttacks  = MakeBitboards(QueenAttacksOnEmptyBoard);
const std::array<Bitboard, 64> kKingAttacks   = MakeBitboards(KingAttacks);

const std::array<PextBitboard, 64> kBishopPexts = MakePexts(BishopOccupancyMask, BishopAttacks);
const std::array<PextBitboard, 64> kRookPexts   = MakePexts(RookOccupancyMask, RookAttacks);

const MultiDimArray<Bitboard, 64, 64>::type kInterveningSquares = MakeInterveningSquares();

const MultiDimArray<zobrist_t, 2, 6, 64>::type kPieceSquareHashes    = g_zobrist.piece_square;
const std::array<zobrist_t, 16>               kCastlingRightsHashes  = g_zobrist.castling;
const std::array<zobrist_t, 64>               kEnPassantHashes       = g_zobrist.en_passant;

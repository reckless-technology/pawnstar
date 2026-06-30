#pragma once
/// @file generated_data.h Precomputed lookup tables (attack/ray bitboards, magic sliding-attack tables,
/// intervening-squares masks and Zobrist hashes) used by pawnstar for hashing, move generation and attack
/// detection. The tables are `inline const` globals defined in this header and computed once at startup
/// (dynamic initialisation), so there is no build-time code-generation step.
#include "bitboard.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <cstdint>
#include <map>
#include <random>
#include <vector>

using zobrist_t = uint64_t; ///< Zobrist hash value type.

/// @brief A magic-bitboard entry in the sliding-piece move generator array, for one square of one piece.
/// Maps a masked occupancy to an attack set via a multiply-and-shift hash (hash = (occupancy * magic_) >>
/// shift_). Magic bitboards are portable (plain 64-bit multiply — no pext, fast on every x86-64 CPU
/// including AMD Zen 1/2 where pext is microcoded). The magic_ values are precomputed constants (see
/// kBishopMagicNumbers / kRookMagicNumbers); the tables below are filled once at program startup from them.
///
/// Like the former pext implementation, the hash does not index the attack sets directly: it indexes
/// indices_ (one byte per hash slot) which in turn indexes attacks_ (the de-duplicated 8-byte attack sets).
/// Many distinct occupancies share an attack set (a rook square has 2^12 = 4096 occupancies but only ~100
/// distinct attack sets), so the 1-byte indirection shrinks the tables ~5× (the direct layout would be
/// ~841 KB; this is ~155 KB). Fewer than 256 distinct attack sets per square, so a uint8_t index suffices.
struct MagicBitboard
{
    Bitboard              occupancy_mask_; ///< Relevant-occupancy mask (excludes board edges / final square).
    uint64_t              magic_;          ///< Multiplier hashing a masked occupancy to a hash slot.
    unsigned              shift_;          ///< Right-shift = 64 - popcount(mask); hash range is 1 << popcount.
    std::vector<Bitboard> attacks_;        ///< De-duplicated attack sets (8 bytes each).
    std::vector<uint8_t>  indices_;        ///< Per-hash-slot index into attacks_ (1 byte each).
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

// ---- Out-of-class definitions (header-only build) ----
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

/// @brief Build one square's magic entry from a precomputed magic multiplier (no search — the magic is a
/// known-good constant, kBishopMagicNumbers / kRookMagicNumbers). For each relevant-occupancy subset the
/// magic hash ((occupancy * magic) >> shift) selects a hash slot; the slot's attack set is de-duplicated
/// into attacks_ and the slot's 1-byte index into indices_. The assert guards against a destructive
/// collision (two subsets with different attacks hashing to one slot) should the mask/attack generators
/// ever change — regenerate the magics with tools/dump_magics if it fires.
inline MagicBitboard BuildMagicTable(uint8_t sq, MaskFn mask_fn, AttFn attack_fn, uint64_t magic)
{
    MagicBitboard  entry;
    const uint64_t mask   = mask_fn(sq);
    const unsigned bits   = static_cast<unsigned>(std::popcount(mask));
    entry.occupancy_mask_ = Bitboard{mask};
    entry.magic_          = magic;
    entry.shift_          = 64 - bits;

    const size_t          size = size_t{1} << bits; // number of hash slots = number of occupancy subsets
    std::vector<uint64_t> slot_attacks(size, 0);    // attack set per hash slot (0 = slot never produced)
    std::vector<uint8_t>  filled(size, 0);
    for (uint64_t occupancy : EnumerateMaskCombinations(mask))
    {
        const size_t   idx = static_cast<size_t>((occupancy * magic) >> entry.shift_);
        const uint64_t att = attack_fn(occupancy, sq);
        assert((filled[idx] == 0 || slot_attacks[idx] == att) && "precomputed magic collides — regenerate");
        slot_attacks[idx] = att;
        filled[idx]       = 1;
    }

    // De-duplicate the per-slot attack sets into attacks_, recording each slot's 1-byte index. Unfilled
    // slots (a constructive collision can leave a hash value unused) are never read at runtime — a real
    // occupancy is always one of the enumerated subsets, so its hash slot is always filled — so they index
    // attacks_[0] arbitrarily.
    entry.indices_.assign(size, 0);
    std::map<uint64_t, uint8_t> unique_index;
    for (size_t idx = 0; idx < size; ++idx)
    {
        if (filled[idx] == 0)
        {
            continue;
        }
        auto [it, inserted] = unique_index.try_emplace(slot_attacks[idx], static_cast<uint8_t>(entry.attacks_.size()));
        if (inserted)
        {
            assert(entry.attacks_.size() < 256 && "more than 256 distinct attack sets — uint8_t index too small");
            entry.attacks_.push_back(Bitboard{slot_attacks[idx]});
        }
        entry.indices_[idx] = it->second;
    }
    return entry;
}

/// @brief Build the 64-square magic table for a sliding piece from its precomputed magic constants.
inline std::array<MagicBitboard, 64> MakeMagics(MaskFn mask_fn, AttFn attack_fn, const std::array<uint64_t, 64> &magics)
{
    std::array<MagicBitboard, 64> table{};
    for (uint8_t sq = 0; sq != 64; ++sq)
    {
        table[sq] = BuildMagicTable(sq, mask_fn, attack_fn, magics[sq]);
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

// ── Precomputed magic multipliers ───────────────────────────────────────────────────────────────────
// Known-good collision-free magics for the occupancy masks below, found once by an offline randomised
// search (see tools/dump_magics for the generator) and baked in here so startup just fills the attack
// tables — no per-launch search. If the occupancy-mask or attack generators change, regenerate these.
// clang-format off
inline constexpr std::array<uint64_t, 64> kBishopMagicNumbers = {{
    0x1850104108008810ull, 0x1060512c09024090ull, 0x4021010112000010ull, 0x0084440280112000ull,
    0x2004042000000840ull, 0xc00088200a238104ull, 0x0004040202109004ull, 0x9102002c06081404ull,
    0x4540101042080040ull, 0x1000900401104604ull, 0x022211480281000dull, 0x0180310502008040ull,
    0x8400011040000812ull, 0x0200010c2a400a80ull, 0x00840b80b0282020ull, 0x0001009084112020ull,
    0x5106012008100104ull, 0x4002498802040420ull, 0xe810000800882408ull, 0x0041001804110010ull,
    0x4011001811400800ull, 0x2202002022100200ull, 0x0302100482100201ull, 0x0800402611008800ull,
    0x0402401090040881ull, 0x0482020048080810ull, 0x1042900818440020ull, 0x1a40104024004080ull,
    0x0685010004104000ull, 0x400400210500a011ull, 0x001410a0340c8400ull, 0x40040040008a1080ull,
    0x109004601050028aull, 0x029208202442024cull, 0x2122082400120800ull, 0x0010400a00002200ull,
    0x2240010200030048ull, 0x0021101080030814ull, 0x0008280480044200ull, 0x0002408a01850440ull,
    0x0008111090004880ull, 0x6100540420180420ull, 0x0210202028005000ull, 0x5800084200810801ull,
    0x000c404081201a00ull, 0x8020200041840140ull, 0x0092041804a00202ull, 0x806440a20a020340ull,
    0x0444110402222280ull, 0x0000862101200010ull, 0x1000004218040312ull, 0xa000080484040000ull,
    0x2141111002088000ull, 0x5000400508548100ull, 0x1020097000808001ull, 0x4005010802028860ull,
    0x00a0208808080281ull, 0x012000a09c0c2082ull, 0x1001020044040400ull, 0x4002100300208808ull,
    0x4040000010204840ull, 0x18001008b0018200ull, 0x0902425002008104ull, 0x5004204202082900ull,
}};
inline constexpr std::array<uint64_t, 64> kRookMagicNumbers = {{
    0x0080081080204000ull, 0x8040002000100042ull, 0x008010008020000dull, 0x0480043000804800ull,
    0x0480040002810800ull, 0x0200080102000410ull, 0x020001220000c804ull, 0x8200008204004133ull,
    0x0400800040008020ull, 0x8408804000200080ull, 0x1041806000803000ull, 0x101c800800900080ull,
    0x0002802400080080ull, 0x2805004300040008ull, 0x0002000801040200ull, 0x001a802080004100ull,
    0x8180014000600040ull, 0x0019010020804004ull, 0x8188420020108200ull, 0x2429210010030900ull,
    0x0198008004008008ull, 0x4089818004000200ull, 0x5000040008100102ull, 0x04000200005108a4ull,
    0x0144208280104000ull, 0x0080400040201000ull, 0x2001084100200010ull, 0x0000120200084220ull,
    0x0000040080080081ull, 0xa001000300080400ull, 0x28108a8c00100108ull, 0xc304010200008044ull,
    0x0000824001800020ull, 0x0830402000401008ull, 0x0320001041002100ull, 0x4050008010800800ull,
    0x8000800400800801ull, 0x0002000400808002ull, 0x1000c80104000210ull, 0x01c0804402000081ull,
    0x8002c00021828000ull, 0x0010002000414008ull, 0x0c01002000410010ull, 0x0002000840120020ull,
    0x4000040008008080ull, 0x0002000810020004ull, 0x2001020108240070ull, 0xa801000080410002ull,
    0x0500410c80220200ull, 0x30ca200482400080ull, 0x0d00820040182200ull, 0x0401000820100100ull,
    0x1480040080080080ull, 0x8002800400020080ull, 0x4040084210014400ull, 0x8019241100488a00ull,
    0x26034300201a8001ull, 0x0240001045082081ull, 0x0000200100400811ull, 0x0030002008041101ull,
    0x1021006800100417ull, 0x2006000144100822ull, 0x8200180081102a04ull, 0x00200a84012c4502ull,
}};
// clang-format on

// ── The precomputed tables, computed once at program startup (dynamic initialisation). ──────────────
// clang-format off
inline const std::array<Bitboard, 64>     kEastWest            = MakeBitboards([](uint8_t sq) { return RayFrom(sq, kEast) | RayFrom(sq, kWest); });
inline const std::array<Bitboard, 64>     kPawnAttacksWhite    = MakeBitboards([](uint8_t sq) { return ShiftNorthwest(SqBB(sq)) | ShiftNortheast(SqBB(sq)); });
inline const std::array<Bitboard, 64>     kPawnAttacksBlack    = MakeBitboards([](uint8_t sq) { return ShiftSouthwest(SqBB(sq)) | ShiftSoutheast(SqBB(sq)); });
inline const std::array<Bitboard, 64>     kKnightAttacks       = MakeBitboards(KnightAttacks);
inline const std::array<Bitboard, 64>     kBishopAttacks       = MakeBitboards(BishopAttacksOnEmptyBoard);
inline const std::array<Bitboard, 64>     kRookAttacks         = MakeBitboards(RookAttacksOnEmptyBoard);
inline const std::array<Bitboard, 64>     kKingAttacks         = MakeBitboards(KingAttacks);
inline const std::array<MagicBitboard, 64> kBishopMagics       = MakeMagics(BishopOccupancyMask, BishopAttacks, kBishopMagicNumbers);
inline const std::array<MagicBitboard, 64> kRookMagics         = MakeMagics(RookOccupancyMask, RookAttacks, kRookMagicNumbers);

inline const MultiDimArray<Bitboard, 64, 64>::type    kInterveningSquares      = MakeInterveningSquares();
// The zobrist tables below MUST stay in this order: they share the global g_zobrist_prng, so reordering them
// changes every drawn key (and breaks book / TT / repetition compatibility).
inline const MultiDimArray<zobrist_t, 2, 6, 64>::type kPieceSquareHashes       = MakePieceSquareHashes();
inline const std::array<zobrist_t, 16>                kCastlingRightsHashes    = MakeCastlingRightsHashes();
inline const std::array<zobrist_t, 64>                kEnPassantHashes         = MakeEnPassantHashes();
// clang-format on

} // namespace gendata_detail

using gendata_detail::kBishopAttacks;
using gendata_detail::kBishopMagics;
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
using gendata_detail::kRookMagics;
/// @file generate_constants.cpp Precomputes, at compile time, constants used by pawnstar.
/// This program is compiled and then executed by the Makefile to create the "generated_data.cpp" file which is then
/// used in the compilation of the main program.

#include <array>
#include <bit>
#include <cctype>
#include <cinttypes>
#include <cstdio>
#include <format>
#include <functional>
#include <immintrin.h>
#include <iostream>
#include <map>
#include <random>
#include <set>
#include <string_view>
#include <vector>

/// @brief Chess piece types.
enum Piece
{
    kNone,   ///< No piece / empty square.
    kPawn,   ///< Pawn.
    kKnight, ///< Knight.
    kBishop, ///< Bishop.
    kRook,   ///< Rook.
    kQueen,  ///< Queen.
    kKing,   ///< King.
};

/// @brief Compass rose directions from white's perspective, i.e. north is "forward" for white.
enum Direction
{
    kNorth,     ///< Up the board (toward rank 8).
    kNortheast, ///< Up and to the right.
    kEast,      ///< To the right (toward the h-file).
    kSoutheast, ///< Down and to the right.
    kSouth,     ///< Down the board (toward rank 1).
    kSouthwest, ///< Down and to the left.
    kWest,      ///< To the left (toward the a-file).
    kNorthwest, ///< Up and to the left.
};

constexpr uint64_t                        kFileA = 0x0101010101010101; ///< The a-file (LERF bitboard mapping).
constexpr uint64_t                        kFileH = kFileA << 7;        ///< The h-file.
constexpr std::array<std::string_view, 7> kPieceNames{"none", "pawn",  "knight", "bishop",
                                                      "rook", "queen", "king"}; ///< Piece names by index.
constexpr std::array<std::string_view, 2> kColorNames{"white", "black"};        ///< Color names by index.
std::mt19937_64                           prng{0xAA55AA55AA55AA55};             ///< PRNG for generating Zobrist hashes.

// clang-format off
constexpr uint8_t       FileOf(uint8_t sq)          { return sq  & 7; }                         ///< Convert square index to file.
constexpr uint8_t       RankOf(uint8_t sq)          { return sq >> 3; }                         ///< Convert square index to rank.
constexpr char          FileChar(uint8_t sq)        { return (char)('a' + FileOf(sq)); }        ///< Convert square index to file character.
constexpr char          RankChar(uint8_t sq)        { return (char)('1' + RankOf(sq)); }        ///< Convert square index to rank character.
constexpr std::string   SquareName(uint8_t sq)      { return {FileChar(sq), RankChar(sq)}; }    ///< Convert square index to name.
constexpr uint64_t      Bitboard(uint8_t sq)        { return 1ull << sq; }                      ///< Convert square index to Bitboard.
constexpr uint64_t      Bitboard(int x, int y)      { return 1ull << (x + 8 * y); }             ///< Convert square (file,rank) co-ords to Bitboard.
constexpr bool          IsInBoard(int x, int y)     { return x == (x & 7) && y == (y & 7); }    ///< Is this square on the board.
constexpr uint64_t      ShiftNorth(uint64_t b)      { return b << 8; }                          ///< Shift a Bitboard one square to the north.
constexpr uint64_t      ShiftNortheast(uint64_t b)  { return (b & ~kFileH) << 9; }              ///< Shift a Bitboard one square to the northeast.
constexpr uint64_t      ShiftEast(uint64_t b)       { return (b & ~kFileH) << 1; }              ///< Shift a Bitboard one square to the east.
constexpr uint64_t      ShiftSoutheast(uint64_t b)  { return (b & ~kFileH) >> 7; }              ///< Shift a Bitboard one square to the southeast.
constexpr uint64_t      ShiftSouth(uint64_t b)      { return b >> 8; }                          ///< Shift a Bitboard one square to the south.
constexpr uint64_t      ShiftSouthwest(uint64_t b)  { return (b & ~kFileA) >> 9; }              ///< Shift a Bitboard one square to the southwest.
constexpr uint64_t      ShiftWest(uint64_t b)       { return (b & ~kFileA) >> 1; }              ///< Shift a Bitboard one square to the west.
constexpr uint64_t      ShiftNorthwest(uint64_t b)  { return (b & ~kFileA) << 7; }              ///< Shift a Bitboard one square to the northwest.

// clang-format on

/// @brief Function pointer for bitboard shift.
using ShiftFn = uint64_t (*)(uint64_t);

/// @brief Shift functions for each compass direction.
constexpr std::array<ShiftFn, 8> kShiftFunctions{ShiftNorth, ShiftNortheast, ShiftEast, ShiftSoutheast,
                                                 ShiftSouth, ShiftSouthwest, ShiftWest, ShiftNorthwest};

/// @brief Generate a ray from a square in a single compass direction.
/// @param sq Source square index.
/// @param direction Compass direction.
/// @return Ray from sq in direction specified, excluding source square.
constexpr uint64_t RayFrom(uint8_t sq, Direction direction)
{
    uint64_t result = 0;
    auto     fn     = kShiftFunctions[direction];
    for (uint64_t b = fn(Bitboard(sq)); b != 0; b = fn(b))
    {
        result |= b;
    }
    return result;
}

/// @brief Knight attacks.
/// @param sq Square index.
/// @return Squares attacked by a knight standing on sq.
constexpr uint64_t KnightAttacks(uint8_t sq)
{
    // The 8 directions in which a knight can jump (dx,dy)
    // clang-format off
    constexpr std::array<std::pair<int, int>, 8> kKnightVectors = 
    {
        { {-2, -1}, {-2, 1}, {-1, -2}, {-1, 2}, {1, -2}, {1, 2}, {2, -1}, {2, 1} }
    };
    // clang-format on
    uint64_t result = 0;
    for (const auto &[dx, dy] : kKnightVectors)
    {
        const int x = FileOf(sq) + dx;
        const int y = RankOf(sq) + dy;
        if (IsInBoard(x, y))
        {
            result |= Bitboard(x, y);
        }
    }
    return result;
}

/// @brief Bishop attacks on an otherwise empty board.
/// @param sq Square index.
/// @return Bishop attacks.
constexpr uint64_t BishopAttacksOnEmptyBoard(uint8_t sq)
{
    return RayFrom(sq, kNortheast) | RayFrom(sq, kNorthwest) | RayFrom(sq, kSoutheast) | RayFrom(sq, kSouthwest);
}

/// @brief Rook attacks on an otherwise empty board.
/// @param sq Square index.
/// @return Rook attacks.
constexpr uint64_t RookAttacksOnEmptyBoard(uint8_t sq)
{
    return RayFrom(sq, kNorth) | RayFrom(sq, kSouth) | RayFrom(sq, kEast) | RayFrom(sq, kWest);
}

/// @brief Queen attacks on an otherwise empty board.
/// @param sq Square index.
/// @return Queen attacks.
constexpr uint64_t QueenAttacksOnEmptyBoard(uint8_t sq)
{
    return BishopAttacksOnEmptyBoard(sq) | RookAttacksOnEmptyBoard(sq);
}

/// @brief King fill attacks.
/// @param b Bitboard containing King square.
/// @return Squares attacked by King standing on b.
constexpr uint64_t KingFill(uint64_t b)
{
    return ShiftNorth(b) | ShiftNortheast(b) | ShiftEast(b) | ShiftSoutheast(b) | ShiftSouth(b) | ShiftSouthwest(b) |
           ShiftWest(b) | ShiftNorthwest(b);
}

/// @brief King attacks.
/// @param sq Square index.
/// @return King attacks by a king standing on sq.
constexpr uint64_t KingAttacks(uint8_t sq)
{
    return KingFill(Bitboard(sq));
}

/// @brief Intervening squares on colinear ray.
/// @param from Source square index.
/// @param to Destination square index.
/// @return If from and to are colinear, the set of squares between them.
constexpr uint64_t InterveningSquares(uint8_t from, uint8_t to)
{
    constexpr std::array<Direction, 8> kDirections = {kNorth, kNortheast, kEast, kSoutheast,
                                                      kSouth, kSouthwest, kWest, kNorthwest};
    for (auto dir : kDirections)
    {
        const uint64_t ray = RayFrom(from, dir);
        if (ray & Bitboard(to))
        {
            return ray ^ RayFrom(to, dir) ^ Bitboard(to);
        }
    }
    return 0;
}

/// @brief Occupancy mask for a single direction.
/// Occupancy masks exclude the final end square of the ray as this does not affect slider move targets.
/// @param sq Square index.
/// @param direction Compass direction.
/// @return Occupancy mask for that sq and direction.
constexpr uint64_t RayOccupancyMask(uint8_t sq, Direction direction)
{
    uint64_t result      = 0;
    uint64_t last_square = 0;
    auto     fn          = kShiftFunctions[direction];
    for (uint64_t b = fn(Bitboard(sq)); b != 0; b = fn(b))
    {
        result |= b;
        last_square = b;
    }
    return result ^ last_square; // Exclude the final square (if any).
}

/// @brief Bishop occupancy mask.
/// @param sq Square index.
/// @return Bishop sliding move occupancy mask for source square sq.
constexpr uint64_t BishopOccupancyMask(uint8_t sq)
{
    return RayOccupancyMask(sq, kNortheast) | RayOccupancyMask(sq, kNorthwest) | RayOccupancyMask(sq, kSoutheast) |
           RayOccupancyMask(sq, kSouthwest);
}

/// @brief Rook occupancy mask.
/// @param sq Square index.
/// @return Rook sliding move occupancy mask for source square sq.
constexpr uint64_t RookOccupancyMask(uint8_t sq)
{
    return RayOccupancyMask(sq, kNorth) | RayOccupancyMask(sq, kSouth) | RayOccupancyMask(sq, kEast) |
           RayOccupancyMask(sq, kWest);
}

/// @brief Move targets in one direction for sliding pieces considering occupancy.
/// @param occupied_squares Set of squares with a piece on them.
/// @param sq Source square index.
/// @param direction Compass rose direction.
/// @return Set of squares attacked in the specified direction.
constexpr uint64_t RayAttacks(uint64_t occupied_squares, uint8_t sq, Direction direction)
{
    uint64_t result = 0;
    auto     fn     = kShiftFunctions[direction];
    for (uint64_t b = fn(Bitboard(sq)); b != 0; b = fn(b))
    {
        result |= b;
        if (b & occupied_squares)
        {
            break;
        }
    }
    return result;
}

/// @brief Bishop move targets.
/// @param occupied_squares Set of squares occupied by a piece.
/// @param sq Source square index.
/// @return Squares to which a bishop on sq can slide to.
constexpr uint64_t BishopAttacks(uint64_t occupied_squares, uint8_t sq)
{
    return RayAttacks(occupied_squares, sq, kNortheast) | RayAttacks(occupied_squares, sq, kSoutheast) |
           RayAttacks(occupied_squares, sq, kSouthwest) | RayAttacks(occupied_squares, sq, kNorthwest);
}

/// @brief Rook move targets.
/// @param occupied_squares Set of squares occupied by a piece.
/// @param sq Source square index.
/// @return Squares to which a rook on sq can slide to.
constexpr uint64_t RookAttacks(uint64_t occupied_squares, uint8_t sq)
{
    return RayAttacks(occupied_squares, sq, kNorth) | RayAttacks(occupied_squares, sq, kEast) |
           RayAttacks(occupied_squares, sq, kSouth) | RayAttacks(occupied_squares, sq, kWest);
}

/// @brief Given a bit mask, enumerate all the possible combinations of bits set from that mask.
/// @param mask The mask value.
/// @return Combinatorial subsets of mask.
constexpr std::vector<uint64_t> EnumerateMaskCombinations(uint64_t mask)
{
    std::vector<uint64_t> result;
    uint64_t              n = 0;
    do
    {
        result.push_back(n);
        n = (n - 1) & mask; // From Hacker's delight!
    } while (n);
    return result;
}

/// @brief Parameters for a pext bitboard for a single square and piece type.
struct PextBitboard
{
    uint64_t              mask;    ///< Occupancy mask.
    std::vector<uint64_t> attacks; ///< The discrete (unique) attack vectors.
    std::vector<uint8_t>  indices; ///< Indices into the discrete attack vector array.
};

/// @brief Maps a square to an occupancy mask for that square.
using MaskFn = uint64_t (*)(uint8_t);

/// @brief Maps occupied squares and location to attacked targets by a sliding piece.
using AttackFn = uint64_t (*)(uint64_t, uint8_t);

/// @brief Compute a pext bitboard entry, for one sliding piece type at one location.
/// @param sq Square index.
/// @param mask_fn Occupancy mask function for this piece.
/// @param attack_fn Attacked squares function for this piece.
/// @return The sliding move targets for this piece at this location considering all relevant occupancies.
PextBitboard ComputePext(uint8_t sq, MaskFn mask_fn, AttackFn attack_fn)
{
    PextBitboard result;
    result.mask            = mask_fn(sq);
    const auto occupancies = EnumerateMaskCombinations(result.mask);
    result.attacks.assign(occupancies.size(), 0);
    for (auto occupancy : occupancies)
    {
        const auto index      = _pext_u64(occupancy, result.mask);
        result.attacks[index] = attack_fn(occupancy, sq);
    }
    // Compress the actual attacks set down to a set of indices into unique attacks.
    // Since the indices are only 1 byte each and the attacks are 8 bytes each, this saves a ton of space in the final
    // move entry tables, which helps with cache pressure at the expense of one extra indirection.
    std::set<uint64_t>    attacks_set{result.attacks.begin(), result.attacks.end()}; // Make unique.
    std::vector<uint64_t> unique_attacks{attacks_set.begin(), attacks_set.end()};
    for (auto attack : result.attacks)
    {
        const auto iter = std::ranges::find(unique_attacks, attack);
        result.indices.push_back(iter - unique_attacks.begin());
    }
    result.attacks = unique_attacks;
    return result;
}

/// @brief A pext Bitboard search vector entry.
struct PiecePextGenerator
{
    std::string_view name;      ///< Piece name (bishop or rook).
    AttackFn         attack_fn; ///< Attacked squares function.
    MaskFn           mask_fn;   ///< Occupancy mask function.
};

// clang-format off
/// @brief The pext generators for the two sliding piece types that use pext lookup.
constexpr std::array<PiecePextGenerator, 2> kPieceGenerators =
{{
    { "kBishop", BishopAttacks, BishopOccupancyMask },
    { "kRook",   RookAttacks,   RookOccupancyMask   },
}};
// clang-format on

/// Maps a location to a set of squares for that location.
using GeneratorFn = uint64_t (*)(uint8_t);

/// @brief A generator to map a square index to a set of target squares for that index, e.g. "All squares north of
/// here", "Squares attacked by a queen standing here" etc.
struct Generator
{
    std::string_view name;     ///< Name of generated output.
    GeneratorFn      function; ///< Function to generate output.
};

/// @brief Generators for the main precomputed Bitboard arrays.
constexpr std::array<Generator, 15> bitboard_generators = {{
    // clang-format off
    { "kNorth",                     [](uint8_t sq) constexpr { return RayFrom(sq, kNorth);                                          }   },
    { "kNortheast",                 [](uint8_t sq) constexpr { return RayFrom(sq, kNortheast);                                      }   },
    { "kEast",                      [](uint8_t sq) constexpr { return RayFrom(sq, kEast);                                           }   },
    { "kSoutheast",                 [](uint8_t sq) constexpr { return RayFrom(sq, kSoutheast);                                      }   },
    { "kSouth",                     [](uint8_t sq) constexpr { return RayFrom(sq, kSouth);                                          }   },
    { "kSouthwest",                 [](uint8_t sq) constexpr { return RayFrom(sq, kSouthwest);                                      }   },
    { "kWest",                      [](uint8_t sq) constexpr { return RayFrom(sq, kWest);                                           }   },
    { "kNorthwest",                 [](uint8_t sq) constexpr { return RayFrom(sq, kNorthwest);                                      }   },
    { "kPawnAttacksWhite",          [](uint8_t sq) constexpr { return ShiftNorthwest(Bitboard(sq)) | ShiftNortheast(Bitboard(sq));  }   },
    { "kPawnAttacksBlack",          [](uint8_t sq) constexpr { return ShiftSouthwest(Bitboard(sq)) | ShiftSoutheast(Bitboard(sq));  }   },
    { "kKnightAttacks",             KnightAttacks                                                                                       },
    { "kBishopAttacks",             BishopAttacksOnEmptyBoard                                                                           },
    { "kRookAttacks",               RookAttacksOnEmptyBoard                                                                             },
    { "kQueenAttacks",              QueenAttacksOnEmptyBoard                                                                            },
    { "kKingAttacks",               KingAttacks                                                                                         },
    // clang-format on
}};

/// @brief Generate the constant bitboard arrays.
static void GenerateBitboards()
{
    for (const auto &generator : bitboard_generators)
    {
        std::cout << std::format("extern const std::array<Bitboard, 64> {} = \n{{", generator.name);
        for (uint8_t sq = 0; sq != 64; ++sq)
        {
            const uint64_t b = generator.function(sq);
            std::cout << std::format("\n    0x{:016X}, // {} popcnt {:2}", b, SquareName(sq), std::popcount(b));
        }
        std::cout << std::format("\n}};\n");
    }
}

/// @brief Generate the intervening squares array.
static void GenerateInterveningSquares()
{
    std::cout << std::format("extern const MultiDimArray<Bitboard, 64, 64>::type kInterveningSquares = \n{{ {{");
    for (uint8_t i = 0; i != 64; ++i)
    {
        std::cout << std::format("\n    {{ {{ // square {}", SquareName(i));
        for (uint8_t j = 0; j != 64; ++j)
        {
            if ((j & 7) == 0)
            {
                std::cout << std::format("\n        ");
            }
            std::cout << std::format("0x{:016X},", InterveningSquares(i, j));
        }
        std::cout << std::format("\n    }} }},");
    }
    std::cout << std::format("\n}} }};\n");
}

/// @brief Generate Zobrist hash keys for pieces, castling rights and en passant capture.
static void GenerateHashes()
{
    std::cout << std::format("extern const MultiDimArray<zobrist_t, 2, 6, 64>::type kPieceSquareHashes = \n{{ {{");
    for (int color = 0; color != 2; ++color)
    {
        std::cout << std::format("\n    {{ {{");
        for (int piece = kPawn; piece <= kKing; ++piece)
        {
            std::cout << std::format("\n        {{ {{   // {} {}", kColorNames[color], kPieceNames[piece]);
            for (uint8_t i = 0; i != 64; ++i)
            {
                if ((i & 7) == 0)
                {
                    std::cout << std::format("\n            ");
                }
                std::cout << std::format("0x{:016X},", prng());
            }
            std::cout << std::format("\n        }} }},");
        }
        std::cout << std::format("\n    }} }},");
    }
    std::cout << std::format("\n}} }};\n");
    std::cout << std::format("extern const std::array<zobrist_t, 16> kCastlingRightsHashes = \n{{");
    for (uint8_t i = 0; i != 16; ++i)
    {
        if ((i & 7) == 0)
        {
            std::cout << std::format("\n    ");
        }
        std::cout << std::format("0x{:016X},", prng());
    }
    std::cout << std::format("\n}};\n");
    std::cout << std::format("extern const std::array<zobrist_t, 64> kEnPassantHashes = \n{{");
    for (uint8_t i = 0; i != 64; ++i)
    {
        if ((i & 7) == 0)
        {
            std::cout << std::format("\n    ");
        }
        const uint8_t rank = RankOf(i);
        std::cout << std::format("0x{:016X},", rank == 2 || rank == 5 ? prng() : 0);
    }
    std::cout << std::format("\n}};\n");
}

/// @brief Generate pext bitboards for bishop and rook sliding attacks.
static void GeneratePextBitboards(void)
{
    for (const PiecePextGenerator &piece : kPieceGenerators)
    {
        // First print the arrays for the discrete attacks and the attack indices.
        std::vector<uint64_t> masks;
        for (uint8_t sq = 0; sq != 64; ++sq)
        {
            auto pext = ComputePext(sq, piece.mask_fn, piece.attack_fn);
            masks.push_back(pext.mask);
            std::cout << std::format("static const std::array<uint8_t, {}> {}PextIndices{} = \n{{", pext.indices.size(),
                                     piece.name, SquareName(sq));
            for (std::size_t i = 0; i != pext.indices.size(); ++i)
            {
                if (i % 16 == 0)
                {
                    std::cout << std::format("\n    ");
                }
                std::cout << std::format("0x{:02X}, ", pext.indices[i]);
            }
            std::cout << std::format("\n}};\n");
            std::cout << std::format("static const std::array<Bitboard, {}> {}PextAttacks{} = \n{{",
                                     pext.attacks.size(), piece.name, SquareName(sq));
            for (std::size_t i = 0; i != pext.attacks.size(); ++i)
            {
                if (i % 8 == 0)
                {
                    std::cout << std::format("\n    ");
                }
                std::cout << std::format("0x{:016X},", pext.attacks[i]);
            }
            std::cout << std::format("\n}};\n");
        }
        // Next print the PextBitboard for this piece / square combination.
        std::cout << std::format("extern constexpr std::array<PextBitboard, 64> {}Pexts = \n{{ {{\n", piece.name);
        for (uint8_t i = 0; i != 64; ++i)
        {
            std::cout << std::format("    {{\n");
            std::cout << std::format("        .occupancy_mask = 0x{:016X},\n", masks[i]);
            std::cout << std::format("        .attacks        = {}PextAttacks{},\n", piece.name, SquareName(i));
            std::cout << std::format("        .indices        = {}PextIndices{},\n", piece.name, SquareName(i));
            std::cout << std::format("    }},\n");
        }
        std::cout << std::format("}} }};\n");
    }
}

/// @brief Main entry point. Prints to stdout which gets redirected in the Makefile to create the generated data source
/// file which is then used by the main program.
int main()
{
    std::cout << "// This file was generated on " __DATE__ " at " __TIME__ "\n";
    std::cout << "#include \"generated_data.h\"\n";
    GenerateBitboards();
    GenerateInterveningSquares();
    GenerateHashes();
    GeneratePextBitboards();
    return 0;
}

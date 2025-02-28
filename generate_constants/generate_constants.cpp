/// @file Precomputes, at compile time, constants used by pawnstar.
/// This program is compiled and then executed by the Makefile to create the "generated_data.cpp" file which is then
/// used in the compilation of the main program.

#include <array>
#include <bit>
#include <cctype>
#include <cinttypes>
#include <cstdio>
#include <format>
#include <functional>
#include <iostream>
#include <random>
#include <set>
#include <string_view>
#include <vector>

/// @brief Chess piece types.
enum Piece
{
    NO_PIECE,
    PAWN,
    KNIGHT,
    BISHOP,
    ROOK,
    QUEEN,
    KING,
};

/// @brief Compass rose directions from white's perspective, i.e. north is "forward" for white.
enum Direction
{
    NORTH,
    NORTHEAST,
    EAST,
    SOUTHEAST,
    SOUTH,
    SOUTHWEST,
    WEST,
    NORTHWEST,
};

constexpr uint64_t                        FILE_A = 0x0101010101010101; ///< Using LERF bitboard mapping.
constexpr uint64_t                        FILE_H = FILE_A << 7;
constexpr std::array<std::string_view, 7> piece_names{"none", "pawn", "knight", "bishop", "rook", "queen", "king"};
constexpr std::array<std::string_view, 2> color_names{"white", "black"};
std::mt19937_64                           prng{0xAA55AA55AA55AA55};

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
constexpr uint64_t      ShiftNortheast(uint64_t b)  { return (b & ~FILE_H) << 9; }              ///< Shift a Bitboard one square to the northeast.
constexpr uint64_t      ShiftEast(uint64_t b)       { return (b & ~FILE_H) << 1; }              ///< Shift a Bitboard one square to the east.
constexpr uint64_t      ShiftSoutheast(uint64_t b)  { return (b & ~FILE_H) >> 7; }              ///< Shift a Bitboard one square to the southeast.
constexpr uint64_t      ShiftSouth(uint64_t b)      { return b >> 8; }                          ///< Shift a Bitboard one square to the south.
constexpr uint64_t      ShiftSouthwest(uint64_t b)  { return (b & ~FILE_A) >> 9; }              ///< Shift a Bitboard one square to the southwest.
constexpr uint64_t      ShiftWest(uint64_t b)       { return (b & ~FILE_A) >> 1; }              ///< Shift a Bitboard one square to the west.
constexpr uint64_t      ShiftNorthwest(uint64_t b)  { return (b & ~FILE_A) << 7; }              ///< Shift a Bitboard one square to the northwest.
// clang-format on

/// @brief Function pointer for bitboard shift.
using ShiftFn = uint64_t (*)(uint64_t);

/// @brief Shift functions for each compass direction.
constexpr std::array<ShiftFn, 8> shift_functions{ShiftNorth, ShiftNortheast, ShiftEast, ShiftSoutheast,
                                                 ShiftSouth, ShiftSouthwest, ShiftWest, ShiftNorthwest};

/// @brief Generate a ray from a square in a single compass direction.
/// @param Source Square index.
/// @param direction Compass direction.
/// @return Ray from sq in direction specified, excluding source square.
constexpr uint64_t RayFrom(uint8_t sq, Direction direction)
{
    uint64_t result = 0;
    ShiftFn  fn     = shift_functions[direction];
    for (uint64_t b = fn(Bitboard(sq)); b != 0; b = fn(b))
    {
        result |= b;
    }
    return result;
}

/// @brief Knight attacks from square sq.
/// @param sq Square index.
/// @return Squares attacked by a knight standing on sq.
constexpr uint64_t KnightAttacks(uint8_t sq)
{
    // The 8 directions in which a knight can jump (dx,dy)
    constexpr std::array<std::pair<int, int>, 8> knight_vectors = {
        {{-2, -1}, {-2, 1}, {-1, -2}, {-1, 2}, {1, -2}, {1, 2}, {2, -1}, {2, 1}}};
    uint64_t result = 0;
    for (const auto &[dx, dy] : knight_vectors)
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
    return RayFrom(sq, NORTHEAST) | RayFrom(sq, NORTHWEST) | RayFrom(sq, SOUTHEAST) | RayFrom(sq, SOUTHWEST);
}

/// @brief Rook attacks on an otherwise empty board.
/// @param sq Square index.
/// @return Rook attacks.
constexpr uint64_t RookAttacksOnEmptyBoard(uint8_t sq)
{
    return RayFrom(sq, NORTH) | RayFrom(sq, SOUTH) | RayFrom(sq, EAST) | RayFrom(sq, WEST);
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

/// @brief King attacks in 2 moves.
/// @param sq Square index.
/// @return Squares reachable by a king in 2 moves.
constexpr uint64_t KingAttacks2(uint8_t sq)
{
    return KingFill(KingFill(Bitboard(sq))) & ~Bitboard(sq);
}

/// @brief King pawn shelter white.
/// @param sq King square index.
/// @return Pawns in front of the King.
constexpr uint64_t KingPawnShelterWhite(uint8_t sq)
{
    const uint64_t b = Bitboard(sq);
    return sq == 4 ? 0 : ShiftNorthwest(b) | ShiftNorth(b) | ShiftNortheast(b); // Ignore square e1.
}

/// @brief King pawn shelter black.
/// @param sq King square index.
/// @return Pawns in front of the King.
constexpr uint64_t KingPawnShelterBlack(uint8_t sq)
{
    const uint64_t b = Bitboard(sq);
    return sq == 60 ? 0 : ShiftSouthwest(b) | ShiftSouth(b) | ShiftSoutheast(b); // Ignore square e8.
}

/// @brief Passed pawn mask for white.
/// @param sq Square index.
/// @return Squares which must be free of black pawns for a white pawn on sq to be passed.
constexpr uint64_t PassedPawnMaskWhite(uint8_t sq)
{
    const uint64_t b = RayFrom(sq, NORTH);
    return b | ShiftWest(b) | ShiftEast(b);
}

/// @brief Passed pawn mask for black.
/// @param sq Square index.
/// @return Squares which must be free of white pawns for a black pawn on sq to be passed.
constexpr uint64_t PassedPawnMaskBlack(uint8_t sq)
{
    const uint64_t b = RayFrom(sq, SOUTH);
    return b | ShiftWest(b) | ShiftEast(b);
}

/// @brief Isolated pawn mask for white.
/// @param sq Square index.
/// @return Squares which must contain at least one white pawn else the pawn on sq is isolated.
constexpr uint64_t IsolatedPawnMaskWhite(uint8_t sq)
{
    const uint64_t b = FILE_A << FileOf(sq);
    return ShiftWest(b) | ShiftEast(b);
}

/// @brief Isolated pawn mask for black.
/// @param sq Square index.
/// @return Squares which must contain at least one black pawn else the pawn on sq is isolated.
constexpr uint64_t IsolatedPawnMaskBlack(uint8_t sq)
{
    return IsolatedPawnMaskWhite(sq); // Files are symmetrical.
}

/// @brief Supported pawn mask white.
/// @param sq Square index.
/// @return Squares which if containing a friendly pawn can potentially defend the pawn on sq.
constexpr uint64_t SupportedPawnMaskWhite(uint8_t sq)
{
    const uint64_t b = RayFrom(sq, SOUTH) | Bitboard(sq);
    return ShiftWest(b) | ShiftEast(b);
}

/// @brief Supported pawn mask black.
/// @param sq Square index.
/// @return Squares which if containing a friendly pawn can potentially defend the pawn on sq.
constexpr uint64_t SupportedPawnMaskBlack(uint8_t sq)
{
    const uint64_t b = RayFrom(sq, NORTH) | Bitboard(sq);
    return ShiftWest(b) | ShiftEast(b);
}

/// @brief Doubled pawn mask white.
/// @param sq Square index.
/// @return Squares which if containing a friendly pawn, make the pawn on sq doubled.
constexpr uint64_t DoubledPawnMaskWhite(uint8_t sq)
{
    return RayFrom(sq, NORTH);
}

/// @brief Doubled pawn mask black.
/// @param sq Square index.
/// @return Squares which if containing a friendly pawn, make the pawn on sq doubled.
constexpr uint64_t DoubledPawnMaskBlack(uint8_t sq)
{
    return RayFrom(sq, SOUTH);
}

/// @brief Intervening squares on colinear ray.
/// @param from Source square index.
/// @param to Destination square index.
/// @return If from and to are colinear, the set of squares between them.
constexpr uint64_t InterveningSquares(uint8_t from, uint8_t to)
{
    constexpr std::array<Direction, 8> directions = {NORTH, NORTHEAST, EAST, SOUTHEAST,
                                                     SOUTH, SOUTHWEST, WEST, NORTHWEST};
    for (auto dir : directions)
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
    ShiftFn  fn          = shift_functions[direction];
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
    return RayOccupancyMask(sq, NORTHEAST) | RayOccupancyMask(sq, NORTHWEST) | RayOccupancyMask(sq, SOUTHEAST) |
           RayOccupancyMask(sq, SOUTHWEST);
}

/// @brief Rook occupancy mask.
/// @param sq Square index.
/// @return Rook sliding move occupancy mask for source square sq.
constexpr uint64_t RookOccupancyMask(uint8_t sq)
{
    return RayOccupancyMask(sq, NORTH) | RayOccupancyMask(sq, SOUTH) | RayOccupancyMask(sq, EAST) |
           RayOccupancyMask(sq, WEST);
}

/// @brief Move targets in one direction for sliding pieces considering occupancy.
/// @param occupied_squares Set of squares with a piece on them.
/// @param sq Source square index.
/// @param direction Compass rose direction.
/// @return Set of squares attacked in the specified direction.
constexpr uint64_t RayAttacks(uint64_t occupied_squares, uint8_t sq, Direction direction)
{
    uint64_t result = 0;
    ShiftFn  fn     = shift_functions[direction];
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
    return RayAttacks(occupied_squares, sq, NORTHEAST) | RayAttacks(occupied_squares, sq, SOUTHEAST) |
           RayAttacks(occupied_squares, sq, SOUTHWEST) | RayAttacks(occupied_squares, sq, NORTHWEST);
}

/// @brief Rook move targets.
/// @param occupied_squares Set of squares occupied by a piece.
/// @param sq Source square index.
/// @return Squares to which a rook on sq can slide to.
constexpr uint64_t RookAttacks(uint64_t occupied_squares, uint8_t sq)
{
    return RayAttacks(occupied_squares, sq, NORTH) | RayAttacks(occupied_squares, sq, EAST) |
           RayAttacks(occupied_squares, sq, SOUTH) | RayAttacks(occupied_squares, sq, WEST);
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

/// @brief Parameters for a magic bitboard for a single square and piece type.
struct Magic
{
    uint64_t              multiplier; ///< The magic value.
    uint64_t              mask;       ///< Occupancy mask.
    int                   shift;      ///< Number of bits to right shift to get indices.
    std::vector<uint64_t> attacks;    ///< The discrete (unique) attack vectors.
    std::vector<uint8_t>  indices;    ///< Indices into the discrete attack vector array.
};

/// @brief Maps a square to an occupancy mask for that square.
using MaskFn = uint64_t (*)(uint8_t);

/// @brief Maps occupied squares and location to attacked targets by a sliding piece.
using AttackFn = uint64_t (*)(uint64_t, uint8_t);

/// @brief Trial and error search for a magic bitboard entry, for one sliding piece type at one location.
/// @param sq Square index.
/// @param mask_fn Occupancy mask function for this slider.
/// @param attack_fn Attacked squares function for this slider.
/// @return The magic values for this slider for this location.
Magic FindMagic(uint8_t sq, MaskFn mask_fn, AttackFn attack_fn)
{
    Magic magic;
    magic.mask                        = mask_fn(sq);
    magic.shift                       = 64 - std::popcount(magic.mask);
    const auto            occupancies = EnumerateMaskCombinations(magic.mask);
    std::vector<uint64_t> attacks;
    for (auto occupancy : occupancies)
    {
        attacks.push_back(attack_fn(occupancy, sq));
    }
    bool is_hash_collision;
    do
    {
        is_hash_collision = false;
        // Magics are typically fairly sparse, so AND a few random numbers together.
        magic.multiplier = prng() & prng() & prng();
        // Use ALL_SQUARES to indicate a vacant slot since NO_SQUARES is a valid attack vector.
        magic.attacks.assign(occupancies.size(), ~0ull);
        for (std::size_t i = 0; i != occupancies.size(); ++i)
        {
            const auto index = (occupancies[i] * magic.multiplier) >> magic.shift;
            if (magic.attacks[index] == ~0ull)
            {
                // This slot is vacant so store the actual attack.
                magic.attacks[index] = attacks[i];
                continue;
            }
            if (magic.attacks[index] != attacks[i])
            {
                // There is a collision with a previous entry; this trial magic is no good.
                is_hash_collision = true;
                break;
            }
        }
    } while (is_hash_collision);
    // We found a magic value which works. Compress the actual attacks set down to a set of indices into unique attacks.
    // Since the indices are only 1 byte each and the attacks are 8 bytes each, this saves a ton of space in the final
    // magic move entry tables, which helps with cache pressure at the expense of one extra indirection.
    std::set<uint64_t>    attacks_set{magic.attacks.begin(), magic.attacks.end()}; // Make unique.
    std::vector<uint64_t> unique_attacks{attacks_set.begin(), attacks_set.end()};
    for (auto attack : magic.attacks)
    {
        const auto iter = std::ranges::find(unique_attacks, attack);
        magic.indices.push_back(iter - unique_attacks.begin());
    }
    magic.attacks = unique_attacks;
    return magic;
}

/// @brief A magic Bitboard search vector entry.
struct PieceMagic
{
    std::string_view name;      ///< Piece name (bishop or rook).
    AttackFn         attack_fn; ///< Attacked squares function.
    MaskFn           mask_fn;   ///< Occupancy mask function.
};

// clang-format off
constexpr std::array<PieceMagic, 2> piece_magics = 
{{
    { "BISHOP", BishopAttacks, BishopOccupancyMask }, 
    { "ROOK",   RookAttacks,   RookOccupancyMask   },
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
constexpr std::array<Generator, 18> bitboard_generators = {{
    // clang-format off
    {"north",                   [](uint8_t sq) constexpr { return RayFrom(sq, NORTH);                                          }  },
    {"northeast",               [](uint8_t sq) constexpr { return RayFrom(sq, NORTHEAST);                                      }  },
    {"east",                    [](uint8_t sq) constexpr { return RayFrom(sq, EAST);                                           }  },
    {"southeast",               [](uint8_t sq) constexpr { return RayFrom(sq, SOUTHEAST);                                      }  },
    {"south",                   [](uint8_t sq) constexpr { return RayFrom(sq, SOUTH);                                          }  },
    {"southwest",               [](uint8_t sq) constexpr { return RayFrom(sq, SOUTHWEST);                                      }  },
    {"west",                    [](uint8_t sq) constexpr { return RayFrom(sq, WEST);                                           }  },
    {"northwest",               [](uint8_t sq) constexpr { return RayFrom(sq, NORTHWEST);                                      }  },
    {"pawn_attacks_white",      [](uint8_t sq) constexpr { return ShiftNorthwest(Bitboard(sq)) | ShiftNortheast(Bitboard(sq)); }  },
    {"pawn_attacks_black",      [](uint8_t sq) constexpr { return ShiftSouthwest(Bitboard(sq)) | ShiftSoutheast(Bitboard(sq)); }  },
    {"knight_attacks",          KnightAttacks                                                                                     },
    {"bishop_attacks",          BishopAttacksOnEmptyBoard                                                                         },
    {"rook_attacks",            RookAttacksOnEmptyBoard                                                                           },
    {"queen_attacks",           QueenAttacksOnEmptyBoard                                                                          },
    {"king_attacks",            KingAttacks                                                                                       },
    {"king_attacks2",           KingAttacks2                                                                                      },
    {"king_pawn_shelter_white", KingPawnShelterWhite                                                                              },
    {"king_pawn_shelter_black", KingPawnShelterBlack                                                                              },
    // clang-format on
}};

/// @brief Generators for white pawn structure squares.
constexpr std::array<Generator, 4> pawn_generators_white = {{
    // clang-format off
    {"passed_pawn_mask",    PassedPawnMaskWhite,    },
    {"isolated_pawn_mask",  IsolatedPawnMaskWhite,  },
    {"supported_pawn_mask", SupportedPawnMaskWhite, },
    {"doubled_pawn_mask",   DoubledPawnMaskWhite,   },
    // clang-format on
}};

/// @brief Generators for black pawn structure squares.
constexpr std::array<Generator, 4> pawn_generators_black = {{
    // clang-format off
    {"passed_pawn_mask",    PassedPawnMaskBlack,    },
    {"isolated_pawn_mask",  IsolatedPawnMaskBlack,  },
    {"supported_pawn_mask", SupportedPawnMaskBlack, },
    {"doubled_pawn_mask",   DoubledPawnMaskBlack,   },
    // clang-format on
}};

/// @brief Generate the generic sets for each square.
static void GenerateSets()
{
    std::cout << std::format("extern const Sets SETS[64] = \n{{");
    for (uint8_t sq = 0; sq != 64; ++sq)
    {
        std::cout << std::format("\n    {{ // square {}", SquareName(sq));
        for (const auto &generator : bitboard_generators)
        {
            const uint64_t b = generator.function(sq);
            std::cout << std::format("\n        .{:<25} = 0x{:016X}, // popcnt {:2}", generator.name, b,
                                     std::popcount(b));
        }
        std::cout << std::format("\n    }},");
    }
    std::cout << std::format("\n}};\n");
}

/// @brief Generate pawn structure sets for each color for each square.
static void GeneratePawnSets()
{
    std::cout << std::format("extern const PawnSets PAWN_SETS[2][64] = \n{{");
    for (int color = 0; color != 2; ++color)
    {
        std::cout << std::format("\n    {{");
        for (uint8_t sq = 0; sq != 64; ++sq)
        {
            std::cout << std::format("\n        {{ // {} {}", color_names[color], SquareName(sq));
            auto &generators{color == 0 ? pawn_generators_white : pawn_generators_black};
            for (const auto &generator : generators)
            {
                const uint64_t b = generator.function(sq);
                std::cout << std::format("\n            .{:<20} = 0x{:016X}, // popcnt {:2}", generator.name, b,
                                         std::popcount(b));
            }
            std::cout << std::format("\n        }},");
        }
        std::cout << std::format("\n    }},");
    }
    std::cout << std::format("\n}};\n");
}

/// @brief Generate the intervening squares array.
static void GenerateInterveningSquares()
{
    std::cout << std::format("extern const Bitboard INTERVENING_SQUARES[64][64] = \n{{");
    for (uint8_t i = 0; i != 64; ++i)
    {
        std::cout << std::format("\n    {{ // square {}", SquareName(i));
        for (uint8_t j = 0; j != 64; ++j)
        {
            if ((j & 7) == 0)
            {
                std::cout << std::format("\n        ");
            }
            std::cout << std::format("0x{:016X},", InterveningSquares(i, j));
        }
        std::cout << std::format("\n    }},");
    }
    std::cout << std::format("\n}};\n");
}

/// @brief Generate Zobrist hash keys for pieces, castling rights and en passant capture.
static void GenerateHashes()
{
    std::cout << std::format("extern const zobrist_t PIECE_SQUARE_HASHES[2][6][64] = \n{{");
    for (int color = 0; color != 2; ++color)
    {
        std::cout << std::format("\n    {{");
        for (int piece = PAWN; piece <= KING; ++piece)
        {
            std::cout << std::format("\n        {{   // {} {}", color_names[color], piece_names[piece]);
            for (uint8_t i = 0; i != 64; ++i)
            {
                if ((i & 7) == 0)
                {
                    std::cout << std::format("\n            ");
                }
                std::cout << std::format("0x{:016X},", prng());
            }
            std::cout << std::format("\n        }},");
        }
        std::cout << std::format("\n    }},");
    }
    std::cout << std::format("\n}};\n");
    std::cout << std::format("extern const zobrist_t CASTLING_RIGHTS_HASHES[16] = \n{{");
    for (uint8_t i = 0; i != 16; ++i)
    {
        if ((i & 7) == 0)
        {
            std::cout << std::format("\n    ");
        }
        std::cout << std::format("0x{:016X},", prng());
    }
    std::cout << std::format("\n}};\n");
    std::cout << std::format("extern const zobrist_t EN_PASSANT_HASHES[64] = \n{{");
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

/// @brief Generate magic bitboards for bishop and rook sliding attacks.
static void GenerateMagics(void)
{
    for (const PieceMagic &pm : piece_magics)
    {
        std::vector<Magic> magics;
        // Find magic values for each square on the board.
        for (uint8_t sq = 0; sq != 64; ++sq)
        {
            magics.push_back(FindMagic(sq, pm.mask_fn, pm.attack_fn));
        }
        // First print the arrays for the discrete attacks and the attack indices.
        for (uint8_t sq = 0; sq != 64; ++sq)
        {
            const int num_attacks = 1 << (64 - magics[sq].shift);
            std::cout << std::format("static const uint8_t {}_MAGIC_INDICES_{}[{}] = \n{{", pm.name, SquareName(sq),
                                     num_attacks);
            for (int j = 0; j != num_attacks; ++j)
            {
                if (j % 16 == 0)
                {
                    std::cout << std::format("\n    ");
                }
                std::cout << std::format("0x{:02X}, ", magics[sq].indices[j]);
            }
            std::cout << std::format("\n}};\n");
            std::cout << std::format("static const Bitboard {}_MAGIC_ATTACKS_{}[{}] = \n{{", pm.name, SquareName(sq),
                                     magics[sq].attacks.size());
            for (std::size_t j = 0; j != magics[sq].attacks.size(); ++j)
            {
                if (j % 4 == 0)
                {
                    std::cout << std::format("\n    ");
                }
                std::cout << std::format("0x{:016X},", magics[sq].attacks[j]);
            }
            std::cout << std::format("\n}};\n");
        }
        // Next print the MagicBitboard for this piece / square combination.
        std::cout << std::format("extern const MagicBitboard {}_MAGICS[64] = \n{{\n", pm.name);
        for (uint8_t i = 0; i != 64; ++i)
        {
            std::cout << std::format("    {{\n");
            std::cout << std::format("        .magic          = 0x{:016X},\n", magics[i].multiplier);
            std::cout << std::format("        .occupancy_mask = 0x{:016X},\n", magics[i].mask);
            std::cout << std::format("        .shift          = {},\n", magics[i].shift);
            std::cout << std::format("        .attacks        = {}_MAGIC_ATTACKS_{},\n", pm.name, SquareName(i));
            std::cout << std::format("        .indices        = {}_MAGIC_INDICES_{},\n", pm.name, SquareName(i));
            std::cout << std::format("    }},\n");
        }
        std::cout << std::format("}};\n");
    }
}

/// @brief Main entry point. Prints to stdout which gets redirected in the Makefile to create the generated data source
/// file which is then used by the main program.
int main()
{
    std::cout << "// This file was generated on " __DATE__ " at " __TIME__ "\n";
    std::cout << "#include \"generated_data.h\"\n";
    GenerateSets();
    GeneratePawnSets();
    GenerateInterveningSquares();
    GenerateHashes();
    GenerateMagics();
    return 0;
}

/**
 * @file Precomputes at compile time, constants used by pawnstar.
 * This program is compiled and then executed by the Makefile to create the "generated_data.cpp" source file which
 * is then used in compilation of the main program.
 */
#include <array>
#include <cinttypes>
#include <cstdio>
#include <functional>
#include <set>
#include <vector>

/* Fixed square definitions */
constexpr uint64_t NOT_FILE_H =
    0x7F7F7F7F7F7F7F7Full; /**< Mask off the h file to prevent wraparound when shifting east. */
constexpr uint64_t NOT_FILE_A =
    0xFEFEFEFEFEFEFEFEull; /**< Mask off the a file to prevent wraparound when shifting west. */
constexpr uint64_t NO_SQUARES = 0ull;

/* clang-format off */
constexpr uint8_t   FileOf(int locn)            { return (uint8_t)locn  & 7; }                  /**< Convert square index to file. */
constexpr uint8_t   RankOf(int locn)            { return (uint8_t)locn >> 3; }                  /**< Convert square index to rank. */
constexpr char      FileChar(int locn)          { return (char)('a' + FileOf(locn)); }          /**< Convert square index to file character. */
constexpr char      RankChar(int locn)          { return (char)('1' + RankOf(locn)); }          /**< Convert square index to rank character. */
constexpr uint64_t  Bitboard(uint8_t locn)      { return 1ull << locn; }                        /**< Convert square index to Bitboard. */
constexpr uint64_t  Bitboard(int x, int y)      { return 1ull << (x + 8 * y); }                 /**< Convert square (file,rank) co-ords to Bitboard. */
constexpr uint64_t  ShiftNorth(uint64_t b)      { return b << 8; }                              /**< Shift a Bitboard one square to the north. */
constexpr uint64_t  ShiftNortheast(uint64_t b)  { return (b & NOT_FILE_H) << 9; }               /**< Shift a Bitboard one square to the northeast. */
constexpr uint64_t  ShiftEast(uint64_t b)       { return (b & NOT_FILE_H) << 1; }               /**< Shift a Bitboard one square to the east. */
constexpr uint64_t  ShiftSoutheast(uint64_t b)  { return (b & NOT_FILE_H) >> 7; }               /**< Shift a Bitboard one square to the southeast. */
constexpr uint64_t  ShiftSouth(uint64_t b)      { return b >> 8; }                              /**< Shift a Bitboard one square to the south. */
constexpr uint64_t  ShiftSouthwest(uint64_t b)  { return (b & NOT_FILE_A) >> 9; }               /**< Shift a Bitboard one square to the southwest. */
constexpr uint64_t  ShiftWest(uint64_t b)       { return (b & NOT_FILE_A) >> 1; }               /**< Shift a Bitboard one square to the west. */
constexpr uint64_t  ShiftNorthwest(uint64_t b)  { return (b & NOT_FILE_A) << 7; }               /**< Shift a Bitboard one square to the northwest. */
constexpr bool      IsInBoard(int x, int y)     { return x >= 0 && x < 8 && y >= 0 && y < 8; }  /**< Is this square on the board. */
/* clang-format on */

/**
 * @brief Chess piece types.
 */
enum Piece
{
    NONE,
    PAWN,
    KNIGHT,
    BISHOP,
    ROOK,
    QUEEN,
    KING,
};

static const char *const PIECE_NAMES[7] = {"none", "pawn", "knight", "bishop", "rook", "queen", "king"};
static const char *const COLOR_NAMES[2] = {"white", "black"};

/**
 * @brief Compass rose directions from white's perspective, i.e. north is "forward" for white.
 */
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

/**
 * @brief X and Y deltas for a compass rose direction.
 */
struct DirectionVector
{
    int dx;
    int dy;
};

/**
 * @brief Each of the directions on the compass.
 */
/* clang-format off */
constexpr DirectionVector DIRECTION_VECTORS[] = 
{
    { .dx =  0, .dy =  1 }, // North
    { .dx =  1, .dy =  1 }, // Northeast
    { .dx =  1, .dy =  0 }, // East
    { .dx =  1, .dy = -1 }, // Southeast
    { .dx =  0, .dy = -1 }, // South
    { .dx = -1, .dy = -1 }, // Southwest
    { .dx = -1, .dy =  0 }, // West
    { .dx = -1, .dy =  1 }, // Northwest
};
/* clang-format on */

/**
 * @brief Population count (number of bits set).
 * @param x Input value.
 * @return Number of 1 bits in x.
 */
constexpr int PopCount(uint64_t x)
{
    int count = 0;
    while (x)
    {
        ++count;
        x &= x - 1;
    }
    return count;
}

/**
 * @brief Generate a 64 bit pseudo random number Zobrist hash key.
 * @return Next value in sequence.
 */
static uint64_t NextRandomKey()
{
    static uint64_t x = 0xAA55AA55AA55AA55ull;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    return x * 0x2545F4914F6CDD1Dull;
}

/**
 * @brief Generate a ray (vector) from a square in a single compass direction.
 * @param locn Source square index.
 * @param direction Compass direction.
 * @return Vector from locn in direction specified, excluding source square.
 */
constexpr uint64_t RayFrom(uint8_t locn, Direction direction)
{
    uint64_t               result = NO_SQUARES;
    const DirectionVector &dv     = DIRECTION_VECTORS[direction];
    for (int x = FileOf(locn) + dv.dx, y = RankOf(locn) + dv.dy; IsInBoard(x, y); x += dv.dx, y += dv.dy)
    {
        result |= Bitboard(x, y);
    }
    return result;
}

/**
 * @brief Knight attacks from square locn.
 * @param locn Square index.
 * @return Squares attacked by a knight standing on locn.
 */
constexpr uint64_t KnightAttacks(uint8_t locn)
{
    constexpr std::pair<int, int> deltas[] = {{-2, -1}, {-2, 1}, {-1, -2}, {-1, 2}, {1, -2}, {1, 2}, {2, -1}, {2, 1}};

    uint64_t  result = NO_SQUARES;
    const int locn_x = FileOf(locn);
    const int locn_y = RankOf(locn);

    for (const auto &[dx, dy] : deltas)
    {
        int x = locn_x + dx;
        int y = locn_y + dy;
        if (IsInBoard(x, y))
        {
            result |= Bitboard(x, y);
        }
    }
    return result;
}

/**
 * @brief Bishop rays.
 * @param locn Square index.
 * @return Bishop attacks on an otherwise empty board.
 */
constexpr uint64_t BishopAttacksOnEmptyBoard(uint8_t locn)
{
    return RayFrom(locn, NORTHEAST) | RayFrom(locn, NORTHWEST) | RayFrom(locn, SOUTHEAST) | RayFrom(locn, SOUTHWEST);
}

/**
 * @brief Rook rays.
 * @param locn Square index.
 * @return Rook attacks on an otherwise empty board.
 */
constexpr uint64_t RookAttacksOnEmptyBoard(uint8_t locn)
{
    return RayFrom(locn, NORTH) | RayFrom(locn, SOUTH) | RayFrom(locn, EAST) | RayFrom(locn, WEST);
}

/**
 * @brief Queen rays.
 * @param locn Square index.
 * @return Queen attacks on an otherwise empty board.
 */
constexpr uint64_t QueenAttacksOnEmptyBoard(uint8_t locn)
{
    return BishopAttacksOnEmptyBoard(locn) | RookAttacksOnEmptyBoard(locn);
}

/**
 * @brief King fill attacks.
 * @param b Bitboard containing King square.
 * @return Squares attacked by King on b.
 */
constexpr uint64_t KingFill(uint64_t b)
{
    return ShiftNorth(b) | ShiftNortheast(b) | ShiftEast(b) | ShiftSoutheast(b) | ShiftSouth(b) | ShiftSouthwest(b) |
           ShiftWest(b) | ShiftNorthwest(b);
}

/**
 * @brief King attacks.
 * @param locn Square index.
 * @return King attacks by a king standing on locn.
 */
constexpr uint64_t KingAttacks(uint8_t locn)
{
    return KingFill(Bitboard(locn));
}

/**
 * @brief King attacks 2.
 * @param locn Square index.
 * @return Squares reachable by a King in 2 moves.
 */
constexpr uint64_t KingAttacks2(uint8_t locn)
{
    return KingFill(KingFill(Bitboard(locn)));
}

/**
 * @brief King pawn shelter white.
 * @param locn King square index.
 * @return Pawns in front of the King.
 */
constexpr uint64_t KingPawnShelterWhite(uint8_t locn)
{
    const uint64_t b = Bitboard(locn);
    return ShiftNorthwest(b) | ShiftNorth(b) | ShiftNortheast(b);
}

/**
 * @brief King pawn shelter black.
 * @param locn King square index.
 * @return Pawns in front of the King.
 */
constexpr uint64_t KingPawnShelterBlack(uint8_t locn)
{
    const uint64_t b = Bitboard(locn);
    return ShiftSouthwest(b) | ShiftSouth(b) | ShiftSoutheast(b);
}

/**
 * @brief Passed pawn mask for white.
 * @param locn Square index.
 * @return Squares which must be free of black pawns for a white pawn on locn to be passed.
 */
constexpr uint64_t PassedPawnMaskWhite(uint8_t locn)
{
    uint64_t  result = NO_SQUARES;
    const int locn_x = FileOf(locn);
    const int locn_y = RankOf(locn);
    for (int x = locn_x - 1; x <= locn_x + 1; ++x)
    {
        if (x < 0 || x > 7)
        {
            continue; /* off board */
        }
        for (int y = locn_y + 1; y < 8; ++y)
        {
            result |= Bitboard(x, y);
        }
    }
    return result;
}

/**
 * @brief Passed pawn mask for black.
 * @param locn Square index.
 * @return Squares which must be free of white pawns for a black pawn on locn to be passed.
 */
constexpr uint64_t PassedPawnMaskBlack(uint8_t locn)
{
    uint64_t  result = NO_SQUARES;
    const int locn_x = FileOf(locn);
    const int locn_y = RankOf(locn);
    for (int x = locn_x - 1; x <= locn_x + 1; ++x)
    {
        if (x < 0 || x > 7)
        {
            continue; /* off board */
        }
        for (int y = locn_y - 1; y >= 0; --y)
        {
            result |= Bitboard(x, y);
        }
    }
    return result;
}

/**
 * @brief Isolated pawn mask for white.
 * @param locn Square index.
 * @return Squares which must contain at least one white pawn else the pawn on locn is isolated.
 */
constexpr uint64_t IsolatedPawnMaskWhite(uint8_t locn)
{
    uint64_t  result = NO_SQUARES;
    const int locn_x = FileOf(locn);
    for (int x = locn_x - 1; x <= locn_x + 1; x += 2)
    {
        if (x < 0 || x > 7)
        {
            continue; /* off board */
        }
        for (int y = 0; y < 8; ++y)
        {
            result |= Bitboard(x, y);
        }
    }
    return result;
}

/**
 * @brief Isolated pawn mask for black.
 * @param locn Square index.
 * @return Squares which must contain at least one black pawn else the pawn on locn is isolated.
 */
constexpr uint64_t IsolatedPawnMaskBlack(uint8_t locn)
{
    return IsolatedPawnMaskWhite(locn); /* files are symmetrical */
}

/**
 * @brief Supported pawn mask white.
 * @param locn Square index.
 * @return Squares which if containing a friendly pawn can potentially defend the pawn on locn.
 */
constexpr uint64_t SupportedPawnMaskWhite(uint8_t locn)
{
    uint64_t  result = NO_SQUARES;
    const int locn_x = FileOf(locn);
    const int locn_y = RankOf(locn);
    ;
    for (int x = locn_x - 1; x <= locn_x + 1; x += 2)
    {
        if (x < 0 || x > 7)
        {
            continue; /* off board */
        }
        for (int y = locn_y; y >= 0; --y)
        {
            result |= Bitboard(x, y);
        }
    }
    return result;
}

/**
 * @brief Supported pawn mask black.
 * @param locn Square index.
 * @return Squares which if containing a friendly pawn can potentially defend the pawn on locn.
 */
constexpr uint64_t SupportedPawnMaskBlack(uint8_t locn)
{
    uint64_t  result = NO_SQUARES;
    const int locn_x = FileOf(locn);
    const int locn_y = RankOf(locn);
    for (int x = locn_x - 1; x <= locn_x + 1; x += 2)
    {
        if (x < 0 || x > 7)
        {
            continue; /* off board */
        }
        for (int y = locn_y; y < 8; ++y)
        {
            result |= Bitboard(x, y);
        }
    }
    return result;
}

/**
 * @brief Doubled pawn mask white.
 * @param locn Square index.
 * @return Squares which if containing a friendly pawn, make the pawn on locn doubled.
 */
constexpr uint64_t DoubledPawnMaskWhite(uint8_t locn)
{
    return RayFrom(locn, NORTH);
}

/**
 * @brief Doubled pawn mask black.
 * @param locn Square index.
 * @return Squares which if containing a friendly pawn, make the pawn on locn doubled.
 */
constexpr uint64_t DoubledPawnMaskBlack(uint8_t locn)
{
    return RayFrom(locn, SOUTH);
}

/**
 * @brief Intervening squares on colinear ray.
 * @param from Source square index.
 * @param to Destination square index.
 * @return If from and to are colinear, the set of squares between them.
 */
constexpr uint64_t InterveningSquares(int from, int to)
{
    constexpr Direction directions[] = {NORTH, NORTHEAST, EAST, SOUTHEAST, SOUTH, SOUTHWEST, WEST, NORTHWEST};
    for (auto dir : directions)
    {
        const uint64_t vector_from = RayFrom(from, dir);
        if (vector_from & Bitboard(to))
        {
            return vector_from ^ RayFrom(to, dir) ^ Bitboard(to);
        }
    }
    return NO_SQUARES;
}

/**
 * @brief Occupancy mask for a single direction.
 * Occupancy masks exclude the final end square of the ray
 * as this does not affect slider move targets.
 * @param locn Square index.
 * @param direction Compass direction.
 * @return Occupancy mask for that locn and direction.
 */
constexpr uint64_t RayOccupancyMask(uint8_t locn, Direction direction)
{
    uint64_t               result = NO_SQUARES;
    const DirectionVector &dv     = DIRECTION_VECTORS[direction];
    for (int x = FileOf(locn) + dv.dx, y = RankOf(locn) + dv.dy; IsInBoard(x + dv.dx, y + dv.dy);
         x += dv.dx, y += dv.dy)
    {
        result |= Bitboard(x, y);
    }
    return result;
}

/**
 * @brief Bishop occupancy mask.
 * @param locn Square index.
 * @return Bishop sliding move occupancy mask for source square locn.
 */
constexpr uint64_t BishopOccupancyMask(uint8_t locn)
{
    return RayOccupancyMask(locn, NORTHEAST) | RayOccupancyMask(locn, NORTHWEST) | RayOccupancyMask(locn, SOUTHEAST) |
           RayOccupancyMask(locn, SOUTHWEST);
}

/**
 * @brief Rook occupancy mask.
 * @param locn Square index.
 * @return Rook sliding move occupancy mask for source square locn.
 */
constexpr uint64_t RookOccupancyMask(uint8_t locn)
{
    return RayOccupancyMask(locn, NORTH) | RayOccupancyMask(locn, SOUTH) | RayOccupancyMask(locn, EAST) |
           RayOccupancyMask(locn, WEST);
}

/**
 * @brief Move targets in one direction for sliding pieces considering occupancy.
 * @param occupied_squares Set of squares with a piece on them.
 * @param locn Source square index.
 * @param direction Compass rose direction.
 * @return Set of squares attacked in the specified direction.
 */
constexpr uint64_t RayAttacks(uint64_t occupied_squares, int locn, Direction direction)
{
    uint64_t               result = NO_SQUARES;
    const DirectionVector &dv     = DIRECTION_VECTORS[direction];
    for (int x = FileOf(locn) + dv.dx, y = RankOf(locn) + dv.dy; IsInBoard(x, y); x += dv.dx, y += dv.dy)
    {
        result |= Bitboard(x, y);
        if (Bitboard(x, y) & occupied_squares)
        {
            break;
        }
    }
    return result;
}

/**
 * @brief Bishop move targets
 * @param occupied_squares Set of squares occupied by a piece.
 * @param locn Source square index.
 * @return Squares to which a bishop on locn can slide to.
 */
constexpr uint64_t BishopAttacks(uint64_t occupied_squares, uint8_t locn)
{
    return RayAttacks(occupied_squares, locn, NORTHEAST) | RayAttacks(occupied_squares, locn, SOUTHEAST) |
           RayAttacks(occupied_squares, locn, SOUTHWEST) | RayAttacks(occupied_squares, locn, NORTHWEST);
}

/**
 * @brief Rook move targets
 * @param occupied_squares Set of squares occupied by a piece.
 * @param locn Source square index.
 * @return Squares to which a rook on locn can slide to.
 */
constexpr uint64_t RookAttacks(uint64_t occupied_squares, uint8_t locn)
{
    return RayAttacks(occupied_squares, locn, NORTH) | RayAttacks(occupied_squares, locn, EAST) |
           RayAttacks(occupied_squares, locn, SOUTH) | RayAttacks(occupied_squares, locn, WEST);
}

/**
 * @brief Given a bit mask, enumerate all the possible combinations of bits set from that mask.
 * @param mask The mask value.
 * @return Combinational subsets of mask.
 */
constexpr std::vector<uint64_t> EnumerateMaskCombinations(uint64_t mask)
{
    std::vector<uint64_t> result;
    uint64_t              n = 0;
    do
    {
        result.push_back(n);
        n = (n - mask) & mask;
    } while (n);
    return result;
}

/** Maps location to an occupancy mask for that square. */
typedef uint64_t (*MaskFn)(uint8_t);

/** Maps occupied squares set and location to attacked targets by a sliding piece. */
typedef uint64_t (*AttackFn)(uint64_t, uint8_t);

/**
 * @brief Parameters for magic Bitboard search.
 */
struct Magic
{
    uint64_t              magic;   /**< The magic multiplicand. */
    uint64_t              mask;    /**< Occupancy mask. */
    int                   shift;   /**< Number of bits to right shift to get indices. */
    std::vector<uint64_t> attacks; /**< The discrete attack vectors. */
    std::vector<uint8_t>  indices; /**< Indices into the discrete attack vector array. */
};

/**
 * @brief Trial and error search for a magic bitboard entry, for one sliding piece type at one location.
 * @param locn Square index.
 * @param mask_fn Occupancy mask function for this slider.
 * @param attack_fn Attacked squares function for this slider.
 * @return The magic values for this slider for this location.
 */
static Magic FindMagic(uint8_t locn, MaskFn mask_fn, AttackFn attack_fn)
{
    Magic magic;
    magic.mask  = mask_fn(locn);
    magic.shift = 64 - PopCount(magic.mask);
    /*
    Enumerate all possible occupancy values and compute what the actual
    attack sets for each occupancy are, so we can check to see if a trial
    magic value works.
    */
    const auto            occupancies = EnumerateMaskCombinations(magic.mask);
    std::vector<uint64_t> attacks;
    for (auto occupancy : occupancies)
    {
        attacks.push_back(attack_fn(occupancy, locn));
    }
    /* Try a random magic multiplicand and test it for success. */
    bool     is_hash_collision;
    uint64_t trial_magic;
    do
    {
        is_hash_collision = false;
        /* Magics are typically fairly sparse, so AND a few random numbers together. */
        trial_magic = NextRandomKey() & NextRandomKey() & NextRandomKey();
        /* Reset the attacks for each trial run. */
        magic.attacks.assign(occupancies.size(), ~NO_SQUARES);
        for (std::size_t i = 0; i != occupancies.size(); ++i)
        {
            const auto index = (occupancies[i] * trial_magic) >> magic.shift;
            if (magic.attacks[index] == ~NO_SQUARES)
            {
                /* Nothing here yet so store the attack. */
                magic.attacks[index] = attacks[i];
                continue;
            }
            if (magic.attacks[index] != attacks[i])
            {
                /* A previous entry clashes with this entry - this trial magic is no good. */
                is_hash_collision = true;
                break;
            }
        }
    } while (is_hash_collision);
    /*
    Compress the actual attacks set down to a set of indices
    into unique attacks. Since the indices are only 1 byte each
    and the attacks are 8 bytes each, this saves a lot of space in the
    final magic move entry tables, which helps with cache pressure at the
    expense of one extra indirection.
    */
    std::set<uint64_t>    discrete_attacks_set{magic.attacks.begin(), magic.attacks.end()}; // Make unique.
    std::vector<uint64_t> discrete_attacks{discrete_attacks_set.begin(), discrete_attacks_set.end()};
    magic.indices.clear();
    for (auto attack : magic.attacks)
    {
        const auto j = std::find(discrete_attacks.begin(), discrete_attacks.end(), attack);
        magic.indices.push_back(j - discrete_attacks.begin());
    }
    magic.magic   = trial_magic;
    magic.attacks = discrete_attacks;
    return magic;
}

/**
 * @brief A magic Bitboard search vector entry.
 */
struct MagicVector
{
    const char *name;      /**< Piece name (bishop or rook). */
    AttackFn    attack_fn; /**< Attacked squares function. */
    MaskFn      mask_fn;   /**< Occupancy mask function. */
};

/* clang-format off */
constexpr MagicVector MAGIC_VECTORS[] = 
{
    { "BISHOP", BishopAttacks, BishopOccupancyMask }, 
    { "ROOK",   RookAttacks,   RookOccupancyMask   },
};
/* clang-format on */

/** Maps a location to a set of squares for that location. */
typedef std::function<uint64_t(uint8_t locn)> GeneratorFn;

/**
 * @brief A generator to map a square index to a set of target squares for that index,
 * e.g. "All squares north of here", "Squares attacked by a queen standing here" etc.
 */
struct Generator
{
    const char *name;     /**< Name of generated output.      */
    GeneratorFn function; /**< Function to generate output.   */
};

/**
 * @brief Generators for the main precomputed Bitboard arrays.
 */
static const Generator SET_GENERATORS[] = {
    /* clang-format off */
    {"north",                   [](uint8_t locn){ return RayFrom(locn, NORTH);                                              }   },
    {"northeast",               [](uint8_t locn){ return RayFrom(locn, NORTHEAST);                                          }   },
    {"east",                    [](uint8_t locn){ return RayFrom(locn, EAST);                                               }   },
    {"southeast",               [](uint8_t locn){ return RayFrom(locn, SOUTHEAST);                                          }   },
    {"south",                   [](uint8_t locn){ return RayFrom(locn, SOUTH);                                              }   },
    {"southwest",               [](uint8_t locn){ return RayFrom(locn, SOUTHWEST);                                          }   },
    {"west",                    [](uint8_t locn){ return RayFrom(locn, WEST);                                               }   },
    {"northwest",               [](uint8_t locn){ return RayFrom(locn, NORTHWEST);                                          }   },
    {"pawn_attacks_white",      [](uint8_t locn){ return ShiftNorthwest(Bitboard(locn)) | ShiftNortheast(Bitboard(locn));   },  },
    {"pawn_attacks_black",      [](uint8_t locn){ return ShiftSouthwest(Bitboard(locn)) | ShiftSoutheast(Bitboard(locn));   },  },
    {"knight_attacks",          KnightAttacks                                                                                   },
    {"bishop_attacks",          BishopAttacksOnEmptyBoard                                                                       },
    {"rook_attacks",            RookAttacksOnEmptyBoard                                                                         },
    {"queen_attacks",           QueenAttacksOnEmptyBoard                                                                        },
    {"king_attacks",            KingAttacks                                                                                     },
    {"king_attacks2",           KingAttacks2                                                                                    },
    {"king_pawn_shelter_white", KingPawnShelterWhite                                                                            },
    {"king_pawn_shelter_black", KingPawnShelterBlack                                                                            },
    /* clang-format on */
};

/**
 * @brief Generators for white pawn structure squares.
 */

static const Generator PAWN_GENERATORS_WHITE[] = {
    /* clang-format off */
    {"passed_pawn_mask",    PassedPawnMaskWhite,    },
    {"isolated_pawn_mask",  IsolatedPawnMaskWhite,  },
    {"supported_pawn_mask", SupportedPawnMaskWhite, },
    {"doubled_pawn_mask",   DoubledPawnMaskWhite,   },
    /* clang-format on */
};

/**
 * @brief Generators for black pawn structure squares.
 */

static const Generator PAWN_GENERATORS_BLACK[] = {
    /* clang-format off */
    {"passed_pawn_mask",    PassedPawnMaskBlack,    },
    {"isolated_pawn_mask",  IsolatedPawnMaskBlack,  },
    {"supported_pawn_mask", SupportedPawnMaskBlack, },
    {"doubled_pawn_mask",   DoubledPawnMaskBlack,   },
    /* clang-format on */
};

/**
 * @brief Generate the generic sets for each square.
 */
static void GenerateSets()
{
    printf("extern const Sets SETS[64] = \n{");
    for (uint8_t locn = 0; locn != 64; ++locn)
    {
        printf("\n    { /* square %c%c */", FileChar(locn), RankChar(locn));
        for (const auto &generator : SET_GENERATORS)
        {
            const uint64_t b = generator.function(locn);
            printf("\n        .%-25s = 0x%016" PRIX64 "ull, /* popcnt %2d */", generator.name, b, PopCount(b));
        }
        printf("\n    },");
    }
    printf("\n};\n");
}

/**
 * @brief Generate pawn structure sets for each color for each square.
 */
static void GeneratePawnSets()
{
    printf("extern const PawnSets PAWN_SETS[2][64] = \n{");
    for (int color = 0; color != 2; ++color)
    {
        printf("\n    {");
        for (uint8_t locn = 0; locn != 64; ++locn)
        {
            printf("\n        { /* %s square %c%c */", COLOR_NAMES[color], FileChar(locn), RankChar(locn));
            auto &generators{color == 0 ? PAWN_GENERATORS_WHITE : PAWN_GENERATORS_BLACK};
            for (const auto &generator : generators)
            {
                const uint64_t b = generator.function(locn);
                printf("\n            .%-28s = 0x%016" PRIX64 "ull, /* popcnt %2d */", generator.name, b, PopCount(b));
            }
            printf("\n        },");
        }
        printf("\n    },");
    }
    printf("\n};\n");
}

static void GenerateInterveningSquares()
{
    printf("extern const Bitboard INTERVENING_SQUARES[64][64] = \n{");
    for (int i = 0; i != 64; ++i)
    {
        printf("\n    { /* square %c%c */", FileChar(i), RankChar(i));
        for (int j = 0; j != 64; ++j)
        {
            if ((j & 7) == 0)
            {
                printf("\n        ");
            }
            printf("0x%016" PRIX64 "ull,", InterveningSquares(i, j));
        }
        printf("\n    },");
    }
    printf("\n};\n");
}

static void GenerateHashes()
{
    printf("extern const uint64_t PIECE_SQUARE_HASHES[2][6][64] = \n{");
    for (int color = 0; color != 2; ++color)
    {
        printf("\n    {");
        for (int piece = PAWN; piece <= KING; ++piece)
        {
            printf("\n        {   /* %s %s */", COLOR_NAMES[color], PIECE_NAMES[piece]);
            for (int i = 0; i != 64; ++i)
            {
                if ((i & 7) == 0)
                {
                    printf("\n            ");
                }
                printf("0x%016" PRIX64 "ull,", NextRandomKey());
            }
            printf("\n        },");
        }
        printf("\n    },");
    }
    printf("\n};\n");
    printf("extern const uint64_t CASTLING_RIGHTS_HASHES[16] = \n{");
    for (int i = 0; i != 16; ++i)
    {
        if ((i & 7) == 0)
        {
            printf("\n    ");
        }
        printf("0x%016" PRIX64 "ull,", NextRandomKey());
    }
    printf("\n};\n");
    printf("extern const uint64_t EN_PASSANT_HASHES[64] = \n{");
    for (int i = 0; i != 64; ++i)
    {
        if ((i & 7) == 0)
        {
            printf("\n    ");
        }
        const int rank = RankOf(i);
        printf("0x%016" PRIX64 "ull,", rank == 2 || rank == 5 ? NextRandomKey() : 0);
    }
    printf("\n};\n");
}

/**
 * @brief Generate magic bitboards.
 */
static void GenerateMagics(void)
{
    for (const MagicVector &mv : MAGIC_VECTORS)
    {
        std::vector<Magic> magics;
        /* Find magic values for each square on the board. */
        for (uint8_t locn = 0; locn != 64; ++locn)
        {
            magics.push_back(FindMagic(locn, mv.mask_fn, mv.attack_fn));
        }
        /* First print the arrays for the discrete attacks and the attack indices. */
        for (uint8_t locn = 0; locn != 64; ++locn)
        {
            char      square_name[3] = {(char)('A' + FileOf(locn)), RankChar(locn), 0};
            const int num_attacks    = 1 << (64 - magics[locn].shift);
            printf("static const uint8_t %s_MAGIC_INDICES_%s[%d] = \n{", mv.name, square_name, num_attacks);
            for (int j = 0; j != num_attacks; ++j)
            {
                if (j % 16 == 0)
                {
                    printf("\n    ");
                }
                printf("0x%02X, ", magics[locn].indices[j]);
            }
            printf("\n};\n");
            printf("static const uint64_t %s_MAGIC_ATTACKS_%s[%zu] = \n{", mv.name, square_name,
                   magics[locn].attacks.size());
            for (std::size_t j = 0; j != magics[locn].attacks.size(); ++j)
            {
                if (j % 8 == 0)
                {
                    printf("\n    ");
                }
                printf("0x%016" PRIX64 "ull, ", magics[locn].attacks[j]);
            }
            printf("\n};\n");
        }
        /* Next print the MagicMoveEntry for this piece / square combination. */
        printf("extern const MagicMoveEntry %s_MAGICS[64] = \n{\n", mv.name);
        for (int i = 0; i != 64; ++i)
        {
            char square_name[3] = {(char)('A' + FileOf(i)), RankChar(i), 0};
            printf("    {\n");
            printf("        .magic          = 0x%016" PRIX64 "ull,\n", magics[i].magic);
            printf("        .occupancy_mask = 0x%016" PRIX64 "ull,\n", magics[i].mask);
            printf("        .shift          = %d,\n", magics[i].shift);
            printf("        .attacks        = %s_MAGIC_ATTACKS_%s,\n", mv.name, square_name);
            printf("        .indices        = %s_MAGIC_INDICES_%s,\n", mv.name, square_name);
            printf("    },\n");
        }
        printf("};\n");
    }
}

/**
 * @brief Main entry point. Prints to stdout which gets redirected in the Makefile to create
 * the generated data source file which is then used by the main program.
 */
int main()
{
    printf("/* This file was generated on " __DATE__ " at " __TIME__ " */\n");
    printf("#include \"generated_data.h\"\n");
    GenerateSets();
    GeneratePawnSets();
    GenerateInterveningSquares();
    GenerateHashes();
    GenerateMagics();
    return 0;
}

/**
 * @file Precomputes at compile time, constants used by the main Pawnstar program.
 */
#include <algorithm>
#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <map>
#include <set>
#include <vector>

/* Fixed square definitions */
constexpr uint64_t NOT_H_FILE = 0x7F7F7F7F7F7F7F7Full; /**< Mask off the H file to prevent wraparound when shifting east. */
constexpr uint64_t NOT_A_FILE = 0xFEFEFEFEFEFEFEFEull; /**< Mask off the A file to prevent wraparound when shifting west. */
constexpr uint64_t NO_SQUARES = 0x0000000000000000ull;

/* clang-format off */
constexpr uint8_t   FileOf(int locn)            { return (uint8_t)locn & 7          ; } /**< Convert square index to file. */
constexpr uint8_t   RankOf(int locn)            { return (uint8_t)locn >> 3         ; } /**< Convert square index to rank. */
constexpr char      FileChar(int locn)          { return (char)('a' + FileOf(locn)) ; } /**< Convert square index to file character. */
constexpr char      RankChar(int locn)          { return (char)('1' + RankOf(locn)) ; } /**< Convert square index to rank character. */
constexpr uint64_t  Bitboard(uint8_t locn)      { return 1ull << locn               ; } /**< Convert square index to Bitboard. */
constexpr uint64_t  Bitboard(int x, int y)      { return 1ull << (x + 8 * y)        ; } /**< Convert square (file,rank) co-ords to Bitboard. */
constexpr uint64_t  ShiftNorth(uint64_t b)      { return b << 8                     ; } /**< Shift a Bitboard one square to the north. */
constexpr uint64_t  ShiftNortheast(uint64_t b)  { return (b & NOT_H_FILE) << 9      ; } /**< Shift a Bitboard one square to the northeast. */
constexpr uint64_t  ShiftEast(uint64_t b)       { return (b & NOT_H_FILE) << 1      ; } /**< Shift a Bitboard one square to the east. */
constexpr uint64_t  ShiftSoutheast(uint64_t b)  { return (b & NOT_H_FILE) >> 7      ; } /**< Shift a Bitboard one square to the southeast. */
constexpr uint64_t  ShiftSouth(uint64_t b)      { return b >> 8                     ; } /**< Shift a Bitboard one square to the south. */
constexpr uint64_t  ShiftSouthwest(uint64_t b)  { return (b & NOT_A_FILE) >> 9      ; } /**< Shift a Bitboard one square to the southwest. */
constexpr uint64_t  ShiftWest(uint64_t b)       { return (b & NOT_A_FILE) >> 1      ; } /**< Shift a Bitboard one square to the west. */
constexpr uint64_t  ShiftNorthwest(uint64_t b)  { return (b & NOT_A_FILE) << 7      ; } /**< Shift a Bitboard one square to the northwest. */
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

/**
 * @brief Compass rose directions from white's perspective.
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
constexpr DirectionVector direction_vectors[8] = 
{
    [NORTH]     = { .dx =  0, .dy =  1 },
    [NORTHEAST] = { .dx =  1, .dy =  1 },
    [EAST]      = { .dx =  1, .dy =  0 },
    [SOUTHEAST] = { .dx =  1, .dy = -1 },
    [SOUTH]     = { .dx =  0, .dy = -1 },
    [SOUTHWEST] = { .dx = -1, .dy = -1 },
    [WEST]      = { .dx = -1, .dy =  0 },
    [NORTHWEST] = { .dx = -1, .dy =  1 },
};
/* clang-format on */

/**
 * @brief Population count (number of bits set).
 * @param x Input value.
 * @return Number of 1 bits in x.
 */
static int PopCount(uint64_t x)
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
 * @brief Find the index of the least significant bit.
 * @param x Input value.
 * @return Index of least significant 1 bit in x.
 */
static int Lsb(uint64_t x)
{
    int result = 0;
    while (!(x & 1))
    {
        ++result;
        x >>= 1;
    }
    return result;
}

/**
 * @brief Find and clear the Lsb.
 * @param x Pointer to input value. On return has its Lsb cleared.
 * @return Index of bit which was cleared in x.
 */
static int FindAndClearLsb(uint64_t &x)
{
    const int result = Lsb(x);
    x &= x - 1;
    return result;
}

/**
 * @brief RC4 random byte stream generator.
 */
class RC4
{
    uint8_t state[256];
    uint8_t x;
    uint8_t y;
    bool    is_initialized;

  public:
    RC4() : x(0), y(0), is_initialized(false)
    {
    }

    uint8_t next()
    {
        if (!is_initialized)
        {
            is_initialized = true;
            for (int i = 0; i != 256; ++i)
            {
                state[i] = i ^ 0x55;
            }
            /* Stir up the state to get some randomness going */
            for (int i = 0; i != 1013; ++i)
            {
                this->next();
            }
        }
        ++x;
        y += state[x];
        std::swap(state[x], state[y]);
        const uint8_t z = state[x] + state[y];
        return state[z];
    }
};

/**
 * @brief Generate a pseudo random number.
 * @return 64 bit next value in sequence.
 */
static uint64_t PseudoRandom64()
{
    static uint64_t x = 1;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    return x * 0x2545F4914F6CDD1Dull;
}

/**
 * @brief Generate a ray (vector) from a square in a single compass direction.
 * @param location Source square index.
 * @param direction Compass direction.
 * @return Vector from location in direction specified, excluding source square.
 */
static uint64_t VectorFrom(uint8_t location, int direction)
{
    uint64_t               result = NO_SQUARES;
    const DirectionVector &dv     = direction_vectors[direction];
    for (int x = FileOf(location) + dv.dx, y = RankOf(location) + dv.dy; x >= 0 && x < 8 && y >= 0 && y < 8; x += dv.dx, y += dv.dy)
    {
        result |= Bitboard(x, y);
    }
    return result;
}

static uint64_t NorthOf(uint8_t location)
{
    return VectorFrom(location, NORTH);
}

static uint64_t NortheastOf(uint8_t location)
{
    return VectorFrom(location, NORTHEAST);
}

static uint64_t EastOf(uint8_t location)
{
    return VectorFrom(location, EAST);
}

static uint64_t SoutheastOf(uint8_t location)
{
    return VectorFrom(location, SOUTHEAST);
}

static uint64_t SouthOf(uint8_t location)
{
    return VectorFrom(location, SOUTH);
}

static uint64_t SouthwestOf(uint8_t location)
{
    return VectorFrom(location, SOUTHWEST);
}

static uint64_t WestOf(uint8_t location)
{
    return VectorFrom(location, WEST);
}

static uint64_t NorthwestOf(uint8_t location)
{
    return VectorFrom(location, NORTHWEST);
}

/**
 * @brief Pawn attacks for white.
 * @param location Square index.
 * @return Bitboard of squares attacked by a white pawn on location.
 */
static uint64_t PawnAttacksWhite(uint8_t location)
{
    return ShiftNorthwest(Bitboard(location)) | ShiftNortheast(Bitboard(location));
}

/**
 * @brief Pawn attacks for black.
 * @param location Square index.
 * @return Bitboard of squares attacked by a black pawn on location.
 */
static uint64_t PawnAttacksBlack(uint8_t location)
{
    return ShiftSouthwest(Bitboard(location)) | ShiftSoutheast(Bitboard(location));
}

/**
 * @brief Knight attacks from Bitboard.
 * @param b Bitboard containing knight square.
 * @return Squares attacked by a knight on b.
 */
static uint64_t KnightFill(uint64_t b)
{
    const uint64_t west1     = ShiftWest(b);
    const uint64_t west2     = ShiftWest(west1);
    const uint64_t east1     = ShiftEast(b);
    const uint64_t east2     = ShiftEast(east1);
    const uint64_t eastwest1 = west1 | east1;
    const uint64_t eastwest2 = west2 | east2;
    return (eastwest1 << 16) | (eastwest1 >> 16) | (eastwest2 << 8) | (eastwest2 >> 8);
}

/**
 * @brief Knight attacks from square location.
 * @param location Square index.
 * @return Squares attacked by a knight standing on location.
 */
static uint64_t KnightAttacks(uint8_t location)
{
    return KnightFill(Bitboard(location));
}

/**
 * @brief Bishop attacks.
 * @param location Square index.
 * @return Bishop attacks on an otherwise empty board.
 */
static uint64_t BishopAttacks(uint8_t location)
{
    return NortheastOf(location) | NorthwestOf(location) | SoutheastOf(location) | SouthwestOf(location);
}

/**
 * @brief Rook attacks.
 * @param location Square index.
 * @return Rook attacks on an otherwise empty board.
 */
static uint64_t RookAttacks(uint8_t location)
{
    return NorthOf(location) | SouthOf(location) | EastOf(location) | WestOf(location);
}

/**
 * @brief Queen attacks.
 * @param location Square index.
 * @return Queen attacks on an otherwise empty board.
 */
static uint64_t QueenAttacks(uint8_t location)
{
    return BishopAttacks(location) | RookAttacks(location);
}

/**
 * @brief King fill attacks.
 * @param b Bitboard containing King square.
 * @return Squares attacked by King on b.
 */
static uint64_t KingFill(uint64_t b)
{
    return ShiftNorth(b) | ShiftNortheast(b) | ShiftEast(b) | ShiftSoutheast(b) | ShiftSouth(b) | ShiftSouthwest(b) | ShiftWest(b) |
           ShiftNorthwest(b);
}

/**
 * @brief King attacks.
 * @param location Square index.
 * @return King attacks by a king standing on location.
 */
static uint64_t KingAttacks(uint8_t location)
{
    return KingFill(Bitboard(location));
}

static uint64_t KingAttacks2(uint8_t location)
{
    return KingFill(KingFill(Bitboard(location)));
}

static uint64_t KingPawnShelterWhite(uint8_t location)
{
    const uint64_t b = Bitboard(location);
    return ShiftNorthwest(b) | ShiftNorth(b) | ShiftNortheast(b);
}

static uint64_t KingPawnShelterBlack(uint8_t location)
{
    const uint64_t b = Bitboard(location);
    return ShiftSouthwest(b) | ShiftSouth(b) | ShiftSoutheast(b);
}

/**
 * @brief Passed pawn mask for white.
 * @param location Square index.
 * @return Squares which must be free of black pawns for a white pawn on location to be passed.
 */
static uint64_t PassedPawnMaskWhite(uint8_t location)
{
    uint64_t  result = NO_SQUARES;
    const int locn_x = FileOf(location);
    const int locn_y = RankOf(location);
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
 * @param location Square index.
 * @return Squares which must be free of white pawns for a black pawn on location to be passed.
 */
static uint64_t PassedPawnMaskBlack(uint8_t location)
{
    uint64_t  result = NO_SQUARES;
    const int locn_x = FileOf(location);
    const int locn_y = RankOf(location);
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
 * @param location Square index.
 * @return Squares which must contain at least one white pawn else the pawn on location is isolated.
 */
static uint64_t IsolatedPawnMaskWhite(uint8_t location)
{
    uint64_t  result = NO_SQUARES;
    const int locn_x = FileOf(location);
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
 * @param location Square index.
 * @return Squares which must contain at least one black pawn else the pawn on location is isolated.
 */
static uint64_t IsolatedPawnMaskBlack(uint8_t location)
{
    return IsolatedPawnMaskWhite(location); /* files are symmetrical */
}

/**
 * @brief Supported pawn mask white.
 * @param location Square index.
 * @return Squares which if containing a friendly pawn can potentially defend the pawn on location.
 */
static uint64_t SupportedPawnMaskWhite(uint8_t location)
{
    uint64_t  result = NO_SQUARES;
    const int locn_x = FileOf(location);
    const int locn_y = RankOf(location);
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
 * @param location Square index.
 * @return Squares which if containing a friendly pawn can potentially defend the pawn on location.
 */
static uint64_t SupportedPawnMaskBlack(uint8_t location)
{
    uint64_t  result = NO_SQUARES;
    const int locn_x = FileOf(location);
    const int locn_y = RankOf(location);
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
 * @param location Square index.
 * @return Squares which if containing a friendly pawn, make the pawn on location doubled.
 */
static uint64_t DoubledPawnMaskWhite(uint8_t location)
{
    return NorthOf(location);
}

/**
 * @brief Doubled pawn mask black.
 * @param location Square index.
 * @return Squares which if containing a friendly pawn, make the pawn on location doubled.
 */
static uint64_t DoubledPawnMaskBlack(uint8_t location)
{
    return SouthOf(location);
}

/**
 * @brief Intervening squares on colinear ray.
 * @param from Source square index.
 * @param to Destination square index.
 * @return If from and to are colinear, the set of squares between them.
 */
static uint64_t InterveningSquares(int from, int to)
{
    for (int dir = NORTH; dir <= NORTHWEST; ++dir)
    {
        const uint64_t vector_from = VectorFrom(from, dir);
        if (vector_from & Bitboard(to))
        {
            return vector_from ^ VectorFrom(to, dir) ^ Bitboard(to);
        }
    }
    return NO_SQUARES;
}

/**
 * @brief Occupancy mask vector for a single direction.
 * Occupancy masks exclude the final end square of the ray
 * as this does not affect slider move targets.
 * @param location Square index.
 * @param direction Compass direction.
 * @return Occupancy mask for that location and direction.
 */
static uint64_t OccupancyMaskVector(uint8_t location, int direction)
{
    uint64_t               result = NO_SQUARES;
    const DirectionVector &dv     = direction_vectors[direction];
    for (int x = FileOf(location) + dv.dx, y = RankOf(location) + dv.dy; x + dv.dx >= 0 && x + dv.dx < 8 && y + dv.dy >= 0 && y + dv.dy < 8;
         x += dv.dx, y += dv.dy)
    {
        result |= Bitboard(x, y);
    }
    return result;
}

/**
 * @brief Bishop occupancy mask.
 * @param location Square index.
 * @return Bishop sliding move occupancy mask for source square location.
 */
static uint64_t BishopOccupancyMask(uint8_t location)
{
    return OccupancyMaskVector(location, NORTHEAST) | OccupancyMaskVector(location, NORTHWEST) | OccupancyMaskVector(location, SOUTHEAST) |
           OccupancyMaskVector(location, SOUTHWEST);
}

/**
 * @brief Rook occupancy mask.
 * @param location Square index.
 * @return Rook sliding move occupancy mask for source square location.
 */
static uint64_t RookOccupancyMask(uint8_t location)
{
    return OccupancyMaskVector(location, NORTH) | OccupancyMaskVector(location, SOUTH) | OccupancyMaskVector(location, EAST) |
           OccupancyMaskVector(location, WEST);
}

/**
 * @brief Vector move targets for sliding pieces considering occupancy.
 * @param occupied_squares Set of squares with a piece on them.
 * @param location Source square index.
 * @param direction Compass rose direction.
 * @return Set of sliding move squares in the specified direction.
 */
static uint64_t VectorMoveTargets(uint64_t occupied_squares, int location, int direction)
{
    uint64_t               result = NO_SQUARES;
    const DirectionVector &dv     = direction_vectors[direction];
    for (int x = FileOf(location) + dv.dx, y = RankOf(location) + dv.dy; x >= 0 && x < 8 && y >= 0 && y < 8; x += dv.dx, y += dv.dy)
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
 * @param location Source square index.
 * @return Squares to which a bishop on location can slide to.
 */
static uint64_t BishopMoveTargets(uint64_t occupied_squares, uint8_t location)
{
    return VectorMoveTargets(occupied_squares, location, NORTHEAST) | VectorMoveTargets(occupied_squares, location, SOUTHEAST) |
           VectorMoveTargets(occupied_squares, location, SOUTHWEST) | VectorMoveTargets(occupied_squares, location, NORTHWEST);
}

/**
 * @brief Rook move targets
 * @param occupied_squares Set of squares occupied by a piece.
 * @param location Source square index.
 * @return Squares to which a rook on location can slide to.
 */
static uint64_t RookMoveTargets(uint64_t occupied_squares, uint8_t location)
{
    return VectorMoveTargets(occupied_squares, location, NORTH) | VectorMoveTargets(occupied_squares, location, SOUTH) |
           VectorMoveTargets(occupied_squares, location, EAST) | VectorMoveTargets(occupied_squares, location, WEST);
}

/**
 * @brief Given a bit mask, enumerate all the possible combinations
 * of bits set from that mask.
 * @param mask The mask value.
 * @param values Array to store the combination bitset values.
 * @return Number of combinations generated.
 */
static int EnumerateMaskCombinations(uint64_t mask, uint64_t values[])
{
    uint8_t  bit_indices[64] = {0};
    uint64_t b               = mask;
    for (int i = 0; b != 0; ++i)
    {
        bit_indices[i] = FindAndClearLsb(b);
    }
    const int num_bits_in_mask = PopCount(mask);
    const int num_combinations = 1 << num_bits_in_mask;
    for (int i = 0; i != num_combinations; ++i)
    {
        uint64_t value = NO_SQUARES;
        b              = i;
        while (b)
        {
            value |= Bitboard(bit_indices[FindAndClearLsb(b)]);
        }
        values[i] = value;
    }
    return num_combinations;
}

typedef uint64_t (*OccupancyFn)(uint8_t location);
typedef uint64_t (*MoveTargetFn)(uint64_t occupied_squares, uint8_t location);
#define MAX_ATTACK_SET_SIZE 4096 /* Rooks have a 12 bit occupancy mask. */

/**
 * @brief Parameters for magic Bitboard search.
 */
typedef struct Magic
{
    MoveTargetFn move_target_fn;               /**< Move target function for actual moves available. */
    OccupancyFn  occupancy_mask_fn;            /**< Occupancy mask function. */
    uint64_t     magic;                        /**< The magic multiplicand. */
    uint64_t     mask;                         /**< Occupancy mask. */
    int          shift;                        /**< Number of bits to right shift to get indices. */
    int          num_discrete_attacks;         /**< Number of separate discrete attack vectors generated. */
    uint64_t     attacks[MAX_ATTACK_SET_SIZE]; /**< The discrete attack vectors. */
    uint8_t      indices[MAX_ATTACK_SET_SIZE]; /**< Indices into the discrete attack vector array. */
} Magic;

/**
 * @brief Trial and error search for a magic Bitboard entry.
 * @param location Square index.
 * @param magic Where to store the found magic values.
 *              Has functions populated on entry.
 */
static void FindMagic(int location, Magic *magic)
{
    uint64_t occupancies[MAX_ATTACK_SET_SIZE];
    uint64_t actual_attacks[MAX_ATTACK_SET_SIZE];
    magic->mask  = magic->occupancy_mask_fn(location);
    magic->shift = 64 - PopCount(magic->mask);
    /*
    Enumerate all possible occupancy values and compute what the actual
    attack sets for each occupancy are, so we can check to see if a trial
    magic value works.
    */
    const int num_values = EnumerateMaskCombinations(magic->mask, occupancies);
    for (int i = 0; i != num_values; ++i)
    {
        actual_attacks[i] = magic->move_target_fn(occupancies[i], location);
    }
    /*
    Try a random magic multiplicand and test it for success.
    */
    bool     is_hash_collision = false;
    uint64_t trial_magic;
    do
    {
        is_hash_collision = false;
        /* Magics are typically fairly sparse, so AND a few random numbers together. */
        trial_magic = PseudoRandom64() & PseudoRandom64() & PseudoRandom64();
        memset(magic->attacks, 0xFF, sizeof(magic->attacks));
        memset(magic->indices, 0xFF, sizeof(magic->indices));
        for (int i = 0; i != num_values; ++i)
        {
            const unsigned int index = (unsigned int)((occupancies[i] * trial_magic) >> magic->shift);
            if (magic->attacks[index] == ~NO_SQUARES)
            {
                /* Nothing here yet so store the attack. */
                magic->attacks[index] = actual_attacks[i];
            }
            if (magic->attacks[index] != actual_attacks[i])
            {
                /* A previous entry clashes with this entry - this trial magic is no good. */
                is_hash_collision = true;
                break;
            }
        }
    } while (is_hash_collision);

    /*
    Compress the actual attacks set down to a set of indices
    and unique attack vectors. Since the indices are 1 byte each
    and the vectors are 8 bytes each this saves a lot of space in the
    final magic move entry tables, which helps with cache pressure.
    */
    uint64_t discrete_attacks[MAX_ATTACK_SET_SIZE];
    int      num_discrete_attacks = 0;
    for (int i = 0; i != num_values; ++i)
    {
        bool has_found_attack = false;
        for (int j = 0; j != num_discrete_attacks; ++j)
        {
            if (discrete_attacks[j] == magic->attacks[i])
            {
                magic->indices[i] = j;
                has_found_attack  = true;
                break;
            }
        }
        if (!has_found_attack)
        {
            discrete_attacks[num_discrete_attacks] = magic->attacks[i];
            magic->indices[i]                      = num_discrete_attacks;
            ++num_discrete_attacks;
        }
    }
    magic->num_discrete_attacks = num_discrete_attacks;
    magic->magic                = trial_magic;
    memcpy(magic->attacks, discrete_attacks, num_discrete_attacks * sizeof(uint64_t));
    return;
}

/**
 * @brief A magic Bitboard search vector entry.
 */
typedef struct MagicVector
{
    const char  *name;              /**< Piece name (bishop or rook). */
    MoveTargetFn move_target_fn;    /**< Function pointer to actual move generation. */
    OccupancyFn  occupancy_mask_fn; /**< Function pointer to occupancy mask. */
} MagicVector;

static const MagicVector magic_vectors[] = {
    {"BISHOP", BishopMoveTargets, BishopOccupancyMask}, {"ROOK", RookMoveTargets, RookOccupancyMask}, {NULL, NULL, NULL}};

/**
 * @brief Magics for each of the 64 squares on the chess board.
 * Too big to sit on the stack!
 */
static Magic magics[64];

/**
 * @brief Generate magic bitboards.
 */
static void GenerateMagics(void)
{
    for (const MagicVector *v = magic_vectors; v->name; ++v)
    {
        /* Find magic values for each square on the board. */
        for (int i = 0; i != 64; ++i)
        {
            magics[i].occupancy_mask_fn = v->occupancy_mask_fn;
            magics[i].move_target_fn    = v->move_target_fn;
            FindMagic(i, &magics[i]);
        }
        /* First print the arrays for the discrete attacks and the attack indices. */
        for (int i = 0; i != 64; ++i)
        {
            char      square_name[3] = {(char)('A' + FileOf(i)), RankChar(i), 0};
            const int num_attacks    = 1 << (64 - magics[i].shift);
            printf("static const uint8_t %s_MAGIC_INDICES_%s[%d] = \n{", v->name, square_name, num_attacks);
            for (int j = 0; j != num_attacks; ++j)
            {
                if (j % 16 == 0)
                {
                    printf("\n    ");
                }
                printf("0x%02X, ", magics[i].indices[j]);
            }
            printf("\n};\n");
            printf("static const uint64_t %s_MAGIC_ATTACKS_%s[%d] = \n{", v->name, square_name, magics[i].num_discrete_attacks);
            for (int j = 0; j != magics[i].num_discrete_attacks; ++j)
            {
                if (j % 8 == 0)
                {
                    printf("\n    ");
                }
                printf("0x%016" PRIX64 "ull, ", magics[i].attacks[j]);
            }
            printf("\n};\n");
        }
        /* Next print the MagicMoveEntry for this piece / square combination. */
        printf("extern const MagicMoveEntry %s_MAGICS[64] = \n{\n", v->name);
        for (int i = 0; i != 64; ++i)
        {
            char square_name[3] = {(char)('A' + FileOf(i)), RankChar(i), 0};
            printf("    {\n");
            printf("        .magic          = 0x%016" PRIX64 "ull,\n", magics[i].magic);
            printf("        .occupancy_mask = 0x%016" PRIX64 "ull,\n", magics[i].mask);
            printf("        .shift          = %d,\n", magics[i].shift);
            printf("        .attacks        = %s_MAGIC_ATTACKS_%s,\n", v->name, square_name);
            printf("        .indices        = %s_MAGIC_INDICES_%s,\n", v->name, square_name);
            printf("    },\n");
        }
        printf("};\n");
    }
}

/**
 * @brief Function pointer for a Bitboard generator function,
 * i.e. a function which converts a square location into a Bitboard
 * based on some property.
 */
typedef uint64_t (*BitboardFn)(uint8_t location);

/**
 * @brief Bitboard generator.
 */
typedef struct BitboardGen
{
    const char *name;     /**< Name of generated output.      */
    BitboardFn  function; /**< Function to generate output.   */
} BitboardGen;

/**
 * @brief Generators for the main precomputed Bitboard arrays.
 */
static const BitboardGen ray_generators[] = {
    {"north", NorthOf},
    {"northeast", NortheastOf},
    {"east", EastOf},
    {"southeast", SoutheastOf},
    {"south", SouthOf},
    {"southwest", SouthwestOf},
    {"west", WestOf},
    {"northwest", NorthwestOf},
    {"pawn_attacks_white", PawnAttacksWhite},
    {"pawn_attacks_black", PawnAttacksBlack},
    {"knight_attacks", KnightAttacks},
    {"bishop_attacks", BishopAttacks},
    {"rook_attacks", RookAttacks},
    {"queen_attacks", QueenAttacks},
    {"king_attacks", KingAttacks},
    {"king_attacks2", KingAttacks2},
    {"king_pawn_shelter_white", KingPawnShelterWhite},
    {"king_pawn_shelter_black", KingPawnShelterBlack},
    {NULL, NULL},
};

static const BitboardGen pawn_generators_white[] = {
    {
        "passed_pawn_mask",
        PassedPawnMaskWhite,
    },
    {
        "isolated_pawn_mask",
        IsolatedPawnMaskWhite,
    },
    {
        "supported_pawn_mask",
        SupportedPawnMaskWhite,
    },
    {
        "doubled_pawn_mask",
        DoubledPawnMaskWhite,
    },
    {NULL, NULL},
};

static const BitboardGen pawn_generators_black[] = {
    {
        "passed_pawn_mask",
        PassedPawnMaskBlack,
    },
    {
        "isolated_pawn_mask",
        IsolatedPawnMaskBlack,
    },
    {
        "supported_pawn_mask",
        SupportedPawnMaskBlack,
    },
    {
        "doubled_pawn_mask",
        DoubledPawnMaskBlack,
    },
    {NULL, NULL},
};

static const BitboardGen *pawn_generators[] = {
    pawn_generators_white,
    pawn_generators_black,
};

/**
 * @brief Piece names.
 */
static const char *const piece_names[7] = {"none", "pawn", "knight", "bishop", "rook", "queen", "king"};

/**
 * @brief Color names.
 */
static const char *const color_names[2] = {"white", "black"};

/**
 * @brief Main entry point. Prints to stdout which gets redirected in the Makefile to create
 * the generated data source file which is then used by the main program.
 * @return 0
 */
int main()
{
    printf("/* This file was generated on " __DATE__ " at " __TIME__ " */\n");
    printf("#include \"generated_data.h\"\n");
    printf("extern const Sets SETS[64] = \n{");
    for (uint8_t location = 0; location != 64; ++location)
    {
        printf("\n    { /* square %c%c */", FileChar(location), RankChar(location));
        for (const BitboardGen *generator = ray_generators; generator->name; ++generator)
        {
            const uint64_t b = generator->function(location);
            printf("\n        .%-25s = 0x%016" PRIX64 "ull, /* popcnt %2d */", generator->name, b, PopCount(b));
        }
        printf("\n    },");
    }
    printf("\n};\n");

    printf("extern const PawnSets PAWN_SETS[2][64] = \n{");
    for (int color = 0; color != 2; ++color)
    {
        printf("\n    {");
        for (uint8_t location = 0; location != 64; ++location)
        {
            printf("\n        { /* %s square %c%c */", color_names[color], FileChar(location), RankChar(location));
            for (const BitboardGen *generator = pawn_generators[color]; generator->name; ++generator)
            {
                const uint64_t b = generator->function(location);
                printf("\n            .%-28s = 0x%016" PRIX64 "ull, /* popcnt %2d */", generator->name, b, PopCount(b));
            }
            printf("\n        },");
        }
        printf("\n    },");
    }
    printf("\n};\n");

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
    printf("extern const uint64_t PIECE_SQUARE_HASHES[2][6][64] = \n{");
    for (int color = 0; color != 2; ++color)
    {
        printf("\n    {");
        for (int piece = PAWN; piece <= KING; ++piece)
        {
            printf("\n        {   /* %s %s */", color_names[color], piece_names[piece]);
            for (int i = 0; i != 64; ++i)
            {
                if ((i & 7) == 0)
                {
                    printf("\n            ");
                }
                printf("0x%016" PRIX64 "ull,", PseudoRandom64());
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
        printf("0x%016" PRIX64 "ull,", PseudoRandom64());
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
        printf("0x%016" PRIX64 "ull,", rank == 2 || rank == 5 ? PseudoRandom64() : 0);
    }
    printf("\n};\n");
    GenerateMagics();
    return 0;
}

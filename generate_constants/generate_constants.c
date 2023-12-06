/**
 * @file Precomputes at compile time, constants used by the main Pawnstar program.
*/
#include <stdio.h>
#include <stdbool.h>
#include <memory.h>
#include <string.h>
#include <math.h>

#define DO_EXTRA_PAWN_EVAL  1

typedef unsigned long long  uint64_t;
typedef unsigned char       uint8_t;

#define MASK_EAST_1         0x7F7F7F7F7F7F7F7Full
#define MASK_WEST_1         0xFEFEFEFEFEFEFEFEull
#define NO_SQUARES          0x0000000000000000ull
#define OUTSIDE_SQUARES     0xFF818181818181FFull
#define FILE_OF(locn)       ((locn) & 7)
#define RANK_OF(locn)       ((locn) >> 3)
#define FILE_CHAR(locn)     ('a' + FILE_OF(locn))
#define RANK_CHAR(locn)     ('1' + RANK_OF(locn))
#define BITBOARD(locn)      (1ull << (locn))
#define BITBOARD_XY(x,y)    (1ull << ((x) + 8 * (y)))
#define SHIFT_NORTH(b)      ((b) << 8)
#define SHIFT_NORTHEAST(b)  (((b) & MASK_EAST_1) << 9)
#define SHIFT_EAST(b)       (((b) & MASK_EAST_1) << 1)
#define SHIFT_SOUTHEAST(b)  (((b) & MASK_EAST_1) >> 7)
#define SHIFT_SOUTH(b)      ((b) >> 8)
#define SHIFT_SOUTHWEST(b)  (((b) & MASK_WEST_1) >> 9)
#define SHIFT_WEST(b)       (((b) & MASK_WEST_1) >> 1)
#define SHIFT_NORTHWEST(b)  (((b) & MASK_WEST_1) << 7)

/**
 * @brief Chess piece types.
 */
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

/**
 * @brief Compass rose directions from White's perspective.
 */
enum Direction
{
    DIR_NORTH,
    DIR_NORTHEAST,
    DIR_EAST,
    DIR_SOUTHEAST,
    DIR_SOUTH,
    DIR_SOUTHWEST,
    DIR_WEST,
    DIR_NORTHWEST,
};

/**
 * @brief X and Y deltas for a compass rose direction.
 */
typedef struct DirectionVector
{
    int dx;
    int dy;
} DirectionVector;

/**
 * @brief Function pointer for a bitboard generator function, i.e. a function
 * which converts a square location into a bitboard.
*/
typedef uint64_t(*BitboardFn)(int location);

/**
 * @brief Bitboard generator.
 */
typedef struct BitboardGen
{
    const char* name;       /**< Name of generated output.      */
    BitboardFn  function;   /**< Function to generate output.   */
} BitboardGen;

/**
 * @brief Each of the directions on the compass.
 */
static const DirectionVector direction_vectors[] =
{
    [DIR_NORTH]     = { .dx =  0, .dy =  1 },
    [DIR_NORTHEAST] = { .dx =  1, .dy =  1 },
    [DIR_EAST]      = { .dx =  1, .dy =  0 },
    [DIR_SOUTHEAST] = { .dx =  1, .dy = -1 },
    [DIR_SOUTH]     = { .dx =  0, .dy = -1 },
    [DIR_SOUTHWEST] = { .dx = -1, .dy = -1 },
    [DIR_WEST]      = { .dx = -1, .dy =  0 },
    [DIR_NORTHWEST] = { .dx = -1, .dy =  1 },
};

/**
 * @brief Population count (number of bits set).
 * @param x Value to count bits of.
 * @return Number of 1 bits in x.
 */
static int
PopCount(uint64_t x)
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
 * @param x Value scan.
 * @return Index of least significant 1 bit in x.
 */
static int
Lsb(uint64_t x)
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
 * @brief Find and clear the LSB.
 * @param x On return has its LSB cleared.
 * @return Index of bit which was cleared in x.
 */
static int
FindAndClearLsb(uint64_t* x)
{
    const uint64_t y = *x;
    const int result = Lsb(y);
    *x = y & (y - 1);
    return result;
}

/**
 * @brief Generate a pseudo random 64 bit int. Uses XORSHIFT twice.
 * @return Next value in sequence.
 */
static uint64_t PseudoRandom64(void)
{
    static uint64_t x = 0x2545F4914F6CDD1Dull;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    uint64_t result = (x * 0x2545F4914F6CDD1Dull) >> 32;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    result |= (x * 0x2545F4914F6CDD1Dull) & 0xFFFFFFFF00000000ull;
    return result;
}

/**
 * @brief Generate a ray (vector) from a square in a single compass direction.
 * @param location Source square index.
 * @param direction Compass direction.
 * @return Vector from location in direction specified, excluding source square.
 */
static uint64_t VectorFrom(int location, int direction)
{
    uint64_t result = NO_SQUARES;
    int x, y;
    const DirectionVector* dv = &direction_vectors[direction];
    for (x = FILE_OF(location) + dv->dx, y = RANK_OF(location) + dv->dy;
         x >= 0 && x < 8 && y >= 0 && y < 8;
         x += dv->dx, y += dv->dy)
    {
        result |= BITBOARD_XY(x, y);
    }
    return result;
}

static uint64_t NorthOf(int location)
{
    return VectorFrom(location, DIR_NORTH);
}

static uint64_t NortheastOf(int location)
{
    return VectorFrom(location, DIR_NORTHEAST);
}

static uint64_t EastOf(int location)
{
    return VectorFrom(location, DIR_EAST);
}

static uint64_t SoutheastOf(int location)
{
    return VectorFrom(location, DIR_SOUTHEAST);
}

static uint64_t SouthOf(int location)
{
    return VectorFrom(location, DIR_SOUTH);
}

static uint64_t SouthwestOf(int location)
{
    return VectorFrom(location, DIR_SOUTHWEST);
}

static uint64_t WestOf(int location)
{
    return VectorFrom(location, DIR_WEST);
}

static uint64_t NorthwestOf(int location)
{
    return VectorFrom(location, DIR_NORTHWEST);
}

/**
 * @brief Pawn attacks for white.
 * @param location Square index.
 * @return Bitboard of squares attacked by a white pawn on location.
 */
static uint64_t PawnAttacksWhite(int location)
{
    return SHIFT_NORTHWEST(BITBOARD(location)) | SHIFT_NORTHEAST(BITBOARD(location));
}

/**
 * @brief Pawn attacks for black.
 * @param location Square index.
 * @return Bitboard of squares attacked by a black pawn on location.
 */
static uint64_t PawnAttacksBlack(int location)
{
    return SHIFT_SOUTHWEST(BITBOARD(location)) | SHIFT_SOUTHEAST(BITBOARD(location));
}

/**
 * @brief Knight attacks from bitboard.
 * @param b Bitboard containing knight square.
 * @return Squares attacked by a knight on b.
 */
static uint64_t KnightFill(uint64_t b)
{
    const uint64_t west1 = SHIFT_WEST(b);
    const uint64_t west2 = SHIFT_WEST(west1);
    const uint64_t east1 = SHIFT_EAST(b);
    const uint64_t east2 = SHIFT_EAST(east1);
    const uint64_t eastwest1 = west1 | east1;
    const uint64_t eastwest2 = west2 | east2;
    return (eastwest1 << 16) | (eastwest1 >> 16) | (eastwest2 << 8) | (eastwest2 >> 8);
}

/**
 * @brief Knight attacks from location.
 * @param location Square index.
 * @return Squares attacked by a knight on location.
 */
static uint64_t KnightAttacks(int location)
{
    return KnightFill(BITBOARD(location));
}

/**
 * @brief Bishop attacks.
 * @param location Square index.
 * @return Bishop attacks on an otherwise empty board.
 */
static uint64_t BishopAttacks(int location)
{
    return NortheastOf(location) | 
           NorthwestOf(location) | 
           SoutheastOf(location) | 
           SouthwestOf(location);
}

/**
 * @brief Rook attacks.
 * @param location Square index.
 * @return Rook attacks on an otherwise empty board.
 */
static uint64_t RookAttacks(int location)
{
    return NorthOf(location) | 
           SouthOf(location) | 
           EastOf(location)  | 
           WestOf(location);
}

/**
 * @brief Queen attacks.
 * @param location Square index.
 * @return Queen attacks on an otherwise empty board.
 */
static uint64_t QueenAttacks(int location)
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
    uint64_t x = SHIFT_WEST(b) | SHIFT_EAST(b);
    x |= SHIFT_NORTH(x) | SHIFT_SOUTH(x);
    return x | SHIFT_NORTH(b) | SHIFT_SOUTH(b);
}

/**
 * @brief King attacks.
 * @param location Square index.
 * @return King attacks by a king standing on location.
 */
static uint64_t KingAttacks(int location)
{
    return KingFill(BITBOARD(location));
}

#if DO_EXTRA_PAWN_EVAL

/**
 * @brief Passed pawn mask for white.
 * @param location Square index.
 * @return Squares which must be free of black pawns for a white pawn on location to be passed.
 */
static uint64_t PassedPawnMaskWhite(int location)
{
    uint64_t result     = NO_SQUARES;
    const int locn_x    = FILE_OF(location);
    const int locn_y    = RANK_OF(location);
    for (int x = locn_x - 1; x <= locn_x + 1; ++x)
    {
        if (x < 0 || x > 7)
        {
            continue; /* off board */
        }
        for (int y = locn_y + 1; y < 8; ++y)
        {
            result |= BITBOARD_XY(x, y);
        }
    }
    return result;
}

/**
 * @brief Passed pawn mask for black.
 * @param location Square index.
 * @return Squares which must be free of white pawns for a black pawn on location to be passed.
 */
static uint64_t PassedPawnMaskBlack(int location)
{
    uint64_t result     = NO_SQUARES;
    const int locn_x    = FILE_OF(location);
    const int locn_y    = RANK_OF(location);
    for (int x = locn_x - 1; x <= locn_x + 1; ++x)
    {
        if (x < 0 || x > 7)
        {
            continue; /* off board */
        }
        for (int y = locn_y - 1; y >= 0; --y)
        {
            result |= BITBOARD_XY(x, y);
        }
    }
    return result;
}

/**
 * @brief Isolated pawn mask for white.
 * @param location Square index.
 * @return Squares which must contain at least one white pawn else the pawn on location is isolated.
 */
static uint64_t IsolatedPawnMaskWhite(int location)
{
    uint64_t result     = NO_SQUARES;
    const int locn_x    = FILE_OF(location);
    for (int x = locn_x - 1; x <= locn_x + 1; x += 2)
    {
        if (x < 0 || x > 7)
        {
            continue; /* off board */
        }
        for (int y = 0; y < 8; ++y)
        {
            result |= BITBOARD_XY(x, y);
        }
    }
    return result;
}

/**
 * @brief Isolated pawn mask for black.
 * @param location Square index.
 * @return Squares which must contain at least one black pawn else the pawn on location is isolated.
 */
static uint64_t IsolatedPawnMaskBlack(int location)
{
    return IsolatedPawnMaskWhite(location); /* files are symmetrical */
}

/**
 * @brief Supported pawn mask white.
 * @param location Square index.
 * @return Squares which if containing a friendly pawn can potentially defend the pawn on location.
 */
static uint64_t SupportedPawnMaskWhite(int location)
{
    uint64_t result       = NO_SQUARES;
    const int locn_x    = FILE_OF(location);
    const int locn_y    = RANK_OF(location);;
    for (int x = locn_x - 1; x <= locn_x + 1; x += 2)
    {
        if (x < 0 || x > 7)
        {
            continue; /* off board */
        }
        for (int y = locn_y - 1; y >= 0; --y)
        {
            result |= BITBOARD_XY(x, y);
        }
    }
    return result;
}

/**
 * @brief Supported pawn mask black.
 * @param location Square index.
 * @return Squares which if containing a friendly pawn can potentially defend the pawn on location.
 */
static uint64_t SupportedPawnMaskBlack(int location)
{
    uint64_t result    = NO_SQUARES;
    const int locn_x = FILE_OF(location);
    const int locn_y = RANK_OF(location);
    for (int x = locn_x - 1; x <= locn_x + 1; x += 2)
    {
        if (x < 0 || x > 7)
        {
            continue; /* off board */
        }
        for (int y = locn_y + 1; y < 8; ++y)
        {
            result |= BITBOARD_XY(x, y);
        }
    }
    return result;
}

/**
 * @brief Doubled pawn mask white.
 * @param location Square index.
 * @return Squares which if containing a friendly pawn, make the pawn on location doubled.
 */
static uint64_t DoubledPawnMaskWhite(int location)
{
    return NorthOf(location);
}

/**
 * @brief Doubled pawn mask black.
 * @param location Square index.
 * @return Squares which if containing a friendly pawn, make the pawn on location doubled.
 */
static uint64_t DoubledPawnMaskBlack(int location)
{
    return SouthOf(location);
}

#endif /* DO_EXTRA_PAWN_EVAL */

/**
 * @brief Intervening squares on colinear ray.
 * @param from Source square index.
 * @param to Destination square index.
 * @return If from and to are colinear, the set of intervening squares between them.
 */
static uint64_t InterveningSquares(int from, int to)
{
    for (int dir = DIR_NORTH; dir <= DIR_NORTHWEST; ++dir)
    {
        const uint64_t vector_from = VectorFrom(from, dir);
        if (vector_from & BITBOARD(to))
        {
            return vector_from ^ VectorFrom(to, dir) ^ BITBOARD(to);
        }
    }
    return NO_SQUARES;
}

/**
 * @brief Occupancy mask forward diagonal.
 * @param location Square index.
 * @return Occupancy mask for sliding moves in the a1->h8 direction.
 */
static uint64_t
DiagonalOccupancyMask(int location)
{
    uint64_t result = NO_SQUARES;
    int x, y;
    for (x = FILE_OF(location) + 1, y = RANK_OF(location) + 1; x < 7 && y < 7; ++x, ++y)
    {
        result |= BITBOARD_XY(x, y);
    }
    for (x = FILE_OF(location) - 1, y = RANK_OF(location) - 1; x > 0 && y > 0; --x, --y)
    {
        result |= BITBOARD_XY(x, y);
    }
    return result;
}

/**
 * @brief Occupancy mask anti diagonal.
 * @param location Square index.
 * @return Occupancy mask for sliding moves in the a8->h1 direction.
 */
static uint64_t
AntiDiagonalOccupancyMask(int location)
{
    uint64_t result = NO_SQUARES;
    int x, y;
    for (x = FILE_OF(location) + 1, y = RANK_OF(location) - 1; x < 7 && y > 0; ++x, --y)
    {
        result |= BITBOARD_XY(x, y);
    }
    for (x = FILE_OF(location) - 1, y = RANK_OF(location) + 1; x > 0 && y < 7; --x, ++y)
    {
        result |= BITBOARD_XY(x, y);
    }
    return result;
}

/**
 * @brief File occupancy mask.
 * @param location Square index.
 * @return Occupancy mask for sliding moves along a file.
 */
static uint64_t
FileOccupancyMask(int location)
{
    uint64_t result = NO_SQUARES;
    int x, y;
    x = FILE_OF(location);
    for (y = RANK_OF(location) + 1; y < 7; ++y)
    {
        result |= BITBOARD_XY(x, y);
    }
    for (y = RANK_OF(location) - 1; y > 0; --y)
    {
        result |= BITBOARD_XY(x, y);
    }
    return result;
}

/**
 * @brief Rank occupancy mask.
 * @param location Square index.
 * @return Occupancy mask for sliding moves along a rank.
 */
static uint64_t
RankOccupancyMask(int location)
{
    uint64_t result = NO_SQUARES;
    int x, y;
    y = RANK_OF(location);
    for (x = FILE_OF(location) + 1; x < 7; ++x)
    {
        result |= BITBOARD_XY(x, y);
    }
    for (x = FILE_OF(location) - 1; x > 0; --x)
    {
        result |= BITBOARD_XY(x, y);
    }
    return result;
}

/**
 * @brief Bishop occupancy mask.
 * @param location Square index.
 * @return Bishop sliding move occupancy mask for source square location.
 */
static uint64_t
BishopOccupancyMask(int location)
{
    return DiagonalOccupancyMask(location) | AntiDiagonalOccupancyMask(location);
}

/**
 * @brief Rook occupancy mask.
 * @param location Square index.
 * @return Rook sliding move occupancy mask for source square location.
 */
static uint64_t
RookOccupancyMask(int location)
{
    return FileOccupancyMask(location) | RankOccupancyMask(location);
}

/**
 * @brief Diagonal move targets.
 * @param occupied_squares Set of squares occupied by a piece.
 * @param location Source square index.
 * @return Set of forward diagonal move targets which a piece on location can slide to.
 */
static uint64_t
DiagonalMoveTargets(uint64_t occupied_squares, 
                    int      location)
{
    uint64_t result = NO_SQUARES;
    for (uint64_t square = SHIFT_NORTHEAST(BITBOARD(location)); 
         square; 
         square = SHIFT_NORTHEAST(square))
    {
        result |= square;
        if (square & occupied_squares)
        {
            break;
        }
    }
    for (uint64_t square = SHIFT_SOUTHWEST(BITBOARD(location)); 
         square; 
         square = SHIFT_SOUTHWEST(square))
    {
        result |= square;
        if (square & occupied_squares)
        {
            break;
        }
    }
    return result;
}

/**
 * @brief Antidiagonal move targets.
 * @param occupied_squares Set of squares occupied by a piece.
 * @param location Source square index.
 * @return Set of anti diagonal move targets which a piece on location can slide to.
 */
static uint64_t
AntiDiagonalMoveTargets(uint64_t occupied_squares,
                        int      location)
{
    uint64_t result = NO_SQUARES;
    for (uint64_t square = SHIFT_SOUTHEAST(BITBOARD(location)); 
        square; 
        square = SHIFT_SOUTHEAST(square))
    {
        result |= square;
        if (square & occupied_squares)
        {
            break;
        }
    }
    for (uint64_t square = SHIFT_NORTHWEST(BITBOARD(location)); 
         square; 
         square = SHIFT_NORTHWEST(square))
    {
        result |= square;
        if (square & occupied_squares)
        {
            break;
        }
    }
    return result;
}

/**
 * @brief Rank move targets.
 * @param occupied_squares Set of squares occupied by a piece.
 * @param location Source square index.
 * @return Set of rank move targets which a piece on location can slide to.
 */
static uint64_t
RankMoveTargets(uint64_t occupied_squares,
                int      location)
{
    uint64_t result = NO_SQUARES;
    for (uint64_t square = SHIFT_EAST(BITBOARD(location)); 
         square; 
         square = SHIFT_EAST(square))
    {
        result |= square;
        if (square & occupied_squares)
        {
            break;
        }
    }
    for (uint64_t square = SHIFT_WEST(BITBOARD(location)); 
         square; 
         square = SHIFT_WEST(square))
    {
        result |= square;
        if (square & occupied_squares)
        {
            break;
        }
    }
    return result;
}

/**
 * @brief File move targets.
 * @param occupied_squares Set of squares occupied by a piece.
 * @param location Source square index.
 * @return Set of file move targets which a piece on location can slide to.
 */
static uint64_t
FileMoveTargets(uint64_t occupied_squares,
                int      location)
{
    uint64_t result = NO_SQUARES;
    for (uint64_t square = SHIFT_NORTH(BITBOARD(location)); 
         square; 
         square = SHIFT_NORTH(square))
    {
        result |= square;
        if (square & occupied_squares)
        {
            break;
        }
    }
    for (uint64_t square = SHIFT_SOUTH(BITBOARD(location)); 
         square; 
         square = SHIFT_SOUTH(square))
    {
        result |= square;
        if (square & occupied_squares)
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
static uint64_t
BishopMoveTargets(uint64_t occupied_squares, 
                    int    location)
{
    return DiagonalMoveTargets(occupied_squares, location) | 
           AntiDiagonalMoveTargets(occupied_squares, location);
}

/**
 * @brief Rook move targets
 * @param occupied_squares Set of squares occupied by a piece.
 * @param location Source square index.
 * @return Squares to which a rook on location can slide to.
 */
static uint64_t
RookMoveTargets(uint64_t occupied_squares, 
                  int    location)
{
    return RankMoveTargets(occupied_squares, location) | 
           FileMoveTargets(occupied_squares, location);
}

/**
 * @brief Given a bit mask, enumerate all the possible combinations of bits set from that mask.
 * @param mask The mask value.
 * @param values Array to store the combination bitset values.
 * @return Number of combinations generated.
 */
static int
EnumerateMaskCombinations(uint64_t mask,
                          uint64_t values[])
{
    const int num_bits_in_mask = PopCount(mask);
    const int num_combinations = 1 << num_bits_in_mask;
    int bit_indices[64] = { 0 };
    uint64_t b = mask;
    for (int i = 0; i != num_bits_in_mask; ++i)
    {
        bit_indices[i] = FindAndClearLsb(&b);
    }
    for (int i = 0; i != num_combinations; ++i)
    {
        uint64_t value = 0;
        uint64_t j = i;
        while (j)
        {
            value |= BITBOARD(bit_indices[FindAndClearLsb(&j)]);
        }
        values[i] = value;
    }
    return num_combinations;
}

typedef uint64_t(*OccupancyFn)   (int location);
typedef uint64_t(*MoveTargetFn)  (uint64_t occupied_squares, int location);
#define MAX_ATTACK_SET_SIZE 4096 /* 12 bit occupancy mask for rooks */

/**
 * @brief Parameters for magic bitboard search.
 */
typedef struct Magic
{
    MoveTargetFn    move_target_fn;                 /**< Move target function for actual moves available. */
    OccupancyFn     occupancy_mask_fn;              /**< Occupancy mask function. */
    uint64_t        magic;                          /**< The magic multiplicand. */
    uint64_t        mask;                           /**< Occupancy mask. */
    int             shift;                          /**< Number of bits to right shift to get indices. */
    int             num_discrete_attacks;           /**< Number of separate discrete attack vectors generated. */
    uint64_t        attacks[MAX_ATTACK_SET_SIZE];   /**< The discrete attack vectors. */
    uint8_t         indices[MAX_ATTACK_SET_SIZE];   /**< Indices into the discrete attack vector array. */
} Magic;

/**
 * @brief Trial and error search for a magic bitboard entry.
 * @param location Square index.
 * @param magic Where to store the found magic values.
 */
static void
FindMagic(int    location,
          Magic* magic)
{
    uint64_t occupancies[MAX_ATTACK_SET_SIZE];
    uint64_t actual_attacks[MAX_ATTACK_SET_SIZE];
    magic->mask = magic->occupancy_mask_fn(location);
    magic->shift = 64 - PopCount(magic->mask);
    /*
    Enumerate all possible occupancy values and compute what the actual
    attack sets for each occupancy are, so we can check to see if the trial
    magic value works.
    */
    const int num_values = EnumerateMaskCombinations(magic->mask, occupancies);
    for (int i = 0; i != num_values; ++i)
    {
        actual_attacks[i] = magic->move_target_fn(occupancies[i], location);
    }
    /*
    Try a random magic multiplicand and test it for success.
    Keep going round until we find a magic number that works!
    */
    for (; ; )
    {
        bool is_hash_collision = false;
        /* Magics are typically fairly sparse so AND a few random numbers together. */
        const uint64_t trial_magic = PseudoRandom64() & PseudoRandom64() & PseudoRandom64();
        memset(magic->attacks, 0, sizeof(magic->attacks));
        memset(magic->indices, 0, sizeof(magic->indices));
        for (int i = 0; i != num_values; ++i)
        {
            const unsigned int index = (unsigned int)((occupancies[i] * trial_magic) >> magic->shift);
            if (magic->attacks[index] == NO_SQUARES)
            {
                /* Nothing here yet so store the attack. */
                magic->attacks[index] = actual_attacks[i];
                continue;
            }
            if (magic->attacks[index] != actual_attacks[i])
            {
                /* A previous entry clashes with this entry - this trial magic is no good. */
                is_hash_collision = true;
                break;
            }
        }
        if (!is_hash_collision)
        {
            /* 
            Compress the actual attacks set down to a set of indices 
            and unique attack vectors. Since the indices are 1 byte each
            and the vectors are 8 bytes each this saves a lot of space in the
            final magic move entry tables, which helps with cache pressure.
            */
            uint64_t discrete_attacks[MAX_ATTACK_SET_SIZE];
            int num_discrete_attacks = 0;
            for (int i = 0; i != num_values; ++i)
            {
                bool has_found_attack = false;
                for (int j = 0; j != num_discrete_attacks; ++j)
                {
                    if (discrete_attacks[j] == magic->attacks[i])
                    {
                        magic->indices[i] = j;
                        has_found_attack = true;
                        break;
                    }
                }
                if (!has_found_attack)
                {
                    discrete_attacks[num_discrete_attacks] = magic->attacks[i];
                    magic->indices[i] = num_discrete_attacks;
                    ++num_discrete_attacks;
                }
            }
            magic->num_discrete_attacks = num_discrete_attacks;
            magic->magic = trial_magic;
            memcpy(magic->attacks, discrete_attacks, num_discrete_attacks * sizeof(uint64_t));
            return;
        }
    }
}

/**
 * @brief A magic bitboard search vector entry.
 */
typedef struct MagicVector
{
    const char*     name;               /**< Piece name (bishop or rook). */
    MoveTargetFn    move_target_fn;     /**< Function pointer to actual move generation. */
    OccupancyFn     occupancy_mask_fn;  /**< Function pointer to occupancy mask. */
} MagicVector;

static const MagicVector magic_vectors[] = 
{
    { "BISHOP", BishopMoveTargets,  BishopOccupancyMask },
    { "ROOK",   RookMoveTargets,    RookOccupancyMask   },
    { NULL,     NULL,               NULL                }
};

/**
 * @brief Magics for each of the 64 squares on the chess board.
 * Too big to sit on the stack!
 */
static Magic magics[64];

/**
 * @brief Generate magic bitboards.
 */
static void
GenerateMagics(void)
{
    for (const MagicVector* v = magic_vectors; v->name; ++v)
    {
        /* Find magic values for each square on the board. */
        for (int i = 0; i != 64; ++i)
        {
            magics[i].occupancy_mask_fn = v->occupancy_mask_fn;
            magics[i].move_target_fn = v->move_target_fn;
            FindMagic(i, &magics[i]);
        }
        /* First print the arrays for the discrete attacks and the attack indices. */
        for (int i = 0; i != 64; ++i)
        {
            char square_name[3] = { 'A' + FILE_OF(i), RANK_CHAR(i), 0 };
            const int num_attacks = 1 << (64 - magics[i].shift);
            printf("static const uint8_t %s_MAGIC_INDICES_%s[%d] = \n{", v->name, square_name, num_attacks);
            for (int j = 0; j != num_attacks; ++j)
            {
                if (j % 32 == 0)
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
                printf("0x%016llXull, ", magics[i].attacks[j]);
            }
            printf("\n};\n");
            /* Next print the MagicMoveEntry for this piece / square combination. */
            printf("static const MagicMoveEntry %s_MAGIC_%s = \n{\n", v->name, square_name);
            printf("    .magic          = 0x%016llXull,\n", magics[i].magic);
            printf("    .occupancy_mask = 0x%016llXull,\n", magics[i].mask);
            printf("    .shift          = %d,\n", magics[i].shift);
            printf("    .attacks        = %s_MAGIC_ATTACKS_%s,\n", v->name, square_name);
            printf("    .indices        = %s_MAGIC_INDICES_%s,\n", v->name, square_name);
            printf("};\n");
        }
    }
    /* Finally print the arrays of pointers to the magic move entries. */
    printf("const MagicMoveEntry* ROOK_MAGICS[64] = \n{");
    for (int i = 0; i != 64; ++i)
    {
        char square_name[3] = { 'A' + FILE_OF(i), RANK_CHAR(i), 0 };
        if (i % 8 == 0)
        {
            printf("\n    ");
        }
        printf("&ROOK_MAGIC_%s, ", square_name);
    }
    printf("\n};\n");
    printf("const MagicMoveEntry* BISHOP_MAGICS[64] = \n{");
    for (int i = 0; i != 64; ++i)
    {
        char square_name[3] = { 'A' + FILE_OF(i), RANK_CHAR(i), 0 };
        if (i % 8 == 0)
        {
            printf("\n    ");
        }
        printf("&BISHOP_MAGIC_%s, ", square_name);
    }
    printf("\n};\n");
}

/**
 * @brief Generators for the main precomputed bitboard arrays.
 */
static const BitboardGen ray_generators[] = 
{
    { "north",                      NorthOf                 },
    { "northeast",                  NortheastOf             },
    { "east",                       EastOf                  },
    { "southeast",                  SoutheastOf             },
    { "south",                      SouthOf                 },
    { "southwest",                  SouthwestOf             },
    { "west",                       WestOf                  },
    { "northwest",                  NorthwestOf             },
    { "pawn_attacks_white",         PawnAttacksWhite        },
    { "pawn_attacks_black",         PawnAttacksBlack        },
    { "knight_attacks",             KnightAttacks           },
    { "bishop_attacks",             BishopAttacks           },
    { "rook_attacks",               RookAttacks             },
    { "queen_attacks",              QueenAttacks            },
    { "king_attacks",               KingAttacks             },
    { NULL,                         NULL                    },
};

#if DO_EXTRA_PAWN_EVAL

static const BitboardGen pawn_generators_white[] =
{
    { "passed_pawn_mask",           PassedPawnMaskWhite,    },
    { "isolated_pawn_mask",         IsolatedPawnMaskWhite,  },
    { "supported_pawn_mask",        SupportedPawnMaskWhite, },
    { "doubled_pawn_mask",          DoubledPawnMaskWhite,   },
    { NULL,                         NULL                    },
};

static const BitboardGen pawn_generators_black[] =
{
    { "passed_pawn_mask",           PassedPawnMaskBlack,    },
    { "isolated_pawn_mask",         IsolatedPawnMaskBlack,  },
    { "supported_pawn_mask",        SupportedPawnMaskBlack, },
    { "doubled_pawn_mask",          DoubledPawnMaskBlack,   },
    { NULL,                         NULL                    },
};

static const BitboardGen* pawn_generators[] =
{
    pawn_generators_white,
    pawn_generators_black,
};

#endif

/**
 * @brief Piece names.
 */
static const char* const piece_names[7] = 
{
    "none",
    "pawn",
    "knight",
    "bishop",
    "rook",
    "queen",
    "king"
};

/**
 * @brief Color names.
 */
static const char* const color_names[2] = 
{
    "white",
    "black"
};

/**
 * @brief Main entry point. Prints to stdout which gets redirected in the Makefile to create
 * the generated data source file which is then used by the main program.
 * @return 0
 */
int main()
{
    printf("/* This file was generated on " __DATE__ " at " __TIME__ " */\n");
    printf("#include \"types.h\"\n");
    printf("const Sets SETS[64] = \n{");
    for (int location = 0; location != 64; ++location)
    {
        printf("\n    { /* square %c%c */", FILE_CHAR(location), RANK_CHAR(location));
        for (const BitboardGen* generator = ray_generators; generator->name; ++generator)
        {
            const uint64_t b = generator->function(location);
            printf("\n        .%-20s = 0x%016llXull, /* popcnt %2d */",
                generator->name,
                b,
                PopCount(b));
        }
        printf("\n    },");
    }
    printf("\n};\n");

#if DO_EXTRA_PAWN_EVAL

    printf("const PawnSets PAWN_SETS[2][64] = \n{");
    for (int color = 0; color != 2; ++color)
    {
        printf("\n    {");
        for (int location = 0; location != 64; ++location)
        {
            printf("\n        { /* %s square %c%c */", color_names[color], FILE_CHAR(location), RANK_CHAR(location));
            for (const BitboardGen* generator = pawn_generators[color];
                 generator->name; 
                ++generator)
            {
                const uint64_t b = generator->function(location);
                printf("\n            .%-28s = 0x%016llXull, /* popcnt %2d */",
                    generator->name,
                    b,
                    PopCount(b));
            }
            printf("\n        },");
        }
        printf("\n    },");
    }
    printf("\n};\n");

#endif

    printf("const bitboard INTERVENING_SQUARES[64][64] = \n{");
    for (int i = 0; i != 64; ++i)
    {
        printf("\n    { /* square %c%c */", FILE_CHAR(i), RANK_CHAR(i));
        for (int j = 0; j != 64; ++j)
        {
            if ((j & 3) == 0)
            {
                printf("\n        ");
            }
            printf("0x%016llXull,", InterveningSquares(i, j));
        }
        printf("\n    },");
    }
    printf("\n};\n");
    printf("const uint64_t PIECE_SQUARE_HASHES[2][6][64] = \n{");
    for (int color = 0; color != 2; ++color)
    {
        printf("\n    {");
        for (int piece = PAWN; piece <= KING; ++piece)
        {
            printf("\n        {   /* %s %s */", color_names[color], piece_names[piece]);
            for (int i = 0; i != 64; ++i)
            {
                if ((i & 3) == 0)
                {
                    printf("\n            ");
                }
                printf("0x%016llXull,", piece >= PAWN && piece <= KING ? PseudoRandom64() : 0ull);
            }
            printf("\n        },");
        }
        printf("\n    },");
    }
    printf("\n};\n");
    printf("const uint64_t CASTLING_RIGHTS_HASHES[16] = \n{");
    for (int i = 0; i != 16; ++i)
    {
        if ((i & 3) == 0)
        {
            printf("\n    ");
        }
        printf("0x%016llXull,", PseudoRandom64());
    }
    printf("\n};\n");
    printf("const uint64_t EN_PASSANT_HASHES[8] = \n{");
    for (int i = 0; i != 8; ++i)
    {
        if ((i & 3) == 0)
        {
            printf("\n    ");
        }
        printf("0x%016llXull,", PseudoRandom64());
    }
    printf("\n};\n");
    GenerateMagics();
    return 0;
}

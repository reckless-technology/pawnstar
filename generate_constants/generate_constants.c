/**
 * @file Precomputes constants used by the main program.
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

typedef struct DirectionVector
{
    int dx;
    int dy;
} DirectionVector;

typedef uint64_t(*BitboardFn)(int location);

typedef struct BitboardGen
{
    const char* name;
    BitboardFn  function;
} BitboardGen;

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

static int
FindAndClearLsb(uint64_t* x)
{
    const uint64_t y = *x;
    const int result = Lsb(y);
    *x = y & (y - 1);
    return result;
}

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

static uint64_t PawnAttacksWhite(int location)
{
    return SHIFT_NORTHWEST(BITBOARD(location)) | SHIFT_NORTHEAST(BITBOARD(location));
}

static uint64_t PawnAttacksBlack(int location)
{
    return SHIFT_SOUTHWEST(BITBOARD(location)) | SHIFT_SOUTHEAST(BITBOARD(location));
}

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

static uint64_t KnightAttacks(int location)
{
    return KnightFill(BITBOARD(location));
}

static uint64_t BishopAttacks(int location)
{
    return NortheastOf(location) | NorthwestOf(location) | SoutheastOf(location) | SouthwestOf(location);
}

static uint64_t RookAttacks(int location)
{
    return NorthOf(location) | SouthOf(location) | EastOf(location) | WestOf(location);
}

static uint64_t QueenAttacks(int location)
{
    return BishopAttacks(location) | RookAttacks(location);
}

static uint64_t KingFill(uint64_t b)
{
    uint64_t x = SHIFT_WEST(b) | SHIFT_EAST(b);
    x |= SHIFT_NORTH(x) | SHIFT_SOUTH(x);
    return x | SHIFT_NORTH(b) | SHIFT_SOUTH(b);
}

static uint64_t KingAttacks(int location)
{
    return KingFill(BITBOARD(location));
}

#if DO_EXTRA_PAWN_EVAL

static uint64_t PassedPawnMaskWhite(int location)
{
    uint64_t result       = NO_SQUARES;
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

static uint64_t PassedPawnMaskBlack(int location)
{
    uint64_t result       = NO_SQUARES;
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

static uint64_t IsolatedPawnMaskBlack(int location)
{
    return IsolatedPawnMaskWhite(location); /* files are symmetrical */
}

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

static uint64_t DoubledPawnMaskWhite(int location)
{
    return NorthOf(location);
}

static uint64_t DoubledPawnMaskBlack(int location)
{
    return SouthOf(location);
}

#endif /* DO_EXTRA_PAWN_EVAL */

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

static uint64_t
BishopOccupancyMask(int location)
{
    return DiagonalOccupancyMask(location) | AntiDiagonalOccupancyMask(location);
}

static uint64_t
RookOccupancyMask(int location)
{
    return RankOccupancyMask(location) | FileOccupancyMask(location);
}

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

static uint64_t
BishopMoveTargets(uint64_t occupied_squares, 
                    int    location)
{
    return DiagonalMoveTargets(occupied_squares, location) | 
           AntiDiagonalMoveTargets(occupied_squares, location);
}

static uint64_t
RookMoveTargets(uint64_t occupied_squares, 
                  int    location)
{
    return RankMoveTargets(occupied_squares, location) | 
           FileMoveTargets(occupied_squares, location);
}

static uint64_t
EnumerateMaskCombinations(uint64_t mask,
                          uint64_t values[])
{
    const int num_bits_in_mask = PopCount(mask);
    const uint64_t num_combinations = 1ull << num_bits_in_mask;
    int bit_indices[64] = { 0 };
    uint64_t b = mask;
    for (int i = 0; i != num_bits_in_mask; ++i)
    {
        bit_indices[i] = FindAndClearLsb(&b);
    }
    for (uint64_t i = 0; i != num_combinations; ++i)
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
#define MAX_ATTACK_SET_SIZE 4096

static void
FindMagic(int           location,
          MoveTargetFn  move_target_fn,
          OccupancyFn   occupancy_mask_fn,
          uint64_t*     magic,
          uint64_t*     mask,
          uint64_t      attacks[static MAX_ATTACK_SET_SIZE],
          int*          shift)
{
    uint64_t occupancies[MAX_ATTACK_SET_SIZE];
    uint64_t actual_attacks[MAX_ATTACK_SET_SIZE];
    *mask = occupancy_mask_fn(location);
    *shift = 64 - PopCount(*mask);
    const uint64_t num_values = EnumerateMaskCombinations(*mask, occupancies);
    for (uint64_t i = 0; i != num_values; ++i)
    {
        actual_attacks[i] = move_target_fn(occupancies[i], location);
    }
    for (; ; )
    {
        bool has_found_magic = true;
        const uint64_t trial_magic = PseudoRandom64() & PseudoRandom64() & PseudoRandom64();
        memset(attacks, 0, MAX_ATTACK_SET_SIZE * sizeof(uint64_t));
        for (uint64_t i = 0; i != num_values; ++i)
        {
            const unsigned int index = (unsigned int)((occupancies[i] * trial_magic) >> *shift);
            if (attacks[index] == NO_SQUARES)
            {
                attacks[index] = actual_attacks[i];
                continue;
            }
            if (attacks[index] != actual_attacks[i])
            {
                /* bad hash collision occurred */
                has_found_magic = false;
                break;
            }
        }
        if (has_found_magic)
        {
            *magic = trial_magic;
            return;
        }
    }
}

typedef struct MagicVector
{
    const char*     name;
    MoveTargetFn    move_target_fn;
    OccupancyFn     occupancy_mask_fn;
} MagicVector;

static const MagicVector magic_vectors[] = 
{
    { "BISHOP", BishopMoveTargets,  BishopOccupancyMask },
    { "ROOK",   RookMoveTargets,    RookOccupancyMask   },
};

static struct Magics
{
    uint64_t    magic;
    uint64_t    mask;
    uint64_t    attacks[4096];
    int         shift;
} magics[64];

static void
GenerateMagics(void)
{
    for (int v = 0; v != sizeof(magic_vectors) / sizeof(magic_vectors[0]); ++v)
    {
        for (int j = 0; j != 64; ++j)
        {
            FindMagic(j,
                magic_vectors[v].move_target_fn,
                magic_vectors[v].occupancy_mask_fn,
                &magics[j].magic,
                &magics[j].mask,
                magics[j].attacks,
                &magics[j].shift);
        }
        for (int j = 0; j != 64; ++j)
        {
            printf("static const MagicMoveEntry %s_MAGIC_%c%c = {\n", magic_vectors[v].name, 'A' + FILE_OF(j), RANK_CHAR(j));
            printf("    .magic          = 0x%016llXull,\n", magics[j].magic);
            printf("    .occupancy_mask = 0x%016llXull,\n", magics[j].mask);
            printf("    .shift          = %d,\n", magics[j].shift);
            printf("    .attacks        = {\n");
            const int num_attacks = 1 << (64 - magics[j].shift);
            for (int k = 0; k != num_attacks; ++k)
            {
                printf("        [%4d] = 0x%016llXull,\n", k, magics[j].attacks[k]);
            }
            printf("    },\n");
            printf("};\n");
        }
        printf("const MagicMoveEntry* const %s_MAGICS[64] = {\n", magic_vectors[v].name);
        for (int j = 0; j != 64; ++j)
        {
            printf("    &%s_MAGIC_%c%c,\n", magic_vectors[v].name, 'A' + FILE_OF(j), RANK_CHAR(j));
        }
        printf("};\n");
    }
}

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

static const char* const color_names[2] = 
{
    "white",
    "black"
};

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

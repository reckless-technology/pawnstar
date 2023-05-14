/*
Generates the file "generated_data.c" which is used by the main program and
contains various precomputed constants
*/
#include <stdio.h>
#include <stdbool.h>
#include <memory.h>
#include <string.h>
#include <math.h>

typedef unsigned long long  uint64;
typedef unsigned char       uint8;

#define MASK_EAST_1         0x7F7F7F7F7F7F7F7Full
#define MASK_WEST_1         0xFEFEFEFEFEFEFEFEull
#define NO_SQUARES          0x0000000000000000ull
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

typedef struct
{
    int dx;
    int dy;
} DirectionVector;

typedef uint64(*BitboardFn)(int location);

typedef struct
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
PopCount(uint64 x)
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
Lsb(uint64 x)
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
FindAndClearLsb(uint64* x)
{
    const uint64 y = *x;
    *x = y & (y - 1);
    return Lsb(y);
}

static uint64 VectorFrom(int location, int direction)
{
    uint64 result = NO_SQUARES;
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

static uint64 NorthOf(int location)
{
    return VectorFrom(location, DIR_NORTH);
}

static uint64 NortheastOf(int location)
{
    return VectorFrom(location, DIR_NORTHEAST);
}

static uint64 EastOf(int location)
{
    return VectorFrom(location, DIR_EAST);
}

static uint64 SoutheastOf(int location)
{
    return VectorFrom(location, DIR_SOUTHEAST);
}

static uint64 SouthOf(int location)
{
    return VectorFrom(location, DIR_SOUTH);
}

static uint64 SouthwestOf(int location)
{
    return VectorFrom(location, DIR_SOUTHWEST);
}

static uint64 WestOf(int location)
{
    return VectorFrom(location, DIR_WEST);
}

static uint64 NorthwestOf(int location)
{
    return VectorFrom(location, DIR_NORTHWEST);
}

static uint64 PawnAttacksWhite(int location)
{
    return SHIFT_NORTHWEST(BITBOARD(location)) | SHIFT_NORTHEAST(BITBOARD(location));
}

static uint64 PawnAttacksBlack(int location)
{
    return SHIFT_SOUTHWEST(BITBOARD(location)) | SHIFT_SOUTHEAST(BITBOARD(location));
}

static uint64 KnightFill(uint64 b)
{
    const uint64 west1 = SHIFT_WEST(b);
    const uint64 west2 = SHIFT_WEST(west1);
    const uint64 east1 = SHIFT_EAST(b);
    const uint64 east2 = SHIFT_EAST(east1);
    const uint64 eastwest1 = west1 | east1;
    const uint64 eastwest2 = west2 | east2;
    return (eastwest1 << 16) | (eastwest1 >> 16) | (eastwest2 << 8) | (eastwest2 >> 8);
}

static uint64 KnightAttacks(int location)
{
    return KnightFill(BITBOARD(location));
}

static uint64 BishopAttacks(int location)
{
    return NortheastOf(location) | NorthwestOf(location) | SoutheastOf(location) | SouthwestOf(location);
}

static uint64 RookAttacks(int location)
{
    return NorthOf(location) | SouthOf(location) | EastOf(location) | WestOf(location);
}

static uint64 QueenAttacks(int location)
{
    return BishopAttacks(location) | RookAttacks(location);
}

static uint64 KingFill(uint64 b)
{
    uint64 x = SHIFT_WEST(b) | SHIFT_EAST(b);
    x |= SHIFT_NORTH(x) | SHIFT_SOUTH(x);
    return x | SHIFT_NORTH(b) | SHIFT_SOUTH(b);
}

static uint64 KingAttacks(int location)
{
    return KingFill(BITBOARD(location));
}

static uint64 PassedPawnMaskWhite(int location)
{
    uint64 result       = NO_SQUARES;
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

static uint64 PassedPawnMaskBlack(int location)
{
    uint64 result       = NO_SQUARES;
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

static uint64 IsolatedPawnMaskWhite(int location)
{
    uint64 result       = NO_SQUARES;
    const int locn_x    = FILE_OF(location);
    const int isolated_files[2] = { locn_x - 1, locn_x + 1 };
    for (int i = 0; i != 2; ++i)
    {
        const int x = isolated_files[i];
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

static uint64 IsolatedPawnMaskBlack(int location)
{
    return IsolatedPawnMaskWhite(location); /* files are symmetrical */
}

static uint64 SupportedPawnMaskWhite(int location)
{
    uint64 result       = NO_SQUARES;
    const int locn_x    = FILE_OF(location);
    const int locn_y    = RANK_OF(location);
    const int supported_files[2] = { locn_x - 1, locn_x + 1 };
    for (int i = 0; i != 2; ++i)
    {
        const int x = supported_files[i];
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

static uint64 SupportedPawnMaskBlack(int location)
{
    uint64 result    = NO_SQUARES;
    const int locn_x = FILE_OF(location);
    const int locn_y = RANK_OF(location);
    const int supported_files[2] = { locn_x - 1, locn_x + 1 };
    for (int i = 0; i != 2; ++i)
    {
        const int x = supported_files[i];
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

static uint64 DoubledPawnMaskWhite(int location)
{
    return NorthOf(location);
}

static uint64 DoubledPawnMaskBlack(int location)
{
    return SouthOf(location);
}

static uint64 InterveningSquares(int from, int to)
{
    for (int dir = DIR_NORTH; dir <= DIR_NORTHWEST; ++dir)
    {
        const uint64 vector_from = VectorFrom(from, dir);
        if (vector_from & BITBOARD(to))
        {
            return vector_from ^ VectorFrom(to, dir) ^ BITBOARD(to);
        }
    }
    return NO_SQUARES;
}

static uint64 PseudoRandom64(void)
{
    static uint64 x = 0x2545F4914F6CDD1Dull;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    return x * 0x2545F4914F6CDD1Dull;
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

static const BitboardGen pawn_generators[] =
{
    { "passed_pawn_mask_white",     PassedPawnMaskWhite,    },
    { "passed_pawn_mask_black",     PassedPawnMaskBlack,    },
    { "isolated_pawn_mask_white",   IsolatedPawnMaskWhite,  },
    { "isolated_pawn_mask_black",   IsolatedPawnMaskBlack,  },
    { "supported_pawn_mask_white",  SupportedPawnMaskWhite, },
    { "supported_pawn_mask_black",  SupportedPawnMaskBlack, },
    { "doubled_pawn_mask_white",    DoubledPawnMaskWhite,   },
    { "doubled_pawn_mask_black",    DoubledPawnMaskBlack,   },
    { NULL,                         NULL                    },
};

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
            const uint64 b = generator->function(location);
            printf("\n        .%-20s = 0x%016llXull, /* popcnt %2d */",
                generator->name,
                b,
                PopCount(b));
        }
        printf("\n    },");
    }
    printf("\n};\n");
    printf("const PawnSets PAWN_SETS[64] = \n{");
    for (int location = 0; location != 64; ++location)
    {
        printf("\n    { /* square %c%c */", FILE_CHAR(location), RANK_CHAR(location));
        for (const BitboardGen* generator = pawn_generators; generator->name; ++generator)
        {
            const uint64 b = generator->function(location);
            printf("\n        .%-28s = 0x%016llXull, /* popcnt %2d */",
                generator->name,
                b,
                PopCount(b));
        }
        printf("\n    },");
    }
    printf("\n};\n");
    printf("const bitboard INTERVENING_SQUARES[64][64] = \n{");
    for (int i = 0; i != 64; ++i)
    {
        printf("\n    {");
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
    return 0;
}

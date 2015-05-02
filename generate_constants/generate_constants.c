/******************************************************************************
Generates the file "generated_data.c" which is used by the main program and
contains various precomputed constants
*******************************************************************************/
#include <stdio.h>

#include "../macros.h"
#include "../types.h"

typedef struct
{
    int     direction;
    int     dx;
    int     dy;
} DirectionVector;

static const DirectionVector DIRECTION_VECTORS[] =
{/*   direction      dx  dy */
    { DIR_NORTH,      0,  1 },
    { DIR_NORTHEAST,  1,  1 },
    { DIR_EAST,       1,  0 },
    { DIR_SOUTHEAST,  1, -1 },
    { DIR_SOUTH,      0, -1 },
    { DIR_SOUTHWEST, -1, -1 },
    { DIR_WEST,      -1,  0 },
    { DIR_NORTHWEST, -1,  1 },
    { DIR_NONE,       0,  0 },
};

static const char* const DIRECTION_NAMES[] = 
{
    [DIR_NONE]      = "NO DIRECTION",
    [DIR_NORTH]     = "NORTH",
    [DIR_NORTHEAST] = "NORTHEAST",
    [DIR_EAST]      = "EAST",
    [DIR_SOUTHEAST] = "SOUTHEAST",
    [DIR_SOUTH]     = "SOUTH",
    [DIR_SOUTHWEST] = "SOUTHWEST",
    [DIR_WEST]      = "WEST",
    [DIR_NORTHWEST] = "NORTHWEST",
};

static uint64 NextHashKey()
{
    static unsigned int lcg = 0;
    uint64 key = 0;
    int i;
    for (i = 0; i != 64; ++i)
    {
        lcg = lcg * 1103515245 + 12345;
        key = (key << 1) | (lcg >> 31);
    }
    return key;
}

static bitboard VectorFrom(int location, int direction)
{
    bitboard result = NO_SQUARES;
    const DirectionVector* dv;
    for (dv = DIRECTION_VECTORS; dv->direction != DIR_NONE; ++dv)
    {
        if (dv->direction == direction)
        {
            int x, y;
            for (x = FILE_OF(location) + dv->dx, y = RANK_OF(location) + dv->dy;
                x >= 0 && x < 8 && y >= 0 && y < 8;
                x += dv->dx, y += dv->dy)
            {
                result |= BITBOARD_XY(x, y);
            }
            break;
        }        
    }
    return result;
}

static void WriteSquaresFrom(int location, int direction, FILE* file)
{
    int x = FILE_OF(location);
    int y = RANK_OF(location);
    const DirectionVector* dv;
    for (dv = DIRECTION_VECTORS; dv->direction != DIR_NONE; ++dv)
    {
        if (dv->direction == direction)
        {
            fprintf(file, "    { ");
            for (int i = 0; i != 8; ++i)
            {
                x += dv->dx;
                y += dv->dy;
                if (x >= 0 && x < 8 && y >= 0 && y < 8)
                {
                    fprintf(file, "%c%c, ", 'A' + x, '1' + y);
                }
                else
                {
                    fprintf(file, "-1, ");
                }
            }
            fprintf(file, "}, /* from %c%c */\n", FILE_CHAR(location), RANK_CHAR(location));
        }
    }
}

static bitboard NorthOf(int location)
{
    return VectorFrom(location, DIR_NORTH);
}

static bitboard NortheastOf(int location)
{
    return VectorFrom(location, DIR_NORTHEAST);
}

static bitboard EastOf(int location)
{
    return VectorFrom(location, DIR_EAST);
}

static bitboard SoutheastOf(int location)
{
    return VectorFrom(location, DIR_SOUTHEAST);
}

static bitboard SouthOf(int location)
{
    return VectorFrom(location, DIR_SOUTH);
}

static bitboard SouthwestOf(int location)
{
    return VectorFrom(location, DIR_SOUTHWEST);
}

static bitboard WestOf(int location)
{
    return VectorFrom(location, DIR_WEST);
}

static bitboard NorthwestOf(int location)
{
    return VectorFrom(location, DIR_NORTHWEST);
}

static bitboard PawnAttacksWhite(int location)
{
    return SHIFT_NORTHWEST(BITBOARD(location)) | SHIFT_NORTHEAST(BITBOARD(location));
}

static bitboard PawnAttacksBlack(int location)
{
    return SHIFT_SOUTHWEST(BITBOARD(location)) | SHIFT_SOUTHEAST(BITBOARD(location));
}

static bitboard KnightFill(bitboard b)
{
    const bitboard west1		= SHIFT_WEST(b);
    const bitboard west2		= SHIFT_WEST(west1);
    const bitboard east1		= SHIFT_EAST(b);
    const bitboard east2		= SHIFT_EAST(east1);
    const bitboard eastwest1	= west1 | east1;
    const bitboard eastwest2	= west2 | east2;
    return (eastwest1 << 16) | (eastwest1 >> 16) | (eastwest2 << 8) | (eastwest2 >> 8);
}

static bitboard KnightAttacks(int location)
{
    return KnightFill(BITBOARD(location));
}

static bitboard BishopAttacks(int location)
{
    return NortheastOf(location) | NorthwestOf(location) | SoutheastOf(location) | SouthwestOf(location);
}

static bitboard RookAttacks(int location)
{
    return NorthOf(location) | SouthOf(location) | EastOf(location) | WestOf(location);
}

static bitboard QueenAttacks(int location)
{
    return BishopAttacks(location) | RookAttacks(location);
}

static bitboard KingFill(bitboard b)
{
    bitboard x = SHIFT_WEST(b) | SHIFT_EAST(b);
    x |= SHIFT_NORTH(x) | SHIFT_SOUTH(x);
    return x | SHIFT_NORTH(b) | SHIFT_SOUTH(b);
}

static bitboard KingAttacks(int location)
{
    return KingFill(BITBOARD(location));
}

static bitboard KingAttacks2(int location)
{
    return KingFill(KingAttacks(location));
}

static bitboard KingPawnShieldWhite(int location)
{
    const bitboard b = BITBOARD(location);
    switch (location)
    {
    case A1:
    case B1:
    case C1:
    case F1:
    case G1:
    case H1:
    case A2:
    case B2:
    case C2:
    case F2:
    case G2:
    case H2:
        return SHIFT_NORTHEAST(b) | SHIFT_NORTH(b) | SHIFT_NORTHWEST(b);
    default:
        return NO_SQUARES;
    }
}

static bitboard KingPawnShieldBlack(int location)
{
    const bitboard b = BITBOARD(location);
    switch (location)
    {
    case A8:
    case B8:
    case C8:
    case F8:
    case G8:
    case H8:
    case A7:
    case B7:
    case C7:
    case F7:
    case G7:
    case H7:
        return SHIFT_SOUTHEAST(b) | SHIFT_SOUTH(b) | SHIFT_SOUTHWEST(b);
    default:
        return NO_SQUARES;
    }
}

static bitboard KingPawnShield2White(int location)
{
    return SHIFT_NORTH(KingPawnShieldWhite(location));
}

static bitboard KingPawnShield2Black(int location)
{
    return SHIFT_SOUTH(KingPawnShieldBlack(location));
}

static bitboard InterveningSquares(int from, int to)
{
    const DirectionVector* dv;
    for (dv = DIRECTION_VECTORS; dv->direction != DIR_NONE; ++dv)
    {
        const bitboard vector_from = VectorFrom(from, dv->direction);
        if (vector_from & BITBOARD(to))
        {
            return vector_from ^ VectorFrom(to, dv->direction) ^ BITBOARD(to);
        }
    }
    return NO_SQUARES;
}

typedef bitboard (*BitboardFn)(int location);

typedef struct
{
    const char*	name;
    BitboardFn	function;
} BitboardGen;

static const BitboardGen GENERATORS[] = {
    { "NORTH_OF",			      NorthOf			   },
    { "NORTHEAST_OF",		      NortheastOf		   },
    { "EAST_OF",			      EastOf			   },
    { "SOUTHEAST_OF",		      SoutheastOf		   },
    { "SOUTH_OF",			      SouthOf			   },
    { "SOUTHWEST_OF",		      SouthwestOf		   },
    { "WEST_OF",			      WestOf			   },
    { "NORTHWEST_OF",		      NorthwestOf		   },
    { "PAWN_ATTACKS_WHITE",	      PawnAttacksWhite	   },
    { "PAWN_ATTACKS_BLACK",	      PawnAttacksBlack	   },
    { "KNIGHT_ATTACKS",		      KnightAttacks		   },
    { "BISHOP_ATTACKS",		      BishopAttacks		   },
    { "ROOK_ATTACKS",		      RookAttacks		   },
    { "QUEEN_ATTACKS",		      QueenAttacks		   },
    { "KING_ATTACKS",		      KingAttacks		   },
    { "KING_ATTACKS_2",           KingAttacks2         },
    { "KING_PAWN_SHIELD_WHITE",   KingPawnShieldWhite  },
    { "KING_PAWN_SHIELD_BLACK",   KingPawnShieldBlack  },
    { "KING_PAWN_SHIELD_WHITE_2", KingPawnShield2White },
    { "KING_PAWN_SHIELD_BLACK_2", KingPawnShield2Black },
    { NULL,					      NULL			       },
};

static int PopCount(bitboard b)
{
    int count = 0;
    while (b)
    {
        ++count;
        b &= b - 1;
    }
    return count;
}

int main()
{
    int i, j, piece, color;
    const BitboardGen* generator;
    const DirectionVector* dv;
    FILE* file = fopen("../generated_data.c", "w");
    if (!file)
    {
        printf("ERROR: unable to open 'generated_data.c' for writing\n");
        return -1;
    }
    fprintf(file, "/* This file was generated on " __DATE__ " at " __TIME__ " */\n"); 
    fprintf(file, "#include \"types.h\"\n");
    for (generator = GENERATORS; generator->name; ++generator)
    {
        int location;
        fprintf(file, "const bitboard %s[64] = \n{\n", generator->name);
        for (location = 0; location != 64; ++location)
        {
            const bitboard b = generator->function(location);
            fprintf(file, "    0x%016llXull, /* %c%c popcnt %d */\n",
                b,
                FILE_CHAR(location),
                RANK_CHAR(location),
                PopCount(b));
        }
        fprintf(file, "};\n");
    }
    fprintf(file, "const bitboard INTERVENING_SQUARES[64][64] = \n{");
    for (i = 0; i != 64; ++i)
    {
        fprintf(file, "\n{");
        for (j = 0; j != 64; ++j)
        {
            if ((j & 7) == 0)
            {
                fprintf(file, "\n    ");
            }
            fprintf(file, "0x%016llXull,", InterveningSquares(i, j));
        }
        fprintf(file, "\n},");
    }
    fprintf(file, "};\n");
    for (dv = DIRECTION_VECTORS; dv->direction != DIR_NONE; ++dv)
    {
        fprintf(file, "const signed char %s_FROM[64][8] = {\n", DIRECTION_NAMES[dv->direction]);
        for (i = 0; i != 64; ++i)
        {
            WriteSquaresFrom(i, dv->direction, file);
        }
        fprintf(file, "};\n");
    }
    fprintf(file, "const uint64 PIECE_SQUARE_HASHES[2][8][64] = \n{\n");
    for (color = 0; color != 2; ++color)
    {
        fprintf(file, "{");
        for (piece = 0; piece != 8; ++piece)
        {
            fprintf(file, "{");
            for (i = 0; i != 64; ++i)
            {
                if ((i & 7) == 0)
                {
                    fprintf(file, "\n    ");
                }
                fprintf(file, "0x%016llXull,", piece >= PAWN && piece <= KING ? NextHashKey() : 0ull);
            }
            fprintf(file, "\n},");
        }
        fprintf(file, "},\n");
    }
    fprintf(file, "};\n");
    fprintf(file, "const uint64 CASTLING_RIGHTS_HASHES[16] = \n{");
    for (i = 0; i != 16; ++i)
    {
        if ((i & 7) == 0)
        {
            fprintf(file, "\n    ");
        }
        fprintf(file, "0x%016llXull,", NextHashKey());
    }
    fprintf(file, "\n};\n");
    fprintf(file, "const uint64 EN_PASSANT_HASHES[8] = \n{\n    ");
    for (i = 0; i != 8; ++i)
    {
        fprintf(file, "0x%016llXull,", NextHashKey());
    }
    fprintf(file, "\n};\n");
    fclose(file);
    return 0;
}

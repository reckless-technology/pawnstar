/******************************************************************************
Generates the file "generated_data.c" which is used by the main program and
contains various precomputed constants
*******************************************************************************/
#include <stdio.h>
#include <stdbool.h>
#include <memory.h>
#include <string.h>
#include <math.h>

typedef unsigned long long      uint64;
typedef unsigned char           uint8;

#define MASK_EAST_1             0x7F7F7F7F7F7F7F7Full 
#define MASK_WEST_1             0xFEFEFEFEFEFEFEFEull 
#define NO_SQUARES              0x0000000000000000ull
#define ALL_SQUARES             0xFFFFFFFFFFFFFFFFull
#define FILE_OF(locn)           ((locn) & 7)
#define RANK_OF(locn)           ((locn) >> 3)
#define FILE_CHAR(locn)         ('a' + ((locn) & 7))
#define RANK_CHAR(locn)         ('1' + ((locn) >> 3))
#define BITBOARD(locn)          (1ull << (locn))
#define BITBOARD_XY(x,y)        (1ull << ((x) + 8 * (y)))
#define IS_SUBSET(sub,super)    (((sub) & (super)) == (sub))
#define SHIFT_NORTH(b)          ((b) << 8)
#define SHIFT_NORTHEAST(b)      (((b) & MASK_EAST_1) << 9)
#define SHIFT_EAST(b)           (((b) & MASK_EAST_1) << 1)
#define SHIFT_SOUTHEAST(b)      (((b) & MASK_EAST_1) >> 7)
#define SHIFT_SOUTH(b)          ((b) >> 8)
#define SHIFT_SOUTHWEST(b)      (((b) & MASK_WEST_1) >> 9)
#define SHIFT_WEST(b)           (((b) & MASK_WEST_1) >> 1)
#define SHIFT_NORTHWEST(b)      (((b) & MASK_WEST_1) << 7)

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
    DIR_NONE,
    DIR_NORTH,
    DIR_NORTHEAST,
    DIR_EAST,
    DIR_SOUTHEAST,
    DIR_SOUTH,
    DIR_SOUTHWEST,
    DIR_WEST,
    DIR_NORTHWEST,
};

enum Squares
{
    A1, B1, C1, D1, E1, F1, G1, H1,
    A2, B2, C2, D2, E2, F2, G2, H2,
    A3, B3, C3, D3, E3, F3, G3, H3,
    A4, B4, C4, D4, E4, F4, G4, H4,
    A5, B5, C5, D5, E5, F5, G5, H5,
    A6, B6, C6, D6, E6, F6, G6, H6,
    A7, B7, C7, D7, E7, F7, G7, H7,
    A8, B8, C8, D8, E8, F8, G8, H8,
};

typedef struct
{
    int     direction;
    int     dx;
    int     dy;
} DirectionVector;

typedef uint64 (*BitboardFn)(int location);

typedef struct
{
    const char*	name;
    BitboardFn	function;
} BitboardGen;

static const DirectionVector direction_vectors[] =
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

static struct
{
    uint64 magic;
    uint64 mask;
    uint64 attacks[4096];
    uint8  attack_indices[4096];
    int    shift;
    int    num_discrete_attacks;
} magics[64];

/******************************************************************************
Determine the population count (Hamming weight) of x
*******************************************************************************/
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
/******************************************************************************
Find the index of the least significant bit set in x (x is non zero)
This is the slow brute force way since we don't care about performance here.
*******************************************************************************/
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
/******************************************************************************
Find the index of and then clear the least significant bit set in *x
*******************************************************************************/
static int 
FindAndClearLsb(uint64* x)
{    
    const uint64 y = *x;
    *x = y & (y - 1);
    return Lsb(y);
}
/******************************************************************************
Determine the occupancy mask for bishop attacks for a bishop standing on 
location (excluding outer squares as they do no affect attack set)
*******************************************************************************/
static uint64 
BishopOccupancyMask(int location)
{
    uint64 result = NO_SQUARES;
    int x, y;
    for (x = FILE_OF(location) + 1, y = RANK_OF(location) + 1; x < 7 && y < 7; ++x, ++y)
    {
        result |= BITBOARD_XY(x, y);
    }
    for (x = FILE_OF(location) + 1, y = RANK_OF(location) - 1; x < 7 && y > 0; ++x, --y)
    {
        result |= BITBOARD_XY(x, y);
    }
    for (x = FILE_OF(location) - 1, y = RANK_OF(location) + 1; x > 0 && y < 7; --x, ++y)
    {
        result |= BITBOARD_XY(x, y);
    }
    for (x = FILE_OF(location) - 1, y = RANK_OF(location) - 1; x > 0 && y > 0; --x, --y)
    {
        result |= BITBOARD_XY(x, y);
    }
    return result;
}
/******************************************************************************
Determine the occupancy mask for rook attacks for a rook standing on 
location (excluding outer squares as they do no affect attack set)
*******************************************************************************/
static uint64 
RookOccupancyMask(int location)
{
    uint64 result = NO_SQUARES;
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
/******************************************************************************
Compute attacks for a bishop standing on location in the presence of possible 
blockers
*******************************************************************************/
static uint64 
BishopActualAttacks(uint64 occupied_squares, 
                    int    location)
{
    uint64 result = NO_SQUARES;
    for (uint64 square = SHIFT_NORTHEAST(BITBOARD(location)); square; square = SHIFT_NORTHEAST(square))
    {
        result |= square;
        if (square & occupied_squares)
        {
            break;
        }
    }
    for (uint64 square = SHIFT_SOUTHEAST(BITBOARD(location)); square; square = SHIFT_SOUTHEAST(square))
    {
        result |= square;
        if (square & occupied_squares)
        {
            break;
        }
    }
    for (uint64 square = SHIFT_SOUTHWEST(BITBOARD(location)); square; square = SHIFT_SOUTHWEST(square))
    {
        result |= square;
        if (square & occupied_squares)
        {
            break;
        }
    }
    for (uint64 square = SHIFT_NORTHWEST(BITBOARD(location)); square; square = SHIFT_NORTHWEST(square))
    {
        result |= square;
        if (square & occupied_squares)
        {
            break;
        }
    }
    return result;
}
/******************************************************************************
Compute attacks for a rook standing on location in the presence of possible 
blockers
*******************************************************************************/
static uint64 
RookActualAttacks(uint64 occupied_squares, 
                  int    location)
{
    uint64 result = NO_SQUARES;
    for (uint64 square = SHIFT_NORTH(BITBOARD(location)); square; square = SHIFT_NORTH(square))
    {
        result |= square;
        if (square & occupied_squares)
        {
            break;
        }
    }
    for (uint64 square = SHIFT_EAST(BITBOARD(location)); square; square = SHIFT_EAST(square))
    {
        result |= square;
        if (square & occupied_squares)
        {
            break;
        }
    }
    for (uint64 square = SHIFT_SOUTH(BITBOARD(location)); square; square = SHIFT_SOUTH(square))
    {
        result |= square;
        if (square & occupied_squares)
        {
            break;
        }
    }
    for (uint64 square = SHIFT_WEST(BITBOARD(location)); square; square = SHIFT_WEST(square))
    {
        result |= square;
        if (square & occupied_squares)
        {
            break;
        }
    }
    return result;
}
/******************************************************************************
Use an RC4 cipher to generate a repeatable, pseudo random stream of bytes
*******************************************************************************/
static uint8
RandomByte(void)
{
    static uint8 rc4[256];
    static bool is_first_time = true;
    static uint8 i, j;
    if (is_first_time)
    {
        is_first_time = false;
        for (int k = 0xFF; k >= 0; --k)
        {
            rc4[k] = k ^ (k >> 1);
        }
        i = 0;
        j = 0;
        for (int k = 1093; k != 0; --k)
        {
            RandomByte();
        }
    }
    ++i;
    j += rc4[i];
    const uint8 tmp = rc4[i];
    rc4[i] = rc4[j];
    rc4[j] = tmp;
    return rc4[(uint8)(rc4[i] + rc4[j])];
}
/******************************************************************************
Generate a pseudo random 64 bit value
*******************************************************************************/
static uint64 
PseudoRandom64(void)
{
    uint64 result = 0;
    for (int i = 0; i != 8; ++i)
    {
        result = (result << 8) | RandomByte();
    }
    return result;
}
/******************************************************************************
Given a bit set in mask, determine all the subsets of this set, i.e. determine
all combinations of occupancy from an occupancy mask value.
*******************************************************************************/
static uint64 
EnumerateMaskCombinations(uint64 mask, 
                          uint64 values[])
{   
    const int num_bits_in_mask = PopCount(mask);
    const uint64 num_values = 1ull << num_bits_in_mask;    
    int bit_indices[64];
    uint64 b = mask;
    for (int i = 0; i != num_bits_in_mask; ++i)
    {
        bit_indices[i] = FindAndClearLsb(&b);
    }
    for (uint64 i = 0; i != num_values; ++i)
    {
        uint64 value = 0;
        uint64 j = i;
        while (j)
        {
            value |= BITBOARD(bit_indices[FindAndClearLsb(&j)]);
        }
        values[i] = value;
    }
    return num_values;
}
/******************************************************************************
Find magic bitboard multiplier, occupancy mask, shift value and attack set for 
a bishop or a rook standing on location
Refer to:
http://chessprogramming.wikispaces.com/Magic+Bitboards
*******************************************************************************/
static void 
FindMagic(int       location, 
          bool      is_rook, 
          uint64*   magic, 
          uint64*   mask, 
          uint64    attacks[4096], 
          int*      shift)
{   
    uint64 occupancies[4096];
    uint64 actual_attacks[4096];
    *mask = is_rook ? RookOccupancyMask(location) : BishopOccupancyMask(location);
    *shift = 64 - PopCount(*mask);
    const uint64 num_values = EnumerateMaskCombinations(*mask, occupancies);
    for (int i = 0; i != num_values; ++i)
    {
        actual_attacks[i] = is_rook ? RookActualAttacks(occupancies[i], location) : BishopActualAttacks(occupancies[i], location);
    }
    for ( ; ; )
    {
        bool has_found_magic = true;
        const uint64 trial_magic = PseudoRandom64() & PseudoRandom64() & PseudoRandom64();
        memset(attacks, 0, 4096 * sizeof(uint64));
        for (uint64 i = 0; i != num_values; ++i)
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
/******************************************************************************
Compress the magic attacks by forming a set of unique attacked squares and 
replacing the attacks themselves with a set of one byte indices into the
unique attack vector. At the cost of one extra indirection we save approx 8x
the size of the magic tables which reduces cache pressure considerably.
*******************************************************************************/
static void 
CompressAttacks(uint64  attacks[4096], 
                int     num_attacks_in,  
                uint8   indices[4096],
                int*    num_attacks_out)
{
    uint8 out_count = 0;
    for (int i = 0; i != num_attacks_in; ++i)
    {
        const uint64 attack = attacks[i];
        uint8 j;
        for (j = 0; j != out_count; ++j)
        {
            if (attacks[j] == attack)
            {
                indices[i] = j;
                break;
            }
        }
        if (j == out_count)
        {
            /* we didn't find the attack so add it to the set */
            indices[i] = out_count;
            attacks[out_count] = attack;
            if (++out_count == 0)
            {
                printf("OVERFLOW!\n");
                return;
            }
        }
    }
    *num_attacks_out = out_count;
}
/******************************************************************************
Generate the source file containing the magic bitboard data for bishop and rook
attack sets. The magic bitboard attack generator uses this structure:

typedef struct
{
    uint64          magic;              // multiplier
    bitboard        occupancy_mask;     // pre mask (excl outside squares)  
    const uint8*    attack_indices;     // lookup index
    const bitboard* attacks;            // unique attack set
    int             shift;              // right shift amount
} MagicMoveEntry;

Then for example we have:

MagicMoveEntry rook_magics[64];         // one per square for bishop and rook

To get sliding attacks, given a set of occupied_squares, we compute:
x->attacks[x->attack_indices[((occupied_squares & x->occupancy_mask) * x->magic) >> x->shift]];
*******************************************************************************/
static void 
GenerateMagics(void)
{
    for (int piece = BISHOP; piece <= ROOK; ++piece)
    {
        const char* const piece_name = (piece == BISHOP) ? "BISHOP" : "ROOK";
       
        for (int j = 0; j != 64; ++j)
        {
            FindMagic(j, piece == ROOK, &magics[j].magic, &magics[j].mask, magics[j].attacks, &magics[j].shift);
            CompressAttacks(magics[j].attacks, 1 << (64 - magics[j].shift), magics[j].attack_indices, &magics[j].num_discrete_attacks);
        }
        for (int j = 0; j != 64; ++j)
        {
            const int num_entries = magics[j].num_discrete_attacks;
            printf("static const bitboard %s_MAGIC_ATTACKS_%c%c[%d] = \n{",
                    piece_name,
                    'A' + FILE_OF(j),
                    '1' + RANK_OF(j),
                    num_entries);
            for (int k = 0; k != num_entries; ++k)
            {
                if ((k & 7) == 0)
                {
                    printf("\n    ");
                }
                printf("0x%016llXull,",  magics[j].attacks[k]);                
            }
            printf("\n};\n");
            const int num_indices = 1 << (64 - magics[j].shift);
            printf("static const uint8 %s_MAGIC_INDICES_%c%c[%d] = \n{",
                    piece_name,
                    'A' + FILE_OF(j),
                    '1' + RANK_OF(j),
                    num_indices);
            for (int k = 0; k != num_indices; ++k)
            {
                if ((k & 0x1F) == 0)
                {
                    printf("\n    ");
                }
                printf("0x%02hhX,",  magics[j].attack_indices[k]);                
            }
            printf("\n};\n");

        }
        printf("const MagicMoveEntry %s_MAGICS[64] = \n{\n", piece_name);
        for (int j = 0; j != 64; ++j)
        {
            printf("    { 0x%016llXull,0x%016llXull,%s_MAGIC_INDICES_%c%c,%s_MAGIC_ATTACKS_%c%c,%d },\n",
                    magics[j].magic,
                    magics[j].mask,
                    piece_name,
                    'A' + FILE_OF(j),
                    '1' + RANK_OF(j),
                    piece_name,
                    'A' + FILE_OF(j),
                    '1' + RANK_OF(j),
                    magics[j].shift);
        }
        printf("};\n");
    }
}
/******************************************************************************
Generate a vector (line) bitboard from location in the specified direction.
*******************************************************************************/
static uint64 VectorFrom(int location, int direction)
{
    uint64 result = NO_SQUARES;
    const DirectionVector* dv;
    for (dv = direction_vectors; dv->direction != DIR_NONE; ++dv)
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
    const uint64 west1		= SHIFT_WEST(b);
    const uint64 west2		= SHIFT_WEST(west1);
    const uint64 east1		= SHIFT_EAST(b);
    const uint64 east2		= SHIFT_EAST(east1);
    const uint64 eastwest1	= west1 | east1;
    const uint64 eastwest2	= west2 | east2;
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

static uint64 KingAttacks2(int location)
{
    return KingFill(KingAttacks(location));
}

static uint64 KingAttacks3(int location)
{
    return KingFill(KingAttacks2(location));
}

static uint64 KingPawnShieldWhite(int location)
{
    const uint64 b = BITBOARD(location);
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

static uint64 KingPawnShieldBlack(int location)
{
    const uint64 b = BITBOARD(location);
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

static uint64 KingPawnShield2White(int location)
{
    return SHIFT_NORTH(KingPawnShieldWhite(location));
}

static uint64 KingPawnShield2Black(int location)
{
    return SHIFT_SOUTH(KingPawnShieldBlack(location));
}

static uint64 InterveningSquares(int from, int to)
{
    const DirectionVector* dv;
    for (dv = direction_vectors; dv->direction != DIR_NONE; ++dv)
    {
        const uint64 vector_from = VectorFrom(from, dv->direction);
        if (vector_from & BITBOARD(to))
        {
            return vector_from ^ VectorFrom(to, dv->direction) ^ BITBOARD(to);
        }
    }
    return NO_SQUARES;
}

static int ManhattanDistance(int x, int y)
{
    return abs(FILE_OF(x) - FILE_OF(y)) + abs(RANK_OF(x) - RANK_OF(y));
}

static const BitboardGen bitboard_generators[] = {
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
    { "KING_ATTACKS_3",           KingAttacks3         },
    { "KING_PAWN_SHIELD_WHITE",   KingPawnShieldWhite  },
    { "KING_PAWN_SHIELD_BLACK",   KingPawnShieldBlack  },
    { "KING_PAWN_SHIELD_WHITE_2", KingPawnShield2White },
    { "KING_PAWN_SHIELD_BLACK_2", KingPawnShield2Black },
    { NULL,					      NULL			       },
};

int main()
{
    int i, j, piece, color;
    const BitboardGen* generator;
    printf("/* This file was generated on " __DATE__ " at " __TIME__ " */\n"); 
    printf("#include \"types.h\"\n");
    for (generator = bitboard_generators; generator->name; ++generator)
    {
        int location;
        printf("const bitboard %s[64] = \n{\n", generator->name);
        for (location = 0; location != 64; ++location)
        {
            const uint64 b = generator->function(location);
            printf("    0x%016llXull, /* %c%c popcnt %d */\n",
                b,
                FILE_CHAR(location),
                RANK_CHAR(location),
                PopCount(b));
        }
        printf("};\n");
    }
    printf("const uint8 MANHATTAN_DISTANCE[64][64] = \n{");
    for (i = 0; i != 64; ++i)
    {
        printf("\n{");
        for (j = 0; j != 64; ++j)
        {
            if ((j & 7) == 0)
            {
                printf("\n    ");
            }
            printf("%2d,", ManhattanDistance(i, j));
        }
        printf("\n},");
    }
    printf("};\n");
    printf("const uint64 INTERVENING_SQUARES[64][64] = \n{");
    for (i = 0; i != 64; ++i)
    {
        printf("\n{");
        for (j = 0; j != 64; ++j)
        {
            if ((j & 7) == 0)
            {
                printf("\n    ");
            }
            printf("0x%016llXull,", InterveningSquares(i, j));
        }
        printf("\n},");
    }
    printf("};\n");
    printf("const uint64 PIECE_SQUARE_HASHES[2][8][64] = \n{\n");
    for (color = 0; color != 2; ++color)
    {
        printf("{");
        for (piece = 0; piece != 8; ++piece)
        {
            printf("{");
            for (i = 0; i != 64; ++i)
            {
                if ((i & 7) == 0)
                {
                    printf("\n    ");
                }
                printf("0x%016llXull,", piece >= PAWN && piece <= KING ? PseudoRandom64() : 0ull);
            }
            printf("\n},");
        }
        printf("},\n");
    }
    printf("};\n");
    printf("const uint64 CASTLING_RIGHTS_HASHES[16] = \n{");
    for (i = 0; i != 16; ++i)
    {
        if ((i & 7) == 0)
        {
            printf("\n    ");
        }
        printf("0x%016llXull,", PseudoRandom64());
    }
    printf("\n};\n");
    printf("const uint64 EN_PASSANT_HASHES[8] = \n{\n    ");
    for (i = 0; i != 8; ++i)
    {
        printf("0x%016llXull,", PseudoRandom64());
    }
    printf("\n};\n");
    GenerateMagics();
    return 0;
}

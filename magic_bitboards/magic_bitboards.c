#pragma warning(disable:4996)

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <stdbool.h>

/******************************************************************************
This code assumes a bitboard mapping where a1 is bit 0 (LSB), h1 is bit 7, and 
h8 is bit 63 (MSB)
*******************************************************************************/

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

/******************************************************************************
Find the index of the least significant bit set in x (x is non zero)
*******************************************************************************/
static int Lsb(uint64 x) 
{
    static const int de_bruijn[64] = 
    {
        0, 47,  1, 56, 48, 27,  2, 60,
       57, 49, 41, 37, 28, 16,  3, 61,
       54, 58, 35, 52, 50, 42, 21, 44,
       38, 32, 29, 23, 17, 11,  4, 62,
       46, 55, 26, 59, 40, 36, 15, 53,
       34, 51, 20, 43, 31, 22, 10, 45,
       25, 39, 14, 33, 19, 30,  9, 24,
       13, 18,  8, 12,  7,  6,  5, 63
    };
   return de_bruijn[((x ^ (x - 1)) * 0x03F79D71B4CB0A89ull) >> 58];
}

/******************************************************************************
Find the index of and then clear the least significant bit set in *x
*******************************************************************************/
static int FindAndClearLsb(uint64* x)
{    
    const uint64 y = *x;
    *x = y & (y - 1);
    return Lsb(y);
}

/******************************************************************************
Find the number of bits set in x
*******************************************************************************/
static int PopCount(uint64 x)
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
Determine the occupancy mask for bishop attacks for a bishop standing on 
location (excluding outer squares as they do no affect attack set)
*******************************************************************************/
static uint64 BishopOccupancyMask(int location)
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
static uint64 RookOccupancyMask(int location)
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
static uint64 BishopAttacks(uint64 occupied_squares, int location)
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
static uint64 RookAttacks(uint64 occupied_squares, int location)
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
Generate a pseudo random 64 bit value. Uses 2 iterations of an LCG per value
to get slightly better randomness in the low order bits.
*******************************************************************************/
static uint64 PseudoRandom64(void)
{
    static uint64 lcg = 0;
    lcg = lcg * 6364136223846793005ull + 1442695040888963407ull;
    uint64 result = lcg >> 32;
    lcg = lcg * 6364136223846793005ull + 1442695040888963407ull;
    result |= lcg & 0xFFFFFFFF00000000ull;
    return result;
}

/******************************************************************************
Given a bit set in mask, determine all the subsets of this set, i.e. determine
all combinations of occupancy from an occupancy mask value.
*******************************************************************************/
static uint64 EnumerateMaskCombinations(uint64 mask, uint64 values[])
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
static void FindMagic(int location, bool is_rook, uint64* magic, uint64* mask, uint64 attacks[4096], int* shift)
{   
    uint64 occupancies[4096];
    uint64 actual_attacks[4096];
    *mask = is_rook ? RookOccupancyMask(location) : BishopOccupancyMask(location);
    *shift = 64 - PopCount(*mask);
    const uint64 num_values = EnumerateMaskCombinations(*mask, occupancies);
    for (int i = 0; i != num_values; ++i)
    {
        actual_attacks[i] = is_rook ? RookAttacks(occupancies[i], location) : BishopAttacks(occupancies[i], location);
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
replacing the attacks themselves with a set of one byte indicies into the
unique attack vector. At the cost of one extra indirection we save approx 8x
the size of the magic tables which reduces cache pressure considerably.
*******************************************************************************/
static void CompressAttacks(uint64 attacks[4096], 
                            int num_attacks_in,  
                            uint8 indices[4096],
                            int* num_attacks_out)
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
Generate the source file containing the magic bitboard data for bishop and rook
attack sets. The magic bitboard attack generator uses this structure:

typedef struct
{
    uint64          magic;              // multiplier
    bitboard        occupancy_mask;     // pre mask (excl outside squares)  
    const uchar*    attack_indices;     // lookup index
    const bitboard* attacks;            // unique attack set
    int             shift;              // right shift amount
} MagicMoveEntry;

Then for example we have

MagicMoveEntry rook_magics[64];         // one per square for bishop and rook

To get sliding attacks, given a set of occupied_squares, we compute:
entry->attacks[entry->attack_indices[((occupied_squares & entry->occupancy_mask) * entry->magic) >> entry->shift]];

I decided to make the attacks a unique set with one byte indices as this
compresses the overall size of the magic tables by a significant factor and
helps to reduce pressure on the cache.

It's a shame the MagicMoveEntry struct can't be 64 bytes in size.
*******************************************************************************/
int main()
{
    FILE* file = fopen("../generated_magics.c", "w");
    if (!file)
    {
        printf("ERROR: unable to open generated_magics.c for write\n");
        return -1;
    }
    fprintf(file, "/* This file was generated on " __DATE__ " at " __TIME__ " */\n"); 
    fprintf(file, "#include \"types.h\"\n");
    for (int i = 0; i != 2; ++i)
    {
        const char* const piece_name = (i == 0) ? "BISHOP" : "ROOK";
       
        for (int j = 0; j != 64; ++j)
        {
            FindMagic(j, !!i, &magics[j].magic, &magics[j].mask, magics[j].attacks, &magics[j].shift);
            CompressAttacks(magics[j].attacks, 1 << (64 - magics[j].shift), magics[j].attack_indices, &magics[j].num_discrete_attacks);
        }
        for (int j = 0; j != 64; ++j)
        {
            const int num_entries = magics[j].num_discrete_attacks;
            fprintf(file, "static const bitboard %s_MAGIC_ATTACKS_%c%c[%d] = {",
                    piece_name,
                    'A' + FILE_OF(j),
                    '1' + RANK_OF(j),
                    num_entries);
            for (int k = 0; k != num_entries; ++k)
            {
                if ((k & 7) == 0)
                {
                    fprintf(file, "\n    ");
                }
                fprintf(file, "0x%016llXull, ",  magics[j].attacks[k]);                
            }
            fprintf(file, "\n};\n");
            const int num_indices = 1 << (64 - magics[j].shift);
            fprintf(file, "static const uchar %s_MAGIC_INDICES_%c%c[%d] = {",
                    piece_name,
                    'A' + FILE_OF(j),
                    '1' + RANK_OF(j),
                    num_indices);
            for (int k = 0; k != num_indices; ++k)
            {
                if ((k & 0x1F) == 0)
                {
                    fprintf(file, "\n    ");
                }
                fprintf(file, "0x%02hhX, ",  magics[j].attack_indices[k]);                
            }
            fprintf(file, "\n};\n");

        }
        fprintf(file, "const MagicMoveEntry %s_MAGICS[64] = {\n", piece_name);
        for (int j = 0; j != 64; ++j)
        {
            fprintf(file, "    { 0x%016llXull, 0x%016llXull, %s_MAGIC_INDICES_%c%c, %s_MAGIC_ATTACKS_%c%c, %d },\n",
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
        fprintf(file, "};\n");
    }
    fclose(file);
}

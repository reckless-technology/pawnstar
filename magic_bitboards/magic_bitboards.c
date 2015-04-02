#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <stdbool.h>

typedef unsigned long long      uint64;

#define MASK_EAST_1             0x7F7F7F7F7F7F7F7Full 
#define MASK_WEST_1             0xFEFEFEFEFEFEFEFEull 
#define ALL_SQUARES             0xFFFFFFFFFFFFFFFFull
#define NO_SQUARES              0x0000000000000000ull
#define SHIFT_NORTH(b)          ((b) << 8)
#define SHIFT_NORTHEAST(b)      (((b) & MASK_EAST_1) << 9)
#define SHIFT_EAST(b)           (((b) & MASK_EAST_1) << 1)
#define SHIFT_SOUTHEAST(b)      (((b) & MASK_EAST_1) >> 7)
#define SHIFT_SOUTH(b)          ((b) >> 8)
#define SHIFT_SOUTHWEST(b)      (((b) & MASK_WEST_1) >> 9)
#define SHIFT_WEST(b)           (((b) & MASK_WEST_1) >> 1)
#define SHIFT_NORTHWEST(b)      (((b) & MASK_WEST_1) << 7)
#define FILE_OF(locn)           ((locn) & 7)
#define RANK_OF(locn)           ((locn) >> 3)
#define FILE_CHAR(locn)         ('a' + ((locn) & 7))
#define RANK_CHAR(locn)         ('1' + ((locn) >> 3))
#define BITBOARD(locn)          (1ull << (locn))
#define BITBOARD_XY(x,y)        (1ull << ((x) + 8 * (y)))
#define IS_SUBSET(sub,super)    (((sub) & (super)) == (sub))

/******************************************************************************
Find the index of the least significant bit set in x (x is non zero)
*******************************************************************************/
static int Lsb(uint64 x) 
{
    static const int de_bruijn[64] = 
    {
        0,  1, 48,  2, 57, 49, 28,  3,
       61, 58, 50, 42, 38, 29, 17,  4,
       62, 55, 59, 36, 53, 51, 43, 22,
       45, 39, 33, 30, 24, 18, 12,  5,
       63, 47, 56, 27, 60, 41, 37, 16,
       54, 35, 52, 21, 44, 32, 23, 11,
       46, 26, 40, 15, 34, 20, 31, 10,
       25, 14, 19,  9, 13,  8,  7,  6
    };
   return de_bruijn[((x & -x) * 0x03F79D71B4CB0A89ull) >> 58];
}

/******************************************************************************
Find the index of and then clear the least significant bit set in *x
*******************************************************************************/
static int FindAndClearLsb(uint64* x)
{    
    const uint64 y = *x;
    int result = Lsb(y);
    *x = y & (y - 1);
    return result;
}

/******************************************************************************
Find the number of bits set in x
*******************************************************************************/
static int PopCount(uint64 x)
{
    int count = 0;
    while (x)
    {
        x &= x - 1;
        ++count;
    }
    return count;
}

/******************************************************************************
Determine the occupancy mask for bishop attacks for a bishop standing on 
location (excluding outer squares as they do no affect attack set)
*******************************************************************************/
static uint64 BishopOccupancyMask(int location)
{
    uint64 result = 0;
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
    uint64 result = 0;
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
    uint64 result = 0;
    uint64 square;
    for (square = SHIFT_NORTHEAST(BITBOARD(location)); square; square = SHIFT_NORTHEAST(square))
    {
        result |= square;
        if (square & occupied_squares)
        {
            break;
        }
    }
    for (square = SHIFT_SOUTHEAST(BITBOARD(location)); square; square = SHIFT_SOUTHEAST(square))
    {
        result |= square;
        if (square & occupied_squares)
        {
            break;
        }
    }
    for (square = SHIFT_SOUTHWEST(BITBOARD(location)); square; square = SHIFT_SOUTHWEST(square))
    {
        result |= square;
        if (square & occupied_squares)
        {
            break;
        }
    }
    for (square = SHIFT_NORTHWEST(BITBOARD(location)); square; square = SHIFT_NORTHWEST(square))
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
    uint64 result = 0;
    uint64 square;
    for (square = SHIFT_NORTH(BITBOARD(location)); square; square = SHIFT_NORTH(square))
    {
        result |= square;
        if (square & occupied_squares)
        {
            break;
        }
    }
    for (square = SHIFT_EAST(BITBOARD(location)); square; square = SHIFT_EAST(square))
    {
        result |= square;
        if (square & occupied_squares)
        {
            break;
        }
    }
    for (square = SHIFT_SOUTH(BITBOARD(location)); square; square = SHIFT_SOUTH(square))
    {
        result |= square;
        if (square & occupied_squares)
        {
            break;
        }
    }
    for (square = SHIFT_WEST(BITBOARD(location)); square; square = SHIFT_WEST(square))
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
all the possible variations in occupancy from an occupancy mask value.
*******************************************************************************/
static uint64 EnumerateBitMask(uint64 mask, uint64 values[])
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
Find a magic multiplicand for a bishop or rook standing on location - assumes
occupancy mask has at most 12 bits (for a rook on a1 or h8)
*******************************************************************************/
static uint64 FindMagic(int location, bool is_rook)
{
    const uint64 occupancy_mask = is_rook ? RookOccupancyMask(location) : BishopOccupancyMask(location);
    uint64 occupancies[4096];
    uint64 targets[4096];
    uint64 magic_moves[4096]; 
    const int shift = 64 - PopCount(occupancy_mask);
    const uint64 num_values = EnumerateBitMask(occupancy_mask, occupancies);
    for (int i = 0; i != num_values; ++i)
    {
        targets[i] = is_rook ? RookAttacks(occupancies[i], location) : BishopAttacks(occupancies[i], location);
    }
    for (int num_attempts = 0; ; ++num_attempts)
    {
        bool is_good = true;
        const uint64 trial_magic = PseudoRandom64() & PseudoRandom64() & PseudoRandom64();
        memset(magic_moves, 0xFF, sizeof(magic_moves));
        for (uint64 i = 0; i != num_values; ++i)
        {
            const unsigned int index = (unsigned int)((occupancies[i] * trial_magic) >> shift);
            if (magic_moves[index] == ALL_SQUARES)
            {
                magic_moves[index] = targets[i];
                continue;
            }
            if (magic_moves[index] != targets[i])
            {
                /* collision */
                is_good = false;
                break;
            }
        }
        if (is_good)
        {
            printf("%s magic %c%c 0x%016llX shift %2d occupancy mask 0x%016llX (attempts %d)\n", 
                   is_rook ? "rook  " : "bishop", 
                   FILE_CHAR(location), 
                   RANK_CHAR(location), 
                   trial_magic, 
                   shift,                
                   occupancy_mask,
                   num_attempts);
            return trial_magic;
        }
    }
}

static uint64 FindMagicCompressed(int location, bool is_rook)
{
    const uint64 occupancy_mask = is_rook ? RookOccupancyMask(location) : BishopOccupancyMask(location);
    uint64 occupancies[4096];
    uint64 targets[4096];
    uint64 magic_moves[4096]; 
    const uint64 attack_mask = is_rook ? RookAttacks(NO_SQUARES, location) : BishopAttacks(NO_SQUARES, location);
    const int shift = 64 - PopCount(occupancy_mask) + 1;
    const uint64 num_values = EnumerateBitMask(occupancy_mask, occupancies);
    for (int i = 0; i != num_values; ++i)
    {
        targets[i] = is_rook ? RookAttacks(occupancies[i], location) : BishopAttacks(occupancies[i], location);
    }
    for (int num_attempts = 0; ; ++num_attempts)
    {
        bool is_good = true;
        const uint64 trial_magic = PseudoRandom64() & PseudoRandom64() & PseudoRandom64();
        memset(magic_moves, 0xFF, sizeof(magic_moves));
Restart:
        for (uint64 i = 0; i != num_values; ++i)
        {
            const unsigned int index = (unsigned int)((occupancies[i] * trial_magic) >> shift);
            if (magic_moves[index] == ALL_SQUARES)
            {
                magic_moves[index] = targets[i];
                continue;
            }
            const uint64 magic_move = magic_moves[index] & attack_mask;
            if (magic_move == targets[i])
            {
                continue;
            }
            if (IS_SUBSET(magic_move, targets[i]))
            {
                magic_moves[index] |= targets[i];
                goto Restart;
            }
            /* collision */
            is_good = false;
            break;
        }
        if (is_good)
        {
            printf("compressed %s magic %c%c 0x%016llX shift %2d occupancy mask 0x%016llX (attempts %d)\n", 
                   is_rook ? "rook  " : "bishop", 
                   FILE_CHAR(location), 
                   RANK_CHAR(location), 
                   trial_magic, 
                   shift,                
                   occupancy_mask,
                   num_attempts);
            return trial_magic;
        }
    }
}

int main()
{
    for (int i = 0; i != 64; ++i)
    {
        FindMagic(i, false);
    }
    for (int i = 0; i != 64; ++i)
    {
        FindMagic(i, true);
    }
}

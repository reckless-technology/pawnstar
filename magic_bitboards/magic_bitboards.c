#include <stdio.h>
#include <stdlib.h>
#include <memory.h>

#include "../macros.h"
#include "../types.h"
#include "../bitboard_constants.h"
#include "../inline_functions.h"

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

static uint64 RandomMagic(void)
{
    static uint64 lcg = 0;
    lcg = lcg * 6364136223846793005ull + 1442695040888963407ull;
    uint64 result = lcg >> 32;
    lcg = lcg * 6364136223846793005ull + 1442695040888963407ull;
    result |= lcg & 0xFFFFFFFF00000000ull;
    return result;
}

static uint64 PermuteBitMask(uint64 mask, uint64 permutations[])
{   
    const int num_bits_in_mask = PopCount(mask);
    const uint64 num_permutations = 1ull << num_bits_in_mask;
    uint64 b = mask;
    int bit_indices[64];
    for (int i = 0; i != num_bits_in_mask; ++i)
    {
        bit_indices[i] = FindAndClearLsb(&b);
    }
    for (uint64 i = 0; i != num_permutations; ++i)
    {
        uint64 permutation = 0;
        uint64 j = i;
        while (j)
        {
            permutation |= BITBOARD(bit_indices[FindAndClearLsb(&j)]);
        }
        permutations[i] = permutation;
    }
    return num_permutations;
}

static void DisplayCombinations(int n, int k)
{
    int i, j, c;

    /* i goes through all n-bit numbers */

    for (i = 0; i < (1 << n); i++) {

        /* masking the j'th bit as j goes through all the bits,
         * count the number of 1 bits.  this is called finding
         * a population count.
         */
        for (j = 0, c = 0; j < 32; j++)
        {
            if (i & (1 << j)) c++;
        }

        /* if that number is equal to k, print the combination... */

        if (c == k) 
        {
            /* by again going through all the bits indices,
             * printing only the ones with 1-bits
             */

            for (j = 0; j < 32; j++)
            {
                if (i & (1 << j)) 
                {
                    printf("%i ", j + 1);
                }
            }
            printf("\n");
        }
    }
}

static uint64 FindMagic(int location, bool is_rook)
{
    uint64 occupancy_mask = is_rook ? RookOccupancyMask(location) : BishopOccupancyMask(location);
    uint64 permutations[4096];
    uint64 targets[4096];
    uint64 moves[4096]; 
    int shift = 64 - PopCount(occupancy_mask) + 1;
    uint64 num_permutations = PermuteBitMask(occupancy_mask, permutations);
    for (int i = 0; i != num_permutations; ++i)
    {
        targets[i] = is_rook ? RookAttacks(permutations[i], location) : BishopAttacks(permutations[i], location);
    }
    for (int num_attempts = 0; ; ++num_attempts)
    {
        bool is_good = true;
        uint64 trial_magic = RandomMagic() & RandomMagic() & RandomMagic();
        memset(moves, 0xFF, sizeof(moves));
        for (uint64 i = 0; i != num_permutations; ++i)
        {
            unsigned int index = (unsigned int)((permutations[i] * trial_magic) >> shift);
            if (moves[index] == ALL_SQUARES)
            {
                moves[index] = targets[i];
                continue;
            }
            if (moves[index] != targets[i])
            {
                /* collision */
                is_good = false;
                break;
            }
        }
        if (is_good)
        {
            /* we found a magic */
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

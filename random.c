#include "pawnstar.h"

static uint64 lcg;

/******************************************************************************
Very simple, platform-independent, pseudo-random number generator
*******************************************************************************/
int NextRandom(void)
{
    lcg = lcg * 6364136223846793005ull + 1442695040888963407ull;
    return lcg >> 32; /* only use the top 32 bits as lower bits are useless as a PRNG */
}

void InitializeRandom(void)
{
    lcg = (uint64)time(NULL) * GetMilliseconds();
    int i = (lcg % 7919) + 337;
    while (--i)
    {
        lcg = (lcg * 997) % 2305843009213693951ull;
    }
}
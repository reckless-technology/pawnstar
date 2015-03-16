#include "pawnstar.h"

/******************************************************************************
Very simple, platform-independent, pseudo-random number generator
*******************************************************************************/
int NextRandom(void)
{
    static uint64 lcg = 0;
    if (lcg == 0)
    {
        /**********************************************************************
        Seed the LCG on first invocation
        ***********************************************************************/
        int i; 
        lcg = (uint64)time(NULL);
        i = (lcg % 337) + 337;
        while (--i)
        {
            lcg = NextRandom();
        }
    }
    lcg = lcg * 6364136223846793005ull + 1442695040888963407ull;
    return lcg >> 32; /* only use the top 32 bits as lower bits are useless as a PRNG */
}
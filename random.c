#include "pawnstar.h"

/**
 * @brief Xorshift simple PRNG
 * @return next pseudo random integer
*/
int NextRandom(void)
{
    static uint64_t x;
    if (x == 0)
    {
        x = (uint64_t)time(NULL);
        x *= NextRandom();
    }
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    return (int)((x * 2685821657736338717ull) >> 32);
}
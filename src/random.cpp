#include <chrono>
#include <cstdint>

using namespace std::chrono;

static uint64_t x;

void RandomSeed(uint64_t seed)
{
    x = seed;
}

/**
 * @brief XORshift simple PRNG
 * @return next pseudo random integer
 */
int NextRandom(void)
{
    if (x == 0)
    {
        x = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
        for (int i = 0; i != 1000; ++i)
        {
            NextRandom();
        }
    }
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    return (int)((x * 0x2545F4914F6CDD1Dull) >> 32);
}
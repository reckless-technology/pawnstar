#include "pawnstar.h"
bool RunMergeSortTests(void)
{
    Move values[MAX_MOVES_PER_POSITION];
    for (int run = 0; run != 100000; ++run)
    {
        const int num_elements = (uint32_t)NextRandom() % MAX_MOVES_PER_POSITION;
        for (int i = 0; i != num_elements; ++i)
        {
            const int score = NextRandom() % 10000;
            values[i] = SCORED_MOVE(i, score);
        }
        MergeSort(num_elements, values);
        for (int i = 0; i < num_elements - 1; ++i)
        {
            if (values[i + 1] > values[i])
            {
                return false;
            }
        }
    }
    return true;
}
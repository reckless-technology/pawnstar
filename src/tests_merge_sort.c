#include "pawnstar.h"

bool RunMergeSortTests(void)
{
    ScoredMove values[MAX_MOVES_PER_POSITION];
    for (int run = 0; run != 100000; ++run)
    {
        const int num_elements = (uint32_t)NextRandom() % MAX_MOVES_PER_POSITION;
        for (int i = 0; i != num_elements; ++i)
        {
            values[i].move = i;
            values[i].score = NextRandom() % 1000;
        }
        MergeSort(num_elements, values);
        for (int i = 0; i < num_elements - 1; ++i)
        {
            // Check sorted property
            if (values[i + 1].score > values[i].score)
            {
                return false;
            }
            // Check stable property
            if (values[i + 1].score == values[i].score && values[i + 1].move <= values[i].move)
            {
                return false;
            }
        }
    }
    return true;
}
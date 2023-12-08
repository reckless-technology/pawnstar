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
        SortMoves(num_elements, values);
        for (int i = 0; i < num_elements - 1; ++i)
        {
            const int score_left     = MOVE_SCORE(values[i    ]);
            const int score_right    = MOVE_SCORE(values[i + 1]);
            const int sequence_left  = MOVE_BITS (values[i    ]);
            const int sequence_right = MOVE_BITS (values[i + 1]);
            if (score_left < score_right)
            {
                return false;
            }
            if (score_left == score_right && sequence_left > sequence_right)
            {
                return false;
            }
        }
    }
    return true;
}
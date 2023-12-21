#include <vector>

#include "position.h"
#include "debug_hashtable.h"
#include "transposition_table.h"
#include "function_prototypes.h"

bool RunMergeSortTests(void)
{
    MoveList values {};
    for (int run = 0; run != 100000; ++run)
    {
        values.clear();
        const uint32_t num_elements = (uint32_t)NextRandom() % MAX_MOVES_PER_POSITION;
        if (num_elements < 2)
        {
            continue;
        }
        for (uint32_t i = 0; i != num_elements; ++i)
        {
            const int score = NextRandom() % 10000;
            values.push_back(ScoredMove(i, score));
            Move test_move = values.back();
            if (MoveScore(test_move) != score ||
                MoveBits(test_move) != i)
            {
                printf("ERROR in merge test gen\n");
            }
        }
        SortMoves(values, true);
        for (uint32_t i = 0; i < num_elements - 1; ++i)
        {
            const int score_left     = MoveScore(values[i    ]);
            const int score_right    = MoveScore(values[i + 1]);
            const int sequence_left  = MoveBits (values[i    ]);
            const int sequence_right = MoveBits (values[i + 1]);
            if (score_left < score_right)
            {
                return false;
            }
            if (score_left == score_right && sequence_left >= sequence_right)
            {
                return false;
            }
        }
    }
    return true;
}
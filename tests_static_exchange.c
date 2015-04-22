#include "pawnstar.h"
typedef struct
{
    const char* fen_string;
    int         move;
} SeeTest;

static const SeeTest tests[] = 
{
    { "1k1r3q/1ppn3p/p4b2/4p3/8/P2N2P1/1PP1R1BP/2K1Q3 w - -", CONSTRUCT_MOVE(D3, E5, KNIGHT, PAWN) },
    { "1k1r4/1ppn3p/p4b2/4p3/8/P2N2P1/1PP1R1BP/2K1Q3 w - -",  CONSTRUCT_MOVE(D3, E5, KNIGHT, PAWN) },
    { NULL, },
};

void RunStaticExchangeTests(void)
{
    const SeeTest* test;
    for (test = tests; test->fen_string; ++test)
    {
        Position position[1];
        if (!PositionFromString(test->fen_string, position))
        {
            printf("ERROR: unable to produce position from string\n");
            continue;
        }
        char move_string[8];
        MoveToSanString(position, test->move, move_string);
        int score = EvaluateStaticExchange(position, test->move);
        printf("\n%s\nSEE for %s = %d\n", test->fen_string, move_string, score);

    }
    printf("done\n");
}
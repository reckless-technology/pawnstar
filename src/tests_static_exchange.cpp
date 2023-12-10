#include "pawnstar.h"

typedef struct SeeTest
{
    const char* fen_string;
    int         move;
    int         see_score;
} SeeTest;

static const SeeTest tests[] = 
{   
    { "1k1r4/1pp4p/p7/4p3/8/P5P1/1PP4P/2K1R3 w - -",            CaptureMove(E1, E5, ROOK,   PAWN),  100 },
    { "1k1r3q/1ppn3p/p4b2/4p3/8/P2N2P1/1PP1R1BP/2K1Q3 w - -",   CaptureMove(D3, E5, KNIGHT, PAWN), -200 },
    { NULL,                                                                                                0,    0 },
};

void RunStaticExchangeTests(void)
{
    bool is_pass = true;
    for (const SeeTest* test = tests; test->fen_string; ++test)
    {
        Position position;
        if (!PositionFromString(test->fen_string, position))
        {
            printf("ERROR: unable to produce position from string\n");
            continue;
        }
        char move_string[16];
        MoveToString(position, test->move, move_string);
        int score = EvaluateStaticExchange(position, test->move);
        printf("\n%s\nSEE for %s = %d\n", test->fen_string, move_string, score);
        is_pass &= (score == test->see_score);

    }
    printf("\n%s\n", is_pass ? "PASSED" : "FAILED");
}
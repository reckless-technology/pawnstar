#include "pawnstar.h"
/*
Structure to hold the counts of different move types
*/
typedef struct
{
    int legal_moves;
    int captures;
    int ep_captures;
    int castles;
    int promotions;
    int checks;
} PerftCounts;
/*
Structure to hold a single perft test
*/
typedef struct
{
    const char* position;
    int         depth;
    PerftCounts counts;
} PerftTest;
/*
Standard test positions for Perft move generation tests
Refer to:
http://chessprogramming.wikispaces.com/Perft
http://chessprogramming.wikispaces.com/Perft+Results
*/
static const PerftTest perft_tests[] =
{//    position                                                          depth        nodes   captures      ep   castles promotions   checks
    { "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq -",                6, { 119060324,   2812008,   5248,        0,        0,   809099 } },
    { "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -",    5, { 193690690,  35043416,  73365,  4993637,     8392,  3309887 } },
    { "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - -",                               7, { 178633661,  14519036, 294874,        0,   140024, 12797406 } },
    { "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq -",        6, { 706045033, 210369132,    212, 10882006, 81102984, 26973664 } },
    { "rnbqkb1r/pp1p1ppp/2p5/4P3/2B5/8/PPP1NnPP/RNBQK2R w KQkq -",           5, {  70202861,   6440150,  80978,  1006722,     4700,  2856725 } },
    { "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - -", 5, { 164075551,  19528068,    122,        0,        0,  2998608 } },
    { NULL, },
};
/*
Categorize a null-terminated set of pseudo-legal moves into strictly-legal
moves of various types, storing the result in counts
*/
static void 
CategorizeMoves(const Position* src_position, 
                const int       moves[], 
                PerftCounts*    counts)
{
    Position position;
    for (const int* move = moves; *move; ++move)
    {
        MakeMove(&position, src_position, *move);
        if (position.state_flags & IS_MOVED_INTO_CHECK)
        {
            continue;
        }
        ++counts->legal_moves;
        if (position.state_flags & IS_CHECK)
        {
            ++counts->checks;
        }
        if (MOVE_CAPTURED(*move))
        {
            ++counts->captures;
        }
        if (MOVE_PROMOTED(*move))
        {
            ++counts->promotions;
        }
        if (MOVE_IS_SPECIAL(*move))
        {
            if (MOVE_PIECE(*move) == PAWN)
            {
                ++counts->ep_captures;
            }
            else
            {
                ++counts->castles;
            }
        }
    }
}
/*
Recursive standard perft test
Refer to:
http://chessprogramming.wikispaces.com/Perft
http://chessprogramming.wikispaces.com/Perft+Results
*/
#pragma warning(disable:4221)
static void 
Perft(const Position* src_position, 
      int             depth, 
      int             color, 
      PerftCounts*    counts)
{
    static int call_count = 0;
    int captures[MAX_MOVES_PER_POSITION];
    int non_captures[MAX_MOVES_PER_POSITION];
    const int* move_phases[] = { captures, non_captures, NULL };
    GeneratePseudoLegalMoves(src_position, captures, non_captures);
    if (!(++call_count & 0x3FFFF))
    {
        printf("\rpositions processed %10u", counts->legal_moves);
    }
    if (depth > 1)
    {
        for (const int** phase = move_phases; *phase; ++phase)
        {
            for (const int* move = *phase; *move; ++move)
            {
                Position position;
                MakeMove(&position, src_position, *move);
#if DO_TEST_HASH_DURING_PERFT
                if (position.hash != ComputeHash(&position))
                {
                    printf("ERROR in hash during perft\n");
                }
#endif
                if (position.state_flags & IS_MOVED_INTO_CHECK)
                {
                    continue;
                }
                Perft(&position, depth - 1, ENEMY(color), counts);
            }           
        }
        return;
    }
    CategorizeMoves(src_position, captures, counts);
    CategorizeMoves(src_position, non_captures, counts);
}
/*
Run perft test on the standard test positions
*/
void 
RunPerftTests(void)
{
    PerftCounts counts;
    Position position;
    int start, first_start, stop = 0;
    bool is_good = true;
    int total_nodes = 0;
    const PerftTest* test;
    first_start = GetMilliseconds();
    for (test = perft_tests; test->position; ++test)
    {
        printf("\n%s\n", test->position);
        if (!PositionFromString(test->position, &position))
        {
            printf("ERROR: cannot create position from FEN string\n");
            continue;
        }
        memset(&counts, 0, sizeof(counts));
        total_nodes += test->counts.legal_moves;
        start = GetMilliseconds();
        Perft(&position, test->depth, COLOR_TO_MOVE(&position), &counts);
        stop = GetMilliseconds();
        if (stop == start)
        {
            stop = start + 1; // avoid divide by zero error in positions per second for short tests
        }
        printf("\rdepth                                   %10u\n", test->depth);
        printf(  "positions                               %10u\n", counts.legal_moves);
        printf(  "captures                                %10u\n", counts.captures);
        printf(  "ep captures                             %10u\n", counts.ep_captures);
        printf(  "castles                                 %10u\n", counts.castles);
        printf(  "promotions                              %10u\n", counts.promotions);
        printf(  "checks                                  %10u\n", counts.checks);
        printf(  "elapsed milliseconds                    %10u\n", stop - start);
        printf(  "positions per second                    %10u\n", counts.legal_moves / (stop - start) * 1000);
        if (!!memcmp(&counts, &test->counts, sizeof(PerftCounts)))
        {
            printf("************* ERROR on this position *************\n");
            is_good = false;
        }
    }
    printf("total elapsed milliseconds              %10u\n", stop - first_start);
    printf("mean positions per second               %10u\n", total_nodes / (stop - first_start) * 1000);
    if (is_good)
    {
        printf("******************* PERFT PASS *******************\n");
    }
    else
    {
        printf("******************* PERFT FAIL *******************\n");
    }
}
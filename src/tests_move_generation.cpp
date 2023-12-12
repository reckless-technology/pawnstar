#include <string>

using std::string;

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

#if 0
static char* 
DebugPrintMove(Move move, char* buffer)
{
    static const char* const names[] = {
        "none",
        "pawn",
        "knight",
        "bishop",
        "rook",
        "queen",
        "king",
    };
    buffer += sprintf(buffer, "piece:%s captured:%s promoted:%s from:%c%c to:%c%c flags:%d",
                      names[MovePiece(move)], 
                      names[MoveCaptured(move)], 
                      names[MovePromoted(move)],
                      FileChar(MoveFrom(move)), RankChar(MoveFrom(move)),
                      FileChar(MoveTo  (move)), RankChar(MoveTo  (move)), 
                      (int)((move >> 21) & 7));
    return buffer;
}

static char*
DebugPrintBitboard(Bitboard b, char* buffer)
{
    for (int y = 7; y >= 0; --y)
    {
        for (int x = 0; x != 8; ++x)
        {
            *buffer++ = (BITBOARD(x,y) & b) ? '1' : '0';
            *buffer++ = ' ';
        }
        *buffer++ = '\n';
    }
    *buffer = 0;
    return buffer;
}

static void
DebugPrintPosition(const Position& position, Move move, char* buffer)
{
    static const char* const names[] = {
        "pawns",
        "knights",
        "bishops",
        "rooks",
        "queens",
        "kings",
        "white pieces",
        "black pieces"
    };
    for (int i = 0; i != 8; ++i)
    {
        buffer += sprintf(buffer, "\n%s\n", names[i]);
        buffer = DebugPrintBitboard(position.piece[i], buffer);
    }
    if (move)
    {
        buffer = DebugPrintMove(move, buffer);
    }
}

#endif

/*
Standard test positions for Perft move generation tests
Refer to:
http://chessprogramming.wikispaces.com/Perft
http://chessprogramming.wikispaces.com/Perft+Results
*/
static const PerftTest perft_tests[] =
{/*    position                                                            depth      nodes   captures      ep   castles promotions   checks */
    { "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq -",                6, { 119060324,   2812008,   5248,        0,        0,   809099 } },
    { "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -",    5, { 193690690,  35043416,  73365,  4993637,     8392,  3309887 } },
    { "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - -",                               7, { 178633661,  14519036, 294874,        0,   140024, 12797406 } },
    { "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq -",        6, { 706045033, 210369132,    212, 10882006, 81102984, 26973664 } },
    { "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ -",               5, {  89941194,  12320378,    140,  1240828,  6655216,  3078299 } },
    { "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - -", 5, { 164075551,  19528068,    122,        0,        0,  2998608 } },
    { NULL,                                                                  0, {         0,         0,      0,        0,        0,        0 } },
};
/*
Categorize a null-terminated set of pseudo-legal moves into strictly-legal
moves of various types, storing the result in counts
*/
static void 
CategorizeMoves(const Position& src_position, 
                const Move      moves[], 
                PerftCounts*    counts)
{
    for (const Move* move = moves; *move; ++move)
    {
        Position position { src_position, *move };
        if (position.flags_ & IS_MOVED_INTO_CHECK)
        {
            continue;
        }
        ++counts->legal_moves;
        if (position.flags_ & IS_CHECK)
        {
            ++counts->checks;
        }
        if (MoveCaptured(*move))
        {
            ++counts->captures;
        }
        if (MovePromoted(*move))
        {
            ++counts->promotions;
        }
        if (IsEpCaptureMove(*move))
        {
            ++counts->ep_captures;
        }
        if (IsCastlingMove(*move))
        {
            ++counts->castles;
        }
    }
}
/*
Recursive standard perft test
Refer to:
http://chessprogramming.wikispaces.com/Perft
http://chessprogramming.wikispaces.com/Perft+Results
*/
static void 
Perft(const Position& src_position, 
      int             depth, 
      int             color, 
      PerftCounts*    counts)
{
    static int call_count = 0;
    Move moves[MAX_MOVES_PER_POSITION];
    GeneratePseudoLegalMoves(src_position, moves);
    if (!(++call_count & 0x3FFFF))
    {
        printf("\rpositions processed %10u", counts->legal_moves);
    }
    if (depth > 1)
    {
        for (const Move* move = moves; *move; ++move)
        {
            Position position { src_position, *move };
#if DO_TEST_HASH_DURING_PERFT
            if (position.hash != position.ComputeHash())
            {
                printf("\nERROR in hash during perft\n");
            }
#endif
            if (position.flags_ & IS_MOVED_INTO_CHECK)
            {
                continue;
            }
            Perft(position, depth - 1, EnemyOf(color), counts);
        }           
        return;
    }
    CategorizeMoves(src_position, moves, counts);
}
/*
Run perft test on the standard test positions
*/
void 
RunPerftTests(void)
{
    PerftCounts counts;
    int start, first_start, stop = 0;
    bool is_good = true;
    uint64_t total_nodes = 0;
    const PerftTest* test;
    first_start = GetMilliseconds();
    for (test = perft_tests; test->position; ++test)
    {
        Position position { test->position };
        string pos_string = position.operator std::string();
        printf("\n%s\n", pos_string.c_str());
        memset(&counts, 0, sizeof(counts));
        total_nodes += test->counts.legal_moves;
        start = GetMilliseconds();
        Perft(position, test->depth, position.ColorToMove(), &counts);
        stop = GetMilliseconds();
        if (stop == start)
        {
            stop = start + 1; // avoid divide by zero error in positions per second for short tests
        }
        printf("\rdepth                                   %10u\n",   test->depth);
        printf(  "positions                               %10u\n",   counts.legal_moves);
        printf(  "captures                                %10u\n",   counts.captures);
        printf(  "ep captures                             %10u\n",   counts.ep_captures);
        printf(  "castles                                 %10u\n",   counts.castles);
        printf(  "promotions                              %10u\n",   counts.promotions);
        printf(  "checks                                  %10u\n",   counts.checks);
        printf(  "elapsed milliseconds                    %10u\n",   stop - start);
        printf(  "positions per second                    %10" PRIu64 "\n", (uint64_t)counts.legal_moves * 1000 / (stop - start));
        if (!!memcmp(&counts, &test->counts, sizeof(PerftCounts)))
        {
            printf("************* ERROR on this position *************\n");
            is_good = false;
        }
    }
    printf("total elapsed milliseconds              %10u\n",   stop - first_start);
    printf("mean positions per second               %10" PRIu64 "\n", total_nodes * 1000 / (stop - first_start));
    if (is_good)
    {
        printf("******************* PERFT PASS *******************\n");
    }
    else
    {
        printf("******************* PERFT FAIL *******************\n");
    }
}
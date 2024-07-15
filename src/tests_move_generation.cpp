#include <algorithm>
#include <cinttypes>
#include <cstring>
#include <string>
#include <string_view>

using std::string;
using std::string_view;

#include "debug_hashtable.h"
#include "function_prototypes.h"
#include "position.h"
#include "transposition_table.h"

struct PerftCounts
{
    uint64_t legal_moves;
    uint64_t captures;
    uint64_t ep_captures;
    uint64_t castles;
    uint64_t promotions;
    uint64_t checks;

    bool operator==(const PerftCounts &) const = default;
};

struct PerftTest
{
    string_view position;
    int         depth;
    PerftCounts counts;
};

/*
Standard test positions for Perft move generation tests
Refer to:
http://chessprogramming.wikispaces.com/Perft
http://chessprogramming.wikispaces.com/Perft+Results
*/
/* clang-format off */
constexpr PerftTest perft_tests[] = {
    /*    position                                                      depth         nodes     captures        ep     castles  promotions      checks */
    {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq -",                7, { 3195901860,   108329926,   319617,     883453,          0,   33103848 } },
    {"r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -",    6, { 8031647685,  1558445089,  3577504,  184513607,   56627920,   92238050 } },
    {"8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - -",                               8, { 3009794393,   267586558,  8009239,          0,    6578076,  135626805 } },
    {"r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq -",        6, {  706045033,   210369132,      212,   10882006,   81102984,   26973664 } },
    {"rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ -",               5, {   89941194,    12320378,      140,    1240828,    6655216,    3078299 } },
    {"r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - -", 5, {  164075551,    19528068,      122,          0,          0,    2998608 } },
};
/* clang-format on */

static void PerftExtra(const Position &src_position, int depth, Color color, PerftCounts &counts)
{
    static uint32_t call_count = 0;
    if (!(++call_count & 0xFFFFF))
    {
        printf("\rpositions processed %10" PRIu64, counts.legal_moves);
    }
    MoveList move_list{src_position.GenerateLegalMoves()};
    if (depth > 1)
    {
        for (Move move : move_list)
        {
            Position position{src_position.MakeMove(move)};
            PerftExtra(position, depth - 1, EnemyOf(color), counts);
        }
        return;
    }
    for (Move move : move_list)
    {
        Position position{src_position.MakeMove(move)};
        ++counts.legal_moves;
        if (position.IsInCheck())
        {
            ++counts.checks;
        }
        if (move.captured() != NONE)
        {
            ++counts.captures;
        }
        if (move.promoted() != NONE)
        {
            ++counts.promotions;
        }
        if (move.IsEpCaptureMove())
        {
            ++counts.ep_captures;
        }
        if (move.IsCastlingMove())
        {
            ++counts.castles;
        }
    }
}

static void Perft(const Position &src_position, int depth, Color color, uint64_t &num_moves)
{
    static uint32_t call_count = 0;
    if (!(++call_count & 0xFFFFF))
    {
        printf("\rpositions processed %10" PRIu64, num_moves);
    }
    MoveList move_list{src_position.GenerateLegalMoves()};
    if (depth > 1)
    {
        for (Move move : move_list)
        {
            Position position{src_position.MakeMove(move)};
            Perft(position, depth - 1, EnemyOf(color), num_moves);
        }
        return;
    }
    num_moves += move_list.size();
}

void RunPerftTests(void)
{
    int      start, first_start, stop = 0;
    bool     is_good     = true;
    uint64_t total_nodes = 0;
    first_start          = GetMilliseconds();
    for (const PerftTest &test : perft_tests)
    {
        Position position{test.position};
        string   pos_string{position.ToString()};
        printf("\n%s\n", pos_string.c_str());
        uint64_t num_moves = 0;
        total_nodes += test.counts.legal_moves;
        start = GetMilliseconds();
        Perft(position, test.depth, position.ColorToMove(), num_moves);
        stop = GetMilliseconds();
        if (stop == start)
        {
            stop = start + 1; // avoid divide by zero error in positions per second for short tests
        }
        printf("\r");
        printf("%-40s%10d\n", "depth", test.depth);
        printf("%-40s%10" PRIu64 "\n", "legal moves", num_moves);
        printf("%-40s%10d\n", "elapsed ms", stop - start);
        printf("%-40s%10" PRIu64 "\n", "moves per second", num_moves * 1000 / (stop - start));
        if (num_moves != test.counts.legal_moves)
        {
            printf("************* ERROR on this position *************\n");
            is_good = false;
        }
    }
    printf("%-40s%10u\n", "total elapsed milliseconds", stop - first_start);
    printf("%-40s%10" PRIu64 "\n", "mean positions per second", total_nodes * 1000 / (stop - first_start));
    if (is_good)
    {
        printf("******************* PERFT PASS *******************\n");
    }
    else
    {
        printf("******************* PERFT FAIL *******************\n");
    }
}

void RunPerftTestsExtra(void)
{
    int      start, first_start, stop = 0;
    bool     is_good     = true;
    uint64_t total_nodes = 0;
    first_start          = GetMilliseconds();
    for (const PerftTest &test : perft_tests)
    {
        Position position{test.position};
        string   pos_string{position.ToString()};
        printf("\n%s\n", pos_string.c_str());
        PerftCounts counts{0, 0, 0, 0, 0, 0};
        total_nodes += test.counts.legal_moves;
        start = GetMilliseconds();
        PerftExtra(position, test.depth, position.ColorToMove(), counts);
        stop = GetMilliseconds();
        if (stop == start)
        {
            stop = start + 1; // avoid divide by zero error in positions per second for short tests
        }
        printf("\r");
        printf("%-40s%10d\n", "depth", test.depth);
        printf("%-40s%10" PRIu64 "\n", "legal moves", counts.legal_moves);
        printf("%-40s%10" PRIu64 "\n", "captures", counts.captures);
        printf("%-40s%10" PRIu64 "\n", "ep captures", counts.ep_captures);
        printf("%-40s%10" PRIu64 "\n", "castles", counts.castles);
        printf("%-40s%10" PRIu64 "\n", "promotions", counts.promotions);
        printf("%-40s%10" PRIu64 "\n", "checks", counts.checks);
        printf("%-40s%10d\n", "elapsed ms", stop - start);
        printf("%-40s%10" PRIu64 "\n", "moves per second", counts.legal_moves * 1000 / (stop - start));
        if (counts != test.counts)
        {
            printf("************* ERROR on this position *************\n");
            is_good = false;
        }
    }
    printf("%-40s%10u\n", "total elapsed milliseconds", stop - first_start);
    printf("%-40s%10" PRIu64 "\n", "mean positions per second", total_nodes * 1000 / (stop - first_start));
    if (is_good)
    {
        printf("******************* PERFT PASS *******************\n");
    }
    else
    {
        printf("******************* PERFT FAIL *******************\n");
    }
}
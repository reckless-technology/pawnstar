#include <string>
#include <cstring>
#include <cinttypes>

using std::string;

#include "position.h"
#include "debug_hashtable.h"
#include "transposition_table.h"
#include "function_prototypes.h"

/*
Structure to hold the counts of different move types
*/
struct PerftCounts
{
    uint64_t legal_moves;
    uint64_t captures;
    uint64_t ep_captures;
    uint64_t castles;
    uint64_t promotions;
    uint64_t checks;

    bool operator==(const PerftCounts&) const = default;
};
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
static constexpr PerftTest perft_tests[] =
{/*    position                                                            depth      nodes   captures      ep   castles promotions   checks */
    { "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq -",                6, { 119060324,   2812008,   5248,        0,        0,   809099 } },
    { "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -",    5, { 193690690,  35043416,  73365,  4993637,     8392,  3309887 } },
    { "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - -",                               7, { 178633661,  14519036, 294874,        0,   140024, 12797406 } },
    { "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq -",        6, { 706045033, 210369132,    212, 10882006, 81102984, 26973664 } },
    { "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ -",               5, {  89941194,  12320378,    140,  1240828,  6655216,  3078299 } },
    { "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - -", 5, { 164075551,  19528068,    122,        0,        0,  2998608 } },
};
/*
Categorize a null-terminated set of pseudo-legal moves into strictly-legal
moves of various types, storing the result in counts
*/
static void 
CategorizeMoves(const Position& src_position, 
                const MoveList& move_list, 
                PerftCounts&    counts)
{
    for (auto move : move_list)
    {
        Position position { src_position, move };
        if (position.flags_ & IS_MOVED_INTO_CHECK)
        {
            printf("ERROR illegal move!\n");
            continue;
        }
        ++counts.legal_moves;
        if (position.flags_ & IS_CHECK)
        {
            ++counts.checks;
        }
        if (MoveCaptured(move))
        {
            ++counts.captures;
        }
        if (MovePromoted(move))
        {
            ++counts.promotions;
        }
        if (IsEpCaptureMove(move))
        {
            ++counts.ep_captures;
        }
        if (IsCastlingMove(move))
        {
            ++counts.castles;
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
      Color           color, 
      PerftCounts&    counts)
{
    static int call_count = 0;
    MoveList move_list = src_position.GeneratePseudoLegalMoves();
    ScoreAndSortMoves(src_position, move_list, 0, 0);
    if (!(++call_count & 0x3FFFF))
    {
        printf("\rpositions processed %10" PRIu64, counts.legal_moves);
    }
    if (depth > 1)
    {
        for (const auto move : move_list)
        {
            Position position { src_position, move };
            if (position.hash_ != position.ComputeHash())
            {
                printf("\nERROR in hash during perft\n");
            }
            if (position.flags_ & IS_MOVED_INTO_CHECK)
            {
                printf("ERROR illegal move!\n");
                continue;
            }
            Perft(position, depth - 1, EnemyOf(color), counts);
        }           
        return;
    }
    CategorizeMoves(src_position, move_list, counts);
}
/*
Run perft test on the standard test positions
*/
void 
RunPerftTests(void)
{
    int start, first_start, stop = 0;
    bool is_good = true;
    uint64_t total_nodes = 0;
    first_start = GetMilliseconds();
    for (auto& test : perft_tests)
    {
        Position position { test.position };
        string pos_string { position };
        printf("\n%s\n", pos_string.c_str());
        PerftCounts counts { 0, 0, 0, 0, 0, 0 };
        total_nodes += test.counts.legal_moves;
        start = GetMilliseconds();
        Perft(position, test.depth, position.ColorToMove(), counts);
        stop = GetMilliseconds();
        if (stop == start)
        {
            stop = start + 1; // avoid divide by zero error in positions per second for short tests
        }
        printf("\rdepth                                   %10d\n",          test.depth);
        printf(  "positions                               %10" PRIu64 "\n", counts.legal_moves);
        printf(  "captures                                %10" PRIu64 "\n", counts.captures);
        printf(  "ep captures                             %10" PRIu64 "\n", counts.ep_captures);
        printf(  "castles                                 %10" PRIu64 "\n", counts.castles);
        printf(  "promotions                              %10" PRIu64 "\n", counts.promotions);
        printf(  "checks                                  %10" PRIu64 "\n", counts.checks);
        printf(  "elapsed milliseconds                    %10d\n",          stop - start);
        printf(  "positions per second                    %10" PRIu64 "\n", counts.legal_moves * 1000 / (stop - start));
        if (counts != test.counts)
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
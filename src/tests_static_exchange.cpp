#include "debug_hashtable.h"
#include "function_prototypes.h"
#include "position.h"
#include "transposition_table.h"

#include <string>
#include <string_view>

using std::string;
using std::string_view;

struct SeeTest
{
    string_view fen_string;
    int         move;
    int         see_score;
};

constexpr SeeTest tests[] = {
    {"1k1r4/1pp4p/p7/4p3/8/P5P1/1PP4P/2K1R3 w - -", CaptureMove(E1, E5, ROOK, PAWN), 100},
    {"1k1r3q/1ppn3p/p4b2/4p3/8/P2N2P1/1PP1R1BP/2K1Q3 w - -", CaptureMove(D3, E5, KNIGHT, PAWN), -200},
};

void RunStaticExchangeTests(void)
{
    bool is_pass = true;
    for (const SeeTest &test : tests)
    {
        Position position{test.fen_string};
        string   pos_str{position.ToString()};
        string   move_string{position.MoveToString(test.move)};
        bool     is_checking;
        int      score = EvaluateStaticExchange(position, test.move, is_checking);
        printf("\n%s\nSEE for %s = %d\n", pos_str.c_str(), move_string.c_str(), score);
        is_pass &= (score == test.see_score);
    }
    printf("\n%s\n", is_pass ? "PASSED" : "FAILED");
}
/// @file Tests of static exchange evaluation.

#include "debug_hashtable.h"
#include "evaluation.h"
#include "position.h"
#include "static_exchange_evaluation.h"
#include "transposition_table.h"

#include <string>
#include <string_view>

using std::string;
using std::string_view;

/// @brief A SEE test vector.
struct SeeTest
{
    string_view fen_string; ///< Position to test.
    Move        move;       ///< Move to analyze.
    int         see_score;  ///< Expected SEE score.
};

/// @brief SEE test vectors.
// clang-format off
constexpr SeeTest tests[]{
    // Position                                                 Move to evaluate        Score
    {"1k1r4/1pp4p/p7/4p3/8/P5P1/1PP4P/2K1R3 w - -",             Move::Regular(E1, E5),  100},
    {"1k1r3q/1ppn3p/p4b2/4p3/8/P2N2P1/1PP1R1BP/2K1Q3 w - -",    Move::Regular(D3, E5), -200},
};
// clang-format on

/// @brief Run the set of SEE tests.
void RunStaticExchangeTests(void)
{
    bool is_pass = true;
    for (const SeeTest &test : tests)
    {
        Position position = Position::FromString(test.fen_string);
        string   pos_str{position.ToString()};
        string   move_string{test.move.ToString()};
        bool     is_checking;
        int      score = EvaluateStaticExchange(position, test.move, is_checking);
        printf("\n%s\nSEE for %s = %d\n", pos_str.c_str(), move_string.c_str(), score);
        is_pass &= (score == test.see_score);
    }
    printf("\n%s\n", is_pass ? "PASSED" : "FAILED");
}
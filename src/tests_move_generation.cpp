/// @file Standard perft move generation tests.

#include <algorithm>
#include <chrono>
#include <cinttypes>
#include <cstring>
#include <format>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>

using namespace std::chrono;

using std::string;
using std::string_view;

#include "chess_clock.h"
#include "debug_hashtable.h"
#include "position.h"
#include "transposition_table.h"

/// @brief Structure to hold a basic perft test.
struct PerftTest
{
    std::string position; ///< FEN string of starting position
    int         depth;    ///< perft search depth
    uint64_t    count;    ///< total number of leaf node moves
};

/// @brief Run standard perft test - recursive search.
/// @param src_position position to search
/// @param depth search depth
/// @param num_moves total number of moves at terminal / leaf nodes
static void Perft(const Position &src_position, int depth, uint64_t &num_moves)
{
    MoveList move_list{src_position.GenerateLegalMoves()};
    if (depth > 1)
    {
        for (Move move : move_list)
        {
            Position position{src_position.MakeMove(move)};
            Perft(position, depth - 1, num_moves);
        }
        return;
    }
    num_moves += move_list.size();
}

extern const std::vector<std::string> perft_results;

/// @brief Rund the set of perft tests.
void RunPerftTests(void)
{
    std::vector<PerftTest> tests;
    for (auto test_str : perft_results)
    {
        // Split into tokens separated by semicolons
        std::vector<std::string> tokens;
        for (;;)
        {
            const auto i = test_str.find(';');
            if (i == std::string::npos)
            {
                tokens.push_back(test_str);
                break;
            }
            tokens.push_back(test_str.substr(0, i));
            test_str = test_str.substr(i + 1);
        }
        // First token is FEN position
        std::string fen = tokens[0];
        tokens.erase(tokens.begin());
        // Remaining tokens are depth and counts
        for (auto token : tokens)
        {
            // Format is "Dxx NNNN"
            int      depth;
            uint64_t count;
            if (std::sscanf(token.c_str(), " D%d %" PRIu64, &depth, &count) != 2)
            {
                std::cout << std::format("ERROR: unable to scan perft test {}\n", test_str);
            }
            tests.push_back(PerftTest{fen, depth, count});
        }
    }
    bool     is_good     = true;
    uint64_t total_nodes = 0;
    int64_t  stop        = 0;
    auto     first_start = ElapsedMicroseconds();
    for (const auto &test : tests)
    {
        Position position{Position::FromString(test.position)};
        string   pos_string{position.ToString()};
        uint64_t num_moves = 0;
        total_nodes += test.count;
        auto start = ElapsedMicroseconds();
        Perft(position, test.depth, num_moves);
        stop = ElapsedMicroseconds();
        if (stop == start)
        {
            stop = start + 1; // avoid divide by zero error in positions per second for short tests
        }
        std::cout << std::format("{:<75} depth:{:2} moves:{:10} Mnps:{:4}\n", pos_string, test.depth, num_moves,
                                 num_moves / (stop - start));
        if (num_moves != test.count)
        {
            std::cout << "************* ERROR on this position *************\n";
            is_good = false;
        }
    }
    std::cout << std::format("{:<40}{:10}\n", "total elapsed milliseconds", (stop - first_start) / 1000);
    std::cout << std::format("{:<40}{:10}\n", "mean positions per second",
                             total_nodes * 1000000 / (stop - first_start));
    if (is_good)
    {
        std::cout << "******************* PERFT PASS *******************\n";
    }
    else
    {
        std::cout << "******************* PERFT FAIL *******************\n";
    }
}
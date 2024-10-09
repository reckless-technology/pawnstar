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
/// @param color color to move
/// @param num_moves total number of moves at terminal / leaf nodes
static void Perft(const Position &src_position, int depth, Color color, uint64_t &num_moves)
{
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

extern const std::vector<std::string> perft_results;

/// @brief Rund the set of perft tests.
void RunPerftTests(void)
{
    std::vector<PerftTest> tests;
    for (auto test : perft_results)
    {
        // Split into tokens separated by semicolons
        std::vector<std::string> tokens;
        for (;;)
        {
            const auto i = test.find(';');
            if (i == std::string::npos)
            {
                tokens.push_back(test);
                break;
            }
            tokens.push_back(test.substr(0, i));
            test = test.substr(i + 1);
        }
        // First token is FEN position
        std::string fen = tokens[0];
        // Remaining tokens are depth and counts
        for (std::size_t i = 1; i < tokens.size(); ++i)
        {
            // Format is "Dxx NNNN"
            int      depth;
            uint64_t count;
            if (std::sscanf(tokens[i].c_str(), "D%d %" PRIu64, &depth, &count) != 2)
            {
                std::cout << std::format("ERROR: unable to scan perft test {}\n", test);
            }
            tests.push_back(PerftTest{fen, depth, count});
        }
    }
    bool     is_good     = true;
    uint64_t total_nodes = 0;
    auto     first_start = ElapsedMicroseconds();
    int64_t  stop;
    for (const auto &test : tests)
    {
        Position position = Position::FromString(test.position);
        string   pos_string{position.ToString()};
        uint64_t num_moves = 0;
        total_nodes += test.count;
        auto start = ElapsedMicroseconds();
        Perft(position, test.depth, position.ColorToMove(), num_moves);
        stop = ElapsedMicroseconds();
        if (stop == start)
        {
            stop = start + 1; // avoid divide by zero error in positions per second for short tests
        }
        std::cout << std::format("{:<80} depth:{:2} moves:{:10} Mnps:{:4}\n", pos_string, test.depth, num_moves,
                                 num_moves / (stop - start));
        if (num_moves != test.count)
        {
            std::cout << "************* ERROR on this position *************\n";
            is_good = false;
        }
    }
    std::cout << std::format("total elapsed milliseconds              {:10}\n", (stop - first_start) / 1000);
    std::cout << std::format("mean positions per second               {:10}\n",
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
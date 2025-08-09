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
    string   position; ///< FEN string view of starting position
    int      depth;    ///< perft search depth
    uint64_t count;    ///< total number of leaf node moves
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
        for (const auto &move : move_list)
        {
            Position position{src_position.MakeMove(move)};
            Perft(position, depth - 1, num_moves);
        }
        return;
    }
    num_moves += move_list.size();
}

// Standard Perft test position set.
extern const std::array<const char *, 133> perft_results;

/// @brief Run the suite of perft tests.
void RunPerftTests(void)
{
    std::vector<PerftTest> tests;
    for (const char *line : perft_results)
    {
        // Split into tokens separated by semicolons
        std::vector<string> tokens;
        string              test_str{line};
        for (std::size_t i = test_str.find(';'); i != string::npos; i = test_str.find(';'))
        {
            tokens.push_back(test_str.substr(0, i));
            test_str = test_str.substr(i + 1);
        }
        tokens.push_back(test_str);
        // First token is FEN position
        const auto fen = tokens[0];
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
    bool       is_good     = true;
    uint64_t   total_nodes = 0;
    const auto first_start = std::chrono::system_clock::now();
    for (const auto &test : tests)
    {
        Position position{Position::FromString(test.position)};
        string   pos_string{position.ToString()};
        uint64_t num_moves = 0;
        total_nodes += test.count;
        const auto start = std::chrono::system_clock::now();
        Perft(position, test.depth, num_moves);
        const auto stop       = std::chrono::system_clock::now();
        auto       elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(stop - start).count();
        if (elapsed_us == 0)
        {
            elapsed_us = 1;
        }
        std::cout << std::format("{:<75} depth:{:2} moves:{:10} Mnps:{:4}\n", pos_string, test.depth, num_moves,
                                 num_moves / elapsed_us);
        if (num_moves != test.count)
        {
            std::cout << "************* ERROR on this position *************\n";
            is_good = false;
        }
    }
    const auto last_stop = std::chrono::system_clock::now();
    std::cout << std::format("{:<40}{:10}\n", "total elapsed milliseconds",
                             std::chrono::duration_cast<std::chrono::milliseconds>(last_stop - first_start).count());
    std::cout << std::format(
        "{:<40}{:10}\n", "mean positions per second",
        total_nodes * 1000 / std::chrono::duration_cast<std::chrono::milliseconds>(last_stop - first_start).count());
    if (is_good)
    {
        std::cout << "******************* PERFT PASS *******************\n";
    }
    else
    {
        std::cout << "******************* PERFT FAIL *******************\n";
    }
}
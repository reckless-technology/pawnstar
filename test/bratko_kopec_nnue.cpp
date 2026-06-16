/// @file bratko_kopec_nnue.cpp Bratko-Kopec search-quality suite using the NNUE evaluation.
///
/// It searches the 24 positions in bratko_kopec_cases.h with the NNUE evaluator and checks the *move* (the
/// classic Bratko-Kopec metric — did the engine find a good move). A position is "solved" when the move
/// found is among that position's accepted moves (the set of best moves the engine produces over depths
/// 8–11; see bratko_kopec_cases.h). The search is forced single-threaded (`PAWNSTAR_THREADS=1`) so it is
/// deterministic, and the suite is a hard gate: it must solve all 24.
///
/// The net is taken from argv[1]; with no argument the suite is skipped (so `make check` stays green when
/// the net is absent). Optional argv[2] sets the search depth (default 8).
///
/// Usage:  test_bratko_kopec_nnue [net.bin] [depth]

#include "bratko_kopec_cases.h"
#include "chess_clock.h"
#include "debug_hashtable.h"
#include "game.h"
#include "move.h"
#include "position.h"
#include "search.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <format>
#include <iostream>
#include <string>
#include <string_view>

/// @brief Run the Bratko-Kopec suite with NNUE evaluation.
/// @param argc Argument count. @param argv [net.bin] [depth].
/// @return Non-zero on any failure: net load error, a search returning no move, or any position whose best
///         move is not among its accepted moves (the suite must solve all 24).
int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        std::cout << "test_bratko_kopec_nnue skipped (no net supplied)\n";
        return 0;
    }
    const std::string net_path = argv[1];
    const int         depth    = argc > 2 ? std::atoi(argv[2]) : bk::kDefaultDepth;

    // Deterministic single-threaded search so the solved count is reproducible.
    setenv("PAWNSTAR_THREADS", "1", 1);

    Game game;
    if (!game.NnueNetwork().Load(net_path))
    {
        std::cerr << "test_bratko_kopec_nnue: failed to load net '" << net_path << "'\n";
        return 1;
    }
    if (!game.NnueActive())
    {
        std::cerr << "test_bratko_kopec_nnue: NNUE not active after load\n";
        return 1;
    }

    using Clock    = std::chrono::steady_clock;
    int  solved    = 0;
    int  errors    = 0;
    auto t_overall = Clock::now();

    for (int i = 0; i < (int)bk::kCases.size(); ++i)
    {
        const bk::BkCase &tc = bk::kCases[i];

        game.NewGame(tc.fen);
        game.book.Free();
        game.time_control.clock_type = ChessClock::kFixedDepth;
        game.time_control.depth      = depth;
        DebugXClear();
        auto t_start    = Clock::now();
        Move m          = SearchRootNode(game);
        auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - t_start).count();
        DebugXWrite();

        const std::string got_move = m.ToString();

        const bool found = m != Move::None();
        const bool match =
            found && std::find(tc.moves.begin(), tc.moves.end(), got_move) != tc.moves.end();
        if (!found)
        {
            ++errors; // a legal move should always come back for these positions
        }
        if (match)
        {
            ++solved;
        }

        std::cout << std::format("[{}] pos{:02d} got={}/{:<6} accepted=[{}] {}ms {}\n", match ? "SOLVED" : " MISS ",
                                 i + 1, got_move, m.score(), bk::AcceptedMovesString(tc), elapsed_ms, tc.fen);
    }

    auto total_ms = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - t_overall).count();
    const int total = (int)bk::kCases.size();
    std::cout << std::format("\nNNUE Bratko-Kopec: solved {}/{} at depth {}  total {}ms  (net {})\n", solved, total,
                             depth, total_ms, net_path);
    if (errors > 0)
    {
        std::cout << "FAIL: " << errors << " position(s) returned no move\n";
        return 1;
    }
    if (solved != total)
    {
        std::cout << std::format("FAIL: solved {}/{} (expected all {})\n", solved, total, total);
        return 1;
    }
    std::cout << "BRATKO-KOPEC NNUE PASS\n";
    return 0;
}

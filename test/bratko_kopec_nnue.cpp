/// @file bratko_kopec_nnue.cpp Bratko-Kopec search-quality suite using the NNUE evaluation.
///
/// Companion to bratko_kopec.cpp. It searches the same 24 positions (bratko_kopec_cases.h) but with the
/// NNUE evaluator active, and reports how many find a documented best move. NNUE scores are on a different
/// scale from the handcrafted reference scores, so this suite checks the *move* (the classic Bratko-Kopec
/// metric — did the engine find a good move), not the score. A position counts as "solved" when the move
/// found is among that position's reference moves (the union across the single-threaded depth-8..12 sets).
///
/// The search is forced single-threaded (`PAWNSTAR_THREADS=1`) so the result is deterministic. The
/// net is taken from argv[1]; with no argument the suite is skipped (so `make check` stays green when the
/// net is absent). Optional argv[2] sets the search depth (default 8).
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
#include <vector>

namespace
{
/// @brief Distinct reference moves for a position: the union across its depth-8..12 reference sets.
std::vector<std::string_view> AcceptedMoves(const bk::BkCase &tc)
{
    std::vector<std::string_view> moves;
    for (const bk::BkDepth &d : tc.depths)
    {
        for (std::string_view mv : d.moves)
        {
            if (!mv.empty() && std::find(moves.begin(), moves.end(), mv) == moves.end())
            {
                moves.push_back(mv);
            }
        }
    }
    return moves;
}
} // namespace

/// @brief Run the Bratko-Kopec suite with NNUE evaluation.
/// @param argc Argument count. @param argv [net.bin] [depth].
/// @return Non-zero only on a real failure (net load error, or a search returning no move); the
///         solved count is reported as a quality metric, not a hard gate.
int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        std::cout << "test_bratko_kopec_nnue skipped (no net supplied)\n";
        return 0;
    }
    const std::string net_path = argv[1];
    const int         depth    = argc > 2 ? std::atoi(argv[2]) : bk::kMinDepth;

    // Deterministic single-threaded search so the solved count is reproducible.
    setenv("PAWNSTAR_THREADS", "1", 1);

    Game game;
    if (!game.NnueNetwork().Load(net_path))
    {
        std::cerr << "test_bratko_kopec_nnue: failed to load net '" << net_path << "'\n";
        return 1;
    }
    game.SetUseNnue(true);
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

        const std::vector<std::string_view> accepted = AcceptedMoves(tc);
        const bool                          found    = m != Move::None();
        const bool                          match =
            found && std::find(accepted.begin(), accepted.end(), got_move) != accepted.end();
        if (!found)
        {
            ++errors; // a legal move should always come back for these positions
        }
        if (match)
        {
            ++solved;
        }

        std::string accepted_str;
        for (std::string_view mv : accepted)
        {
            if (!accepted_str.empty())
            {
                accepted_str += ' ';
            }
            accepted_str += mv;
        }
        std::cout << std::format("[{}] pos{:02d} got={}/{:<6} bestmoves=[{}] {}ms {}\n", match ? "SOLVED" : " miss ",
                                 i + 1, got_move, m.score(), accepted_str, elapsed_ms, tc.fen);
    }

    auto total_ms = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - t_overall).count();
    std::cout << std::format("\nNNUE Bratko-Kopec: solved {}/24 at depth {}  total {}ms  (net {})\n", solved, depth,
                             total_ms, net_path);
    return errors > 0 ? 1 : 0;
}

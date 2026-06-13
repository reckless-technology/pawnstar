/// @file search_root_node.cpp Functions to search for best move.
#include "chess_clock.h"
#include "debug_hashtable.h"
#include "game.h"
#include "opening_book.h"
#include "position.h"
#include "search.h"
#include "search_state.h"
#include "transposition_table.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <format>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

using std::string;
using std::stringstream;

/// @brief Run iterative deepening on a single search state — one Lazy SMP thread.
/// The main thread (@p is_main) prints info, applies time management and produces the authoritative best
/// move. Helper threads search silently to deepen the shared transposition table and stop as soon as the
/// global cancellation flag is set. Each thread owns its own @p state and @p move_list copy but shares the
/// TT and history through the game.
/// @param state Per-thread search state.
/// @param move_list This thread's copy of the root move list (re-sorted per iteration).
/// @param best_move Best move from the shallow ordering pass (seed).
/// @param is_main Whether this is the authoritative main thread.
/// @param thread_id Thread index (0 = main); drives the per-thread iteration-skip schedule.
/// @param start_ms Search start time (elapsed ms) for info/time accounting.
/// @param ms_allocated Soft time budget (standard clock only).
/// @return The best move found by this thread.
static Move IterativeDeepen(SearchState &state, MoveList move_list, Move best_move, bool is_main, int thread_id,
                            int64_t start_ms, int ms_allocated)
{
    // Per-thread iteration-skip schedule (Stockfish-style): helper threads skip certain iteration depths so
    // their searches desynchronize from the main thread and from each other, prefilling the shared TT with
    // diverse entries instead of recomputing the same tree in lockstep. The main thread (thread_id 0) never skips.
    static constexpr std::array<int, 20> kSkipSize{1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4};
    static constexpr std::array<int, 20> kSkipPhase{0, 1, 0, 1, 2, 3, 0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5, 6, 7};
    constexpr int                        kSkipEntries = (int)kSkipSize.size();

    Game             &game = state.game;
    Variation         principal_variation{};
    std::vector<Move> best_moves; // Best move found at each search depth (main thread, for stability checks).

    for (int depth = kStartDepth + 1; depth != kMaxPly; ++depth)
    {
        if (game.time_control.clock_type == ChessClock::kFixedDepth && depth > game.time_control.depth)
        {
            break;
        }
        if (thread_id > 0)
        {
            const int i = (thread_id - 1) % kSkipEntries;
            if (((depth + kSkipPhase[i]) / kSkipSize[i]) % 2 != 0)
            {
                continue; // this helper thread skips this iteration depth
            }
        }
        SortMoves<true>(move_list); // Sort based on scores from the previous iteration.
        Variation child_pv{};
        int       alpha  = kAlpha;
        state.node_count = 0;
        if (is_main)
        {
            best_moves.push_back(best_move);
        }
        for (auto &move : move_list)
        {
            const int move_index = (int)(&move - move_list.begin());
            const int score      = SearchSingleMove(state, depth, 0, alpha, kBeta, move, child_pv, move_index).score;
            move.AssignScore(score);
            if (state.IsCancelled())
            {
                return best_move;
            }
            if (score > alpha)
            {
                alpha     = score;
                best_move = move;
                if (is_main)
                {
                    // Show thinking output.
                    CopyVariation(principal_variation, child_pv, best_move);
                    string pv_string;
                    for (const auto &m : principal_variation)
                    {
                        pv_string.append(m.ToString());
                        pv_string.push_back(' ');
                    }
                    pv_string.pop_back();
                    std::cout << std::format("info depth {:2} score cp {:5} time {:5} nodes {:8} pv {}\n", depth, alpha,
                                             game.time_control.ElapsedMilliseconds() - start_ms, state.node_count,
                                             pv_string);
                }
            }
        }
        if (alpha > kWinThreshold || alpha < kLoseThreshold)
        {
            break;
        }
        if (is_main && game.time_control.clock_type == ChessClock::kStandard)
        {
            const int64_t elapsed_ms = game.time_control.ElapsedMilliseconds() - start_ms;
            // clang-format off
            const bool is_best_move_consistent =
                std::ranges::all_of(best_moves, [best_move](const Move &m) { return m == best_move; });
            const bool is_score_stable =
                std::ranges::all_of(best_moves, [best_move](const Move &m) {
                    return std::abs(m.score() - best_move.score()) <= kScoreInstabilityThreshold;
                });
            // clang-format on
            if ((is_best_move_consistent && is_score_stable) && (elapsed_ms * 4) > ms_allocated)
            {
                std::cout << "info string Best move consistent AND score stable - search terminated.\n";
                break;
            }
            if ((is_best_move_consistent || is_score_stable) && (elapsed_ms * 2) > ms_allocated)
            {
                std::cout << std::format("info string {} - search terminated.\n",
                                         is_best_move_consistent ? "Best move consistent" : "Score stable");
                break;
            }
            if (elapsed_ms > ms_allocated)
            {
                break;
            }
        }
    }
    return best_move;
}

/// @brief Search the root node and find the best move.
/// @param game Game to search.
/// @return The best move found.
Move SearchRootNode(Game &game)
{
    // If there is a book move for this position, do not bother with search.
    Move book_move = game.book.GetMove(game.CurrentPosition().Hash());
    if (book_move != Move::None())
    {
        return book_move;
    }
    MoveList move_list = game.CurrentPosition().GenerateLegalMoves();
    // If there is only 1 legal move available, there is no point wasting time searching it, just play it.
    if (move_list.size() == 0)
    {
        return Move::None();
    }
    if (move_list.size() == 1)
    {
        return move_list[0];
    }
    // Plan time usage for this search.
    int ms_timeout   = 0; // Hard stop: cancel search when this time expires.
    int ms_allocated = 0; // Soft allocated time for the move.
    switch (game.time_control.clock_type)
    {
    case ChessClock::kStandard:
    default:
        if (game.time_control.num_moves_remaining != 0)
        {
            ms_allocated = game.time_control.ms_remaining / game.time_control.num_moves_remaining;
        }
        else
        {
            const int num_moves_to_go_estimate = std::max(40 - game.CurrentPosition().MoveCount(), 5);
            ms_allocated                       = game.time_control.ms_remaining / num_moves_to_go_estimate;
        }
        ms_timeout = std::max(100, std::min(ms_allocated * 2, game.time_control.ms_remaining - 1000));
        game.time_control.hard_stop_ms = game.time_control.ElapsedMilliseconds() + ms_timeout;
        break;

    case ChessClock::kFixedDepth:
        ms_timeout                     = 0;
        ms_allocated                   = 0;
        game.time_control.hard_stop_ms = 0;
        break;

    case ChessClock::kFixedTime:
        ms_timeout                     = game.time_control.ms_remaining;
        game.time_control.hard_stop_ms = game.time_control.ElapsedMilliseconds() + ms_timeout;
        ms_allocated                   = 0;
        break;

    case ChessClock::kInfinite:
        game.time_control.hard_stop_ms = 0;
        break;
    }

    const int64_t start_ms = game.time_control.ElapsedMilliseconds();
    game.is_cancel_pending.store(false, std::memory_order_relaxed);
    DebugXClear();
    game.transposition_table.Age();

    // Shallow pass for initial move ordering (on a temporary state shared as the launch ordering).
    SearchState state{game};
    Variation   shallow_pv{};
    for (auto &move : move_list)
    {
        const int move_index = (int)(&move - move_list.begin());
        const int score = SearchSingleMove(state, kStartDepth, 0, kAlpha, kBeta, move, shallow_pv, move_index).score;
        move.AssignScore(score);
    }
    SortMoves<true>(move_list);
    Move best_move = move_list[0];

    // Lazy SMP: each thread runs an independent iterative-deepening search of the same root, sharing the
    // (lockless) transposition table and (atomic) history table. The main thread is authoritative; helper
    // threads deepen the shared TT to accelerate it. Each thread has its own SearchState and move-list copy.
    unsigned hw        = std::thread::hardware_concurrency();
    int      n_threads = std::clamp<int>(hw == 0 ? 1 : (int)hw, 1, kMaxSearchThreads);
    if (const char *env = std::getenv("PAWNSTAR_THREADS"))
    {
        const int requested = std::atoi(env);
        if (requested > 0)
        {
            n_threads = std::clamp(requested, 1, kMaxSearchThreads);
        }
    }
    std::vector<std::thread> helpers;
    helpers.reserve(n_threads - 1);
    for (int i = 1; i < n_threads; ++i)
    {
        // Each helper gets a distinct thread id, which selects its iteration-skip schedule (see IterativeDeepen).
        helpers.emplace_back([&game, &move_list, best_move, i, start_ms, ms_allocated] {
            SearchState helper_state{game};
            IterativeDeepen(helper_state, move_list, best_move, /*is_main=*/false, /*thread_id=*/i, start_ms,
                            ms_allocated);
        });
    }

    best_move = IterativeDeepen(state, move_list, best_move, /*is_main=*/true, /*thread_id=*/0, start_ms, ms_allocated);

    game.is_cancel_pending.store(true, std::memory_order_relaxed); // signal helpers to stop
    for (auto &helper : helpers)
    {
        helper.join();
    }
    return best_move;
}

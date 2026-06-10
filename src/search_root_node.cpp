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
#include <format>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using std::string;
using std::stringstream;

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
    game.history_table.Reset();
    game.transposition_table.Age();

    // Construct the main-thread search state from the current game position.
    SearchState state{game};

    Variation         principal_variation{};
    std::vector<Move> best_moves; // Best move found at each search depth.

    // Shallow pass for initial move ordering.
    for (auto &move : move_list)
    {
        const int move_index = (int)(&move - move_list.begin());
        const int score =
            SearchSingleMove(state, kStartDepth, 0, kAlpha, kBeta, move, principal_variation, move_index).score;
        move.AssignScore(score);
    }
    SortMoves<true>(move_list);
    Move best_move = move_list[0];

    for (int depth = kStartDepth + 1; depth != kMaxPly; ++depth)
    {
        if (game.time_control.clock_type == ChessClock::kFixedDepth && depth > game.time_control.depth)
        {
            break;
        }
        SortMoves<true>(move_list); // Sort based on scores from the previous iteration.
        Variation child_pv{};
        int       alpha  = kAlpha;
        state.node_count = 0;
        best_moves.push_back(best_move);
        for (auto &move : move_list)
        {
            const int move_index = (int)(&move - move_list.begin());
            const int score      = SearchSingleMove(state, depth, 0, alpha, kBeta, move, child_pv, move_index).score;
            move.AssignScore(score);
            if (game.is_cancel_pending.load(std::memory_order_relaxed))
            {
                return best_move;
            }
            if (score > alpha)
            {
                alpha     = score;
                best_move = move;
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
        const int64_t stop_ms = game.time_control.ElapsedMilliseconds();
        if (alpha > kWinThreshold || alpha < kLoseThreshold)
        {
            break;
        }
        if (game.time_control.clock_type == ChessClock::kStandard)
        {
            const int64_t elapsed_ms = stop_ms - start_ms;
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

/// @file Functions to search for best move.
#include "chess_clock.h"
#include "debug_hashtable.h"
#include "game.h"
#include "opening_book.h"
#include "position.h"
#include "search.h"
#include "sort_moves.h"
#include "transposition_table.h"

#include <format>
#include <iostream>
#include <sstream>
#include <string>

using std::string;
using std::stringstream;

/// @brief Search the root node and find the best move
/// @param src_position the position to search
/// @return the best move found
Move SearchRootNode(Game &game)
{
    // If there is a book move for this position, do not bother with search.
    Move book_move = GetBookMove(game.CurrentPosition().Hash());
    if (book_move)
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
    switch (game.time_control_.clock_type)
    {
    case CHESS_CLOCK_STANDARD:
    default:
        if (game.time_control_.num_moves_remaining != 0)
        {
            ms_allocated = game.time_control_.ms_remaining / game.time_control_.num_moves_remaining;
        }
        else
        {
            const int num_moves_to_go_estimate = std::max(40 - game.CurrentPosition().MoveCount(), 5);
            ms_allocated                       = game.time_control_.ms_remaining / num_moves_to_go_estimate;
        }
        ms_timeout = std::max(100, std::min(ms_allocated * 2, game.time_control_.ms_remaining - 1000));
        game.time_control_.hard_stop_ms = ElapsedMilliseconds() + ms_timeout;
        break;

    case CHESS_CLOCK_FIXED_DEPTH:
        ms_timeout                      = 0;
        ms_allocated                    = 0;
        game.time_control_.hard_stop_ms = 0;
        break;

    case CHESS_CLOCK_FIXED_TIME:
        ms_timeout                      = game.time_control_.ms_remaining;
        game.time_control_.hard_stop_ms = ElapsedMilliseconds() + ms_timeout;
        ms_allocated                    = 0;
        break;

    case CHESS_CLOCK_INFINITE:
        game.time_control_.hard_stop_ms = 0;
        break;
    }
    DebugXClear();
    int start_ms            = ElapsedMilliseconds();
    game.is_cancel_pending_ = false;
    // For first pass move ordering before we do any search, just use a shallow search with wide open alpha beta window.
    // Subsequent passes will use the results of the previous iteration to sort the moves (the merge sort is stable).
    ResetKillerCounts();
    AgeTranspositionTable();
    Variation principal_variation{};
    Move      best_moves[MAX_PLY]; // Best move found at each ply of search.
    for (std::size_t i = 0; i != move_list.size(); ++i)
    {
        bool      is_checking;
        const int score = SearchSingleMove(game, STARTING_SEARCH_DEPTH, 0, ALPHA, BETA, move_list[i],
                                           principal_variation, i, is_checking);
        move_list[i].AssignScore(score);
    }
    SortMoves<true>(move_list);
    Move best_move                    = move_list[0];
    best_moves[STARTING_SEARCH_DEPTH] = best_move;
    for (int depth = STARTING_SEARCH_DEPTH + 1; depth != MAX_PLY; ++depth)
    {
        if (game.time_control_.clock_type == CHESS_CLOCK_FIXED_DEPTH && depth > game.time_control_.depth)
        {
            break;
        }
        SortMoves<true>(move_list); // Sort moves based on scores from the previous iteration.
        Variation child_pv{};
        int       alpha  = ALPHA;
        game.node_count_ = 0;
        for (std::size_t i = 0; i != move_list.size(); ++i)
        {
            bool      is_checking;
            const int score = SearchSingleMove(game, depth, 0, alpha, BETA, move_list[i], child_pv, i, is_checking);
            move_list[i].AssignScore(score);
            if (game.is_cancel_pending_)
            {
                return best_move;
            }
            if (score > alpha)
            {
                alpha             = score;
                best_move         = move_list[i];
                best_moves[depth] = move_list[i];
                // Show thinking output
                CopyVariation(principal_variation, child_pv, best_move);
                string pv_string;
                bool   is_first_move = true;
                for (const auto &move : principal_variation)
                {
                    if (!is_first_move)
                    {
                        pv_string.push_back(' ');
                    }
                    pv_string.append(move.ToString());
                    is_first_move = false;
                }
                std::cout << std::format("info depth {:2} score cp {:5} time {:5} nodes {:8} pv {}\n", depth,
                                         best_moves[depth].score(), ElapsedMilliseconds() - start_ms, game.node_count_,
                                         pv_string);
            }
        }
        int stop_ms = ElapsedMilliseconds();
        if (alpha > WIN_THRESHOLD || alpha < LOSE_THRESHOLD)
        {
            break;
        }
        if (game.time_control_.clock_type == CHESS_CLOCK_STANDARD)
        {
            // Plan our use of the time with some care. If both the score we find and the best move are consistent
            // between successive iterations, we can probably stop searching before our allocated time is elapsed and
            // save some time for other, potentially more difficult positions. Similarly we can save a smaller amount of
            // time if either of these conditions but not both are true.
            const int elapsed_ms              = stop_ms - start_ms;
            bool      is_best_move_consistent = true;
            bool      is_score_stable         = true;
            for (int i = STARTING_SEARCH_DEPTH; i != depth; ++i)
            {
                if (best_moves[i] != best_moves[depth])
                {
                    is_best_move_consistent = false;
                }
                if (std::abs(best_moves[i].score() - best_moves[depth].score()) > SCORE_INSTABILITY_THRESHOLD)
                {
                    is_score_stable = false;
                }
            }
            if ((is_best_move_consistent && is_score_stable) && (elapsed_ms * 4) > ms_allocated)
            {
                break;
            }
            if ((is_best_move_consistent || is_score_stable) && (elapsed_ms * 3) > ms_allocated)
            {
                break;
            }
            if (elapsed_ms * 2 > ms_allocated)
            {
                break;
            }
        }
    }
    return best_move;
}

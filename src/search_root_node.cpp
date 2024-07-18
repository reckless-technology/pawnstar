#include "chess_clock.h"
#include "debug_hashtable.h"
#include "game.h"
#include "opening_book.h"
#include "position.h"
#include "search.h"
#include "sort_moves.h"
#include "transposition_table.h"

#include <string>
using std::string;

constexpr int min(int a, int b)
{
    return a < b ? a : b;
}
constexpr int max(int a, int b)
{
    return a > b ? a : b;
}

/**
 * @brief Search the root node and find the best move
 * @param src_position the position to search
 * @return the best move found
 */
Move SearchRootNode(Game &game)
{
    /* If there is a book move for this position, do not bother with search. */
    Move book_move = GetBookMove(game.CurrentPosition().Hash());
    if (book_move)
    {
        return book_move;
    }
    MoveList move_list = game.CurrentPosition().GenerateLegalMoves();
    /* If there is only 1 legal move available, no point wasting time searching, just play it. */
    if (move_list.size() == 0)
    {
        return Move::None();
    }
    if (move_list.size() == 1)
    {
        return move_list[0];
    }
    /* Plan time usage for this search. */
    int timeout_ms   = 0; /* cancel search when this time expires             */
    int ms_allocated = 0; /* soft allocated time for the move                 */
    int moves_to_go  = 0; /* number of remaining moves in this clock period   */
    switch (game.time_control_.clock_type)
    {
    case CLOCK_STANDARD:
    default:
        moves_to_go = game.time_control_.standard.moves_per_period -
                      (game.CurrentPosition().MoveCount() % game.time_control_.standard.moves_per_period);
        ms_allocated                           = game.time_control_.standard.milliseconds_remaining / moves_to_go;
        timeout_ms                             = max(100, min(ms_allocated * 2, game.time_control_.standard.milliseconds_remaining - 3000));
        game.time_control_.hard_stop_search_ms = ElapsedMilliseconds() + timeout_ms; /* stop searching regardless when this elapses */
        break;

    case CLOCK_FIXED_DEPTH:
        timeout_ms                             = 0;
        ms_allocated                           = 0;
        game.time_control_.hard_stop_search_ms = 0;
        break;

    case CLOCK_FIXED_TIME:
        timeout_ms                             = game.time_control_.fixed_time.milliseconds;
        game.time_control_.hard_stop_search_ms = ElapsedMilliseconds() + timeout_ms;
        ms_allocated                           = 0;
        break;

    case CLOCK_INCREMENTAL:
        ms_allocated = game.time_control_.incremental.increment_milliseconds + (game.time_control_.incremental.milliseconds_remaining / 30);
        timeout_ms   = max(100, min(ms_allocated * 2, game.time_control_.incremental.milliseconds_remaining - 3000));
        game.time_control_.hard_stop_search_ms = ElapsedMilliseconds() + timeout_ms;
        break;
    }
    DebugXClear();
    int start_ms            = ElapsedMilliseconds();
    game.is_cancel_pending_ = false;
    /*
    For first pass move ordering before we do any search, just use a shallow
    search with wide open alpha beta window. Subsequent passes will use the
    results of the previous iteration to sort the moves (the merge sort is
    stable).
    */
    ResetKillerCounts();
    AgeTranspositionTable();
    Variation principal_variation{};
    Move      best_moves[MAX_PLY]; /* Best move found at each ply of search. */
    for (std::size_t i = 0; i != move_list.size(); ++i)
    {
        const int score = SearchSingleMove(game, STARTING_SEARCH_DEPTH, 0, ALPHA, BETA, move_list[i], principal_variation, i);
        move_list[i].AssignScore(score);
    }
    SortMoves<true>(move_list);
    Move best_move                    = move_list[0];
    best_moves[STARTING_SEARCH_DEPTH] = best_move;
    for (int depth = STARTING_SEARCH_DEPTH + 1; depth != MAX_PLY; ++depth)
    {
        if (game.time_control_.clock_type == CLOCK_FIXED_DEPTH && depth > game.time_control_.fixed_depth.depth)
        {
            break;
        }
        SortMoves<true>(move_list); /* Sort moves from previous iteration depth. */
        Variation child_pv{};
        int       alpha  = ALPHA;
        game.node_count_ = 0;
        for (std::size_t i = 0; i != move_list.size(); ++i)
        {
            const int score = SearchSingleMove(game, depth, 0, alpha, BETA, move_list[i], child_pv, i);
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
                CopyVariation(principal_variation, child_pv, best_move);
                if (game.do_show_thinking_)
                {
                    const string move_string{game.CurrentPosition().VariationToString(principal_variation)};
                    printf("%2u %5d %4u %8u %s\n", depth, best_moves[depth].score(), (ElapsedMilliseconds() - start_ms) / 10,
                           game.node_count_, move_string.c_str());
                }
            }
        }
        int stop_ms = ElapsedMilliseconds();
        if (alpha > WIN_THRESHOLD || alpha < LOSE_THRESHOLD)
        {
            break;
        }
        if (game.time_control_.clock_type == CLOCK_STANDARD || game.time_control_.clock_type == CLOCK_INCREMENTAL)
        {
            /*
            Plan our use of the time with some care. If both the score we find
            and the best move are consistent between successive iterations,
            we can probably stop searching before our allocated time is elapsed
            and save some time for other, potentially more difficult positions.
            Similarly we can save a smaller amount of time if either of these
            conditions but not both are true.
            */
            const int elapsed_ms              = stop_ms - start_ms;
            bool      is_best_move_consistent = true;
            bool      is_score_stable         = true;
            for (int i = STARTING_SEARCH_DEPTH; i != depth; ++i)
            {
                if (best_moves[i] != best_moves[depth])
                {
                    is_best_move_consistent = false;
                }
                if (abs(best_moves[i].score() - best_moves[depth].score()) > SCORE_INSTABILITY_THRESHOLD)
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

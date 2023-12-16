#include "search.h"
#include "position.h"
#include "debug_hashtable.h"
#include "transposition_table.h"
#include "function_prototypes.h"
#include "game.h"
#include "opening_book.h"

#include <string>
using std::string;

/**
 * @brief Search the root node and find the best move
 * @param src_position the position to search
 * @return the best move found
*/
Move SearchRootNode(Game& game)
{
    /* If there is a book move for this position, do not bother with search. */
    Move book_move = GetBookMove(game.position_->hash_);
    if (book_move)
    {
        return book_move;
    } 
    MoveList move_list = game.position_->GenerateLegalMoves();
    int num_legal_moves = move_list.size();
    /* If there is only 1 legal move available, no point wasting time searching, just play it. */
    if (num_legal_moves == 0)
    {
        return 0;
    }
    if (num_legal_moves == 1)
    {
        return move_list[0];
    }  
    /* Plan time usage for this search. */
    int timeout_ms      = 0;    /* cancel search when this time expires             */
    int ms_allocated    = 0;    /* soft allocated time for the move                 */
    int moves_to_go     = 0;    /* number of remaining moves in this clock period   */
    switch (game.time_control_.clock_type)
    {
    case CLOCK_STANDARD:
    default:
        moves_to_go     = game.time_control_.standard.moves_per_period - (game.position_->full_move_count_ % game.time_control_.standard.moves_per_period);
        ms_allocated    = game.time_control_.standard.milliseconds_remaining / moves_to_go;
        timeout_ms      = max(100, min(ms_allocated * 2, game.time_control_.standard.milliseconds_remaining - 3000));
        game.time_control_.hard_stop_search_ms = GetMilliseconds() + timeout_ms; /* stop searching regardless when this elapses */
        break; 
    
    case CLOCK_FIXED_DEPTH:
        timeout_ms     = 0;
        ms_allocated   = 0;
		game.time_control_.hard_stop_search_ms = 0;
        break;
    
    case CLOCK_FIXED_TIME:
        timeout_ms = game.time_control_.fixed_time.milliseconds;
		game.time_control_.hard_stop_search_ms = GetMilliseconds() + timeout_ms;
        ms_allocated = 0;
        break;
    
    case CLOCK_INCREMENTAL:
        ms_allocated   = game.time_control_.incremental.increment_milliseconds + (game.time_control_.incremental.milliseconds_remaining / 30);
        timeout_ms     = max(100, min(ms_allocated * 2, game.time_control_.incremental.milliseconds_remaining - 3000));
		game.time_control_.hard_stop_search_ms = GetMilliseconds() + timeout_ms;
        break;
    }
    InitializeGoodMoveCounts();
    DEBUG_STATEMENT(DebugXClear());
    int start_ms = GetMilliseconds();
    game.is_cancel_pending_ = false;
    /*
    For first pass move ordering before we do any search, just use a shallow
    search with wide open alpha beta window. Subsequent passes will use the
    results of the previous iteration to sort the moves (the merge sort is
    stable).
    */
    Variation principal_variation {};
    Move best_moves[MAX_PLY]; /* Best move found at each ply of search. */
    for (int i = 0; i != num_legal_moves; ++i)
    {
        const int score = SearchSingleMove(game, STARTING_SEARCH_DEPTH, 0, ALPHA, BETA, move_list[i], principal_variation, i);
        move_list[i] = ScoredMove(move_list[i], score);
    }   
    SortMoves(move_list, true);
    Move best_move = move_list[0];
    best_moves[STARTING_SEARCH_DEPTH] = best_move;
    
    for (int depth = STARTING_SEARCH_DEPTH + 1; depth != MAX_PLY; ++depth)
    {       
        if (game.time_control_.clock_type == CLOCK_FIXED_DEPTH && depth > game.time_control_.fixed_depth.depth)
        {
            break;
        }
        SortMoves(move_list, true); /* Sort moves from previous iteration depth. */
        Variation child_pv {};
        int alpha = ALPHA;
        game.node_count_ = 0;
        for (int i = 0; i != num_legal_moves; ++i)
        {
            const int score = SearchSingleMove(game, depth, 0, alpha, BETA, move_list[i], child_pv, i);
            move_list[i] = ScoredMove(move_list[i], score);
            if (game.is_cancel_pending_)
            {
                return best_move;
            }
            if (score > alpha)
            {
                alpha             = score;
                best_move         = move_list[i];
                best_moves[depth] = move_list[i];
                RecordTransposition(game.position_->hash_, depth, alpha, best_move, NODE_PV);
                CopyVariation(principal_variation, child_pv, best_move);
                if (game.do_show_thinking_)
                {
                    string move_string { game.position_->VariationToString(principal_variation) };
                    printf("%2u %5d %4u %8u %s\n", depth, MoveScore(best_moves[depth]), (GetMilliseconds() - start_ms) / 10, game.node_count_, move_string.c_str());
                }
            }
        }        
        int stop_ms = GetMilliseconds();
        if (alpha > WIN_THRESHOLD || 
            alpha < LOSE_THRESHOLD)
        {
            break;
        }
        if (game.time_control_.clock_type == CLOCK_STANDARD || 
            game.time_control_.clock_type == CLOCK_INCREMENTAL)
        {
            /* 
            Plan our use of the time with some care. If both the score we find
            and the best move are consistent between successive iterations, 
            we can probably stop searching before our allocated time is elapsed 
            and save some time for other, potentially more difficult positions. 
            Similarly we can save a smaller amount of time if either of these 
            conditions but not both are true.
            */
            const int elapsed_ms = stop_ms - start_ms;
            bool is_best_move_consistent = true;
            bool is_score_stable = true;
            for (int i = STARTING_SEARCH_DEPTH; i != depth; ++i)
            {
                if (MoveBits(best_moves[i]) != MoveBits(best_moves[depth]))
                {
                    is_best_move_consistent = false;
                }
                if (abs(MoveScore(best_moves[i]) - MoveScore(best_moves[depth])) > SCORE_INSTABILITY_THRESHOLD)
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

#include "pawnstar.h"

static volatile bool cancel;
/*
If the worker thread is running, set the cancel flag then wait for it to
finish
*/
void StopThinkingMoveImmediately()
{
    cancel = true;
}

#define max(a,b)             \
({                           \
    __typeof__ (a) _a = (a); \
    __typeof__ (b) _b = (b); \
    _a > _b ? _a : _b;       \
})

#define min(a,b)             \
({                           \
    __typeof__ (a) _a = (a); \
    __typeof__ (b) _b = (b); \
    _a < _b ? _a : _b;       \
})

/**
 * @brief Search the root node and find the best move
 * @param src_position the position to search
 * @return the best move found
*/
int SearchRootNode(const Position* src_position)
{
    if (src_position->state_flags & IS_GAME_OVER)
    {
        return 0;
    }
    /* If there is a book move for this position, do not bother with search. */
    Move book_move = GetBookMove(src_position->hash);
    if (book_move)
    {
        return book_move;
    } 
    Move moves[MAX_MOVES_PER_POSITION];
    int move_count = GenerateLegalMoves(src_position, moves);
    /* If there is only 1 legal move available, no point wasting time searching, just play it. */
    if (move_count == 1)
    {
        return moves[0];
    }  
    /* Plan time usage for this search. */
    int timeout_ms      = 0;    /* cancel search when this time expires             */
    int ms_allocated    = 0;    /* soft allocated time for the move                 */
    int moves_to_go     = 0;    /* number of remaining moves in this clock period   */
    switch (the_game.time_control.clock_type)
    {
    case CLOCK_STANDARD:
    default:
        moves_to_go     = the_game.time_control.standard.moves_per_period - (src_position->full_move_count % the_game.time_control.standard.moves_per_period);
        ms_allocated    = the_game.time_control.standard.milliseconds_remaining / moves_to_go;
        timeout_ms      = max(100, min(ms_allocated * 2, the_game.time_control.standard.milliseconds_remaining - 3000));
        the_game.time_control.hard_stop_search_ms = GetMilliseconds() + timeout_ms; /* stop searching regardless when this elapses */
        break; 
    
    case CLOCK_FIXED_DEPTH:
        timeout_ms     = 0;
        ms_allocated   = 0;
		the_game.time_control.hard_stop_search_ms = 0;
        break;
    
    case CLOCK_FIXED_TIME:
        timeout_ms = the_game.time_control.fixed_time.milliseconds;
		the_game.time_control.hard_stop_search_ms = GetMilliseconds() + timeout_ms;
        ms_allocated = 0;
        break;
    
    case CLOCK_INCREMENTAL:
        ms_allocated   = the_game.time_control.incremental.increment_milliseconds + (the_game.time_control.incremental.milliseconds_remaining / 30);
        timeout_ms     = max(100, min(ms_allocated * 2, the_game.time_control.incremental.milliseconds_remaining - 3000));
		the_game.time_control.hard_stop_search_ms = GetMilliseconds() + timeout_ms;
        break;
    }
    InitializeGoodMoveCounts();
    DEBUG_STATEMENT(DebugXClear());
    int start_ms = GetMilliseconds();
    cancel = false;
    /*
    For first pass move ordering before we do any search, just use a shallow
    search with wide open alpha beta window. Subsequent passes will use the
    results of the previous iteration to sort the moves (the merge sort is
    stable).
    */
    Move best_moves[MAX_PLY];
    for (int i = 0; i != move_count; ++i)
    {
        const int score = SearchSingleMove(src_position, STARTING_SEARCH_DEPTH, 0, ALPHA, BETA, &cancel, moves[i], NULL, i);
        moves[i] = SCORED_MOVE(moves[i], score);
    }   
    MergeSort(move_count, moves);
    Move best_move = moves[0];
    Variation principal_variation = { 0 };
    for (int depth = STARTING_SEARCH_DEPTH + 1; depth != MAX_PLY; ++depth)
    {       
        if (the_game.time_control.clock_type == CLOCK_FIXED_DEPTH && depth > the_game.time_control.fixed_depth.depth)
        {
            break;
        }
        the_game.node_count = 0;
        MergeSort(move_count, moves);
        Variation child_pv = { 0 };
        int alpha = ALPHA;
        for (int i = 0; i != move_count; ++i)
        {      
            int score;    
            if (i == 0)
            {
                score = SearchSingleMove(src_position, depth, 0, alpha, BETA, &cancel, moves[i], &child_pv, i);
                moves[i] = SCORED_MOVE(moves[i], score);
            }
            else
            {
                INCREMENT("pvs root node attempts");
                score = SearchSingleMove(src_position, depth, 0, alpha, alpha + 1, &cancel, moves[i], &child_pv, i);
                moves[i] = SCORED_MOVE(moves[i], score);
                if (score > alpha)
                {
                    INCREMENT("pvs root node fails");
                    score = SearchSingleMove(src_position, depth, 0, alpha, BETA, &cancel, moves[i], &child_pv, i);
                    moves[i] = SCORED_MOVE(moves[i], score);
                }
            }            
            if (cancel)
            {
                return best_move;
            }
            if (score > alpha)
            {
                alpha             = score;
                best_move         = moves[i];
                best_moves[depth] = moves[i];
                RecordTransposition(src_position->hash, depth, alpha, best_move, NODE_PV);
                CopyVariation(&principal_variation, &child_pv, best_move);
                if (the_game.do_show_thinking)
                {
                    char move_string[256];
                    MoveSequenceToSanString(src_position, principal_variation.moves, move_string);
                    printf("%2u %5d %4u %8u %s\n", depth, (int)MOVE_SCORE(best_moves[depth]), (GetMilliseconds() - start_ms) / 10, the_game.node_count, move_string);
                }
            }
        }        
        int stop_ms = GetMilliseconds();
        if (alpha > WIN_THRESHOLD || 
            alpha < LOSE_THRESHOLD)
        {
            break;
        }
        if (the_game.time_control.clock_type == CLOCK_STANDARD || 
            the_game.time_control.clock_type == CLOCK_INCREMENTAL)
        {
            /* 
            Plan our use of the time with some care. If both the score we find and the best move are consistent
            between successive iterations, we can probably stop searchning before our allocated time is elapsed and
            save the time for other, more difficult positions. Similarly we can save a smaller amount of time if either
            of these but not both are true.
            */
            const int elapsed_ms = stop_ms - start_ms;
            bool is_best_move_consistent = true;
            bool is_score_stable = true;
            for (int i = STARTING_SEARCH_DEPTH; i != depth; ++i)
            {
                if (MOVE_BITS(best_moves[i]) != MOVE_BITS(best_moves[depth]))
                {
                    is_best_move_consistent = false;
                }
                if (abs(MOVE_SCORE(best_moves[i]) - MOVE_SCORE(best_moves[depth])) > SCORE_INSTABILITY_THRESHOLD)
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

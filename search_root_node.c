#include "pawnstar.h"

#define MIN(x,y) ((x) < (y) ? (x) : (y))
#define MAX(x,y) ((x) > (y) ? (x) : (y))
#define ABS(x)   ((x) < 0 ? -(x) : (x))

static volatile bool cancel;
/******************************************************************************
If the worker thread is running, set the cancel flag then wait for it to
finish
*******************************************************************************/
void StopThinkingMoveImmediately()
{
    cancel = true;
}
/******************************************************************************
Entry point for the search to find the computer move.
Returns the best move found, or 0 if there is no move (i.e. game over)
*******************************************************************************/
int SearchRootNode(const Position* src_position)
{
    int         i;
    ScoredMove  scored_moves[MAX_MOVES_PER_POSITION];
    ScoredMove  best_move_at_each_iteration[MAX_PLY];
    int         moves[MAX_MOVES_PER_POSITION];
    int         principal_variation[MAX_PLY];
    int         move_count;
    int         depth;
    int         start_ms;
    int         stop_ms;
    int         book_move;
    int         best_move = 0;
    int         timeout_ms;
    int         ms_allocated;
    int         moves_to_go;
    int         alpha;

    if (src_position->state_flags & IS_GAME_OVER)
    {
        return 0;
    }
    if ((book_move = GetBookMove(src_position->hash)) != 0)
    {
        return book_move;
    } 
    move_count = GenerateLegalMoves(src_position, moves);
    if (move_count <= 1)
    {
        return moves[0]; /* If there is only 1 legal move available just play it */
    }  
    switch (globals->time_control.clock_type)
    {
    case STANDARD_CHESS_CLOCK:
    default:
        moves_to_go  = globals->time_control.moves_per_period - (src_position->full_move_count % globals->time_control.moves_per_period);
        ms_allocated = globals->time_control.milliseconds_remaining / moves_to_go;
        timeout_ms   = MAX(100, MIN(ms_allocated * 3, globals->time_control.milliseconds_remaining - 3000));
        break;
    
    case FIXED_DEPTH:
        timeout_ms   = 0;
        ms_allocated = 0;
        break;
    
    case FIXED_TIME:
        timeout_ms   = globals->time_control.fixed_milliseconds;
        ms_allocated = 0;
        break;
    
    case INCREMENTAL_CLOCK:
        ms_allocated = globals->time_control.increment_milliseconds + (globals->time_control.milliseconds_remaining / 20);
        timeout_ms   = MAX(100, MIN(ms_allocated * 3, globals->time_control.milliseconds_remaining - 3000));
        break;
    }
    InitializeGoodMoveCounts();
    /**************************************************************************
    For first pass move ordering before we do any search, just use a depth 1 
    search with wide open alpha beta window. Subsequent passes will use the 
    results of the previous iteration to sort the moves (the merge sort is 
    stable).
    ***************************************************************************/
    for (i = 0; i != move_count; ++i)
    {
        scored_moves[i].move  = moves[i];
        scored_moves[i].score = SearchSingleMove(src_position, 1, 0, ALPHA, BETA, moves[i], i, false, &cancel);
    }
    DEBUG_STATEMENT(DebugXClear());
    start_ms = GetMilliseconds();
    cancel = false;
    if (timeout_ms)
    {
        SetStopThinkingTimer(timeout_ms, &cancel);
    }
    for (depth = 1; depth != MAX_PLY; ++depth)
    {       
        if (globals->time_control.clock_type == FIXED_DEPTH && depth > globals->time_control.fixed_depth)
        {
            break;
        }
        globals->node_count = 0;
        alpha = ALPHA;
        MergeSort(move_count, scored_moves);
        for (i = 0; i != move_count; ++i)
        {
            scored_moves[i].score = SearchSingleMove(src_position, depth, 0, alpha, BETA, scored_moves[i].move, i, false, &cancel);
            if (cancel)
            {
                return best_move;
            }
            if (scored_moves[i].score > alpha)
            {
                alpha                                    = scored_moves[i].score;
                best_move                                = scored_moves[i].move;
                best_move_at_each_iteration[depth].move  = scored_moves[i].move;
                best_move_at_each_iteration[depth].score = scored_moves[i].score;
                RecordGoodMove(0, scored_moves[i].move);
                RecordPrincipalVariationMove(src_position->hash, scored_moves[i].move);
                RecordTransposition(src_position->hash, depth, scored_moves[i].score, scored_moves[i].move, NODE_PV);            
            }
        }        
        stop_ms = GetMilliseconds();
        FindPrincipalVariation(src_position, principal_variation);
        if (principal_variation[0] == 0)
        {
            principal_variation[0] = best_move;
            principal_variation[1] = 0;
        }
        if (globals->do_show_thinking)
        {
            char move_string[256];
            MoveSequenceToSanString(src_position, principal_variation, move_string);
            printf("%2u %5d %4u %8u %s\n", depth, best_move_at_each_iteration[depth].score, (stop_ms - start_ms) / 10, globals->node_count, move_string);
        }
        if (alpha > WIN_THRESHOLD || 
            alpha < LOSE_THRESHOLD)
        {
            break;
        }
        if (globals->time_control.clock_type == STANDARD_CHESS_CLOCK || 
            globals->time_control.clock_type == INCREMENTAL_CLOCK)
        {
            const int elapsed_ms = stop_ms - start_ms;
            bool is_best_move_consistent = true;
            bool is_score_stable = true;
            for (i = 1; i != depth; ++i)
            {
                if (best_move_at_each_iteration[i].move != best_move_at_each_iteration[depth].move)
                {
                    is_best_move_consistent = false;
                }
                if (ABS(best_move_at_each_iteration[i].score - best_move_at_each_iteration[depth].score) > SCORE_INSTABILITY_THRESHOLD)
                {
                    is_score_stable = false;
                }
            }
            if ((is_best_move_consistent && is_score_stable) && (elapsed_ms * 9) > ms_allocated)
            {
                break;
            }
            if ((is_best_move_consistent || is_score_stable) && (elapsed_ms * 3) > ms_allocated)
            {
                break;
            }
            if (elapsed_ms > ms_allocated)
            {
                break;
            }
        }             
    }  
    CancelStopThinkingTimer();
    return best_move;
}

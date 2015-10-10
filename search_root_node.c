#include "pawnstar.h"

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
    ScoredMove  best_moves[MAX_PLY];
    int         moves[MAX_MOVES_PER_POSITION];
    Variation   pv;
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

    pv.num_moves = 0;
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
    case CLOCK_STANDARD:
    default:
        moves_to_go  = globals->time_control.moves_per_period - (src_position->full_move_count % globals->time_control.moves_per_period);
        ms_allocated = globals->time_control.milliseconds_remaining / moves_to_go;
        timeout_ms   = max(100, min(ms_allocated * 4, globals->time_control.milliseconds_remaining - 3000));
        break;
    
    case CLOCK_FIXED_DEPTH:
        timeout_ms   = 0;
        ms_allocated = 0;
        break;
    
    case CLOCK_FIXED_TIME:
        timeout_ms   = globals->time_control.fixed_milliseconds;
        ms_allocated = 0;
        break;
    
    case CLOCK_INCREMENTAL:
        ms_allocated = globals->time_control.increment_milliseconds + (globals->time_control.milliseconds_remaining / 30);
        timeout_ms   = max(100, min(ms_allocated * 4, globals->time_control.milliseconds_remaining - 3000));
        break;
    }
    InitializeGoodMoveCounts();
    /**************************************************************************
    For first pass move ordering before we do any search, just use a shallow
    search with wide open alpha beta window. Subsequent passes will use the 
    results of the previous iteration to sort the moves (the merge sort is 
    stable).
    ***************************************************************************/
    DEBUG_STATEMENT(DebugXClear());
    start_ms = GetMilliseconds();
    cancel = false;
    for (i = 0; i != move_count; ++i)
    {
        scored_moves[i].move  = moves[i];
        scored_moves[i].score = SearchSingleMove(src_position, STARTING_SEARCH_DEPTH, 0, ALPHA, BETA, &cancel, SEARCH_FLAG_ROOT, scored_moves[i].move, NULL);    
    }   
    MergeSort(move_count, scored_moves);
    best_move = scored_moves[0].move;
    if (timeout_ms)
    {
        CancelStopThinkingTimer();
        SetStopThinkingTimer(timeout_ms, &cancel);
    }
    for (depth = STARTING_SEARCH_DEPTH; depth != MAX_PLY; ++depth)
    {       
        if (globals->time_control.clock_type == CLOCK_FIXED_DEPTH && depth > globals->time_control.fixed_depth)
        {
            break;
        }
        globals->node_count = 0;
        alpha = ALPHA;
        MergeSort(move_count, scored_moves);
        pv.num_moves = 0;
        for (i = 0; i != move_count; ++i)
        {          
            if (i < NUM_ROOT_MOVES_BEFORE_PVS)
            {
                scored_moves[i].score = SearchSingleMove(src_position, depth, 0, alpha, BETA, &cancel, SEARCH_FLAG_ROOT, scored_moves[i].move, &pv);
            }
            else
            {
                INCREMENT("pvs root node attempts");
                scored_moves[i].score = SearchSingleMove(src_position, depth, 0, alpha, alpha + 1, &cancel, SEARCH_FLAG_ROOT, scored_moves[i].move, &pv);
                if (scored_moves[i].score > alpha)
                {
                    INCREMENT("pvs root node fails");
                    scored_moves[i].score = SearchSingleMove(src_position, depth, 0, alpha, BETA, &cancel, SEARCH_FLAG_ROOT, scored_moves[i].move, &pv);
                }
            }            
            if (cancel)
            {
                return best_move;
            }
            if (scored_moves[i].score > alpha)
            {
                alpha             = scored_moves[i].score;
                best_move         = scored_moves[i].move;
                best_moves[depth] = scored_moves[i];
                RecordTransposition(src_position->hash, depth, alpha, best_move, NODE_PV);            
            }
        }        
        stop_ms = GetMilliseconds();
        if (globals->do_show_thinking)
        {
            char move_string[256];
            int move_sequence[MAX_PLY];
            move_sequence[0] = best_move;
            memcpy(&move_sequence[1], pv.moves, pv.num_moves * sizeof(int));
            move_sequence[pv.num_moves + 1] = 0;
            MoveSequenceToSanString(src_position, move_sequence, move_string);
            printf("%2u %5d %4u %8u %s\n", depth, best_moves[depth].score, (stop_ms - start_ms) / 10, globals->node_count, move_string);
        }
        if (alpha > WIN_THRESHOLD || 
            alpha < LOSE_THRESHOLD)
        {
            break;
        }
        if (globals->time_control.clock_type == CLOCK_STANDARD || 
            globals->time_control.clock_type == CLOCK_INCREMENTAL)
        {
            const int elapsed_ms = stop_ms - start_ms;
            bool is_best_move_consistent = true;
            bool is_score_stable = true;
            for (i = STARTING_SEARCH_DEPTH; i != depth; ++i)
            {
                if (best_moves[i].move != best_moves[depth].move)
                {
                    is_best_move_consistent = false;
                }
                if (abs(best_moves[i].score - best_moves[depth].score) > SCORE_INSTABILITY_THRESHOLD)
                {
                    is_score_stable = false;
                }
            }
            if ((is_best_move_consistent && is_score_stable) && (elapsed_ms * 4) > ms_allocated)
            {
                break;
            }
            if ((is_best_move_consistent || is_score_stable) && (elapsed_ms * 2) > ms_allocated)
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

#include "chess_clock.h"
#include "debug_hashtable.h"
#include "evaluation.h"
#include "game.h"
#include "position.h"
#include "search.h"
#include "search_state.h"
#include "static_exchange_evaluation.h"
#include "transposition_table.h"

#include <latch>
#include <mutex>
#include <utility>

/// @brief Search a single move and return its alpha-beta score and whether it gives check.
/// @param state Per-thread search state.
/// @param depth Depth to search to.
/// @param ply Distance from root node.
/// @param alpha Alpha value.
/// @param beta Beta value.
/// @param move Move to search.
/// @param pv Parent's principal variation.
/// @param move_index Move number (0 is first move).
/// @return Score for this move, and whether move gives check.
SingleMoveResult SearchSingleMove(SearchState &state, int depth, int ply, int alpha, int beta, Move move, Variation &pv,
                                  int move_index)
{
    const Position &position     = state.CurrentPosition();
    const bool      was_in_check = position.IsInCheck();

    int child_depth = depth - 1;
    switch (move.type())
    {
    case Move::Type::kPromotionKnight:
    case Move::Type::kPromotionBishop:
    case Move::Type::kPromotionRook:
    case Move::Type::kPromotionQueen:
        ++child_depth;
        INCREMENT("extensions promotion");
        break;

    case Move::Type::kEpCapture:
        ++child_depth;
        INCREMENT("extensions ep capture");
        break;

    default:
        break;
    }
    state.PlayMove(move);
    const bool is_checking = state.CurrentPosition().IsInCheck();
    int        score;
    if (beta > alpha + 1 && move_index > 0 && !was_in_check && !is_checking)
    {
        // Try principal variation search.
        INCREMENT("pvs attempts");
        score = -Search(state, child_depth, ply + 1, -alpha - 1, -alpha, pv);
        if (score > alpha)
        {
            INCREMENT("pvs fails");
            score = -Search(state, child_depth, ply + 1, -beta, -alpha, pv);
        }
    }
    else
    {
        score = -Search(state, child_depth, ply + 1, -beta, -alpha, pv);
    }
    state.UndoMove();
    return {score, is_checking};
}

/// @brief Result of a null-move pruning attempt: the null-move score and the static evaluation.
struct NullMoveResult
{
    int score;      ///< Score after null move (or alpha if null move not tried).
    int eval_score; ///< Static evaluation (or kAlpha if not computed).
};

/// @brief Try null move pruning.
/// @param state Current search state.
/// @param depth Current search depth.
/// @param ply Distance from root node.
/// @param alpha Alpha value.
/// @param beta Beta value.
/// @return Null move score plus evaluation score (or kAlpha if evaluation was not called).
static inline NullMoveResult AttemptNullMove(SearchState &state, int depth, int ply, int alpha, int beta)
{
    const Position &position = state.CurrentPosition();
    const Color     color    = position.ColorToMove();
    // Only try null move pruning if all conditions are met.
    if (!position.IsNullMove() &&                     // previous move was not a null move
        !position.IsInCheck() &&                      // we are not in check
        beta == alpha + 1 &&                          // this is not a PV node
        position.PiecesOfColor(color).PopCount() > 3) // we have at least 4 friendly pieces
    {
        const int eval_score = EvaluatePosition(state, alpha, beta);
        if (eval_score >= beta)
        {
            INCREMENT("null move");
            Variation dummy{};
            state.MakeNullMove();
            int score = -Search(state, depth - 3, ply + 1, -beta, -alpha, dummy);
            state.UndoMove();
            if (state.IsCancelled())
            {
                return {kSearchCancelledScore, eval_score};
            }
            if (score >= beta)
            {
                return {beta, eval_score};
            }
            INCREMENT("null move fails");
            return {alpha, eval_score};
        }
    }
    return {alpha, kAlpha};
}

/// @brief Work descriptor for one move searched in the parallel (YBW) phase.
/// Instances live in the dispatching node's stack frame for the duration of the batch (the node
/// blocks on a latch until every worker finishes), so the thread pool can reference them without
/// owning them. The worker writes its result back into @c score.
struct ParallelJob
{
    SearchState *worker;     ///< Worker search state running this move.
    Move         move;       ///< Move to search.
    int          move_index; ///< Move number for ordering heuristics.
    int          lmr_depth;  ///< Reduced search depth (may equal full depth).
    int          depth;      ///< Full search depth.
    int          ply;        ///< Distance from root.
    int          alpha;      ///< Alpha bound at dispatch time.
    int          beta;       ///< Beta bound at dispatch time.
    std::latch  *latch;      ///< Batch completion latch, counted down when done.
    int          score;      ///< Output: alpha-beta score for this move.
};

/// @brief Thread-pool trampoline: search one move on its worker state and record the result.
/// @param arg Pointer to a ParallelJob owned by the dispatching node.
static void RunParallelJob(void *arg)
{
    ParallelJob &job = *static_cast<ParallelJob *>(arg);
    Variation    pv;
    int          score =
        SearchSingleMove(*job.worker, job.lmr_depth, job.ply, job.alpha, job.beta, job.move, pv, job.move_index).score;
    // Re-search at full depth if LMR was applied and the move beat alpha.
    if (!job.worker->IsCancelled() && score > job.alpha && job.lmr_depth < job.depth)
    {
        score =
            SearchSingleMove(*job.worker, job.depth, job.ply, job.alpha, job.beta, job.move, pv, job.move_index).score;
    }
    if (score >= job.beta)
    {
        job.worker->SignalBatchCutoff();
        job.worker->game.history_table.RecordGoodMove(job.ply, job.move);
        if (job.move.IsQuiet())
        {
            job.worker->RecordKiller(job.ply, job.move);
        }
    }
    job.score = score;
    // Return the slot before counting down so it is available immediately.
    Game &g = job.worker->game;
    g.state_pool.release(job.worker);
    job.latch->count_down();
}

/// @brief Alpha-beta main search algorithm.
/// This is the primary recursive search function used to find the best move.
/// Implements Young Brothers Wait (YBW): after 2 sequential moves at a null-window node,
/// remaining moves are dispatched to the thread pool in parallel.
/// @param state Per-thread search state.
/// @param depth Depth to search.
/// @param ply Distance from root node.
/// @param alpha Alpha value.
/// @param beta Beta value.
/// @param parent_pv Principal variation at the parent node.
/// @return The alpha-beta score for this node.
int Search(SearchState &state, int depth, int ply, int alpha, int beta, Variation &parent_pv)
{
    INCREMENT("alpha beta calls");
    if ((++state.node_count & 0xFFFF) == 0 && state.game.time_control.hard_stop_ms != 0 &&
        state.game.time_control.ElapsedMilliseconds() >= state.game.time_control.hard_stop_ms)
    {
        state.game.is_cancel_pending.store(true, std::memory_order_relaxed);
        return kSearchCancelledScore;
    }
    if (state.CurrentPosition().IsDrawByMaterial() || state.IsDrawByFiftyMoves() || state.IsDrawByRepetition())
    {
        return kDrawScore;
    }
    if (state.CurrentPosition().IsInCheck())
    {
        INCREMENT("checks");
        ++depth;
    }
    if (ply == kMaxPly)
    {
        INCREMENT("max ply reached");
        return EvaluatePosition(state, alpha, beta);
    }
    // Determine if there is an entry in the transposition table for this position.
    const auto transposition = state.game.transposition_table.FindTransposition(state.CurrentPosition().Hash());
    if (transposition && transposition->depth() >= depth)
    {
        switch (transposition->node_type())
        {
        case Transposition::NodeType::kCut:
            INCREMENT("table hit cut node");
            if (transposition->score() >= beta)
            {
                INCREMENT("table hit cut node cutoffs");
                return transposition->score();
            }
            break;

        case Transposition::NodeType::kAll:
            INCREMENT("table hit all node");
            if (transposition->score() < alpha)
            {
                INCREMENT("table hit all node cutoffs");
                return transposition->score();
            }
            break;

        case Transposition::NodeType::kPv:
            INCREMENT("table hit pv node");
            ++depth;
            break;
        }
    }
    // Can we go directly to the quiescence search?
    if (depth <= 0 && !state.CurrentPosition().IsInCheck())
    {
        return SearchQuiescent(state, depth, ply, alpha, beta);
    }
    // Try null move pruning.
    auto [null_move_score, eval_score] = AttemptNullMove(state, depth, ply, alpha, beta);
    if (state.IsCancelled())
    {
        return kSearchCancelledScore;
    }
    if (null_move_score >= beta)
    {
        return null_move_score;
    }

    // Try the transposition table move first.
    Variation pv;
    Move      best_move        = Move::None();
    int       best_score       = kAlpha;
    bool      has_raised_alpha = false;
    if (transposition && transposition->move != Move::None())
    {
        INCREMENT("table move");
        best_move       = transposition->move;
        const int score = SearchSingleMove(state, depth, ply, alpha, beta, transposition->move, pv, 0).score;
        if (state.IsCancelled())
        {
            return kSearchCancelledScore;
        }
        if (score >= beta)
        {
            INCREMENT("table move beta cutoffs");
            state.game.transposition_table.RecordTransposition(Transposition{
                state.CurrentPosition().Hash(), transposition->move, score, depth, Transposition::NodeType::kCut});
            state.game.history_table.RecordGoodMove(ply, transposition->move);
            if (transposition->move.IsQuiet())
            {
                state.RecordKiller(ply, transposition->move);
            }
            return score;
        }
        best_score = score;
        if (score > alpha)
        {
            INCREMENT("table move raised alpha");
            alpha            = score;
            has_raised_alpha = true;
            state.game.history_table.RecordGoodMove(ply, transposition->move);
        }
    }

    MoveList move_list{state.CurrentPosition().GenerateLegalMoves()};
    if (move_list.size() == 0)
    {
        if (state.CurrentPosition().IsInCheck())
        {
            INCREMENT("checkmates");
            return kCheckmatedScore + ply;
        }
        INCREMENT("stalemates");
        return kDrawScore;
    }
    state.ScoreAndSortMoves(move_list, ply);

    // ── Sequential phase ─────────────────────────────────────────────────────
    // Search moves one at a time until we get a cutoff or have searched 2 moves
    // at a null-window node, at which point we switch to the parallel phase.
    int        sequential_count  = 0;
    int        parallel_start_mi = -1; // move_list index where the parallel phase begins
    const bool in_check_seq      = state.CurrentPosition().IsInCheck();

    for (const Move &move : move_list)
    {
        const int move_index = (int)(&move - move_list.begin());
        if (transposition && move == transposition->move)
        {
            continue;
        }

        // Late move reduction: reduce quiet, non-check, late moves at null-window nodes.
        int lmr_depth = depth;
        if (beta == alpha + 1 && move_index > 3 && !in_check_seq && depth > 2 && move.captured() == Piece::kNone &&
            move.piece() != kPawn)
        {
            INCREMENT("late move reduction");
            lmr_depth = depth - 1;
            if (depth > 3 && move_index > 6)
            {
                --lmr_depth;
                INCREMENT("late move reduction extreme");
            }
        }

        int score = SearchSingleMove(state, lmr_depth, ply, alpha, beta, move, pv, move_index).score;
        if (score > alpha && lmr_depth < depth)
        {
            INCREMENT("late move reduction fails");
            score = SearchSingleMove(state, depth, ply, alpha, beta, move, pv, move_index).score;
        }
        if (state.IsCancelled())
        {
            return kSearchCancelledScore;
        }
        if (score >= beta)
        {
            INCREMENT("beta cutoffs");
            state.game.transposition_table.RecordTransposition(
                Transposition{state.CurrentPosition().Hash(), move, score, depth, Transposition::NodeType::kCut});
            state.game.history_table.RecordGoodMove(ply, move);
            if (move.IsQuiet())
            {
                state.RecordKiller(ply, move);
            }
            return score;
        }
        if (score > best_score)
        {
            INCREMENT("best score changed");
            best_score = score;
            best_move  = move;
            if (score > alpha)
            {
                INCREMENT("pv changed");
                alpha            = score;
                has_raised_alpha = true;
                state.game.history_table.RecordGoodMove(ply, move);
            }
        }

        ++sequential_count;

        // After 2 sequential searches at a null-window node, hand off remaining
        // moves to the thread pool (Young Brothers Wait).  Only the main thread
        // (batch_cutoff == nullptr) spawns workers; workers never sub-parallelize.
        if (sequential_count >= 2 && state.batch_cutoff == nullptr && beta == alpha + 1 &&
            state.game.thread_pool.idle_count() > 0)
        {
            parallel_start_mi = move_index + 1;
            break;
        }
    }

    // ── Parallel phase (YBW) ─────────────────────────────────────────────────
    if (parallel_start_mi >= 0 && !state.IsCancelled())
    {
        StackList<ParallelJob, kMaxMovesPerPosition> batch;

        // Collect remaining non-TT moves and compute their LMR depths.
        // (Parallel phase is only entered at null-window nodes, so beta == alpha+1 is always true here.)
        for (int mi = parallel_start_mi; mi < (int)move_list.size(); ++mi)
        {
            const Move &mv = move_list[mi];
            if (transposition && mv == transposition->move)
            {
                continue;
            }
            // Late move reduction (this batch is always a null-window node).
            int lmr_d = depth;
            if (mi > 3 && !in_check_seq && depth > 2 && mv.captured() == Piece::kNone && mv.piece() != kPawn)
            {
                lmr_d = depth - 1;
                if (depth > 3 && mi > 6)
                {
                    --lmr_d;
                }
            }
            batch.push_back({nullptr, mv, mi, lmr_d, depth, ply, alpha, beta, nullptr, 0});
        }

        if (batch.size() != 0)
        {
            INCREMENT("parallel batches");
            std::atomic<bool> batch_cutoff{false};
            std::latch        latch{(std::ptrdiff_t)batch.size()};

            // Dispatch each move to a worker SearchState on the thread pool. Each job lives in `batch`
            // (this stack frame) until latch.wait() returns, so the pool can reference it by pointer.
            for (int bi = 0; bi < (int)batch.size(); ++bi)
            {
                ParallelJob &job = batch[bi];
                job.worker       = state.game.state_pool.acquire(state, &batch_cutoff);
                job.latch        = &latch;
                state.game.thread_pool.submit({&RunParallelJob, &job});
            }

            latch.wait(); // Block until every worker has finished.

            // Fold parallel results into best_score / alpha.
            for (const auto &job : batch)
            {
                const Move r_move  = job.move;
                const int  r_score = job.score;
                if (state.game.is_cancel_pending.load(std::memory_order_relaxed))
                {
                    return kSearchCancelledScore;
                }
                if (r_score >= beta)
                {
                    INCREMENT("parallel beta cutoffs");
                    state.game.transposition_table.RecordTransposition(Transposition{
                        state.CurrentPosition().Hash(), r_move, r_score, depth, Transposition::NodeType::kCut});
                    state.game.history_table.RecordGoodMove(ply, r_move);
                    if (r_move.IsQuiet())
                    {
                        state.RecordKiller(ply, r_move);
                    }
                    return r_score;
                }
                if (r_score > best_score)
                {
                    INCREMENT("best score changed");
                    best_score = r_score;
                    best_move  = r_move;
                    if (r_score > alpha)
                    {
                        INCREMENT("pv changed");
                        alpha            = r_score;
                        has_raised_alpha = true;
                        state.game.history_table.RecordGoodMove(ply, r_move);
                    }
                }
            }
        }
    }

    // ── End of main loop ─────────────────────────────────────────────────────
    if (has_raised_alpha)
    {
        // Raised alpha without a cutoff: PV node.
        INCREMENT("pv nodes");
        state.game.transposition_table.RecordTransposition(
            Transposition{state.CurrentPosition().Hash(), best_move, alpha, depth, Transposition::NodeType::kPv});
        state.game.history_table.RecordGoodMove(ply, best_move);
        CopyVariation(parent_pv, pv, best_move);
    }
    else
    {
        // Searched all moves without raising alpha: all-node.
        INCREMENT("all nodes");
        state.game.transposition_table.RecordTransposition(
            Transposition{state.CurrentPosition().Hash(), best_move, best_score, depth, Transposition::NodeType::kAll});
    }
    return best_score;
}

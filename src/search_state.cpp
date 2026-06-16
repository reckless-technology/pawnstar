/// @file search_state.cpp Per-thread search state implementation, including the alpha-beta and quiescence
/// search routines (SearchState members).

#include "search_state.h"
#include "chess_clock.h"
#include "debug_hashtable.h"
#include "evaluation.h"
#include "game.h"
#include "static_exchange_evaluation.h"
#include "transposition_table.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <format>
#include <iostream>
#include <string>
#include <utility>

/// @brief Construct the main-thread search state from the current game.
/// hash_stack_ is populated with all ancestor positions (positions 0..N-1 where N is
/// the current position), so that IsDrawByRepetition can walk backwards through the full
/// game history.
SearchState::SearchState(Game &g) : game(g)
{
    const std::vector<Position> &pos = g.Positions();
    // Reserve the whole game history plus the deepest the search can descend, so no push_back during the
    // search reallocates (and an arbitrarily long game cannot overflow a fixed buffer).
    hash_stack_.reserve(pos.size() + kMaxPly + 4);
    // Push all positions except the last (current) one into the hash history.
    for (std::size_t i = 0; i + 1 < pos.size(); ++i)
    {
        hash_stack_.push_back({pos[i].Hash(), pos[i].ReversibleMoveCount()});
    }
    // Seed the per-thread position stack with only the current game position.
    positions_.push_back(g.CurrentPosition());
    // Seed the NNUE accumulator from the root position (only needed when NNUE is the active evaluator).
    if (game.NnueActive())
    {
        game.NnueNetwork().Refresh(accumulator_, positions_.back());
    }
}

/// @brief Push the current position's hash then make the move on a copy of the position.
/// When NNUE is active, advance the accumulator from the parent to the child by feature deltas.
void SearchState::PlayMove(Move move)
{
    const Position &cur = CurrentPosition();
    hash_stack_.push_back({cur.Hash(), cur.ReversibleMoveCount()});
    Position next = cur.MakeMove(move);
    if (game.NnueActive())
    {
        game.NnueNetwork().Update(accumulator_, cur, next);
    }
    positions_.push_back(next);
}

/// @brief Pop the position and hash stacks to undo the last move.
/// When NNUE is active, reverse the accumulator from the child back to the parent (Update is reversible).
void SearchState::UndoMove()
{
    if (game.NnueActive())
    {
        const std::size_t n = positions_.size();
        game.NnueNetwork().Update(accumulator_, positions_[n - 1], positions_[n - 2]);
    }
    positions_.pop_back();
    hash_stack_.pop_back();
}

/// @brief Push a null move (skip this side's turn) used for null-move pruning.
/// Piece placement is unchanged by a null move, so the accumulator needs no update.
void SearchState::MakeNullMove()
{
    const Position &cur = CurrentPosition();
    hash_stack_.push_back({cur.Hash(), cur.ReversibleMoveCount()});
    positions_.push_back(cur.MakeNullMove());
}

/// @brief Score moves for ordering, then sort descending.
/// The score field is a signed 23-bit value shared with the stored TT score. Move ordering uses, from
/// best to worst:
///   - winning / equal captures and promotions: kWinningCaptureBase + SEE score,
///   - the two killer moves for this ply: just below winning captures, above all quiet moves,
///   - quiet moves: their history count, clamped below the killers,
///   - losing captures (negative SEE): scored by their (negative) SEE, so they sort below quiet moves.
void SearchState::ScoreAndSortMoves(MoveList &moves, int ply) const
{
    static constexpr int kWinningCaptureBase = 1 << 20;         // 1,048,576: winning captures / promotions
    static constexpr int kKillerBase         = 1 << 19;         // 524,288: killers sit just below winning captures
    static constexpr int kMaxQuiet           = kKillerBase - 1; // history scores clamp just below the killers
    const Position      &position            = CurrentPosition();
    const Move           killer0             = killers[ply][0];
    const Move           killer1             = killers[ply][1];
    for (Move &move : moves)
    {
        int sort;
        if (!move.IsQuiet()) // capture or promotion: order by static exchange evaluation
        {
            const int see = EvaluateStaticExchange(position, move).first;
            sort          = see >= 0 ? kWinningCaptureBase + see : see; // losing captures sort below quiet moves
        }
        else if (move == killer0) // first killer: just below winning captures
        {
            sort = kKillerBase + 1;
        }
        else if (move == killer1) // second killer
        {
            sort = kKillerBase;
        }
        else // quiet move: history count, clamped below the killers
        {
            sort = (int)std::min<uint32_t>(history.GetCount(ply, move), (uint32_t)kMaxQuiet);
        }
        move.AssignScore(sort);
    }
    SortMoves<false>(moves);
}

/// @brief Check for draw by the 50-move rule.
bool SearchState::IsDrawByFiftyMoves() const
{
    return CurrentPosition().ReversibleMoveCount() >= 100;
}

/// @brief Check for three-fold repetition using the compact hash history.
/// Walks backwards through hash_stack_ two entries at a time (same side to move),
/// starting four half-moves before the current position.
bool SearchState::IsDrawByRepetition() const
{
    const zobrist_t hash        = CurrentPosition().Hash();
    int             repetitions = 2;
    for (int i = (int)hash_stack_.size() - 4; i >= 0; i -= 2)
    {
        if (hash_stack_[i].hash == hash && --repetitions == 0)
        {
            INCREMENT("draws by repetition SS");
            return true;
        }
        if (hash_stack_[i].reversible_move_count == 0)
        {
            break;
        }
    }
    return false;
}

/// @brief Check whether this thread should abort (the shared stop flag is set).
bool SearchState::IsCancelled() const
{
    return game.is_cancel_pending.load(std::memory_order_relaxed);
}

// ----------------------------------------------------------------------------
// Alpha-beta and quiescence search (formerly the free functions in
// search_alphabeta.cpp / search_quiescent.cpp, now SearchState members). Each
// keeps a `state` alias so the body reads uniformly through `state`.
// ----------------------------------------------------------------------------

SingleMoveResult SearchState::SearchSingleMove(int depth, int ply, int alpha, int beta, Move move, Variation &pv,
                                               int move_index)
{
    SearchState    &state        = *this;
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
        score = -state.Search(child_depth, ply + 1, -alpha - 1, -alpha, pv);
        if (score > alpha)
        {
            INCREMENT("pvs fails");
            INCREMENT_IF(ply == 0, "pvs fails root ply");
            score = -state.Search(child_depth, ply + 1, -beta, -alpha, pv);
        }
    }
    else
    {
        score = -state.Search(child_depth, ply + 1, -beta, -alpha, pv);
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
/// @return Null move score plus evaluation score (or kAlpha if evaluation was not called).
static inline NullMoveResult AttemptNullMove(SearchState &state, int depth, int ply, int alpha, int beta)
{
    const Position &position = state.CurrentPosition();
    const Color     color    = position.color_to_move;
    // Only try null move pruning if all conditions are met.
    const Bitboard friendly = position.colors[color];
    if (!position.is_null_move && // previous move was not a null move
        !position.IsInCheck() &&  // we are not in check
        beta == alpha + 1 &&      // this is not a PV node
        friendly.PopCount() > 3)  // we have at least 4 friendly pieces
    {
        const int eval_score = EvaluatePosition(state);
        if (eval_score >= beta)
        {
            INCREMENT("null move");
            Variation dummy{};
            state.MakeNullMove();
            int score = -state.Search(depth - 4, ply + 1, -beta, -alpha, dummy);
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

int SearchState::Search(int depth, int ply, int alpha, int beta, Variation &parent_pv)
{
    SearchState &state = *this;
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
        return EvaluatePosition(state);
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
        return state.SearchQuiescent(depth, ply, alpha, beta);
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
        const int score = state.SearchSingleMove(depth, ply, alpha, beta, transposition->move, pv, 0).score;
        if (state.IsCancelled())
        {
            return kSearchCancelledScore;
        }
        if (score >= beta)
        {
            INCREMENT("table move beta cutoffs");
            state.game.transposition_table.RecordTransposition(Transposition{
                state.CurrentPosition().Hash(), transposition->move, score, depth, Transposition::NodeType::kCut});
            state.history.RecordGoodMove(ply, transposition->move);
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
            state.history.RecordGoodMove(ply, transposition->move);
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

    // Search the moves one at a time. Parallelism comes from Lazy SMP (independent whole-tree
    // searches sharing the transposition table), not from splitting this node.
    const bool in_check_seq = state.CurrentPosition().IsInCheck();

    for (const Move &move : move_list)
    {
        const int move_index = (int)(&move - move_list.begin());
        if (transposition && move == transposition->move)
        {
            continue;
        }

        // Late move reduction: reduce late moves outside a check sequence at null-window nodes.
        int lmr_depth = depth;
        if (beta == alpha + 1 && move_index > 3 && !in_check_seq && depth > 2)
        {
            INCREMENT("late move reduction");
            lmr_depth = depth - 1;
            if (depth > 3 && move_index > 6)
            {
                --lmr_depth;
                INCREMENT("late move reduction extreme");
            }
        }

        int score = state.SearchSingleMove(lmr_depth, ply, alpha, beta, move, pv, move_index).score;
        if (score > alpha && lmr_depth < depth)
        {
            INCREMENT("late move reduction fails");
            score = state.SearchSingleMove(depth, ply, alpha, beta, move, pv, move_index).score;
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
            state.history.RecordGoodMove(ply, move);
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
                state.history.RecordGoodMove(ply, move);
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
        state.history.RecordGoodMove(ply, best_move);
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

int SearchState::SearchQuiescent(int depth, int ply, int alpha, int beta)
{
    SearchState &state = *this;
    INCREMENT("quiescent calls");
    if (ply == kMaxPly)
    {
        INCREMENT("quiescent max ply");
        return EvaluatePosition(state);
    }
    if (state.CurrentPosition().IsInCheck())
    {
        // We can't handle checks in quiescence.
        INCREMENT("quiescent checks");
        Variation dummy{};
        return state.Search(depth, ply, alpha, beta, dummy);
    }
    auto transposition = state.game.quiescent_table.FindTransposition(state.CurrentPosition().Hash());
    if (transposition && transposition->score() >= beta)
    {
        INCREMENT("quiescent table beta cutoffs");
        return transposition->score();
    }
    int score = EvaluatePosition(state);
    if (score >= beta)
    {
        INCREMENT("quiescent eval beta cutoffs");
        state.game.quiescent_table.RecordTransposition(
            Transposition{state.CurrentPosition().Hash(), Move::None(), score, depth, Transposition::NodeType::kCut});
        return score;
    }
    if (score > alpha)
    {
        INCREMENT("quiescent eval raises alpha");
        alpha = score;
    }
    int best_score = score;
    if (transposition && transposition->move != Move::None())
    {
        INCREMENT("quiescent table move");
        state.PlayMove(transposition->move);
        score = -state.SearchQuiescent(depth - 1, ply + 1, -beta, -alpha);
        state.UndoMove();
        if (state.IsCancelled())
        {
            return kSearchCancelledScore;
        }
        if (score >= beta)
        {
            INCREMENT("quiescent table move beta cutoffs");
            state.history.RecordGoodMove(ply, transposition->move);
            return score;
        }
        if (score > best_score)
        {
            best_score = score;
            if (score > alpha)
            {
                alpha = score;
                INCREMENT("quiescent table move raise alpha");
                state.history.RecordGoodMove(ply, transposition->move);
            }
        }
    }
    MoveList move_list{state.CurrentPosition().GenerateLegalCaptures()};
    state.ScoreAndSortMoves(move_list, ply);
    for (Move &move : move_list)
    {
        if (transposition && transposition->move == move)
        {
            continue;
        }
        // SEE pruning: skip captures that lose material and do not give check.
        const auto [see_score, is_checking] = EvaluateStaticExchange(state.CurrentPosition(), move);
        if (see_score < 0 && !is_checking)
        {
            INCREMENT("quiescent negative see");
            continue;
        }
        state.PlayMove(move);
        score = -state.SearchQuiescent(depth - 1, ply + 1, -beta, -alpha);
        state.UndoMove();
        if (state.IsCancelled())
        {
            return kSearchCancelledScore;
        }
        if (score >= beta)
        {
            INCREMENT("quiescent beta cutoffs");
            state.history.RecordGoodMove(ply, move);
            state.game.quiescent_table.RecordTransposition(
                Transposition{state.CurrentPosition().Hash(), move, score, depth, Transposition::NodeType::kCut});
            return score;
        }
        if (score > best_score)
        {
            best_score = score;
            if (score > alpha)
            {
                alpha = score;
                INCREMENT("quiescent pv changed");
                state.history.RecordGoodMove(ply, move);
                state.game.quiescent_table.RecordTransposition(
                    Transposition{state.CurrentPosition().Hash(), move, score, depth, Transposition::NodeType::kCut});
            }
        }
    }
    return best_score;
}

// ----------------------------------------------------------------------------
// Iterative deepening — one Lazy SMP thread (formerly the root-node driver in
// search_root_node.cpp). Game::SearchRootNode (in game.cpp) launches one of
// these per thread.
// ----------------------------------------------------------------------------

Move SearchState::IterativeDeepen(MoveList move_list, Move best_move, int thread_id, int64_t start_ms, int ms_allocated)
{
    SearchState &state   = *this;
    const bool   is_main = thread_id == 0; // thread 0 is the authoritative main thread
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
        if (!is_main)
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
            const int score      = state.SearchSingleMove(depth, 0, alpha, kBeta, move, child_pv, move_index).score;
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
                    std::string pv_string;
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
            const bool    is_best_move_consistent =
                std::ranges::all_of(best_moves, [best_move](const Move &m) { return m == best_move; });
            const bool is_score_stable = std::ranges::all_of(best_moves, [best_move](const Move &m) {
                return std::abs(m.score() - best_move.score()) <= kScoreInstabilityThreshold;
            });
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

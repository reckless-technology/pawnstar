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
SearchState::SearchState(Game &g) : game(g), continuation_history_(std::make_unique<int16_t[]>(kContKeys * kContKeys))
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
    // Reserve the deepest the search can descend so no push_back during search reallocates.
    positions_.reserve(kMaxPly + 4);
    positions_.push_back(g.CurrentPosition());
    // Seed the NNUE accumulator from the root position.
    game.NnueNetwork().Refresh(accumulator_, positions_.back());
}

/// @brief Push the current position's hash then make the move on a copy of the position.
/// Advance the NNUE accumulator from the parent to the child by feature deltas.
void SearchState::PlayMove(Move move)
{
    const Position &cur = CurrentPosition();
    hash_stack_.push_back({cur.Hash(), cur.ReversibleMoveCount()});
    Position next = cur.MakeMove(move);
    game.NnueNetwork().Update(accumulator_, cur, next);
    positions_.push_back(next);
}

/// @brief Pop the position and hash stacks to undo the last move.
/// Reverse the NNUE accumulator from the child back to the parent (Update is reversible).
void SearchState::UndoMove()
{
    const std::size_t n = positions_.size();
    game.NnueNetwork().Update(accumulator_, positions_[n - 1], positions_[n - 2]);
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
void SearchState::ScoreAndSortMoves(MoveList &moves, int ply, Move prev_move) const
{
    static constexpr int kWinningCaptureBase = 1 << 20;         // 1,048,576: winning captures / promotions
    static constexpr int kKillerBase         = 1 << 19;         // 524,288: killers sit just below winning captures
    static constexpr int kCountermoveScore   = kKillerBase - 1; // countermove: just below the two killers
    static constexpr int kMaxQuiet           = kKillerBase - 2; // history scores clamp just below the countermove
    const Position      &position            = CurrentPosition();
    const Move           killer0             = killers[ply][0];
    const Move           killer1             = killers[ply][1];
    for (Move &move : moves)
    {
        int sort;
        if (!move.IsQuiet()) // capture or promotion: order by static exchange evaluation
        {
            const auto [see, is_checking] = EvaluateStaticExchange(position, move);
            if (is_checking)
            {
                move.GivesCheck();
            }
            sort = see >= 0 ? kWinningCaptureBase + see : see; // losing captures sort below quiet moves
        }
        else if (move == killer0) // first killer: just below winning captures
        {
            sort = kKillerBase + 1;
        }
        else if (move == killer1) // second killer
        {
            sort = kKillerBase;
        }
        else if (IsCountermove(prev_move, move)) // countermove: best quiet refutation to the previous move
        {
            sort = kCountermoveScore;
        }
        else // quiet move: main history + 1-ply continuation history, clamped below the countermove
        {
            const uint32_t h = history.GetCount(ply, move) + (uint32_t)ContinuationHistScore(prev_move, move);
            sort             = (int)std::min<uint32_t>(h, (uint32_t)kMaxQuiet);
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
// Alpha-beta and quiescence search (SearchState members).
// ----------------------------------------------------------------------------

SingleMoveResult SearchState::SearchSingleMove(int depth, int ply, int alpha, int beta, Move move, Variation &pv,
                                               int move_index)
{
    const Position &position     = CurrentPosition();
    const bool      was_in_check = position.IsInCheck(); // check extensions are added in main Search()

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
    PlayMove(move);
    // Prefetch the child's TT cell now; the recursive Search probes it after its own prologue, by which
    // time the (random-access, cache-missing) line should be resident. Result-neutral speed win.
    game.transposition_table.Prefetch(CurrentPosition().Hash());
    const bool is_checking = CurrentPosition().IsInCheck();
    int        score;
    if (beta > alpha + 1 && move_index > 0 && !was_in_check && !is_checking)
    {
        // Try principal variation search.
        INCREMENT("pvs attempts");
        score = -Search(child_depth, ply + 1, -alpha - 1, -alpha, pv, move);
        if (score > alpha)
        {
            INCREMENT("pvs fails");
            INCREMENT_IF(ply == 0, "pvs fails root ply");
            score = -Search(child_depth, ply + 1, -beta, -alpha, pv, move);
        }
    }
    else
    {
        score = -Search(child_depth, ply + 1, -beta, -alpha, pv, move);
    }
    UndoMove();
    return {score, is_checking};
}

/// @brief Try null move pruning, reusing the caller's precomputed static eval.
/// The caller has already established this is a non-PV node that is not in check.
/// @param eval_score The node's static evaluation (already computed by the caller).
/// @return The null-move score, or alpha if null move was not tried / did not cut.
int SearchState::AttemptNullMove(int depth, int ply, int alpha, int beta, int eval_score)
{
    const Position &position = CurrentPosition();
    const Color     color    = position.color_to_move;
    // Only try null move pruning if all conditions are met.
    const Bitboard friendly = position.colors[color];
    if (!position.is_null_move &&  // previous move was not a null move
        friendly.PopCount() > 3 && // we have at least 4 friendly pieces
        eval_score >= beta)        // and we are already failing high statically
    {
        INCREMENT("null move");
        Variation dummy{};
        MakeNullMove();
        int score = -Search(depth - 4, ply + 1, -beta, -alpha, dummy, Move::None());
        UndoMove();
        if (IsCancelled())
        {
            return kSearchCancelledScore;
        }
        if (score >= beta)
        {
            return beta;
        }
        INCREMENT("null move fails");
    }
    return alpha;
}

int SearchState::Search(int depth, int ply, int alpha, int beta, Variation &parent_pv, Move prev_move)
{
    INCREMENT("alpha beta calls");
    if ((++node_count & 0xFFFF) == 0 && game.time_control.HasReachedHardDeadline())
    {
        game.is_cancel_pending.store(true, std::memory_order_relaxed);
        return kSearchCancelledScore;
    }
    if (CurrentPosition().IsDrawByMaterial() || IsDrawByFiftyMoves() || IsDrawByRepetition())
    {
        return kDrawScore;
    }
    if (CurrentPosition().IsInCheck())
    {
        INCREMENT("extensions check");
        ++depth;
    }
    if (ply == kMaxPly)
    {
        INCREMENT("max ply reached");
        return EvaluatePosition(*this);
    }
    // Determine if there is an entry in the transposition table for this position.
    const auto transposition = game.transposition_table.FindTransposition(CurrentPosition().Hash());
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
            if (transposition->score() <= alpha)
            {
                INCREMENT("table hit all node cutoffs");
                return transposition->score();
            }
            break;

        case Transposition::NodeType::kPv:
            INCREMENT("table hit pv node");
            ++depth; // extend depth if we think we are on the PV
            break;
        }
    }
    // Internal iterative reduction: with no usable transposition-table move our move ordering is weak (the TT
    // move is searched first; lacking it the first move searched is only a guess). Rather than spend the full
    // depth with poor ordering, reduce by one ply — the shallower search still fills the TT with a best move,
    // so the cost is recouped by better ordering when this node is revisited. Applied only at sufficient depth,
    // where the per-node saving outweighs the lost ply (qsearch unaffected: depth stays >= the min depth - 1).
    if (depth >= kInternalIterativeReductionMinDepth && (!transposition || transposition->move == Move::None()))
    {
        INCREMENT("internal iterative reduction");
        --depth;
    }
    // Can we go directly to the quiescence search?
    if (depth <= 0 && !CurrentPosition().IsInCheck())
    {
        return SearchQuiescent(depth, ply, alpha, beta);
    }
    // Static-eval-based pruning at non-PV nodes that are not in check. The static eval is computed once
    // here and reused by reverse-futility pruning and null-move pruning.
    const bool is_pv = beta > alpha + 1;
    if (!is_pv && !CurrentPosition().IsInCheck())
    {
        const int eval_score = EvaluatePosition(*this);
        // Reverse futility pruning (a.k.a. static null move): at shallow depth, if we are far enough above
        // beta that even conceding kRfpMargin per ply still leaves us at/above beta, assume a quiet move
        // holds the advantage and cut. Disabled in the mate zone so it cannot mask a forced mate.
        if (depth <= kRfpMaxDepth && std::abs(beta) < -kCheckmatedScore - kMaxPly &&
            eval_score - kRfpMargin * depth >= beta)
        {
            INCREMENT("reverse futility prune");
            return eval_score;
        }
        // Null move pruning, reusing the static eval.
        const int null_move_score = AttemptNullMove(depth, ply, alpha, beta, eval_score);
        if (IsCancelled())
        {
            return kSearchCancelledScore;
        }
        if (null_move_score >= beta)
        {
            return null_move_score;
        }
    }

    // Try the transposition table move first.
    Variation pv;
    Move      best_move        = Move::None();
    int       best_score       = kAlpha;
    bool      has_raised_alpha = false;
    if (transposition && transposition->move != Move::None())
    {
        INCREMENT("table move");
        best_move = transposition->move;
        pv.clear(); // child writes its line here only if it is a PV node; clear so a terminal/cutoff child leaves it
                    // empty
        const int score = SearchSingleMove(depth, ply, alpha, beta, transposition->move, pv, 0).score;
        if (IsCancelled())
        {
            return kSearchCancelledScore;
        }
        if (score >= beta)
        {
            INCREMENT("table move beta cutoffs");
            game.transposition_table.RecordTransposition(Transposition{CurrentPosition().Hash(), transposition->move,
                                                                       score, depth, Transposition::NodeType::kCut});
            history.RecordGoodMove(ply, transposition->move);
            RecordContinuationHistory(prev_move, transposition->move);
            if (transposition->move.IsQuiet())
            {
                RecordKiller(ply, transposition->move);
                RecordCountermove(prev_move, transposition->move);
            }
            return score;
        }
        best_score = score;
        if (score > alpha)
        {
            INCREMENT("table move raised alpha");
            alpha            = score;
            has_raised_alpha = true;
            CopyVariation(parent_pv, pv, transposition->move); // capture the PV now, while pv holds THIS move's line
            history.RecordGoodMove(ply, transposition->move);
            RecordContinuationHistory(prev_move, transposition->move);
        }
    }

    MoveList move_list{CurrentPosition().GenerateLegalMoves()};
    if (move_list.size() == 0)
    {
        if (CurrentPosition().IsInCheck())
        {
            INCREMENT("checkmates");
            return kCheckmatedScore + ply;
        }
        INCREMENT("stalemates");
        return kDrawScore;
    }
    ScoreAndSortMoves(move_list, ply, prev_move);

    // Search the moves one at a time. Parallelism comes from Lazy SMP (independent whole-tree
    // searches sharing the transposition table), not from splitting this node.
    const bool in_check_seq = CurrentPosition().IsInCheck();

    for (const Move &move : move_list)
    {
        const int move_index = (int)(&move - move_list.begin());
        if (transposition && move == transposition->move)
        {
            continue; // we already searched that one.
        }

        // Late move pruning (move-count pruning): at shallow depth in a non-PV node not in check, once we
        // are deep into the (history/SEE-sorted) move list, skip the remaining quiet, non-checking moves —
        // they almost never raise alpha, and not searching them spends the saved time on more useful lines.
        // Captures, promotions and checking moves are never pruned.
        if (beta == alpha + 1 && !in_check_seq && depth <= kLmpMaxDepth && move.IsQuiet() && !move.IsChecking() &&
            move_index >= kLmpBase + depth * depth)
        {
            INCREMENT("late move prune");
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
                INCREMENT("late move reduction x2");
                if (history.GetCount(ply, move) == 0)
                {
                    --lmr_depth;
                    INCREMENT("late move reduction x3");
                }
            }
        }

        pv.clear(); // see TT-move note: clear so a non-PV-node child leaves no stale tail in pv
        int score = SearchSingleMove(lmr_depth, ply, alpha, beta, move, pv, move_index).score;
        if (score > alpha && lmr_depth < depth)
        {
            INCREMENT("late move reduction fails");
            score = SearchSingleMove(depth, ply, alpha, beta, move, pv, move_index).score;
        }
        if (IsCancelled())
        {
            return kSearchCancelledScore;
        }
        if (score >= beta)
        {
            INCREMENT("beta cutoffs");
            game.transposition_table.RecordTransposition(
                Transposition{CurrentPosition().Hash(), move, score, depth, Transposition::NodeType::kCut});
            history.RecordGoodMove(ply, move);
            RecordContinuationHistory(prev_move, move);
            if (move.IsQuiet())
            {
                RecordKiller(ply, move);
                RecordCountermove(prev_move, move);
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
                CopyVariation(parent_pv, pv, move); // capture the PV now, while pv holds THIS move's line
                history.RecordGoodMove(ply, move);
                RecordContinuationHistory(prev_move, move);
            }
        }
    }

    // ── End of main loop ─────────────────────────────────────────────────────
    if (has_raised_alpha)
    {
        // Raised alpha without a cutoff: PV node.
        INCREMENT("pv nodes");
        game.transposition_table.RecordTransposition(
            Transposition{CurrentPosition().Hash(), best_move, alpha, depth, Transposition::NodeType::kPv});
        history.RecordGoodMove(ply, best_move);
        RecordContinuationHistory(prev_move, best_move);
        // parent_pv was already set, with the correct line, at the alpha-raise above.
    }
    else
    {
        // Searched all moves without raising alpha: all-node.
        INCREMENT("all nodes");
        game.transposition_table.RecordTransposition(
            Transposition{CurrentPosition().Hash(), best_move, best_score, depth, Transposition::NodeType::kAll});
    }
    return best_score;
}

int SearchState::SearchQuiescent(int depth, int ply, int alpha, int beta)
{
    INCREMENT("quiescent calls");
    if (ply == kMaxPly)
    {
        INCREMENT("quiescent max ply");
        return EvaluatePosition(*this);
    }
    if (CurrentPosition().IsInCheck())
    {
        // We can't handle checks in quiescence.
        INCREMENT("quiescent checks");
        Variation dummy{};
        return Search(depth, ply, alpha, beta, dummy, Move::None());
    }
    int score = EvaluatePosition(*this);
    if (score >= beta)
    {
        INCREMENT("quiescent eval beta cutoffs");
        return score;
    }
    if (score > alpha)
    {
        INCREMENT("quiescent eval raises alpha");
        alpha = score;
    }
    int      best_score = score;
    MoveList move_list{CurrentPosition().GenerateLegalCaptures()};
    ScoreAndSortMoves(move_list, ply, Move::None());
    for (Move &move : move_list)
    {
        // SEE pruning: skip captures that lose material.
        if (move.score() < 0)
        {
            INCREMENT("quiescent negative see");
            return best_score; // All moves after this are -ve SEE and may be skipped.
        }
        PlayMove(move);
        score = -SearchQuiescent(depth - 1, ply + 1, -beta, -alpha);
        UndoMove();
        if (IsCancelled())
        {
            return kSearchCancelledScore;
        }
        if (score >= beta)
        {
            INCREMENT("quiescent beta cutoffs");
            history.RecordGoodMove(ply, move);
            return score;
        }
        if (score > best_score)
        {
            best_score = score;
            if (score > alpha)
            {
                alpha = score;
                INCREMENT("quiescent pv changed");
                history.RecordGoodMove(ply, move);
            }
        }
    }
    return best_score;
}

// ----------------------------------------------------------------------------
// Iterative deepening — one Lazy SMP thread. Game::SearchRootNode (in game.cpp)
// launches one of these per thread.
// ----------------------------------------------------------------------------

Move SearchState::IterativeDeepen(MoveList move_list, Move best_move, int thread_id)
{
    const bool is_main = thread_id == 0; // thread 0 is the authoritative main thread
    // Per-thread iteration-skip schedule (Stockfish-style): helper threads skip certain iteration depths so
    // their searches desynchronize from the main thread and from each other, prefilling the shared TT with
    // diverse entries instead of recomputing the same tree in lockstep. The main thread (thread_id 0) never skips.
    static constexpr std::array<int, 20> kSkipSize{1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4};
    static constexpr std::array<int, 20> kSkipPhase{0, 1, 0, 1, 2, 3, 0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5, 6, 7};
    constexpr int                        kSkipEntries = (int)kSkipSize.size();

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
        int       alpha = kAlpha;
        node_count      = 0;
        if (is_main)
        {
            best_moves.push_back(best_move);
        }
        for (auto &move : move_list)
        {
            const int move_index = (int)(&move - move_list.begin());
            child_pv.clear(); // clear per move so a terminal/non-PV-node child leaves no stale tail
            const int score = SearchSingleMove(depth, 0, alpha, kBeta, move, child_pv, move_index).score;
            move.AssignScore(score);
            if (IsCancelled())
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
                    // Publish the predicted reply (PV[1]) as it improves, so `bestmove <m> ponder <reply>` has
                    // it on every exit path — including a hard-timeout/stop cancellation, which returns early.
                    game.ponder_move = principal_variation.size() >= 2 ? principal_variation[1] : Move::None();
                    std::string pv_string;
                    for (const auto &m : principal_variation)
                    {
                        pv_string.append(m.ToString());
                        pv_string.push_back(' ');
                    }
                    pv_string.pop_back();
                    // Report a forced mate as `score mate <moves>` (positive = we mate, negative = we get
                    // mated) rather than a huge `cp` value; otherwise the centipawn score. Mate scores are
                    // +/-(kMateValue - plies_to_mate), so plies_to_mate = kMateValue - |score| and the move
                    // count rounds up (+1)/2.
                    constexpr int kMateValue = -kCheckmatedScore; // 10000
                    std::string   score_string;
                    if (alpha >= kWinThreshold)
                    {
                        score_string = std::format("mate {}", (kMateValue - alpha + 1) / 2);
                    }
                    else if (alpha <= kLoseThreshold)
                    {
                        score_string = std::format("mate {}", -((kMateValue + alpha + 1) / 2));
                    }
                    else
                    {
                        score_string = std::format("cp {}", alpha);
                    }
                    const long long elapsed_ms = game.time_control.ElapsedSinceSearchStart().count();
                    const long long nps =
                        static_cast<long long>(node_count) * 1000 / std::max<long long>(1, elapsed_ms);
                    std::cout << std::format("info depth {:2} score {} time {:5} nodes {:8} nps {} hashfull {} pv {}\n",
                                             depth, score_string, elapsed_ms, node_count, nps,
                                             game.transposition_table.HashfullPermille(), pv_string);
                }
            }
        }
        if (alpha > kWinThreshold || alpha < kLoseThreshold)
        {
            break;
        }
        // Soft time management: never stop while pondering (no real clock is running yet); once `ponderhit`
        // has reset the search-start instant, elapsed is measured from the ponderhit moment.
        if (is_main && game.time_control.clock_type == ChessClock::kStandard &&
            !game.is_pondering.load(std::memory_order_relaxed))
        {
            const auto allocated = game.time_control.AllocatedTime();
            const auto elapsed   = game.time_control.ElapsedSinceSearchStart();
            const bool is_best_move_consistent =
                std::ranges::all_of(best_moves, [best_move](const Move &m) { return m == best_move; });
            const bool is_score_stable = std::ranges::all_of(best_moves, [best_move](const Move &m) {
                return std::abs(m.score() - best_move.score()) <= kScoreInstabilityThreshold;
            });
            if ((is_best_move_consistent && is_score_stable) && (elapsed * 4) > allocated)
            {
                std::cout << "info string Best move consistent AND score stable - search terminated.\n";
                break;
            }
            if ((is_best_move_consistent || is_score_stable) && (elapsed * 2) > allocated)
            {
                std::cout << std::format("info string {} - search terminated.\n",
                                         is_best_move_consistent ? "Best move consistent" : "Score stable");
                break;
            }
            if (elapsed > allocated)
            {
                break;
            }
        }
    }
    return best_move;
}

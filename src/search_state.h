#pragma once
/// @file search_state.h Per-thread search state for the parallel alpha-beta search.

#include "chess_clock.h"
#include "constants.h"
#include "debug_hashtable.h"
#include "evaluation.h"
#include "game.h"
#include "history_table.h"
#include "move.h"
#include "nnue.h"
#include "position.h"
#include "stack_list.h"
#include "static_exchange_evaluation.h"
#include "transposition_table.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <format>
#include <iostream>
#include <iterator>
#include <memory>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

class Game;

/// @brief Compact position hash entry for draw-by-repetition detection.
struct HashEntry
{
    zobrist_t hash_;                  ///< Zobrist hash of the position.
    uint8_t   reversible_move_count_; ///< Half-move clock at this position.
};

/// @brief A variation (typically principal variation) is a line of play or series of moves.
using Variation = std::vector<Move>;

/// @brief Result of searching a single move: its score and whether it gives check.
struct SingleMoveResult
{
    int  score_;       ///< Alpha-beta score for the move.
    bool is_checking_; ///< True if the move gives check.
};

/// @brief When the PV changes we need to copy the new PV up the tree recursively.
/// @param dst Destination PV.
/// @param src Source PV.
/// @param best_move New best move at this position.
constexpr void CopyVariation(Variation &dst, const Variation &src, const Move &best_move)
{
    dst.clear();
    dst.push_back(best_move);
    std::copy(src.begin(), src.end(), std::back_inserter(dst));
}

/// @brief Per-thread search state for the parallel alpha-beta search.
/// Each Lazy SMP thread owns an independent SearchState with its own position stack, hash history,
/// killers and history heuristic table, while sharing the transposition table and time control through
/// the Game reference. The history table is per-thread to avoid cross-thread cache-line contention on its
/// hot counters.
class SearchState
{
  public:
    Game                                    &game_;      ///< Shared state: TT, clock, cancellation flag.
    HistoryTable                             history_{}; ///< Per-thread history heuristic counts (no sharing).
    std::array<std::array<Move, 2>, kMaxPly> killers_{}; ///< Killer moves indexed by ply.
    uint64_t node_count_{}; ///< Cumulative nodes this thread searched this search (nps + `go nodes`).

    /// @brief Construct a search state from the current game.
    /// Builds the full hash history from the game's position stack and seeds the per-thread
    /// position stack with the current position only.
    /// @param game Game to search.
    explicit SearchState(Game &game);

    /// @brief Current position at the tip of the search tree.
    /// @return Reference to the current position.
    Position &CurrentPosition()
    {
        return positions_.back();
    }

    /// @brief Current position at the tip of the search tree (const overload).
    /// @return Const reference to the current position.
    const Position &CurrentPosition() const
    {
        return positions_.back();
    }

    /// @brief Make a move, pushing the resulting position onto the stack and recording its hash.
    /// @param move Move to play.
    void PlayMove(Move move);

    /// @brief Undo the most recent move, popping the position and hash stacks.
    void UndoMove();

    /// @brief Play a null (pass) move, pushing the resulting position.
    void MakeNullMove();

    /// @brief Assign ordering scores to a move list and sort it best-first.
    /// @param moves Move list to score and sort.
    /// @param ply Current search ply (used for the history heuristic).
    /// @param prev_move The move played to reach this node (for countermove / continuation history).
    void ScoreAndSortMoves(MoveList &moves, int ply, Move prev_move) const;

    /// @brief Fail-soft negamax alpha-beta search of the current node (defined in search_state.cpp).
    /// @param depth Depth to search. @param ply Distance from root. @param alpha Alpha. @param beta Beta.
    /// @param parent_pv Principal variation at the parent node.
    /// @param prev_move The move played at the parent to reach this node (Move::None at root / after null).
    /// @return The alpha-beta score for this node.
    int Search(int depth, int ply, int alpha, int beta, Variation &parent_pv, Move prev_move);

    /// @brief Search a single move from the current node (defined in search_state.cpp).
    /// @return The move's score and whether it gives check.
    SingleMoveResult SearchSingleMove(int depth, int ply, int alpha, int beta, Move move, Variation &pv,
                                      int move_index);

    /// @brief Quiescence search over captures from the current node (defined in search_state.cpp).
    /// @return The quiescent score for this node.
    int SearchQuiescent(int depth, int ply, int alpha, int beta);

    /// @brief Run iterative deepening on this search state — one Lazy SMP thread (defined in search_root_node.cpp).
    /// The main thread (@p thread_id 0) prints info, applies time management and produces the authoritative move.
    /// @param move_list This thread's copy of the root move list (re-sorted per iteration).
    /// @param best_move Best move from the shallow ordering pass (seed).
    /// @param thread_id Thread index (0 = authoritative main thread); drives the iteration-skip schedule.
    /// Time management reads the soft budget / start timestamp live from `game.time_control` (so `ponderhit`
    /// can retarget a running ponder search), and the main thread publishes the ponder move to `game`.
    /// @return The best move found by this thread.
    Move IterativeDeepen(MoveList move_list, Move best_move, int thread_id);

    /// @brief Whether the current position is a draw by threefold repetition.
    /// @return true if drawn by repetition.
    bool IsDrawByRepetition() const;

    /// @brief Whether the current position is a draw by the fifty-move rule.
    /// @return true if drawn by the fifty-move rule.
    bool IsDrawByFiftyMoves() const;

    /// @brief Returns true if this thread should abort because the global stop flag is set
    /// (time limit fired, UCI stop, or another Lazy SMP thread completed the search).
    /// @return true if the search should be abandoned.
    bool IsCancelled() const;

    /// @brief Record a quiet move that caused a beta cutoff as a killer move for this ply.
    /// Shifts the previous first killer into the second slot; ignores duplicates.
    /// @param ply Current search ply.
    /// @param move Quiet move that caused the cutoff.
    void RecordKiller(int ply, Move move)
    {
        if (!(killers_[ply][0] == move))
        {
            INCREMENT("killer moves");
            killers_[ply][1] = killers_[ply][0];
            killers_[ply][0] = move;
        }
    }

    /// @brief Table key for a move in the countermove / continuation-history tables: (piece, to-square).
    /// Move::None (piece 0, to 0) maps to 0, a harmless dedicated slot.
    static constexpr int ContKey(Move m)
    {
        return m.piece() * 64 + m.to();
    }

    static constexpr int kContKeys = 7 * 64; ///< Distinct (piece, to) keys: pieces 0..6 × 64 squares.

    /// @brief Continuation-history score for playing @p move after @p prev (0 if prev/move not quiet-tracked).
    int ContinuationHistScore(Move prev, Move move) const
    {
        return continuation_history_[ContKey(prev) * kContKeys + ContKey(move)];
    }

    /// @brief Whether @p move is the recorded countermove (best quiet refutation) to @p prev.
    bool IsCountermove(Move prev, Move move) const
    {
        return countermoves_[ContKey(prev)] == move;
    }

    /// @brief Reward a quiet @p move that was good (raised alpha or cut) as a follow-up to @p prev.
    /// Captures are ignored (they are ordered by SEE, not history). Saturating count, like HistoryTable.
    void RecordContinuationHistory(Move prev, Move move)
    {
        if (move.IsQuiet())
        {
            int16_t &c = continuation_history_[ContKey(prev) * kContKeys + ContKey(move)];
            if (c < 16384)
            {
                ++c;
            }
        }
    }

    /// @brief Record a quiet @p move that caused a beta cutoff as the countermove to @p prev.
    void RecordCountermove(Move prev, Move move)
    {
        if (move.IsQuiet())
        {
            countermoves_[ContKey(prev)] = move;
        }
    }

  private:
    /// @brief Try null-move pruning at the current node (the caller has established it is a non-PV node not in
    /// check). Reuses the caller's precomputed static eval.
    /// @return The null-move score, or @p alpha if null move was not tried / did not cut.
    int AttemptNullMove(int depth, int ply, int alpha, int beta, int eval_score);

    std::vector<Position> positions_; ///< Per-thread copy-make position stack (reserved in the constructor).

  public:
    nnue::Accumulator accumulator_{}; ///< NNUE accumulator for the tip position (incremental).

  private:
    /// @brief Hash history from game-start through parent of current node (game history + search depth).
    /// A std::vector (reserved up-front in the constructor) so an arbitrarily long game cannot overflow it.
    std::vector<HashEntry> hash_stack_;

    /// @brief Best quiet refutation to each previous move (countermove heuristic), indexed by ContKey(prev).
    std::array<Move, kContKeys> countermoves_{};

    /// @brief 1-ply continuation history: how often a quiet move was good after a given previous move.
    /// Heap-allocated (per-thread, ~0.4 MB) and indexed [ContKey(prev) * kContKeys + ContKey(move)].
    std::unique_ptr<int16_t[]> continuation_history_;
};

// ================= Definitions moved here for the header-only build =================
// search_state.h is the hub where Game and SearchState are both complete, so the cyclic
// definitions (SearchState methods, Game::SearchRootNode, EvaluatePosition) live here inline.

/// @brief Construct the main-thread search state from the current game.
/// hash_stack_ is populated with all ancestor positions (positions 0..N-1 where N is
/// the current position), so that IsDrawByRepetition can walk backwards through the full
/// game history.
inline SearchState::SearchState(Game &g)
    : game_(g), continuation_history_(std::make_unique<int16_t[]>(kContKeys * kContKeys))
{
    const std::vector<Position> &pos = g.positions_;
    // Reserve the whole game history plus the deepest the search can descend, so no push_back during the
    // search reallocates (and an arbitrarily long game cannot overflow a fixed buffer).
    hash_stack_.reserve(pos.size() + kMaxPly + 4);
    // Push all positions except the last (current) one into the hash history.
    for (std::size_t i = 0; i + 1 < pos.size(); ++i)
    {
        hash_stack_.push_back({pos[i].hash_, pos[i].reversible_move_count_});
    }
    // Seed the per-thread position stack with only the current game position.
    // Reserve the deepest the search can descend so no push_back during search reallocates.
    positions_.reserve(kMaxPly + 4);
    positions_.push_back(g.CurrentPosition());
    // Seed the NNUE accumulator from the root position.
    game_.nnue_network_.Refresh(accumulator_, positions_.back());
}

/// @brief Push the current position's hash then make the move on a copy of the position.
/// Advance the NNUE accumulator from the parent to the child by feature deltas.
inline void SearchState::PlayMove(Move move)
{
    const Position &cur = CurrentPosition();
    hash_stack_.push_back({cur.hash_, cur.reversible_move_count_});
    Position next = cur.MakeMove(move);
    game_.nnue_network_.Update(accumulator_, cur, next);
    positions_.push_back(next);
}

/// @brief Pop the position and hash stacks to undo the last move.
/// Reverse the NNUE accumulator from the child back to the parent (Update is reversible).
inline void SearchState::UndoMove()
{
    const std::size_t n = positions_.size();
    game_.nnue_network_.Update(accumulator_, positions_[n - 1], positions_[n - 2]);
    positions_.pop_back();
    hash_stack_.pop_back();
}

/// @brief Push a null move (skip this side's turn) used for null-move pruning.
/// Piece placement is unchanged by a null move, so the accumulator needs no update.
inline void SearchState::MakeNullMove()
{
    const Position &cur = CurrentPosition();
    hash_stack_.push_back({cur.hash_, cur.reversible_move_count_});
    positions_.push_back(cur.MakeNullMove());
}

/// @brief Score moves for ordering, then sort descending.
/// The score field is a signed 23-bit value shared with the stored TT score. Move ordering uses, from
/// best to worst:
///   - winning / equal captures and promotions: kWinningCaptureBase + SEE score,
///   - the two killer moves for this ply: just below winning captures, above all quiet moves,
///   - quiet moves: their history count, clamped below the killers,
///   - losing captures (negative SEE): scored by their (negative) SEE, so they sort below quiet moves.
inline void SearchState::ScoreAndSortMoves(MoveList &moves, int ply, Move prev_move) const
{
    static constexpr int kWinningCaptureBase = 1 << 20;         // 1,048,576: winning captures / promotions
    static constexpr int kKillerBase         = 1 << 19;         // 524,288: killers sit just below winning captures
    static constexpr int kCountermoveScore   = kKillerBase - 1; // countermove: just below the two killers
    static constexpr int kMaxQuiet           = kKillerBase - 2; // history scores clamp just below the countermove
    const Position      &position            = CurrentPosition();
    const Move           killer0             = killers_[ply][0];
    const Move           killer1             = killers_[ply][1];
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
            const uint32_t h = history_.GetCount(ply, move) + (uint32_t)ContinuationHistScore(prev_move, move);
            sort             = (int)std::min<uint32_t>(h, (uint32_t)kMaxQuiet);
        }
        move.AssignScore(sort);
    }
    SortMoves<false>(moves);
}

/// @brief Check for draw by the 50-move rule.
inline bool SearchState::IsDrawByFiftyMoves() const
{
    return CurrentPosition().reversible_move_count_ >= 100;
}

/// @brief Check for three-fold repetition using the compact hash history.
/// Walks backwards through hash_stack_ two entries at a time (same side to move),
/// starting four half-moves before the current position.
inline bool SearchState::IsDrawByRepetition() const
{
    const zobrist_t hash        = CurrentPosition().hash_;
    int             repetitions = 2;
    for (int i = (int)hash_stack_.size() - 4; i >= 0; i -= 2)
    {
        if (hash_stack_[i].hash_ == hash && --repetitions == 0)
        {
            INCREMENT("draws by repetition SS");
            return true;
        }
        if (hash_stack_[i].reversible_move_count_ == 0)
        {
            break;
        }
    }
    return false;
}

/// @brief Check whether this thread should abort (the shared stop flag is set).
inline bool SearchState::IsCancelled() const
{
    return game_.is_cancel_pending_.load(std::memory_order_relaxed);
}

// ----------------------------------------------------------------------------
// Alpha-beta and quiescence search (SearchState members).
// ----------------------------------------------------------------------------

inline SingleMoveResult SearchState::SearchSingleMove(int depth, int ply, int alpha, int beta, Move move, Variation &pv,
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
    game_.transposition_table_.Prefetch(CurrentPosition().hash_);
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
inline int SearchState::AttemptNullMove(int depth, int ply, int alpha, int beta, int eval_score)
{
    const Position &position = CurrentPosition();
    const Color     color    = position.color_to_move_;
    // Only try null move pruning if all conditions are met.
    const Bitboard friendly = position.colors_[color];
    if (!position.is_null_move_ && // previous move was not a null move
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

inline int SearchState::Search(int depth, int ply, int alpha, int beta, Variation &parent_pv, Move prev_move)
{
    INCREMENT("alpha beta calls");
    // Poll the hard deadline (and the `go nodes` limit) often — every 2048 nodes: between polls the search
    // cannot stop, so a coarse interval lets a node-heavy batch overrun the deadline (a ~64k-node interval
    // overran by 6-10 ms and forfeited games once the increment was actually spent). 2048 nodes keeps the
    // overrun sub-ms; the clock read is negligible at this cadence. The node limit (max_nodes, 0 = none) is
    // checked per-thread, so it is exact at Threads=1 (the reproducible-testing case) and approximate above.
    if ((++node_count_ & 0x7FF) == 0 &&
        (game_.time_control_.HasReachedHardDeadline() ||
         (game_.time_control_.max_nodes_ != 0 && node_count_ >= game_.time_control_.max_nodes_)))
    {
        game_.is_cancel_pending_.store(true, std::memory_order_relaxed);
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
    const auto transposition = game_.transposition_table_.FindTransposition(CurrentPosition().hash_);
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
    if (depth >= kInternalIterativeReductionMinDepth && (!transposition || transposition->move_ == Move::None()))
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
    if (transposition && transposition->move_ != Move::None())
    {
        INCREMENT("table move");
        best_move = transposition->move_;
        pv.clear(); // child writes its line here only if it is a PV node; clear so a terminal/cutoff child leaves it
                    // empty
        const int score = SearchSingleMove(depth, ply, alpha, beta, transposition->move_, pv, 0).score_;
        if (IsCancelled())
        {
            return kSearchCancelledScore;
        }
        if (score >= beta)
        {
            INCREMENT("table move beta cutoffs");
            game_.transposition_table_.RecordTransposition(Transposition{CurrentPosition().hash_, transposition->move_,
                                                                         score, depth, Transposition::NodeType::kCut});
            history_.RecordGoodMove(ply, transposition->move_);
            RecordContinuationHistory(prev_move, transposition->move_);
            if (transposition->move_.IsQuiet())
            {
                RecordKiller(ply, transposition->move_);
                RecordCountermove(prev_move, transposition->move_);
            }
            return score;
        }
        best_score = score;
        if (score > alpha)
        {
            INCREMENT("table move raised alpha");
            alpha            = score;
            has_raised_alpha = true;
            CopyVariation(parent_pv, pv, transposition->move_); // capture the PV now, while pv holds THIS move's line
            history_.RecordGoodMove(ply, transposition->move_);
            RecordContinuationHistory(prev_move, transposition->move_);
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
        if (transposition && move == transposition->move_)
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
                if (history_.GetCount(ply, move) == 0)
                {
                    --lmr_depth;
                    INCREMENT("late move reduction x3");
                }
            }
        }

        pv.clear(); // see TT-move note: clear so a non-PV-node child leaves no stale tail in pv
        int score = SearchSingleMove(lmr_depth, ply, alpha, beta, move, pv, move_index).score_;
        if (score > alpha && lmr_depth < depth)
        {
            INCREMENT("late move reduction fails");
            score = SearchSingleMove(depth, ply, alpha, beta, move, pv, move_index).score_;
        }
        if (IsCancelled())
        {
            return kSearchCancelledScore;
        }
        if (score >= beta)
        {
            INCREMENT("beta cutoffs");
            game_.transposition_table_.RecordTransposition(
                Transposition{CurrentPosition().hash_, move, score, depth, Transposition::NodeType::kCut});
            history_.RecordGoodMove(ply, move);
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
                history_.RecordGoodMove(ply, move);
                RecordContinuationHistory(prev_move, move);
            }
        }
    }

    // ── End of main loop ─────────────────────────────────────────────────────
    if (has_raised_alpha)
    {
        // Raised alpha without a cutoff: PV node.
        INCREMENT("pv nodes");
        game_.transposition_table_.RecordTransposition(
            Transposition{CurrentPosition().hash_, best_move, alpha, depth, Transposition::NodeType::kPv});
        history_.RecordGoodMove(ply, best_move);
        RecordContinuationHistory(prev_move, best_move);
        // parent_pv was already set, with the correct line, at the alpha-raise above.
    }
    else
    {
        // Searched all moves without raising alpha: all-node.
        INCREMENT("all nodes");
        game_.transposition_table_.RecordTransposition(
            Transposition{CurrentPosition().hash_, best_move, best_score, depth, Transposition::NodeType::kAll});
    }
    return best_score;
}

inline int SearchState::SearchQuiescent(int depth, int ply, int alpha, int beta)
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
            history_.RecordGoodMove(ply, move);
            return score;
        }
        if (score > best_score)
        {
            best_score = score;
            if (score > alpha)
            {
                alpha = score;
                INCREMENT("quiescent pv changed");
                history_.RecordGoodMove(ply, move);
            }
        }
    }
    return best_score;
}

// ----------------------------------------------------------------------------
// Iterative deepening — one Lazy SMP thread. Game::SearchRootNode (in game.cpp)
// launches one of these per thread.
// ----------------------------------------------------------------------------

inline Move SearchState::IterativeDeepen(MoveList move_list, Move best_move, int thread_id)
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
        if (game_.time_control_.clock_type_ == ChessClock::kFixedDepth && depth > game_.time_control_.depth_)
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
        if (is_main)
        {
            best_moves.push_back(best_move);
        }
        for (auto &move : move_list)
        {
            const int move_index = (int)(&move - move_list.begin());
            child_pv.clear(); // clear per move so a terminal/non-PV-node child leaves no stale tail
            const int score = SearchSingleMove(depth, 0, alpha, kBeta, move, child_pv, move_index).score_;
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
                    game_.ponder_move_ = principal_variation.size() >= 2 ? principal_variation[1] : Move::None();
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
                    const long long elapsed_ms = game_.time_control_.ElapsedSinceSearchStart().count();
                    const long long nps =
                        static_cast<long long>(node_count_) * 1000 / std::max<long long>(1, elapsed_ms);
                    std::cout << std::format("info depth {:2} score {} time {:5} nodes {:8} nps {} hashfull {} pv {}\n",
                                             depth, score_string, elapsed_ms, node_count_, nps,
                                             game_.transposition_table_.HashfullPermille(), pv_string);
                }
            }
        }
        if (alpha > kWinThreshold || alpha < kLoseThreshold)
        {
            break;
        }
        // Soft time management: never stop while pondering (no real clock is running yet); once `ponderhit`
        // has reset the search-start instant, elapsed is measured from the ponderhit moment.
        if (is_main && game_.time_control_.clock_type_ == ChessClock::kStandard &&
            !game_.is_pondering_.load(std::memory_order_relaxed))
        {
            const auto allocated = game_.time_control_.allocated_time_;
            const auto elapsed   = game_.time_control_.ElapsedSinceSearchStart();
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

// ---- Game::SearchRootNode (moved from game.cpp; needs SearchState complete) ----

/// @brief Search the current position and return the best move.
/// Returns a book move immediately on a book hit; otherwise runs the Lazy SMP iterative-deepening search:
/// one authoritative main thread plus (Game::thread_count - 1) helpers, all sharing the transposition table.
/// @return The best move, or Move::None if there are no legal moves.
inline Move Game::SearchRootNode()
{
    Game &game        = *this;        // local alias so the search body reads uniformly through `game`
    game.ponder_move_ = Move::None(); // set by the main thread's IterativeDeepen if the PV has a reply move
    // Clear the stop flag (set true by StartThinking's StopThinking) up front — BEFORE the book / single-move
    // early returns. Otherwise a leftover cancel makes SearchThreadEntry's ponder-wait exit immediately and
    // emit a premature `bestmove` when `go ponder` lands on a book/forced position (which returns without search).
    game.is_cancel_pending_.store(false, std::memory_order_relaxed);
    game.last_search_node_count_ = 0; // set below once a real search runs (stays 0 for book/forced returns)
    // If there is a book move for this position, do not bother with search (unless OwnBook is disabled).
    if (game.use_own_book_)
    {
        Move book_move = game.book_.GetMove(game.CurrentPosition().hash_);
        if (book_move != Move::None())
        {
            return book_move;
        }
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
    using Duration           = ChessClock::Duration;
    Duration allocated_time  = Duration::zero(); // soft budget (drives between-iteration stopping)
    Duration max_search_time = Duration::zero(); // hard budget (deadline); zero means no hard stop
    switch (game.time_control_.clock_type_)
    {
    case ChessClock::kStandard:
    default: {
        const int moves_to_go = game.time_control_.moves_to_go_ != 0
                                    ? game.time_control_.moves_to_go_
                                    : std::max(40 - game.CurrentPosition().full_move_count_, 5);
        // Soft budget: an even slice of the remaining time. NOTE: folding the per-move increment in here
        // (`+ increment / 2`) was SPRT-tested and did NOT help — neutral-to-slightly-negative at both 8+0.08
        // (-14.5 +/- 21, 480 games) and 10+0.1 (-2.5 +/- 7.9, 3562 games), zero forfeits. In equal-clock
        // self-play at fast TCs spending the increment is ~symmetric; it may pay off at much slower increment
        // TCs (untested). `ChessClock::increment` is still parsed (winc/binc) but deliberately unused here.
        allocated_time = game.time_control_.remaining_time_ / moves_to_go;
        // Hard budget: up to 2x the soft budget, but never spend into the last second of our clock, and always
        // allow at least 100 ms.
        max_search_time =
            std::max(Duration{100}, std::min(allocated_time * 2, game.time_control_.remaining_time_ - Duration{1000}));
        break;
    }
    case ChessClock::kFixedDepth:
        break; // no time limit; the depth loop bounds the search
    case ChessClock::kFixedTime:
        max_search_time = game.time_control_.remaining_time_; // movetime: a hard budget, no soft management
        break;
    case ChessClock::kInfinite:
        break; // no time limit; only `stop` ends it
    }
    // Reserve the configured move overhead (GUI/network lag) from the hard deadline so we don't forfeit on
    // time. Only when there is a hard deadline (fixed-depth / infinite searches leave it zero = no stop).
    if (max_search_time > Duration::zero())
    {
        max_search_time = std::max(Duration{1}, max_search_time - game.move_overhead_);
    }

    // Arm the clock. A ponder search has no hard deadline until `ponderhit` (budget-from-ponderhit); the
    // budgets are recorded so the conversion can start the real clock from that moment.
    if (game.is_pondering_)
    {
        game.time_control_.StartPonderSearch(allocated_time, max_search_time);
    }
    else
    {
        game.time_control_.StartSearch(allocated_time, max_search_time);
    }
    DebugXClear();
    game.transposition_table_.Age();

    // Shallow pass for initial move ordering (on a temporary state shared as the launch ordering).
    SearchState state{game};
    Variation   shallow_pv{};
    for (auto &move : move_list)
    {
        const int move_index = (int)(&move - move_list.begin());
        const int score = state.SearchSingleMove(kStartDepth, 0, kAlpha, kBeta, move, shallow_pv, move_index).score_;
        move.AssignScore(score);
    }
    SortMoves<true>(move_list);
    Move best_move = move_list[0];

    // Lazy SMP: each thread runs an independent iterative-deepening search of the same root, sharing the
    // (lockless) transposition table and (atomic) history table. The main thread is authoritative; helper
    // threads deepen the shared TT to accelerate it. Each thread has its own SearchState and move-list copy.
    // Thread count is configured via the UCI `Threads` option (or the PAWNSTAR_THREADS default at startup),
    // stored in game.thread_count; re-clamped here defensively.
    const int                n_threads = std::clamp(game.thread_count_, 1, kMaxSearchThreads);
    std::vector<std::thread> helpers;
    helpers.reserve(n_threads - 1);
    for (int i = 1; i < n_threads; ++i)
    {
        // Each helper gets a distinct thread id, which selects its iteration-skip schedule (see IterativeDeepen).
        helpers.emplace_back([&game, &move_list, best_move, i] {
            SearchState helper_state{game};
            helper_state.IterativeDeepen(move_list, best_move, /*thread_id=*/i);
        });
    }

    best_move                    = state.IterativeDeepen(move_list, best_move, /*thread_id=*/0);
    game.last_search_node_count_ = state.node_count_; // main-thread node total (used by `bench`)

    game.is_cancel_pending_.store(true, std::memory_order_relaxed); // signal helpers to stop
    for (auto &helper : helpers)
    {
        helper.join();
    }
    return best_move;
}

// ---- EvaluatePosition (moved from evaluation.cpp; needs SearchState + Game complete) ----

/// @brief Plies the fifty-move clock may reach before the static eval is discounted. Below this the eval
/// is untouched, so normal play (where the clock is reset often by pawn moves/captures) is unaffected.
inline constexpr int kRule50ScaleThreshold = 20;

/// @brief Scale the static eval toward draw as the fifty-move clock climbs, so the search has a standing
/// incentive to make progress (reset the clock) rather than shuffle in a won-but-not-yet-converted
/// position. NNUE inputs are piece placements only, so without this a winning eval is identical at clock
/// 0 and clock 99 — the search sees no gradient distinguishing progress from shuffling until the
/// fifty-move draw enters the horizon, and leaks won endgames to the rule (notably KBN-vs-K).
/// Identity for clock <= kRule50ScaleThreshold; then a linear ramp to 0 as the clock approaches 100 (the
/// hard draw, handled by the IsDrawByFiftyMoves short-circuit). @param rule50 reversible-move count (plies).
inline int ScaleByRule50(int score, int rule50)
{
    if (rule50 <= kRule50ScaleThreshold)
    {
        return score;
    }
    // num falls 80..1 over rule50 21..99; den is the ramp length. A progress move (clock reset to 0) keeps
    // the full eval, a shuffle (clock +1) keeps less — the difference is the gradient that drives progress.
    const int num = 100 - rule50;
    const int den = 100 - kRule50ScaleThreshold;
    return static_cast<int>(static_cast<int64_t>(score) * num / den);
}

/// @brief Evaluate the current position, assuming neither king is in check and the position is quiet.
/// NNUE is the only evaluator: after the draw short-circuit this returns the network's score for the
/// (incrementally maintained) accumulator, memoised by Zobrist hash in the net-specific eval cache, then
/// scaled toward draw by the fifty-move clock (see ScaleByRule50).
/// @param state Per-thread search state providing the current position, accumulator and draw-detection.
/// @return Score from the perspective of the side to move.
inline int EvaluatePosition(const SearchState &state)
{
    INCREMENT("eval calls");
    if (state.CurrentPosition().IsDrawByMaterial() || state.IsDrawByFiftyMoves() || state.IsDrawByRepetition())
    {
        return kDrawScore;
    }
    const Position &position = state.CurrentPosition();
    // Memoise the (net-specific) NNUE eval by Zobrist hash: a cache hit skips the forward pass. The
    // accumulator is maintained incrementally by SearchState make/undo; the cache only saves the
    // dot-product. The cache is cleared whenever the net changes (SetPosition / EvalFile load).
    //
    // The cached value is the RAW, clock-independent NNUE eval: the Zobrist hash excludes the fifty-move
    // clock, so the same position can be probed at different clock values. The fifty-move scaling is
    // therefore applied AFTER the cache, on the way out — never stored.
    const zobrist_t hash = position.hash_;
    int             raw;
    if (state.game_.eval_cache_.Probe(hash, raw))
    {
        INCREMENT("eval hash hits");
        return ScaleByRule50(raw, position.reversible_move_count_);
    }
    raw = state.game_.nnue_network_.Evaluate(state.accumulator_, position.color_to_move_);
    state.game_.eval_cache_.Store(hash, raw);
    return ScaleByRule50(raw, position.reversible_move_count_);
}

#pragma once
/// @file search_state.h Per-thread search state for the parallel alpha-beta search.

#include "constants.h"
#include "debug_hashtable.h"
#include "history_table.h"
#include "move.h"
#include "nnue.h"
#include "position.h"
#include "stack_list.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <iterator>
#include <vector>

class Game;

/// @brief Compact position hash entry for draw-by-repetition detection.
struct HashEntry
{
    zobrist_t hash;                  ///< Zobrist hash of the position.
    uint8_t   reversible_move_count; ///< Half-move clock at this position.
};

/// @brief A variation (typically principal variation) is a line of play or series of moves.
using Variation = StackList<Move, kMaxPly>;

/// @brief Result of searching a single move: its score and whether it gives check.
struct SingleMoveResult
{
    int  score;       ///< Alpha-beta score for the move.
    bool is_checking; ///< True if the move gives check.
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
    Game                                    &game;         ///< Shared state: TT, clock, cancellation flag.
    HistoryTable                             history{};    ///< Per-thread history heuristic counts (no sharing).
    std::array<std::array<Move, 2>, kMaxPly> killers{};    ///< Killer moves indexed by ply.
    int                                      node_count{}; ///< Nodes searched by this thread.

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

    /// @brief The NNUE accumulator for the current position, maintained incrementally across make/undo.
    /// Only valid (kept in sync) while game.NnueActive(); ignored otherwise.
    /// @return Const reference to the current accumulator.
    const nnue::Accumulator &CurrentAccumulator() const
    {
        return accumulator_;
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
    void ScoreAndSortMoves(MoveList &moves, int ply) const;

    /// @brief Fail-soft negamax alpha-beta search of the current node (defined in search_state.cpp).
    /// @param depth Depth to search. @param ply Distance from root. @param alpha Alpha. @param beta Beta.
    /// @param parent_pv Principal variation at the parent node. @return The alpha-beta score for this node.
    int Search(int depth, int ply, int alpha, int beta, Variation &parent_pv);

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
    /// @param start_ms Search start time (elapsed ms). @param ms_allocated Soft time budget (standard clock).
    /// @return The best move found by this thread.
    Move IterativeDeepen(MoveList move_list, Move best_move, int thread_id, int64_t start_ms, int ms_allocated);

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
        if (!(killers[ply][0] == move))
        {
            INCREMENT("killer moves");
            killers[ply][1] = killers[ply][0];
            killers[ply][0] = move;
        }
    }

  private:
    StackList<Position, kMaxPly + 4> positions_;    ///< Per-thread copy-make position stack.
    nnue::Accumulator                accumulator_{}; ///< NNUE accumulator for the tip position (incremental).
    /// @brief Hash history from game-start through parent of current node (game history + search depth).
    /// A std::vector (reserved up-front in the constructor) so an arbitrarily long game cannot overflow it.
    std::vector<HashEntry> hash_stack_;
};

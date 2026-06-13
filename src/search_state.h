#pragma once
/// @file search_state.h Per-thread search state for the parallel alpha-beta search.

#include "constants.h"
#include "history_table.h"
#include "move.h"
#include "position.h"
#include "stack_list.h"

#include <array>
#include <atomic>
#include <cstdint>

class Game;

/// @brief Compact position hash entry for draw-by-repetition detection.
struct HashEntry
{
    zobrist_t hash;                  ///< Zobrist hash of the position.
    uint8_t   reversible_move_count; ///< Half-move clock at this position.
};

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
            killers[ply][1] = killers[ply][0];
            killers[ply][0] = move;
        }
    }

  private:
    StackList<Position, kMaxPly + 4> positions_; ///< Per-thread copy-make position stack.
    /// @brief Hash history from game-start through parent of current node (game history + search depth).
    StackList<HashEntry, kMaxGameLength + kMaxPly + 4> hash_stack_;
};

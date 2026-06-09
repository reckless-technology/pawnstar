#pragma once
/// @file Per-thread search state for the parallel alpha-beta search.

#include "constants.h"
#include "move.h"
#include "position.h"
#include "stack_list.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

class Game;

/// @brief Compact position hash entry for draw-by-repetition detection.
struct HashEntry
{
    zobrist_t hash;                  ///< Zobrist hash of the position.
    uint8_t   reversible_move_count; ///< Half-move clock at this position.
};

/// @brief Per-thread search state for the parallel alpha-beta search.
/// Each parallel worker owns an independent SearchState with its own position stack and hash
/// history, while sharing the transposition table, history table, and time control through the
/// Game reference.
class SearchState
{
  public:
    Game &                                   game;         ///< Shared state: TT, history, clock, cancellation flag.
    std::array<std::array<Move, 2>, kMaxPly> killers{};    ///< Killer moves indexed by ply.
    int                                      node_count{}; ///< Nodes searched by this thread.
    std::atomic<bool> *                      batch_cutoff; ///< Per-batch abort flag; nullptr on the main thread.

    /// @brief Construct the main-thread search state from the current game.
    /// Builds the full hash history from the game's position stack and seeds the per-thread
    /// position stack with the current position only.
    /// @param game Game to search.
    explicit SearchState(Game &game);

    /// @brief Construct a worker search state forked from a parent state.
    /// Copies the parent's full hash history and seeds the position stack with the parent's
    /// current position. Killers and node count start fresh.
    /// @param parent Parent search state.
    /// @param cutoff Per-batch abort flag shared with sibling workers.
    SearchState(const SearchState &parent, std::atomic<bool> *cutoff);

    /// @brief Current position at the tip of the search tree.
    Position &CurrentPosition()
    {
        return positions_.back();
    }

    /// @brief Current position at the tip of the search tree (const overload).
    const Position &CurrentPosition() const
    {
        return positions_.back();
    }

    void PlayMove(Move move);
    void UndoMove();
    void MakeNullMove();
    void ScoreAndSortMoves(MoveList &moves, int ply) const;
    bool IsDrawByRepetition() const;
    bool IsDrawByFiftyMoves() const;

    /// @brief Returns true if this thread should abort: the global time limit fired, or a
    /// sibling worker in the current batch found a beta cutoff.
    bool IsCancelled() const;

    /// @brief Signal that this worker's batch should be cut off (beta cutoff found).
    void SignalBatchCutoff()
    {
        if (batch_cutoff != nullptr)
            batch_cutoff->store(true, std::memory_order_relaxed);
    }

  private:
    StackList<Position, kMaxPly + 4> positions_;  ///< Per-thread copy-make position stack.
    std::vector<HashEntry>           hash_stack_;  ///< Hash history from game-start through parent of current node.

    /// @brief Piece values for MVV/LVA move scoring (indexed by Piece enum).
    static constexpr std::array<int, 7> kPieceValues{0, 100, 300, 300, 500, 900, 10000};
};

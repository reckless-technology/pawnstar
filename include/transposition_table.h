#pragma once
#include <atomic>
#include <memory>
#include <optional>
#include <vector>

#include "move.h"

/// @brief A transposition table entry.
/// A transposition is the result of a previous search containing pertinent information about what was found when this
/// position was searched earlier.
struct Transposition
{
    /// @brief Alpha beta search tree node types.
    enum NodeType : uint8_t
    {
        kCut, ///< Beta cutoff occurred.
        kAll, ///< No move exceeded alpha.
        kPv,  ///< Principal variation node.
    };

    zobrist_t hash;      ///< Zobrist hash of this position.
    Move      move;      ///< Best move from this position.
    int       score;     ///< The score computed from this position.
    int16_t   depth;     ///< The depth to which this position was searched.
    NodeType  node_type; ///< What type of result was this.
    bool      is_old;    ///< This entry is from a previous search and can thus be replaced.

    constexpr Transposition()
        : hash(0), move(Move::None()), score(0), depth(0), node_type(NodeType::kCut), is_old(false)
    {
    }

    constexpr Transposition(zobrist_t hash, const Move &move, int score, int depth, NodeType type)
        : hash(hash), move(move), score(score), depth((int16_t)depth), node_type(type), is_old(false)
    {
    }
};

static_assert(sizeof(Transposition) == 24);

/// @brief Class to hold the transposition table.
class TranspositionTable
{
  public:
    TranspositionTable(std::size_t megabytes);
    std::optional<Transposition> FindTransposition(zobrist_t hash) const;
    void                         RecordTransposition(const Transposition &transposition);
    void                         Age();
    std::pair<std::size_t, int>  UsageStats() const;

  private:
    std::vector<Transposition>              table_; ///< Indexed using Zobrist hash.
    mutable std::unique_ptr<std::atomic_flag[]> locks_; ///< Per-entry spinlocks, parallel to table_.
};
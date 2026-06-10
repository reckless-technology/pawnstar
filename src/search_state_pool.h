#pragma once
/// @file Slab allocator for per-worker SearchState objects used by the parallel search.

#include "constants.h"
#include "search_state.h"

#include <array>
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <vector>

/// @brief Slab allocator for SearchState objects.
/// Pre-allocates a fixed bank of raw storage slots and hands them out via placement-new.
/// Callers block on acquire() when all slots are in use, providing natural back-pressure.
class SearchStatePool
{
  public:
    SearchStatePool();
    ~SearchStatePool() = default;

    /// @brief Acquire a slot and construct a worker SearchState in-place.
    /// Blocks until a slot is available.
    /// @param parent Parent search state whose position and hash history are copied.
    /// @param cutoff Per-batch abort flag shared with sibling workers.
    /// @return Pointer into the slab where the SearchState was constructed.
    SearchState *acquire(const SearchState &parent, std::atomic<bool> *cutoff);

    /// @brief Destruct the SearchState and return its slot to the pool.
    /// @param state Pointer previously returned by acquire().
    void release(SearchState *state);

    /// @brief Number of slots currently available.
    int available() const;

  private:
    /// @brief Raw storage for one SearchState, aligned to SearchState's alignment requirement.
    struct alignas(alignof(SearchState)) Slot
    {
        std::byte data[sizeof(SearchState)]; ///< Uninitialized storage for a SearchState.
    };

    std::array<Slot, kSearchStatePoolCapacity> slab_;         ///< Pre-allocated storage slab.
    std::vector<int>                           free_indices_; ///< Indices of currently available slots.
    mutable std::mutex                         mutex_;
    std::condition_variable                    cv_;
};

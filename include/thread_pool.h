#pragma once
/// @file Thread pool and SearchState slab allocator for parallel search.

#include "search_state.h"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

/// @brief Fixed-size thread pool. Tasks are queued and executed by persistent worker threads.
class ThreadPool
{
  public:
    /// @brief Construct the thread pool.
    /// @param n_threads Number of worker threads; 0 means hardware_concurrency() - 1 (minimum 1).
    explicit ThreadPool(unsigned n_threads = 0);
    ~ThreadPool();

    /// @brief Enqueue a task for execution on a worker thread.
    /// @param task Callable to run.
    void submit(std::function<void()> task);

    /// @brief Number of threads currently idle (available for immediate dispatch).
    int idle_count() const
    {
        return idle_count_.load(std::memory_order_relaxed);
    }

  private:
    std::vector<std::thread>          workers_;
    std::deque<std::function<void()>> queue_;
    std::mutex                        mutex_;
    std::condition_variable           cv_;
    std::atomic<int>                  idle_count_{0};
    bool                              stop_{false};
};

/// @brief Slab allocator for SearchState objects.
/// Pre-allocates a fixed bank of raw storage slots and hands them out via placement-new.
/// Callers block on acquire() when all slots are in use, providing natural back-pressure.
class SearchStatePool
{
  public:
    /// @brief Maximum number of concurrently live worker search states.
    static constexpr int kCapacity = 64;

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

    std::array<Slot, kCapacity> slab_;         ///< Pre-allocated storage slab.
    std::vector<int>            free_indices_;  ///< Indices of currently available slots.
    mutable std::mutex          mutex_;
    std::condition_variable     cv_;
};

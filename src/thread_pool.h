#pragma once
/// @file Thread pool and SearchState slab allocator for parallel search.

#include "search_state.h"

#include <array>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

/// @brief A unit of work for the thread pool: a function pointer and a non-owning argument.
/// The pool never takes ownership of @c arg; the caller must keep it alive until the task runs
/// (the parallel search blocks on a latch until its whole batch completes, so the argument lives
/// in the caller's stack frame for the batch's duration). Using a plain function pointer instead
/// of std::function avoids both heap allocation and type-erasure overhead per dispatch.
struct Task
{
    void (*fn)(void *); ///< Function to invoke.
    void *arg;          ///< Argument passed to fn (not owned by the pool).
};

/// @brief Fixed-size thread pool. Tasks are queued and executed by persistent worker threads.
class ThreadPool
{
  public:
    /// @brief Construct the thread pool.
    /// @param n_threads Number of worker threads; 0 means hardware_concurrency() - 1 (minimum 1).
    explicit ThreadPool(unsigned n_threads = 0);
    ~ThreadPool();

    /// @brief Enqueue a task for execution on a worker thread.
    /// @param task Function pointer and argument to run.
    void submit(Task task);

    /// @brief Number of threads currently idle (available for immediate dispatch).
    int idle_count() const
    {
        return idle_count_.load(std::memory_order_relaxed);
    }

  private:
    /// @brief Capacity of the task ring buffer. A power of two larger than SearchStatePool::kCapacity:
    /// acquire() back-pressure caps the number of outstanding tasks at kCapacity, so the queue never
    /// fills and no heap allocation is ever needed.
    static constexpr int kQueueCapacity = 128;

    std::vector<std::thread>          workers_;
    std::array<Task, kQueueCapacity>  queue_;       ///< Fixed-capacity ring buffer of pending tasks.
    int                               head_{0};     ///< Index of the next task to pop.
    int                               tail_{0};     ///< Index of the next free slot to push.
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

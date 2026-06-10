#pragma once
/// @file Fixed-size thread pool for parallel search.

#include "constants.h"

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
    // The ring buffer capacity (kThreadPoolQueueCapacity) is a power of two larger than the search-state
    // pool capacity: acquire() back-pressure caps the number of outstanding tasks, so the queue never
    // fills and no heap allocation is ever needed.
    std::vector<std::thread>                   workers_;
    std::array<Task, kThreadPoolQueueCapacity> queue_;   ///< Fixed-capacity ring buffer of pending tasks.
    int                                        head_{0}; ///< Index of the next task to pop.
    int                                        tail_{0}; ///< Index of the next free slot to push.
    std::mutex                                 mutex_;
    std::condition_variable                    cv_;
    std::atomic<int>                           idle_count_{0};
    bool                                       stop_{false};
};

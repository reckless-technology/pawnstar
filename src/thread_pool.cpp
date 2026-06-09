/// @file Thread pool and SearchState slab allocator implementation.

#include "thread_pool.h"

// ─── ThreadPool ──────────────────────────────────────────────────────────────

/// @brief Construct the thread pool, launching n_threads persistent worker threads.
ThreadPool::ThreadPool(unsigned n_threads)
{
    if (n_threads == 0)
    {
        unsigned hw = std::thread::hardware_concurrency();
        n_threads   = hw > 1u ? hw - 1u : 1u;
    }
    workers_.reserve(n_threads);
    for (unsigned i = 0; i < n_threads; ++i)
    {
        workers_.emplace_back([this] {
            for (;;)
            {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(mutex_);
                    ++idle_count_;
                    cv_.wait(lock, [this] { return stop_ || !queue_.empty(); });
                    --idle_count_;
                    if (stop_ && queue_.empty())
                        return;
                    task = std::move(queue_.front());
                    queue_.pop_front();
                }
                task();
            }
        });
    }
}

/// @brief Signal all threads to stop and join them.
ThreadPool::~ThreadPool()
{
    {
        std::unique_lock<std::mutex> lock(mutex_);
        stop_ = true;
    }
    cv_.notify_all();
    for (auto &w : workers_)
        w.join();
}

/// @brief Enqueue a task and wake one idle worker.
void ThreadPool::submit(std::function<void()> task)
{
    {
        std::unique_lock<std::mutex> lock(mutex_);
        queue_.push_back(std::move(task));
    }
    cv_.notify_one();
}

// ─── SearchStatePool ─────────────────────────────────────────────────────────

/// @brief Populate the free-index list with all kCapacity slot indices.
SearchStatePool::SearchStatePool()
{
    free_indices_.reserve(kCapacity);
    for (int i = kCapacity - 1; i >= 0; --i)
        free_indices_.push_back(i);
}

/// @brief Acquire a slot (blocking if all are in use) and construct a SearchState in it.
SearchState *SearchStatePool::acquire(const SearchState &parent, std::atomic<bool> *cutoff)
{
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait(lock, [this] { return !free_indices_.empty(); });
    const int idx  = free_indices_.back();
    free_indices_.pop_back();
    return new (slab_[idx].data) SearchState(parent, cutoff);
}

/// @brief Destruct the SearchState and return its slot.
void SearchStatePool::release(SearchState *state)
{
    // Compute the slot index from the pointer without storing it in the SearchState itself.
    const int idx = (int)(reinterpret_cast<Slot *>(state) - slab_.data());
    state->~SearchState();
    {
        std::unique_lock<std::mutex> lock(mutex_);
        free_indices_.push_back(idx);
    }
    cv_.notify_one();
}

/// @brief Count available slots without blocking.
int SearchStatePool::available() const
{
    std::unique_lock<std::mutex> lock(mutex_);
    return (int)free_indices_.size();
}

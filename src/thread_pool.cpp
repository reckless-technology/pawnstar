/// @file thread_pool.cpp Thread pool implementation.

#include "thread_pool.h"

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
                Task task;
                {
                    std::unique_lock<std::mutex> lock(mutex_);
                    ++idle_count_;
                    cv_.wait(lock, [this] { return stop_ || head_ != tail_; });
                    --idle_count_;
                    if (stop_ && head_ == tail_)
                        return;
                    task  = queue_[head_];
                    head_ = (head_ + 1) % kThreadPoolQueueCapacity;
                }
                task.fn(task.arg);
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
void ThreadPool::submit(Task task)
{
    {
        std::unique_lock<std::mutex> lock(mutex_);
        queue_[tail_] = task;
        tail_         = (tail_ + 1) % kThreadPoolQueueCapacity;
    }
    cv_.notify_one();
}

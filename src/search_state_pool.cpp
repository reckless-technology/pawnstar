/// @file SearchState slab allocator implementation.

#include "search_state_pool.h"

/// @brief Populate the free-index list with all slot indices.
SearchStatePool::SearchStatePool()
{
    free_indices_.reserve(kSearchStatePoolCapacity);
    for (int i = kSearchStatePoolCapacity - 1; i >= 0; --i)
        free_indices_.push_back(i);
}

/// @brief Acquire a slot (blocking if all are in use) and construct a SearchState in it.
SearchState *SearchStatePool::acquire(const SearchState &parent, std::atomic<bool> *cutoff)
{
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait(lock, [this] { return !free_indices_.empty(); });
    const int idx = free_indices_.back();
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

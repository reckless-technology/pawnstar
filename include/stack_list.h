#pragma once

#include <array>

/// @brief Fixed size, very simple stack storage container for lists of moves.
/// Considerably faster than std::vector due to not allocating data on the heap. More convenient than a raw std::array
/// for insertion, sorting and iteration. Perft speed is over 2x faster using this in place of std::vector for move
/// generation, even when using reserve to preallocate the heap buffer for the vector. NB: There is no buffer overflow
/// protection, so use with caution.
template <typename T, int N> class StackList
{
  public:
    constexpr StackList()
    {
        end_ = data_.begin();
    }
    constexpr StackList(const StackList &that)
    {
        std::copy(that.begin(), that.end(), this->begin());
        end_ = data_.begin() + that.size();
    }
    constexpr StackList &operator=(const StackList &that)
    {
        std::copy(that.begin(), that.end(), this->begin());
        end_ = data_.begin() + that.size();
        return *this;
    }
    constexpr void push_back(const T &m)
    {
        *end_++ = m;
    }
    constexpr T pop_back()
    {
        return *--end_;
    }
    constexpr T &back()
    {
        return end_[-1];
    }
    constexpr const T &back() const
    {
        return end_[-1];
    }
    constexpr void clear()
    {
        end_ = data_.begin();
    }
    constexpr T &operator[](std::size_t i)
    {
        return data_[i];
    }
    constexpr const T &operator[](std::size_t i) const
    {
        return data_[i];
    }
    constexpr std::size_t size() const
    {
        return end_ - data_.begin();
    }
    constexpr T *begin()
    {
        return data_.begin();
    }
    constexpr T *end()
    {
        return end_;
    }
    constexpr const T *begin() const
    {
        return data_.begin();
    }
    constexpr const T *end() const
    {
        return end_;
    }

  private:
    std::array<T, N>           data_;
    std::array<T, N>::iterator end_;
};
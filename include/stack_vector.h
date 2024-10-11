#pragma once
/// @brief Fixed size, very simple stack storage "vector" for containing lists of moves.
/// Considerably faster than std::vector due to not allocating data on the heap. More convenient than a std::array for
/// insertion, sorting and iteration. Perft speed is over 2x faster using this in place of std::vector for move
/// generation, even when using reserve to preallocate the heap buffer for the vector. NB: There is no buffer overflow
/// protection.
template <typename T, int N> class StackVector
{
  public:
    constexpr StackVector()
    {
        end_ = data_;
    }
    constexpr StackVector(const StackVector &that)
    {
        std::copy(that.begin(), that.end(), this->begin());
        end_ = data_ + that.size();
    }
    constexpr StackVector &operator=(const StackVector &that)
    {
        std::copy(that.begin(), that.end(), this->begin());
        end_ = data_ + that.size();
        return *this;
    }
    constexpr void push_back(const T &m)
    {
        *end_++ = m;
    }
    constexpr void clear()
    {
        end_ = data_;
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
        return end_ - data_;
    }
    constexpr T *begin()
    {
        return data_;
    }
    constexpr T *end()
    {
        return end_;
    }
    constexpr const T *begin() const
    {
        return data_;
    }
    constexpr const T *end() const
    {
        return end_;
    }

  private:
    T *end_;
    T  data_[N];
};
#pragma once
/// @brief Fixed size, simple stack storage "vector" for containing lists of moves.
/// Considerably faster than std::vector due to not allocating data on the heap.
/// More convenient than a std::array for insertion, sorting and iteration.
/// Perft speed is over 2x faster using this in place of std::vector<Move> for move generation.
/// NB: There is no buffer overflow protection.
template <typename T, int N> class StackVector
{
  public:
    StackVector() : end_(data_)
    {
    }
    void push_back(const T &m)
    {
        *end_++ = m;
    }
    void clear()
    {
        end_ = data_;
    }
    T &operator[](std::size_t i)
    {
        return data_[i];
    }
    const T &operator[](std::size_t i) const
    {
        return data_[i];
    }
    std::size_t size() const
    {
        return end_ - data_;
    }

    /// @brief Iterator class for the simple stack based vector.
    template <typename I> struct Iterator
    {
        using iterator_category = std::random_access_iterator_tag;
        using value_type        = I;
        using difference_type   = std::ptrdiff_t;
        using pointer           = I *;
        using reference         = I &;
        Iterator(I *m) : ptr(m)
        {
        }
        I &operator*()
        {
            return *ptr;
        }
        Iterator &operator++()
        {
            ptr++;
            return *this;
        }
        Iterator &operator--()
        {
            ptr--;
            return *this;
        }
        Iterator &operator+=(int i)
        {
            ptr += i;
            return *this;
        }
        friend bool operator==(const Iterator &a, const Iterator &b)
        {
            return a.ptr == b.ptr;
        };
        friend bool operator<(const Iterator &a, const Iterator &b)
        {
            return a.ptr < b.ptr;
        }
        friend difference_type operator-(const Iterator &a, const Iterator &b)
        {
            return a.ptr - b.ptr;
        }
        friend Iterator operator-(const Iterator &a, int b)
        {
            Iterator tmp{a};
            tmp.ptr -= b;
            return tmp;
        }
        friend Iterator operator+(const Iterator &a, int b)
        {
            Iterator tmp{a};
            tmp.ptr += b;
            return tmp;
        }

      private:
        I *ptr;
    };
    Iterator<T> begin()
    {
        return Iterator<T>(data_);
    }
    Iterator<T> end()
    {
        return Iterator<T>(end_);
    }
    Iterator<const T> begin() const
    {
        return Iterator<const T>(data_);
    }
    Iterator<const T> end() const
    {
        return Iterator<const T>(end_);
    }

  private:
    T  data_[N];
    T *end_;
};
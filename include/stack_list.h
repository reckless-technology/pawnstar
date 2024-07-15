#pragma once

/**
 * @brief Fixed size, stack storage "vector" for containing lists of moves.
 * Considerably faster than std::vector due to not allocating data on the heap.
 * Perft speed is over 2x faster using this instead of std::vector<Move> for move generation.
 */
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
    struct Iterator
    {
        using iterator_category = std::random_access_iterator_tag;
        using value_type        = T;
        using difference_type   = std::ptrdiff_t;
        using pointer           = T *;
        using reference         = T &;
        Iterator(value_type *m) : ptr(m)
        {
        }
        value_type &operator*()
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
        T *ptr;
    };
    Iterator begin()
    {
        return Iterator(data_);
    }
    Iterator end()
    {
        return Iterator(end_);
    }

  private:
    T  data_[N];
    T *end_;
};
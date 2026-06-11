#pragma once
/// @file stack_list.h Fixed-capacity stack-allocated list container.

#include <array>

/// @brief Fixed size, very simple stack storage container for lists of moves.
/// Considerably faster than std::vector due to not allocating data on the heap. More convenient than a raw std::array
/// for insertion, sorting and iteration. Perft speed is over 2x faster using this in place of std::vector for move
/// generation, even when using reserve to preallocate the heap buffer for the vector. NB: There is no buffer overflow
/// protection, so use with caution.
/// @tparam T Element type.
/// @tparam N Fixed capacity (maximum number of elements).
template <typename T, int N> class StackList
{
  public:
    using value_type = T; ///< Element type.

    /// @brief Construct an empty list.
    constexpr StackList()
    {
        end_ = data_.begin();
    }

    /// @brief Copy constructor.
    /// @param that List to copy.
    constexpr StackList(const StackList &that)
    {
        std::copy(that.begin(), that.end(), this->begin());
        end_ = data_.begin() + that.size();
    }

    /// @brief Copy assignment.
    /// @param that List to copy.
    /// @return Reference to this list.
    constexpr StackList &operator=(const StackList &that)
    {
        std::copy(that.begin(), that.end(), this->begin());
        end_ = data_.begin() + that.size();
        return *this;
    }

    /// @brief Append an element to the end of the list.
    /// @param m Element to append.
    constexpr void push_back(const T &m)
    {
        *end_++ = m;
    }

    /// @brief Remove and return the last element.
    /// @return The removed element.
    constexpr T pop_back()
    {
        return *--end_;
    }

    /// @brief Access the last element.
    /// @return Reference to the last element.
    constexpr T &back()
    {
        return end_[-1];
    }

    /// @brief Access the last element (const overload).
    /// @return Const reference to the last element.
    constexpr const T &back() const
    {
        return end_[-1];
    }

    /// @brief Remove all elements.
    constexpr void clear()
    {
        end_ = data_.begin();
    }

    /// @brief Element access by index.
    /// @param i Index of the element.
    /// @return Reference to the element.
    constexpr T &operator[](std::size_t i)
    {
        return data_[i];
    }

    /// @brief Element access by index (const overload).
    /// @param i Index of the element.
    /// @return Const reference to the element.
    constexpr const T &operator[](std::size_t i) const
    {
        return data_[i];
    }

    /// @brief Number of elements currently stored.
    /// @return The element count.
    constexpr std::size_t size() const
    {
        return end_ - data_.begin();
    }

    /// @brief Iterator to the first element.
    /// @return Begin iterator.
    constexpr std::array<T, N>::iterator begin()
    {
        return data_.begin();
    }

    /// @brief Iterator one past the last element.
    /// @return End iterator.
    constexpr std::array<T, N>::iterator end()
    {
        return end_;
    }

    /// @brief Iterator to the first element (const overload).
    /// @return Const begin iterator.
    constexpr std::array<T, N>::const_iterator begin() const
    {
        return data_.begin();
    }

    /// @brief Iterator one past the last element (const overload).
    /// @return Const end iterator.
    constexpr std::array<T, N>::const_iterator end() const
    {
        return end_;
    }

  private:
    std::array<T, N>           data_; ///< Backing fixed-size storage.
    std::array<T, N>::iterator end_;  ///< Iterator one past the last in-use element.
};
#pragma once
/// @file bitboard.h Types, values and functions for using chess Bitboards.

#include <cstdint>
#include <string>

#include "square.h"

/// @brief Class to represent a bitboard.
/// A Bitboard is a set-wise interpretation of a 64-bit unsigned integer, with each bit mapping to a square on the
/// chessboard.
///
/// For example:
/// - The set of squares occupied by pawns
/// - The set of squares occupied by a black piece
/// - The set of squares to which a knight on e4 may move
/// - The set of squares attacked by black queens
/// - The set of squares containing a piece checking the white king
///
/// If the bit is 1, then the corresponding square is a member of that set.
///
/// - Bit  0 maps to square a1 (LSB)
/// - Bit  7 maps to square h1
/// - Bit 56 maps to square a8
/// - Bit 63 maps to square h8 (MSB)
///
/// This is commonly referred to as LERF (little endian rank file mapping).
class Bitboard
{
  private:
    uint64_t                  v_;                                ///< The bitboard value.
    static constexpr uint64_t kNotFileA = 0xFEFEFEFEFEFEFEFEull; ///< Mask off the a file.
    static constexpr uint64_t kNotFileH = 0x7F7F7F7F7F7F7F7Full; ///< Mask off the h file.

  public:
    constexpr Bitboard();
    constexpr Bitboard(uint64_t v);
    constexpr Bitboard(Square location);
    constexpr Bitboard(int x, int y);
    constexpr Bitboard(const Bitboard &that);
    constexpr Bitboard   &operator=(Bitboard that);
    constexpr std::string ToString() const;
    constexpr void        IsolateLsb();
    constexpr bool        operator==(Bitboard that) const;
    constexpr bool        operator!=(Bitboard that) const;
    constexpr Bitboard   &operator&=(Bitboard that);
    constexpr Bitboard   &operator|=(Bitboard that);
    constexpr Bitboard   &operator^=(Bitboard that);
    constexpr Bitboard    operator&(Bitboard that) const;
    constexpr Bitboard    operator|(Bitboard that) const;
    constexpr Bitboard    operator^(Bitboard that) const;
    constexpr Bitboard    operator~() const;
    constexpr Square      Lsb() const;
    constexpr Square      Msb() const;
    constexpr int         PopCount() const;
    constexpr bool        IsEmpty() const;
    constexpr bool        IsNotEmpty() const;
    constexpr explicit    operator uint64_t() const;
    constexpr Bitboard    ShiftNorth() const;
    constexpr Bitboard    ShiftNortheast() const;
    constexpr Bitboard    ShiftEast() const;
    constexpr Bitboard    ShiftSoutheast() const;
    constexpr Bitboard    ShiftSouth() const;
    constexpr Bitboard    ShiftSouthwest() const;
    constexpr Bitboard    ShiftWest() const;
    constexpr Bitboard    ShiftNorthwest() const;

    /// @brief Dummy sentinel class for end of input iterator range.
    class Sentinel
    {
    };

    /// @brief Input iterator to allow iteration over the squares in a Bitboard.
    /// Dereferencing the iterator returns the index of the least significant bit set.
    /// Incrementing the iterator clears the least significant bit set.
    /// This allows nice clean semantics like: "for (Square s : b)" when iterating over the bits set in a bitboard.
    class Iterator
    {
      private:
        uint64_t i_; ///< Remaining set bits still to be iterated over.

      public:
        using value_type = Square; ///< Value produced when dereferencing the iterator.
        constexpr Iterator(Bitboard bb);
        constexpr bool      operator==(const Sentinel &) const;
        constexpr Square    operator*() const;
        constexpr Iterator &operator++();
    };

    constexpr Iterator begin() const;
    constexpr Sentinel end() const;
};

/// @brief Construct an iterator over the set bits of a bitboard.
/// @param bb Bitboard whose set bits will be iterated.
constexpr Bitboard::Iterator::Iterator(Bitboard bb) : i_(bb.v_)
{
}

/// @brief Compare against the end sentinel.
/// @return true when there are no more bits to iterate over.
constexpr bool Bitboard::Iterator::operator==(const Sentinel &) const
{
    return i_ == 0;
}

/// @brief Dereference.
/// @return Square index of the least significant set bit.
constexpr Square Bitboard::Iterator::operator*() const
{
    return Square{(uint8_t)__builtin_ctzll(i_)};
}

/// @brief Advance to the next set bit by clearing the least significant set bit.
/// @return Reference to this iterator.
constexpr Bitboard::Iterator &Bitboard::Iterator::operator++()
{
    i_ &= i_ - 1;
    return *this;
}

/// @brief Default constructor.
constexpr Bitboard::Bitboard()
{
}

/// @brief Constructor.
/// @param v Value to assign.
constexpr Bitboard::Bitboard(uint64_t v) : v_(v)
{
}

/// @brief Constructor.
/// @param location Square index to convert to Bitboard.
constexpr Bitboard::Bitboard(Square location) : v_(1ull << location)
{
}

/// @brief Constructor.
/// @param x File index (0,7)
/// @param y Rank index (0,7)
constexpr Bitboard::Bitboard(int x, int y) : v_(1ull << (x + 8 * y))
{
}

/// @brief Copy constructor.
/// @param that Bitboard to be copied.
constexpr Bitboard::Bitboard(const Bitboard &that) : v_(that.v_)
{
}

/// @brief Assignment operator.
/// @param that Source Bitboard.
/// @return Assignee.
constexpr Bitboard &Bitboard::operator=(Bitboard that)
{
    v_ = that.v_;
    return *this;
}

/// @brief Convert Bitboard to 8 x 8 string for debug purposes.
/// @return Chess board string containing 1s and 0s.
constexpr std::string Bitboard::ToString() const
{
    std::string result;
    for (int y = 7; y >= 0; --y)
    {
        for (int x = 0; x < 8; ++x)
        {
            if (v_ & (1ull << (x + 8 * y)))
            {
                result.push_back('1');
            }
            else
            {
                result.push_back('0');
            }
        }
        result.push_back('\n');
    }
    return result;
}

/// @brief Clear all but the LSB, i.e. make unique square.
constexpr void Bitboard::IsolateLsb()
{
    v_ &= -v_;
}

/// @brief Equality operator.
/// @param that Comparee.
/// @return true if Bitboards are equal.
constexpr bool Bitboard::operator==(Bitboard that) const
{
    return v_ == that.v_;
}

/// @brief Inequality operator.
/// @param that Comparee.
/// @return true if Bitboards are not equal.
constexpr bool Bitboard::operator!=(Bitboard that) const
{
    return v_ != that.v_;
}

/// @brief Bitwise AND assignment.
/// @param that Source
/// @return Assignee.
constexpr Bitboard &Bitboard::operator&=(Bitboard that)
{
    v_ &= that.v_;
    return *this;
}

/// @brief Bitwise OR assignment.
/// @param that Source
/// @return Assignee.
constexpr Bitboard &Bitboard::operator|=(Bitboard that)
{
    v_ |= that.v_;
    return *this;
}

/// @brief Bitwise XOR assignment.
/// @param that Source
/// @return Assignee.
constexpr Bitboard &Bitboard::operator^=(Bitboard that)
{
    v_ ^= that.v_;
    return *this;
}

/// @brief Bitwise AND operator
/// @param that Source
/// @return Result.
constexpr Bitboard Bitboard::operator&(Bitboard that) const
{
    return Bitboard{v_ & that.v_};
}

/// @brief Bitwise OR operator
/// @param that Source
/// @return Result.
constexpr Bitboard Bitboard::operator|(Bitboard that) const
{
    return Bitboard{v_ | that.v_};
}

/// @brief Bitwise XOR operator
/// @param that Source
/// @return Result.
constexpr Bitboard Bitboard::operator^(Bitboard that) const
{
    return Bitboard{v_ ^ that.v_};
}

/// @brief Bitwise complement operator
/// @return Result.
constexpr Bitboard Bitboard::operator~() const
{
    return Bitboard{~v_};
}

/// @brief Least significant bit.
/// @return Square index of LSB.
constexpr Square Bitboard::Lsb() const
{
    return Square{(uint8_t)__builtin_ctzll(v_)};
}

/// @brief Most significant bit.
/// @return Square index of MSB.
constexpr Square Bitboard::Msb() const
{
    return Square{(uint8_t)(63 - __builtin_clzll(v_))};
}

/// @brief Population count.
/// @return Number of bits set.
constexpr int Bitboard::PopCount() const
{
    return __builtin_popcountll(v_);
}

/// @brief Check if bitboard is empty.
/// @return true if no bits are set.
constexpr bool Bitboard::IsEmpty() const
{
    return v_ == 0;
}

/// @brief Check if bitboard is not empty.
/// @return true if at least 1 bit is set.
constexpr bool Bitboard::IsNotEmpty() const
{
    return v_ != 0;
}

/// @brief Convert to native uint64_t
constexpr Bitboard::operator uint64_t() const
{
    return v_;
}

/// @brief Shift one square to the North.
/// @return shifted Bitboard.
constexpr Bitboard Bitboard::ShiftNorth() const
{
    return Bitboard{v_ << 8};
}

/// @brief Shift one square to the Northeast.
/// @return shifted Bitboard.
constexpr Bitboard Bitboard::ShiftNortheast() const
{
    return Bitboard{(v_ & kNotFileH) << 9};
}

/// @brief Shift one square to the East.
/// @return shifted Bitboard.
constexpr Bitboard Bitboard::ShiftEast() const
{
    return Bitboard{(v_ & kNotFileH) << 1};
}

/// @brief Shift one square to the Southeast.
/// @return shifted Bitboard.
constexpr Bitboard Bitboard::ShiftSoutheast() const
{
    return Bitboard{(v_ & kNotFileH) >> 7};
}

/// @brief Shift one square to the South.
/// @return shifted Bitboard.
constexpr Bitboard Bitboard::ShiftSouth() const
{
    return Bitboard{v_ >> 8};
}

/// @brief Shift one square to the Southwest.
/// @return shifted Bitboard.
constexpr Bitboard Bitboard::ShiftSouthwest() const
{
    return Bitboard{(v_ & kNotFileA) >> 9};
}

/// @brief Shift one square to the West.
/// @return shifted Bitboard.
constexpr Bitboard Bitboard::ShiftWest() const
{
    return Bitboard{(v_ & kNotFileA) >> 1};
}

/// @brief Shift one square to the Northwest.
/// @return shifted Bitboard.n
constexpr Bitboard Bitboard::ShiftNorthwest() const
{
    return Bitboard{(v_ & kNotFileA) << 7};
}

/// @brief Begin iteration over the set bits.
/// @return Iterator to the first set bit.
constexpr Bitboard::Iterator Bitboard::begin() const
{
    return Iterator{*this};
}

/// @brief End of iteration.
/// @return Sentinel marking the end of the set bits.
constexpr Bitboard::Sentinel Bitboard::end() const
{
    return Sentinel{};
}

// Useful Bitboard constant values.
// clang-format off
static constexpr Bitboard kNoSquares        {0ull};                 ///< Empty board (no squares set).
static constexpr Bitboard kAllSquares       {~kNoSquares};          ///< Every square set.
static constexpr Bitboard kRank1            {0xFFull};              ///< All squares on rank 1.
static constexpr Bitboard kRank2            {kRank1.ShiftNorth()};  ///< All squares on rank 2.
static constexpr Bitboard kRank3            {kRank2.ShiftNorth()};  ///< All squares on rank 3.
static constexpr Bitboard kRank4            {kRank3.ShiftNorth()};  ///< All squares on rank 4.
static constexpr Bitboard kRank5            {kRank4.ShiftNorth()};  ///< All squares on rank 5.
static constexpr Bitboard kRank6            {kRank5.ShiftNorth()};  ///< All squares on rank 6.
static constexpr Bitboard kRank7            {kRank6.ShiftNorth()};  ///< All squares on rank 7.
static constexpr Bitboard kRank8            {kRank7.ShiftNorth()};  ///< All squares on rank 8.
static constexpr Bitboard kFileA            {0x0101010101010101ull};///< All squares on the a file.
static constexpr Bitboard kFileB            {kFileA.ShiftEast()};   ///< All squares on the b file.
static constexpr Bitboard kFileC            {kFileB.ShiftEast()};   ///< All squares on the c file.
static constexpr Bitboard kFileD            {kFileC.ShiftEast()};   ///< All squares on the d file.
static constexpr Bitboard kFileE            {kFileD.ShiftEast()};   ///< All squares on the e file.
static constexpr Bitboard kFileF            {kFileE.ShiftEast()};   ///< All squares on the f file.
static constexpr Bitboard kFileG            {kFileF.ShiftEast()};   ///< All squares on the g file.
static constexpr Bitboard kFileH            {kFileG.ShiftEast()};   ///< All squares on the h file.
static constexpr Bitboard kWhiteSquares     {0x55AA55AA55AA55AAull};///< All light squares.
static constexpr Bitboard kBlackSquares     {~kWhiteSquares};       ///< All dark squares.
// clang-format on

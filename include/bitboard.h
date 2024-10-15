#pragma once
/// @file Types, values and functions for using chess Bitboards.

#include <cstdint>
#include <string>

#include "move.h"

/// @brief Class to represent a bitboard.
/// A Bitboard is a set-wise interpretation of a 64-bit unsigned integer, with each bit mapping to a square on the
/// chessboard.
///
/// For example:
///
/// The set of squares occupied by pawns
/// The set of squares occupied by a black piece
/// The set of squares to which a knight on e4 may move
/// The set of squares attacked by black queens
/// The set of squares containing a piece checking the white king
///
/// If the bit is 1, then the corresponding square is a member of that set.
///
/// Bit  0 maps to square a1 (LSB)
/// Bit  7 maps to square h1
/// Bit 56 maps to square a8
/// Bit 63 maps to square h8 (MSB)
///
/// This is commonly referred to as LERF (little endian rank file mapping).
class Bitboard
{
  private:
    uint64_t v; ///< The bitboard value.

    static constexpr uint64_t NOT_FILE_A = 0xFEFEFEFEFEFEFEFEull; ///< Mask off the a file.
    static constexpr uint64_t NOT_FILE_H = 0x7F7F7F7F7F7F7F7Full; ///< Mask off the h file.

  public:
    /// @brief Defeault constructor.
    constexpr Bitboard()
    {
    }
    /// @brief Constructor.
    /// @param v Value to assign.
    constexpr Bitboard(uint64_t v) : v(v)
    {
    }
    /// @brief Constructor.
    /// @param location Square index to convert to Bitboard.
    constexpr Bitboard(Square location) : v(1ull << location)
    {
    }
    /// @brief Constructor.
    /// @param x File index (0,7)
    /// @param y Rank index (0,7)
    constexpr Bitboard(int x, int y) : v(1ull << (x + 8 * y))
    {
    }
    /// @brief Copy constructor.
    /// @param that Bitboard to be copied.
    constexpr Bitboard(const Bitboard &that) : v(that.v)
    {
    }
    /// @brief Assignment operator.
    /// @param that Source Bitboard.
    /// @return Assignee.
    constexpr Bitboard &operator=(Bitboard that)
    {
        v = that.v;
        return *this;
    }
    /// @brief Convert Bitboard to 8 x 8 string for debug purposes.
    /// @return Chess board string containing 1s and 0s.
    constexpr std::string ToString() const
    {
        std::string result;
        for (int y = 7; y >= 0; --y)
        {
            for (int x = 0; x < 8; ++x)
            {
                if (v & (1ull << (x + 8 * y)))
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
    constexpr void IsolateLsb()
    {
        v &= -v;
    }
    /// @brief Equality operator.
    /// @param that Comparee.
    /// @return true if Bitboards are equal.
    constexpr bool operator==(Bitboard that) const
    {
        return v == that.v;
    }
    /// @brief Inequality operator.
    /// @param that Comparee.
    /// @return true if Bitboards are not equal.
    constexpr bool operator!=(Bitboard that) const
    {
        return v != that.v;
    }
    /// @brief Bitwise AND assignment.
    /// @param that Source
    /// @return Assignee.
    constexpr Bitboard &operator&=(Bitboard that)
    {
        v &= that.v;
        return *this;
    }
    /// @brief Bitwise OR assignment.
    /// @param that Source
    /// @return Assignee.
    constexpr Bitboard &operator|=(Bitboard that)
    {
        v |= that.v;
        return *this;
    }
    /// @brief Bitwise XOR assignment.
    /// @param that Source
    /// @return Assignee.
    constexpr Bitboard &operator^=(Bitboard that)
    {
        v ^= that.v;
        return *this;
    }
    /// @brief Bitwise AND operator
    /// @param that Source
    /// @return Result.
    constexpr Bitboard operator&(Bitboard that) const
    {
        return Bitboard{v & that.v};
    }
    /// @brief Bitwise OR operator
    /// @param that Source
    /// @return Result.
    constexpr Bitboard operator|(Bitboard that) const
    {
        return Bitboard{v | that.v};
    }
    /// @brief Bitwise XOR operator
    /// @param that Source
    /// @return Result.
    constexpr Bitboard operator^(Bitboard that) const
    {
        return Bitboard{v ^ that.v};
    }
    /// @brief Bitwise complement operator
    /// @return Result.
    constexpr Bitboard operator~() const
    {
        return Bitboard{~v};
    }
    /// @brief Least significant bit.
    /// @return Square index of LSB.
    constexpr Square Lsb() const
    {
        return (Square)__builtin_ctzll(v);
    }
    /// @brief Most significant bit.
    /// @return Square index of MSB.
    constexpr Square Msb() const
    {
        return (Square)(63 - __builtin_clzll(v));
    }
    /// @brief Population count.
    /// @return Number of bits set.
    constexpr int PopCount() const
    {
        return __builtin_popcountll(v);
    }
    /// @brief Check if bitboard is empty.
    /// @return true if no bits are set.
    constexpr bool IsEmpty() const
    {
        return v == 0;
    }
    /// @brief Check if bitboard is not empty.
    /// @return true if at least 1 bit is set.
    constexpr bool IsNotEmpty() const
    {
        return v != 0;
    }
    /// @brief Convert to native uint64_t
    constexpr explicit operator uint64_t() const
    {
        return v;
    }
    /// @brief Shift one square to the North.
    /// @return shifted Bitboard.
    constexpr Bitboard ShiftNorth() const
    {
        return Bitboard{v << 8};
    }
    /// @brief Shift one square to the Northeast.
    /// @return shifted Bitboard.
    constexpr Bitboard ShiftNortheast() const
    {
        return Bitboard{(v & NOT_FILE_H) << 9};
    }
    /// @brief Shift one square to the East.
    /// @return shifted Bitboard.
    constexpr Bitboard ShiftEast() const
    {
        return Bitboard{(v & NOT_FILE_H) << 1};
    }
    /// @brief Shift one square to the Southeast.
    /// @return shifted Bitboard.
    constexpr Bitboard ShiftSoutheast() const
    {
        return Bitboard{(v & NOT_FILE_H) >> 7};
    }
    /// @brief Shift one square to the South.
    /// @return shifted Bitboard.
    constexpr Bitboard ShiftSouth() const
    {
        return Bitboard{v >> 8};
    }
    /// @brief Shift one square to the Southwest.
    /// @return shifted Bitboard.
    constexpr Bitboard ShiftSouthwest() const
    {
        return Bitboard{(v & NOT_FILE_A) >> 9};
    }
    /// @brief Shift one square to the West.
    /// @return shifted Bitboard.
    constexpr Bitboard ShiftWest() const
    {
        return Bitboard{(v & NOT_FILE_A) >> 1};
    }
    /// @brief Shift one square to the Northwest.
    /// @return shifted Bitboard.n
    constexpr Bitboard ShiftNorthwest() const
    {
        return Bitboard{(v & NOT_FILE_A) << 7};
    }

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
        uint64_t i;

      public:
        using value_type = Square;
        constexpr Iterator(Bitboard bb) : i(bb.v)
        {
        }
        constexpr bool operator==(const Sentinel &) const
        {
            return i == 0; ///< No more bits to iterate over.
        }
        constexpr Square operator*() const
        {
            return (Square)__builtin_ctzll(i); ///< Least significant bit.
        }
        constexpr Iterator &operator++()
        {
            i &= i - 1; ///< Clear LSB.
            return *this;
        }
    };

    constexpr Iterator begin() const
    {
        return Iterator{*this};
    }

    constexpr Sentinel end() const
    {
        return Sentinel{};
    }
};

// Useful Bitboard constant values.
// clang-format off
static constexpr Bitboard NO_SQUARES        {0ull};
static constexpr Bitboard ALL_SQUARES       {~NO_SQUARES};
static constexpr Bitboard RANK_1            {0xFFull};
static constexpr Bitboard RANK_2            {RANK_1.ShiftNorth()};
static constexpr Bitboard RANK_3            {RANK_2.ShiftNorth()};
static constexpr Bitboard RANK_4            {RANK_3.ShiftNorth()};
static constexpr Bitboard RANK_5            {RANK_4.ShiftNorth()};
static constexpr Bitboard RANK_6            {RANK_5.ShiftNorth()};
static constexpr Bitboard RANK_7            {RANK_6.ShiftNorth()};
static constexpr Bitboard RANK_8            {RANK_7.ShiftNorth()};
static constexpr Bitboard FILE_A            {0x0101010101010101ull};
static constexpr Bitboard FILE_B            {FILE_A.ShiftEast()};
static constexpr Bitboard FILE_C            {FILE_B.ShiftEast()};
static constexpr Bitboard FILE_D            {FILE_C.ShiftEast()};
static constexpr Bitboard FILE_E            {FILE_D.ShiftEast()};
static constexpr Bitboard FILE_F            {FILE_E.ShiftEast()};
static constexpr Bitboard FILE_G            {FILE_F.ShiftEast()};
static constexpr Bitboard FILE_H            {FILE_G.ShiftEast()};
static constexpr Bitboard WHITE_SQUARES     {0x55AA55AA55AA55AAull};
static constexpr Bitboard BLACK_SQUARES     {~WHITE_SQUARES};
// clang-format on

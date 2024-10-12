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
    constexpr Bitboard()
    {
    }
    constexpr Bitboard(uint64_t v) : v(v)
    {
    }
    constexpr Bitboard(Square location) : v(1ull << location)
    {
    }
    constexpr Bitboard(int x, int y) : v(1ull << (x + 8 * y))
    {
    }
    constexpr Bitboard(const Bitboard &that) : v(that.v)
    {
    }
    constexpr Bitboard &operator=(Bitboard that)
    {
        v = that.v;
        return *this;
    }
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
    constexpr void IsolateLsb()
    {
        v &= -v;
    }
    constexpr bool operator==(Bitboard that) const
    {
        return v == that.v;
    }
    constexpr bool operator!=(Bitboard that) const
    {
        return v != that.v;
    }
    constexpr Bitboard &operator&=(Bitboard that)
    {
        v &= that.v;
        return *this;
    }
    constexpr Bitboard &operator|=(Bitboard that)
    {
        v |= that.v;
        return *this;
    }
    constexpr Bitboard &operator^=(Bitboard that)
    {
        v ^= that.v;
        return *this;
    }
    constexpr Bitboard operator&(Bitboard that) const
    {
        return Bitboard{v & that.v};
    }
    constexpr Bitboard operator|(Bitboard that) const
    {
        return Bitboard{v | that.v};
    }
    constexpr Bitboard operator^(Bitboard that) const
    {
        return Bitboard{v ^ that.v};
    }
    constexpr Bitboard operator~() const
    {
        return Bitboard{~v};
    }
    constexpr Square Lsb() const
    {
        return (Square)__builtin_ctzll(v);
    }
    constexpr Square Msb() const
    {
        return (Square)(63 - __builtin_clzll(v));
    }
    constexpr int PopCount() const
    {
        return __builtin_popcountll(v);
    }
    constexpr bool IsEmpty() const
    {
        return v == 0;
    }
    constexpr bool IsNotEmpty() const
    {
        return v != 0;
    }
    constexpr explicit operator uint64_t() const
    {
        return v;
    }
    constexpr Bitboard ShiftNorth() const
    {
        return Bitboard{v << 8};
    }
    constexpr Bitboard ShiftNortheast() const
    {
        return Bitboard{(v & NOT_FILE_H) << 9};
    }
    constexpr Bitboard ShiftEast() const
    {
        return Bitboard{(v & NOT_FILE_H) << 1};
    }
    constexpr Bitboard ShiftSoutheast() const
    {
        return Bitboard{(v & NOT_FILE_H) >> 7};
    }
    constexpr Bitboard ShiftSouth() const
    {
        return Bitboard{v >> 8};
    }
    constexpr Bitboard ShiftSouthwest() const
    {
        return Bitboard{(v & NOT_FILE_A) >> 9};
    }
    constexpr Bitboard ShiftWest() const
    {
        return Bitboard{(v & NOT_FILE_A) >> 1};
    }
    constexpr Bitboard ShiftNorthwest() const
    {
        return Bitboard{(v & NOT_FILE_A) << 7};
    }

    /// @brief Dummy sentinel for end of input iterator range.
    class Sentinel
    {
    };

    /// @brief Input iterator to allow iteration over the squares in a Bitboard.
    /// Dereferencing the iterator returns the least significant bit set.
    /// Incrementing the iterator clears the least significant bit set.
    /// This allows nice clean semantics like: "for (Square s : v)" for iterating over the bits set in a bitboard.
    class Iterator
    {
      public:
        using value_type = Square;
        constexpr Iterator(Bitboard bb) : i(bb.v)
        {
        }
        constexpr bool operator==(const Sentinel &) const
        {
            return i == 0;
        }
        constexpr Square operator*() const
        {
            return (Square)__builtin_ctzll(i);
        }
        constexpr Iterator &operator++()
        {
            i &= i - 1;
            return *this;
        }

      private:
        uint64_t i;
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
static constexpr Bitboard NO_SQUARES       {0ull};
static constexpr Bitboard ALL_SQUARES      {~NO_SQUARES};
static constexpr Bitboard RANK_1           {0xFFull};
static constexpr Bitboard RANK_2           {RANK_1.ShiftNorth()};
static constexpr Bitboard RANK_3           {RANK_2.ShiftNorth()};
static constexpr Bitboard RANK_4           {RANK_3.ShiftNorth()};
static constexpr Bitboard RANK_5           {RANK_4.ShiftNorth()};
static constexpr Bitboard RANK_6           {RANK_5.ShiftNorth()};
static constexpr Bitboard RANK_7           {RANK_6.ShiftNorth()};
static constexpr Bitboard RANK_8           {RANK_7.ShiftNorth()};
static constexpr Bitboard FILE_A           {0x0101010101010101ull};
static constexpr Bitboard FILE_B           {FILE_A.ShiftEast()};
static constexpr Bitboard FILE_C           {FILE_B.ShiftEast()};
static constexpr Bitboard FILE_D           {FILE_C.ShiftEast()};
static constexpr Bitboard FILE_E           {FILE_D.ShiftEast()};
static constexpr Bitboard FILE_F           {FILE_E.ShiftEast()};
static constexpr Bitboard FILE_G           {FILE_F.ShiftEast()};
static constexpr Bitboard FILE_H           {FILE_G.ShiftEast()};
static constexpr Bitboard WHITE_SQUARES    {0x55AA55AA55AA55AAull};
static constexpr Bitboard BLACK_SQUARES    {~WHITE_SQUARES};
// clang-format on

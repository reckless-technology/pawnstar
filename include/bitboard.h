#pragma once
/// @file Types, values and functions for using Bitboards.

#include <cstdint>

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

  public:
    constexpr Bitboard()
    {
    }
    constexpr Bitboard(uint64_t b) : v(b)
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
    constexpr Bitboard operator*(Bitboard that) const
    {
        return Bitboard{v * that.v};
    }
    constexpr Bitboard operator~() const
    {
        return Bitboard{~v};
    }
    constexpr Bitboard operator<<(int i) const
    {
        return Bitboard{v << i};
    }
    constexpr Bitboard operator>>(int i) const
    {
        return Bitboard{v >> i};
    }
    constexpr Square Lsb() const
    {
        return (Square)__builtin_ctzll(v);
    }
    constexpr int PopCount() const
    {
        return __builtin_popcountll(v);
    }
    constexpr explicit operator uint64_t() const
    {
        return v;
    }

    /// @brief Dummy sentinel for end of input iterator range.
    class Sentinel
    {
    };

    /// @brief Input iterator to allow iteration over the squares in a Bitboard.
    /// Dereferencing the iterator returns the least significant bit set.
    /// Incrementing the iterator clears the least significant bit set.
    /// This allows nice clean semantics like: "for (Square s : b)" for iterating over the bits set in the bitboard.
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

    Iterator begin() const
    {
        return Iterator{*this};
    }

    Sentinel end() const
    {
        return Sentinel{};
    }
};

// Useful Bitboard constant values.
// clang-format off
constexpr Bitboard NO_SQUARES       {0};
constexpr Bitboard ALL_SQUARES      {~NO_SQUARES};
constexpr Bitboard RANK_1           {0xFFull};
constexpr Bitboard RANK_2           {RANK_1 << 8};
constexpr Bitboard RANK_3           {RANK_2 << 8};
constexpr Bitboard RANK_4           {RANK_3 << 8};
constexpr Bitboard RANK_5           {RANK_4 << 8};
constexpr Bitboard RANK_6           {RANK_5 << 8};
constexpr Bitboard RANK_7           {RANK_6 << 8};
constexpr Bitboard RANK_8           {RANK_7 << 8};
constexpr Bitboard FILE_A           {0x0101010101010101ull};
constexpr Bitboard FILE_B           {FILE_A << 1};
constexpr Bitboard FILE_C           {FILE_B << 1};
constexpr Bitboard FILE_D           {FILE_C << 1};
constexpr Bitboard FILE_E           {FILE_D << 1};
constexpr Bitboard FILE_F           {FILE_E << 1};
constexpr Bitboard FILE_G           {FILE_F << 1};
constexpr Bitboard FILE_H           {FILE_G << 1};
constexpr Bitboard NOT_FILE_H       {~FILE_H};
constexpr Bitboard NOT_FILE_A       {~FILE_A};
constexpr Bitboard WHITE_SQUARES    {0x55AA55AA55AA55AAull};
constexpr Bitboard BLACK_SQUARES    {~WHITE_SQUARES};
// clang-format on

/// Functions to shift bitboards by one square in the specified direction. Compass rose directions are from White's
/// perspective.
constexpr Bitboard ShiftNorth(Bitboard b)
{
    return b << 8;
}
constexpr Bitboard ShiftNortheast(Bitboard b)
{
    return (b & NOT_FILE_H) << 9;
}
constexpr Bitboard ShiftEast(Bitboard b)
{
    return (b & NOT_FILE_H) << 1;
}
constexpr Bitboard ShiftSoutheast(Bitboard b)
{
    return (b & NOT_FILE_H) >> 7;
}
constexpr Bitboard ShiftSouth(Bitboard b)
{
    return b >> 8;
}
constexpr Bitboard ShiftSouthwest(Bitboard b)
{
    return (b & NOT_FILE_A) >> 9;
}
constexpr Bitboard ShiftWest(Bitboard b)
{
    return (b & NOT_FILE_A) >> 1;
}
constexpr Bitboard ShiftNorthwest(Bitboard b)
{
    return (b & NOT_FILE_A) << 7;
}
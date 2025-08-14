#pragma once
/// @file Square indices on the chess board.

#include <cstdint>
#include <string>

/// @brief Class to hold a square index (location) on a chess board.
class Square
{
  public:
    /// @brief Default constructor - does nothing.
    constexpr Square()
    {
    }
    /// @brief Constructor
    /// @param x Square index in LERF mapping.
    constexpr Square(int x) : val(x)
    {
    }
    /// @brief Constructor
    /// @param x file
    /// @param y rank
    constexpr Square(int x, int y) : val(x + 8 * y)
    {
    }
    /// @brief Constructor
    /// @param str Name of square e.g. "e4"
    constexpr Square(const char *str) : val((str[0] | 0x20) - 'a' + 8 * (str[1] - '1'))
    {
    }
    /// @brief Convert to square index.
    constexpr operator uint8_t() const
    {
        return val;
    }
    /// @brief File.
    /// @return File on board (0 thru 7 -> a thru h)
    constexpr uint8_t File() const
    {
        return val & 7;
    }
    /// @brief Rank.
    /// @return Rank on board (zero indexed 0 thru 7).
    constexpr uint8_t Rank() const
    {
        return val >> 3;
    }
    /// @brief Square name.
    /// @return String containing name of square e.g. "e4"
    constexpr std::string ToString() const
    {
        return std::string{(char)('a' + File()), (char)('1' + Rank())};
    }

  private:
    uint8_t val; ///< The square index in LERF format.
};
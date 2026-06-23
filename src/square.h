#pragma once
/// @file square.h Square indices on the chess board.

#include <cstdint>
#include <string>

/// @brief Class to hold a square index (location) on a chess board.
class Square
{
  public:
    constexpr Square();
    constexpr Square(int x);
    constexpr Square(int x, int y);
    constexpr Square(const char *str);
    constexpr             operator uint8_t() const;
    constexpr uint8_t     File() const;
    constexpr uint8_t     Rank() const;
    constexpr std::string ToString() const;

  private:
    uint8_t val_; ///< The square index in LERF format.
};

/// @brief Default constructor - does nothing.
constexpr Square::Square()
{
}

/// @brief Constructor
/// @param x Square index in LERF mapping.
constexpr Square::Square(int x) : val_(x)
{
}

/// @brief Constructor
/// @param x file
/// @param y rank
constexpr Square::Square(int x, int y) : val_(x + 8 * y)
{
}

/// @brief Constructor
/// @param str Name of square e.g. "e4"
constexpr Square::Square(const char *str) : val_((str[0] | 0x20) - 'a' + 8 * (str[1] - '1'))
{
}

/// @brief Convert to square index.
constexpr Square::operator uint8_t() const
{
    return val_;
}

/// @brief File.
/// @return File on board (0 thru 7 -> a thru h)
constexpr uint8_t Square::File() const
{
    return val_ & 7;
}

/// @brief Rank.
/// @return Rank on board (zero indexed 0 thru 7).
constexpr uint8_t Square::Rank() const
{
    return val_ >> 3;
}

/// @brief Square name.
/// @return String containing name of square e.g. "e4"
constexpr std::string Square::ToString() const
{
    return std::string{(char)('a' + File()), (char)('1' + Rank())};
}

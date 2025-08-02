#pragma once
/// @file Chess piece colors.

#include <cstdint>

/// @brief Piece colors.
enum Color : uint8_t
{
    WHITE,
    BLACK,
};

/// @brief Return the enemy of a color.
/// @param color the color
/// @return The opposite color
constexpr Color EnemyOf(Color color)
{
    return color == Color::WHITE ? Color::BLACK : Color::WHITE;
}
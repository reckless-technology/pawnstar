#pragma once
/// @file Chess piece colors.

#include <cstdint>

/// @brief Piece colors.
enum Color : uint8_t
{
    kWhite,
    kBlack,
};

/// @brief Return the enemy of a color.
/// @param color the color
/// @return The opposite color
constexpr Color EnemyOf(Color color)
{
    return color == Color::kWhite ? Color::kBlack : Color::kWhite;
}
#pragma once
/// @file Chess piece types.

#include <cstdint>

/// @brief Chess pieces.
enum Piece : uint8_t
{
    NONE,
    PAWN,
    KNIGHT,
    BISHOP,
    ROOK,
    QUEEN,
    KING,
};
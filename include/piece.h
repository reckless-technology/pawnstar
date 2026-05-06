#pragma once
/// @file Chess piece types.

#include <cstdint>

/// @brief Chess pieces.
enum Piece : uint8_t
{
    kNone,
    kPawn,
    kKnight,
    kBishop,
    kRook,
    kQueen,
    kKing,
};
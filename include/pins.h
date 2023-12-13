#pragma once

#include "bitboard.h"
#include "position.h"

/**
 * @brief Holds information about pinned pieces.
 */
struct Pins
{
    Bitboard pinned_pieces;         /**< Set of squares which are pinned. */
    Bitboard allowed_squares[64];   /**< Contains the set of allowed squares a pinned piece may safely move to. */
};

void DeterminePins(const Position& position, Pins& pins);
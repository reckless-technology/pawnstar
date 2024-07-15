#pragma once

#include "bitboard.h"
#include "generated_data.h"
#include "position.h"

class Pins
{
  public:
    Pins(const Position &position)
    {
        pinned_pieces                        = NO_SQUARES;
        const Color     color                = position.ColorToMove();
        const Bitboard  occupied_squares     = position.OccupiedSquares();
        const Bitboard  friendly_pieces      = position.PiecesOfColor(color);
        const uint8_t   king_location        = position.KingLocation(color);
        const Bitboard *intervening          = &INTERVENING_SQUARES[king_location][0];
        Bitboard        enemy_sliding_pieces = ((SETS[king_location].bishop_attacks & (position.Bishops() | position.Queens())) |
                                         (SETS[king_location].rook_attacks & (position.Rooks() | position.Queens()))) &
                                        ~friendly_pieces;
        while (enemy_sliding_pieces)
        {
            const Square   slider_location            = FindAndClearLsb(enemy_sliding_pieces);
            const Bitboard intervening_squares        = intervening[slider_location];
            const Bitboard intervening_pieces         = intervening_squares & occupied_squares;
            const Bitboard intervening_friendly_piece = intervening_pieces & friendly_pieces;
            if (intervening_friendly_piece && PopCount(intervening_pieces) == 1)
            {
                pinned_pieces |= intervening_friendly_piece;
                allowed_squares[Lsb(intervening_friendly_piece)] = intervening_squares | BITBOARD(slider_location);
            }
        }
    }

    Bitboard AllowedSquares(Square locn) const
    {
        return BITBOARD(locn) & pinned_pieces ? allowed_squares[locn] : ALL_SQUARES;
    }

  private:
    Bitboard pinned_pieces;       /**< Set of squares which are pinned. */
    Bitboard allowed_squares[64]; /**< Contains the set of allowed squares a pinned piece may safely move to. */
};
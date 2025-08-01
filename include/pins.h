#pragma once

#include "bitboard.h"
#include "generated_data.h"
#include "position.h"

#include <array>

/// @brief Class for representing pinned pieces on the board and the squares to which they are allowed to move.
class Pins
{
  public:
    /// @brief Constructor.
    /// @param position Position to analyze.
    constexpr Pins(const Position &position)
    {
        pinned_pieces_                         = NO_SQUARES;
        const Color           color            = position.ColorToMove();
        const Bitboard        occupied_squares = position.OccupiedSquares();
        const Bitboard        friendly_pieces  = position.PiecesOfColor(color);
        const Square          king_location    = position.KingLocation(color);
        const Bitboard *const intervening      = &INTERVENING_SQUARES[king_location][0];
        Bitboard enemy_sliding_pieces = ((BISHOP_ATTACKS[king_location] & (position.Bishops() | position.Queens())) |
                                         (ROOK_ATTACKS[king_location] & (position.Rooks() | position.Queens()))) &
                                        ~friendly_pieces;
        for (Square s : enemy_sliding_pieces)
        {
            const Bitboard intervening_squares        = intervening[s];
            const Bitboard intervening_pieces         = intervening_squares & occupied_squares;
            const Bitboard intervening_friendly_piece = intervening_pieces & friendly_pieces;
            if (intervening_friendly_piece.IsNotEmpty() && intervening_pieces.PopCount() == 1)
            {
                // There is only a single piece between the sliding enemy attacker and the king so this piece is pinned
                // along the attack ray. The pinned piece is also allowed to capture the pinning piece.
                pinned_pieces_ |= intervening_friendly_piece;
                allowed_squares_[intervening_friendly_piece.Lsb()] = intervening_squares | Bitboard{s};
            }
        }
    }

    /// @brief Return the allowable squares which a piece may move to considering pins.
    /// @param s Square index
    /// @return Set of allowed destination squares.
    constexpr Bitboard AllowedSquares(Square s) const
    {
        return (Bitboard{s} & pinned_pieces_).IsNotEmpty() ? allowed_squares_[s] : ALL_SQUARES;
    }

  private:
    Bitboard                 pinned_pieces_;   ///< Set of squares which are pinned.
    std::array<Bitboard, 64> allowed_squares_; ///< Allowed squares per source.
};
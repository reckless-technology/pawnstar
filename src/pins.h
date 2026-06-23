#pragma once
/// @file pins.h Absolute-pin detection and the resulting legal move masks.

#include "bitboard.h"
#include "generated_data.h"
#include "position.h"

#include <array>

/// @brief Class for representing pinned pieces on the board and the squares to which they are allowed to move.
class Pins
{
  public:
    constexpr Pins(const Position &position);
    constexpr Bitboard AllowedSquares(Square s) const;
    constexpr bool     IsAllowed(Square from, Square to) const;

  private:
    Bitboard                 pinned_pieces_;   ///< Set of squares which are pinned.
    std::array<Bitboard, 64> allowed_squares_; ///< Allowed squares per source.
};

/// @brief Constructor.
/// @param position Position to analyze.
constexpr Pins::Pins(const Position &position)
{
    pinned_pieces_                  = kNoSquares;
    const Color    color            = position.color_to_move_;
    const Bitboard occupied_squares = position.OccupiedSquares();
    const Bitboard friendly_pieces  = position.colors_[color];
    const Square   king_location    = position.king_location_[color];
    const auto    &intervening      = kInterveningSquares[king_location];
    const Bitboard enemy_sliding_pieces =
        ((kBishopAttacks[king_location] & (position.pieces_[kBishop] | position.pieces_[kQueen])) |
         (kRookAttacks[king_location] & (position.pieces_[kRook] | position.pieces_[kQueen]))) &
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
constexpr Bitboard Pins::AllowedSquares(Square s) const
{
    return (Bitboard{s} & pinned_pieces_).IsNotEmpty() ? allowed_squares_[s] : kAllSquares;
}

/// @brief Determine if a from-to square pair is allowed based on whether the piece is pinned.
/// @param from Source square.
/// @param to Destination square.
/// @return true if allowed, false if pinned.
constexpr bool Pins::IsAllowed(Square from, Square to) const
{
    return (AllowedSquares(from) & Bitboard(to)).IsNotEmpty();
}

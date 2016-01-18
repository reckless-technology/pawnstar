#include "pawnstar.h"
/******************************************************************************
Determine any pinned pieces and their corresponding allowed target squares
NB: the contents of pins->allowed_squares[x] for non-pinned pieces is garbage
(whatever happened to be there before we were called)
*******************************************************************************/
void DeterminePins(const Position* position, Pins* pins)
{
    int color;
    pins->pinned_pieces = NO_SQUARES;
    for (color = WHITE; color <= BLACK; ++color)
    {
        const int king_location = KingLocation(position, color);
        const bitboard* const intervening_squares_arr = &INTERVENING_SQUARES[king_location][0];
        const bitboard friendly_pieces = position->pieces_of_color[color];
        bitboard enemy_sliding_pieces = 
            ((BISHOP_ATTACKS[king_location] & (position->bishops | position->queens)) | 
            (   ROOK_ATTACKS[king_location] & (position->rooks   | position->queens))) & ~friendly_pieces;
        while (enemy_sliding_pieces)
        {
            const int slider_location                = FindAndClearLsb(&enemy_sliding_pieces);
            const bitboard intervening_squares       = intervening_squares_arr[slider_location];
            const bitboard intervening_pieces        = intervening_squares & position->occupied_squares;
            const bitboard intervening_friendly_piece = intervening_pieces & friendly_pieces;           
            if (intervening_friendly_piece && 
                PopCount(intervening_pieces) == 1)
            {
                pins->pinned_pieces |= intervening_friendly_piece;
                pins->allowed_squares[Lsb(intervening_friendly_piece)] = intervening_squares | BITBOARD(slider_location);
            }
        }
    }
}
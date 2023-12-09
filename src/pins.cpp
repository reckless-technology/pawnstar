#include "pawnstar.h"
/**
 * @brief Determine any pinned pieces and their corresponding allowed target squares
 * Note: The contents of pins->allowed_squares[x] for non-pinned pieces is garbage
 * (whatever happened to be there before we were called)
 * @param position Position to operate on.
 * @param pins Pointer where to store result.
*/
void DeterminePins(const Position* position, Pins* pins)
{
    pins->pinned_pieces             = NO_SQUARES;
    const int color                 = COLOR_TO_MOVE(position);
    const Bitboard occupied_squares = position->white_pieces | position->black_pieces;
    const Bitboard friendly_pieces  = position->pieces_of_color[color];
    const int king_location         = position->king_location[color];
    const Bitboard* intervening     = &INTERVENING_SQUARES[king_location][0];
    Bitboard enemy_sliding_pieces = 
        ((SETS[king_location].bishop_attacks & (position->bishops | position->queens)) | 
        ( SETS[king_location].rook_attacks   & (position->rooks   | position->queens))) & ~friendly_pieces;
    while (enemy_sliding_pieces)
    {
        const int slider_location                   = FindAndClearLsb(&enemy_sliding_pieces);
        const Bitboard intervening_squares          = intervening[slider_location];
        const Bitboard intervening_pieces           = intervening_squares & occupied_squares;
        const Bitboard intervening_friendly_piece   = intervening_pieces & friendly_pieces;
        if (intervening_friendly_piece && POPCOUNT(intervening_pieces) == 1)
        {
            pins->pinned_pieces |= intervening_friendly_piece;
            pins->allowed_squares[LSB(intervening_friendly_piece)] = intervening_squares | BITBOARD(slider_location);
        }
    }
#if 0
    if (POPCOUNT(pins->pinned_pieces) == 3)
    {
        char pos_string[256];
        PositionToString(position, pos_string);
        printf("Multiple pin detected in position\n%s\n", pos_string);
        Bitboard b = pins->pinned_pieces;
        while (b)
        {
            const int locn = FindAndClearLsb(&b);
            printf("Piece pinned at %c%c may move to ", FILE_CHAR(locn), RANK_CHAR(locn));
            Bitboard c = pins->allowed_squares[locn];
            while (c)
            {
                const int allowed_locn = FindAndClearLsb(&c);
                printf("%c%c ", FILE_CHAR(allowed_locn), RANK_CHAR(allowed_locn));
            }
            printf("\n");
        }
    }
#endif
}
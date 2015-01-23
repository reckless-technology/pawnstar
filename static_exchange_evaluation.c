#include "pawnstar.h"

static int EvaluateSwapOff(Position* position, int location, int color, int piece_on_square);

extern const bitboard* const ENEMY_PAWN_ATTACKS[2];
/******************************************************************************
Use nominal 1-3-3-5-9 material values for SEE
*******************************************************************************/
static const int PIECE_VALUES[8] = { 0, 100, 300, 300, 500, 900, 10000, 0 };

/******************************************************************************
Determine the SEE (static exchange evaluation) for a move.
Refer to: http://chessprogramming.wikispaces.com/Static+Exchange+Evaluation
*******************************************************************************/
int EvaluateStaticExchange(const Position* src_position, int move)
{
    Position position[1];
    MakeMove(position, src_position, move);
    if (MOVE_PROMOTED(move))
    {
        return PIECE_VALUES[MOVE_CAPTURED(move)] + PIECE_VALUES[MOVE_PROMOTED(move)] - PIECE_VALUES[PAWN] - 
            EvaluateSwapOff(position, MOVE_TO(move), COLOR_TO_MOVE(position), MOVE_PROMOTED(move));
    }
    return PIECE_VALUES[MOVE_CAPTURED(move)] - 
        EvaluateSwapOff(position, MOVE_TO(move), COLOR_TO_MOVE(position), MOVE_PIECE(move));
}
/******************************************************************************
Recursively determine the exchange swap off value on a specific square.

For optimum speed, we neither copy the position, update the state flags nor 
update the hash during evaluation of the static exchange, as none of these is
required by the function.

NB: This function destroys its position during execution.

I tried replacing this recursive form with its iterative equivalent, but saw no
measurable speed gain, and the iterative form was much less easy to understand
and debug, so the recursive version remains...
*******************************************************************************/
static int EvaluateSwapOff(Position* position, int location, int color, int piece_on_square)
{
    int capturing_piece;
    int score;
    const bitboard attacking_pieces = position->pieces_of_color[color];
    const bitboard* const intervening_squares = &INTERVENING_SQUARES[location][0];
    bitboard sliders, square;
    /**************************************************************************
    Find the least valuable piece of "color" which directly attacks "location"
    ***************************************************************************/
    bitboard attacker = ENEMY_PAWN_ATTACKS[color][location] & attacking_pieces & position->pawns;
    if (attacker)
    {
        attacker &= -attacker; // isolate LSB in case there is more than 1 pawn attacker
        capturing_piece = PAWN;
        goto FoundAttacker;
    }
    attacker = KNIGHT_ATTACKS[location] & attacking_pieces & position->knights;
    if (attacker)
    {
        attacker &= -attacker; // isolate LSB in case there is more than 1 knight attacker
        capturing_piece = KNIGHT;
        goto FoundAttacker;
    }
    sliders = BISHOP_ATTACKS[location] & attacking_pieces & position->bishops;
    while (sliders)
    {
        const int slider_locn = FindAndClearLsb(&sliders);
        if (!(intervening_squares[slider_locn] & position->occupied_squares))
        {
            capturing_piece = BISHOP;
            attacker = BITBOARD(slider_locn);
            goto FoundAttacker;
        }
    }
    sliders = ROOK_ATTACKS[location] & attacking_pieces & position->rooks;
    while (sliders)
    {
        const int slider_locn = FindAndClearLsb(&sliders);
        if (!(intervening_squares[slider_locn] & position->occupied_squares))
        {
            capturing_piece = ROOK;
            attacker = BITBOARD(slider_locn);
            goto FoundAttacker;
        }
    }
    sliders = QUEEN_ATTACKS[location] & attacking_pieces & position->queens;
    while (sliders)
    {
        const int slider_locn = FindAndClearLsb(&sliders);
        if (!(intervening_squares[slider_locn] & position->occupied_squares))
        {
            capturing_piece = QUEEN;
            attacker = BITBOARD(slider_locn);
            goto FoundAttacker;
        }
    }
    attacker = KING_ATTACKS[location] & attacking_pieces & position->kings;
    if (attacker)
    {
        capturing_piece = KING;
        goto FoundAttacker;
    }

    return 0; // no more attackers to this square

FoundAttacker:
    square = BITBOARD(location);
    position->pieces[piece_on_square]       ^= square;
    position->pieces_of_color[ENEMY(color)] ^= square;
    position->pieces[capturing_piece]       ^= attacker | square;
    position->pieces_of_color[color]        ^= attacker | square;
    position->occupied_squares              = position->white_pieces | position->black_pieces;
    score = PIECE_VALUES[piece_on_square] - EvaluateSwapOff(position, location, ENEMY(color), capturing_piece);
    return score > 0 ? score : 0;
}

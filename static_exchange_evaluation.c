#include "pawnstar.h"

static int EvaluateSwapOff(Position* position, int location, int color, int piece_on_square);

extern const bitboard* const ENEMY_PAWN_ATTACKS[2];
/*
Use nominal 1-3-3-5-9 material values for SEE
*/
static const int piece_values[7] = { 0, 100, 300, 300, 500, 900, 10000 };
/*
Determine the SEE (static exchange evaluation) for a move.
Refer to: http://chessprogramming.wikispaces.com/Static+Exchange+Evaluation
*/
int EvaluateStaticExchange(const Position* src_position, int move)
{
    Position position;
    MakeMove(&position, src_position, move);
    if (MOVE_PROMOTED(move))
    {
        return piece_values[MOVE_CAPTURED(move)] + piece_values[MOVE_PROMOTED(move)] - piece_values[PAWN] - 
            EvaluateSwapOff(&position, MOVE_TO(move), COLOR_TO_MOVE(&position), MOVE_PROMOTED(move));
    }
    return piece_values[MOVE_CAPTURED(move)] - 
        EvaluateSwapOff(&position, MOVE_TO(move), COLOR_TO_MOVE(&position), MOVE_PIECE(move));
}
/*
Determine the swap off value of a capture move onto a fixed square. This 
function destroys its position during execution, and does not update flags etc
for speed, since it is called for every move at every node in the tree it must
be super fast.
*/
static int EvaluateSwapOff(Position* position, int location, int color, int piece_on_square)
{
    const bitboard square = BITBOARD(location);
    const bitboard* const intervening_squares = &INTERVENING_SQUARES[location][0];
    int scores[32];
    int ply;
    /* First pass: perform all the captures onto the square least valuable piece first */
    for (ply = 0; ply != 32; ++ply)
    {
        /* Find the least valuable piece of color which directly attacks location */
        int capturing_piece;       
        const bitboard attacking_pieces = position->pieces_of_color[color];
        bitboard attacker = ENEMY_PAWN_ATTACKS[color][location] & attacking_pieces & position->pawns;
        if (attacker)
        {
            attacker &= -attacker; /* isolate LSB in case there is more than 1 pawn attacker */
            capturing_piece = PAWN;
            goto FoundAttacker;
        }
        attacker = KNIGHT_ATTACKS[location] & attacking_pieces & position->knights;
        if (attacker)
        {
            attacker &= -attacker; /* isolate LSB in case there is more than 1 knight attacker */
            capturing_piece = KNIGHT;
            goto FoundAttacker;
        }
        bitboard sliders = BISHOP_ATTACKS[location] & attacking_pieces & position->bishops;
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

        /* No more attackers - finished first pass */
        scores[ply] = 0;
        break;

FoundAttacker:
        position->pieces[piece_on_square]       ^= square;
        position->pieces_of_color[ENEMY(color)] ^= square;
        position->pieces[capturing_piece]       ^= attacker | square;
        position->pieces_of_color[color]        ^= attacker | square;
        position->occupied_squares              ^= attacker;        
        scores[ply]                             = piece_values[piece_on_square];
        piece_on_square                         = capturing_piece;
        color                                   = ENEMY(color);
    }
    /* Second pass: unwind the capture stack and propagate values 
       back to the top for material winning sequences */
    for (--ply ; ply >= 0; --ply)
    {
        scores[ply] -= scores[ply + 1];
        if (scores[ply] < 0)
        {
            scores[ply] = 0; /* would not initiate a losing capture sequence */
        }
    }
    return scores[0];
}
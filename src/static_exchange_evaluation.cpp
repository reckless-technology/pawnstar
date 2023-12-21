#include "position.h"
#include "debug_hashtable.h"
#include "transposition_table.h"
#include "function_prototypes.h"

static int EvaluateSwapOff(Position& position, uint8_t location, int color, int piece_on_square);

/*
Use nominal 1-3-3-5-9 material values for SEE
*/
static const int piece_values[7] = { 0, 100, 300, 300, 500, 900, 10000 };
/*
Determine the SEE (static exchange evaluation) for a move.
Refer to: http://chessprogramming.wikispaces.com/Static+Exchange+Evaluation
*/
int EvaluateStaticExchange(const Position& src_position, Move move, bool& is_checking)
{
    Position position { src_position, move };
    if (position.flags_ & IS_MOVED_INTO_CHECK)
    {
        return MOVED_INTO_CHECK_SCORE;
    }
    is_checking = !!(position.flags_ & IS_CHECK);
    if (MovePromoted(move))
    {
        return 
            piece_values[MoveCaptured(move)] 
          + piece_values[MovePromoted(move)] 
          - piece_values[PAWN] 
          - EvaluateSwapOff(position, MoveTo(move), position.ColorToMove(), MovePromoted(move));
    }
    return 
        piece_values[MoveCaptured(move)] 
      - EvaluateSwapOff(position, MoveTo(move), position.ColorToMove(), MovePiece(move));
}
/*
Determine the swap off value of a capture move onto a fixed square. This 
function destroys its position during execution, and does not update flags etc
for speed, since it is called for every move at every node in the tree it must
be super fast.
*/
static int EvaluateSwapOff(Position& position, uint8_t location, int color, int piece_on_square)
{
    const Sets* const sets                    = &SETS[location];
    const Bitboard square                     = BITBOARD(location);
    const Bitboard* const intervening_squares = &INTERVENING_SQUARES[location][0];
    Bitboard occupied_squares                 = position.white_pieces_ | position.black_pieces_;
    int scores[32];
    int ply;
    /* First pass: perform all the captures onto the square least valuable piece first */
    for (ply = 0; ply != 32; ++ply)
    {
        /* Find the least valuable piece of color which directly attacks location */
        int capturing_piece;
        Bitboard sliders; 
        const Bitboard attacking_pieces = position.pieces_of_color_[color];
        Bitboard attacker = sets->pawn_attacks[EnemyOf(color)] & attacking_pieces & position.pawns_;
        if (attacker)
        {
            attacker &= -attacker; /* isolate Lsb in case there is more than 1 pawn attacker */
            capturing_piece = PAWN;
            goto FoundAttacker;
        }
        attacker = sets->knight_attacks & attacking_pieces & position.knights_;
        if (attacker)
        {
            attacker &= -attacker; /* isolate Lsb in case there is more than 1 knight attacker */
            capturing_piece = KNIGHT;
            goto FoundAttacker;
        }
        sliders = sets->bishop_attacks & attacking_pieces & position.bishops_;
        while (sliders)
        {
            const int slider_locn = FindAndClearLsb(sliders);
            if (!(intervening_squares[slider_locn] & occupied_squares))
            {
                capturing_piece = BISHOP;
                attacker = BITBOARD(slider_locn);
                goto FoundAttacker;
            }
        }
        sliders = sets->rook_attacks & attacking_pieces & position.rooks_;
        while (sliders)
        {
            const int slider_locn = FindAndClearLsb(sliders);
            if (!(intervening_squares[slider_locn] & occupied_squares))
            {
                capturing_piece = ROOK;
                attacker = BITBOARD(slider_locn);
                goto FoundAttacker;
            }
        }
        sliders = sets->queen_attacks & attacking_pieces & position.queens_;
        while (sliders)
        {
            const int slider_locn = FindAndClearLsb(sliders);
            if (!(intervening_squares[slider_locn] & occupied_squares))
            {
                capturing_piece = QUEEN;
                attacker = BITBOARD(slider_locn);
                goto FoundAttacker;
            }
        }
        attacker = sets->king_attacks & attacking_pieces & position.kings_;
        if (attacker)
        {
            capturing_piece = KING;
            goto FoundAttacker;
        }

        /* No more attackers - finished first pass */
        scores[ply] = 0;
        break;

FoundAttacker:
        position.pieces_[piece_on_square - 1]       ^= square;
        position.pieces_of_color_[EnemyOf(color)]   ^= square;
        position.pieces_[capturing_piece - 1]       ^= attacker | square;
        position.pieces_of_color_[color]            ^= attacker | square;
        occupied_squares                            ^= attacker;        
        scores[ply]                                 = piece_values[piece_on_square];
        piece_on_square                             = capturing_piece;
        color                                       = EnemyOf(color);
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
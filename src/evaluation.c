#include "pawnstar.h"

static const int PAWN_SQUARE[64] =
{
     0,  0,  0,  0,  0,  0,  0,  0,
    70, 70, 70, 70, 70, 70, 70, 70, 
    40, 40, 40, 40, 40, 40, 40, 40, 
    20, 20, 20, 30, 30, 20, 20, 20,
     0,  0,  0, 20, 20,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,-20,-20,  0,  0,  0
};

static const int KNIGHT_SQUARE[64] =
{
    -50,-40,-30,-30,-30,-30,-40,-50,
    -40,-20,  0,  0,  0,  0,-20,-40,
    -30,  0, 10, 15, 15, 10,  0,-30,
    -30,  5, 15, 20, 20, 15,  5,-30,
    -30,  0, 15, 20, 20, 15,  0,-30,
    -30,  5, 10, 15, 15, 10,  5,-30,
    -40,-20,  0,  5,  5,  0,-20,-40,
    -50,-40,-30,-30,-30,-30,-40,-50,
};

static const int BISHOP_SQUARE[64] = 
{
    -20,-10,-10,-10,-10,-10,-10,-20,
    -10,  0,  0,  0,  0,  0,  0,-10,
    -10,  0,  5, 10, 10,  5,  0,-10,
    -10,  5,  5, 10, 10,  5,  5,-10,
    -10,  0, 10, 10, 10, 10,  0,-10,
    -10, 10, 10, 10, 10, 10, 10,-10,
    -10,  5,  0,  0,  0,  0,  5,-10,
    -20,-10,-10,-10,-10,-10,-10,-20,
};

static const int ROOK_SQUARE[64] = 
{
     0,  0,  0,  0,  0,  0,  0,  0,
     5, 10, 10, 10, 10, 10, 10,  5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
     0,  0,  0,  5,  5,  0,  0,  0
};

static const int QUEEN_SQUARE[64] = 
{
    -20,-10,-10, -5, -5,-10,-10,-20,
    -10,  0,  0,  0,  0,  0,  0,-10,
    -10,  0,  5,  5,  5,  5,  0,-10,
     -5,  0,  5,  5,  5,  5,  0, -5,
      0,  0,  5,  5,  5,  5,  0, -5,
    -10,  5,  5,  5,  5,  5,  0,-10,
    -10,  0,  5,  0,  0,  0,  0,-10,
    -20,-10,-10, -5, -5,-10,-10,-20
};

static const int KING_SQUARE_MIDGAME[64] =
{
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -20,-30,-30,-40,-40,-30,-30,-20,
    -10,-20,-20,-20,-20,-20,-20,-10,
     20, 20,  0,  0,  0,  0, 20, 20,
     20, 40, 10,  0,  0, 10, 40, 20
};

static const int KING_SQUARE_ENDGAME[64] =
{
      0, 10, 20, 30, 30, 20, 10,  0,
     10, 20, 30, 40, 40, 30, 20, 10,
     20, 30, 40, 50, 50, 40, 30, 20,
     30, 40, 50, 60, 60, 50, 40, 30,
     30, 40, 50, 60, 60, 50, 40, 30,
     20, 30, 40, 50, 50, 40, 30, 20,
     10, 20, 30, 40, 40, 30, 20, 10,
      0, 10, 20, 30, 30, 20, 10,  0,
};

static const int* const PIECE_SQUARE_SCORES[] = 
{
    [PAWN]      = PAWN_SQUARE,
    [KNIGHT]    = KNIGHT_SQUARE,
    [BISHOP]    = BISHOP_SQUARE,
    [ROOK]      = ROOK_SQUARE,
    [QUEEN]     = QUEEN_SQUARE,
    [KING]      = KING_SQUARE_MIDGAME
};

static const int KING_HOME_SQUARE[2] = { E1, E8 };

/**
 * @brief Evaluate the current position, assuming neither king is in check 
   and the position is quiet.
 * @param position position to evaluate
 * @param alpha current alpha value
 * @param beta current beta value
 * @return score from the perspective of the side to move
*/
int 
EvaluatePosition(const Position* position, 
                 int alpha, 
                 int beta)
{    
    INCREMENT("eval calls");
    if (IsDrawByMaterial(position))
    {
        return DRAW_SCORE;
    }
    int scores[2] = { 0 };
    /* Phase 1: Evaluate material values alone. */
    for (int color = WHITE; color <= BLACK; ++color)
    {
        const bitboard friendly_pieces  = position->pieces_of_color[color];
        const bitboard pawns            = position->pawns   & friendly_pieces;
        const bitboard knights          = position->knights & friendly_pieces;
        const bitboard bishops          = position->bishops & friendly_pieces;
        const bitboard rooks            = position->rooks   & friendly_pieces;
        const bitboard queens           = position->queens  & friendly_pieces;
        scores[color] = 
            PopCount(pawns)    * 100 +
            PopCount(knights)  * 325 +
            PopCount(bishops)  * 350 +
            PopCount(rooks)    * 500 +
            PopCount(queens)   * 1000;
    }
    /* 
    See if material score alone causes alpha or beta cutoff. 
    This saves time doing more expensive evaluation when clearly it's not a PV node. 
    */
    int score = (position->state_flags & IS_BLACK_TO_MOVE) ?
        scores[BLACK] - scores[WHITE] :
        scores[WHITE] - scores[BLACK];
    if (score >= beta + 150)
    {
        INCREMENT("eval beta cutoff material");
        return beta;
    }
    if (score <= alpha - 150)
    {
        INCREMENT("eval alpha cutoff material");
        return alpha;
    }
    for (int color = WHITE; color <= BLACK; ++color)
    {
        const int rank_flip            = color == WHITE ? RANK_FLIP : 0;
        const bitboard friendly_pieces = position->pieces_of_color[color];
        /* Piece square scores */
        for (int piece = PAWN; piece <= QUEEN; ++piece)
        {
            const int* pst = PIECE_SQUARE_SCORES[piece];
            bitboard p = position->piece[piece - 1] & friendly_pieces;
            while (p)
            {
                const int locn = FindAndClearLsb(&p);
                scores[color] += pst[locn ^ rank_flip];
            }
        }
        if (position->queens == NO_SQUARES)
        {
            scores[color] += KING_SQUARE_ENDGAME[KING_LOCATION(position, color) ^ rank_flip];
        }
        else
        {
            const int king_location = KING_LOCATION(position, color);
            scores[color] += KING_SQUARE_MIDGAME[king_location ^ rank_flip];
            if (king_location != KING_HOME_SQUARE[color])
            {
                scores[color] += 20 * PopCount(SETS[king_location].king_attacks & position->pawns & friendly_pieces);
            }
        }

    }
    score = position->state_flags & IS_BLACK_TO_MOVE ?
        scores[BLACK] - scores[WHITE] :
        scores[WHITE] - scores[BLACK];
    return score;
}
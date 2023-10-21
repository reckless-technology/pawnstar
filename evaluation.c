#include "pawnstar.h"

static const int PAWN_SQUARE[64] =
{
     0,  0,  0,  0,  0,  0,  0,  0,
    60, 50, 50, 50, 50, 50, 50, 60,
    30, 30, 30, 30, 30, 30, 30, 30,
    20, 20, 20, 25, 25, 20, 20, 20,
     0,  0,  0, 20, 20,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,-20,-20,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0
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
    -50,-40,-30,-30,-30,-30,-40,-50
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
     20, 30, 10,  0,  0, 10, 30, 20,
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

/**
 * @brief Evaluate the current position, assuming neither king is in check and the position is quiet.
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
        const int num_friendly_pawns    = PopCount(pawns);

        scores[color] = 
            PopCount(pawns)   * 100 +
            PopCount(knights) * 325 +
            PopCount(bishops) * 325 +
            PopCount(rooks)   * 500 +
            PopCount(queens)  * 1000;
        /* Penalty for no pawns. */
        if (pawns == NO_SQUARES)
        {
            scores[color] -= 100;
        }
        /* Bonus for the bishop pair. */
        if ((bishops & WHITE_SQUARES) && (bishops & BLACK_SQUARES))
        {
            scores[color] += 50;
        }
        /* 
        Adjust bishop values according to number of friendly pawns. 
        Bishops are more valuable with fewer pawns. 
        */
        scores[color] += PopCount(bishops) * 5 * (4 - num_friendly_pawns);
        /* 
        Adjust knight values according to number of friendly pawns. 
        Knights are more valuable with more pawns. 
        */
        scores[color] += PopCount(knights) * 5 * (num_friendly_pawns - 4);
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
    /* Phase 2: Evaluate positional features. */
    const int non_pawn_classical_material = /* Initial value 62 */
        3 * PopCount(position->knights)
        + 3 * PopCount(position->bishops)
        + 5 * PopCount(position->rooks)
        + 9 * PopCount(position->queens);
    for (int color = WHITE; color <= BLACK; ++color)
    {
        const int rank_flip            = color == WHITE ? RANK_FLIP : 0;
        const bitboard friendly_pieces = position->pieces_of_color[color];
        const bitboard friendly_pawns  = position->pawns & friendly_pieces;
        const bitboard enemy_pawns     = position->pawns & ~friendly_pieces;
        bitboard p = friendly_pawns;
        while (p)
        {
            const int locn = FindAndClearLsb(&p);
            scores[color] += PAWN_SQUARE[locn ^ rank_flip];
            const PawnSets* pawn_masks = &PAWN_SETS[color][locn];
            if (!(pawn_masks->passed_pawn_mask & enemy_pawns))
            {
                const int rank = color == WHITE ? RANK_OF(locn) : 7 - RANK_OF(locn);
                scores[color] += 10 * rank;
            }
            if (!(pawn_masks->isolated_pawn_mask & friendly_pawns))
            {
                scores[color] -= 10;
            }
            if (pawn_masks->supported_pawn_mask & friendly_pawns)
            {
                scores[color] += 5;
            }
            if (pawn_masks->doubled_pawn_mask & friendly_pawns)
            {
                scores[color] -= 10;
            }
            if (SETS[locn].pawn_attacks[ENEMY(color)] & friendly_pawns)
            {
                scores[color] += 5;
            }
        }
        /* Mobility scores for knights, bishops, rooks. */
        bitboard knights = position->knights & friendly_pieces;
        while (knights)
        {
            scores[color] += KNIGHT_SQUARE[FindAndClearLsb(&knights) ^ rank_flip];
        }
        bitboard bishops = position->bishops & friendly_pieces;
        while (bishops)
        {
            scores[color] += (PopCount(BishopAttacks(position->occupied_squares, FindAndClearLsb(&bishops))) - 6) * 5;
        }
        bitboard rooks = position->rooks & friendly_pieces;
        while (rooks)
        {
            scores[color] += (PopCount(RookAttacks(position->occupied_squares, FindAndClearLsb(&rooks))) - 7) * 5;
        }
        if (non_pawn_classical_material <= 20)
        {
            scores[color] += KING_SQUARE_ENDGAME[KING_LOCATION(position, color) ^ rank_flip];
        }
        else
        {
            scores[color] += KING_SQUARE_MIDGAME[KING_LOCATION(position, color) ^ rank_flip];
            bitboard b = SETS[KING_LOCATION(position, color)].king_attacks;
            scores[color] += PopCount(friendly_pawns  & b) * 20;
            scores[color] += PopCount(friendly_pieces & b) * 10;
            while (b)
            {
                const int locn = FindAndClearLsb(&b);
                if (IsAttacked(position, locn, ENEMY(color)))
                {
                    scores[color] -= 20;
                }
            }
        }

    }
    score = position->state_flags & IS_BLACK_TO_MOVE ?
        scores[BLACK] - scores[WHITE] :
        scores[WHITE] - scores[BLACK];
    return score;
    (void)alpha;
    (void)beta;
}
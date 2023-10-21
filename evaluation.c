#include "pawnstar.h"

static const int PAWN_SQUARE[64] =
{
     0,  0,  0,  0,  0,  0,  0,  0,
    60, 50, 50, 50, 50, 50, 50, 60,
    30, 30, 30, 30, 30, 30, 30, 30,
    20, 20, 20, 25, 25, 20, 20, 20,
    10, 10, 10, 20, 20, 10, 10, 10,
     0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,-20,-20,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0
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
    int material_scores[2] = { 0 };
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

        material_scores[color] = 
            PopCount(pawns)   * 100 +
            PopCount(knights) * 325 +
            PopCount(bishops) * 325 +
            PopCount(rooks)   * 600 +
            PopCount(queens)  * 1200;
        /* Penalty for no pawns */
        if (pawns == NO_SQUARES)
        {
            material_scores[color] -= 100;
        }
        /* Bonus for the bishop pair. */
        if ((bishops & WHITE_SQUARES) && (bishops & BLACK_SQUARES))
        {
            material_scores[color] += 50;
        }
        /* 
        Adjust bishop values according to number of friendly pawns. 
        Bishops are more valuable with fewer pawns. 
        */
        material_scores[color] += PopCount(bishops) * 5 * (4 - num_friendly_pawns);
        /* 
        Adjust knight values according to number of friendly pawns. 
        Knights are more valuable with more pawns. 
        */
        material_scores[color] += PopCount(knights) * 5 * (num_friendly_pawns - 4);
    }
    /* 
    See if material score alone causes alpha or beta cutoff. 
    This saves time doing more expensive evaluation when clearly it's not a PV node. 
    */
    int score = (position->state_flags & IS_BLACK_TO_MOVE) ?
        material_scores[BLACK] - material_scores[WHITE] :
        material_scores[WHITE] - material_scores[BLACK];
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
    /* Phase 2: positional features. */
    const int non_pawn_classical_material = /* Initial value 62 */
        3 * PopCount(position->knights)
      + 3 * PopCount(position->bishops)
      + 5 * PopCount(position->rooks)
      + 9 * PopCount(position->queens);
    int scores[2] = { 0 };
    for (int color = WHITE; color <= BLACK; ++color)
    {
        const int rank_flip = color == WHITE ? RANK_FLIP : 0;
        const bitboard friendly_pieces = position->pieces_of_color[color];
        const bitboard pawns = position->pawns & friendly_pieces;
        bitboard p = pawns;
        while (p)
        {
            scores[color] += PAWN_SQUARE[FindAndClearLsb(&p) ^ rank_flip];
        }
        const bitboard pawn_attacks = color == WHITE ?
            SHIFT_NORTHEAST(pawns) | SHIFT_NORTHWEST(pawns) :
            SHIFT_SOUTHEAST(pawns) | SHIFT_SOUTHWEST(pawns);
        /* Reward pawns defended by friendly pawns. */
        scores[color] += 10 * PopCount(pawns & pawn_attacks);
        /* Mobility scores for knights, bishops, rooks and queens */
        bitboard knights = position->knights & friendly_pieces;
        while (knights)
        {
            scores[color] += (PopCount(SETS[FindAndClearLsb(&knights)].knight_attacks) - 4) * 10;
        }
        bitboard bishops = position->bishops & friendly_pieces;
        while (bishops)
        {
            scores[color] += (PopCount(BishopAttacks(position->occupied_squares, FindAndClearLsb(&bishops))) - 6) * 10;
        }
        bitboard rooks = position->rooks & friendly_pieces;
        while (rooks)
        {
            scores[color] += (PopCount(RookAttacks(position->occupied_squares, FindAndClearLsb(&rooks))) - 7) * 5;
        }
        bitboard queens = position->queens & friendly_pieces;
        while (queens)
        {
            scores[color] += (PopCount(QueenAttacks(position->occupied_squares, FindAndClearLsb(&queens))) - 14) * 2;
        }
        if (non_pawn_classical_material < 16)
        {
            scores[color] += KING_SQUARE_ENDGAME[position->king_location[color] ^ rank_flip];
        }
        else if (non_pawn_classical_material < 32)
        {
            const int midgame_king_score = KING_SQUARE_MIDGAME[position->king_location[color] ^ rank_flip];
            const int endgame_king_score = KING_SQUARE_ENDGAME[position->king_location[color] ^ rank_flip];
            const int delta = endgame_king_score - midgame_king_score;
            scores[color] += midgame_king_score + (delta * (32 - non_pawn_classical_material)) / 16;
            bitboard b = SETS[position->king_location[color]].king_attacks;
            scores[color] += PopCount(pawns & b) * 10;
            /* penalty for enemy pieces attacking close to our king */
            while (b)
            {
                const int locn = FindAndClearLsb(&b);
                if (IsAttacked(position, locn, ENEMY(color)))
                {
                    scores[color] -= 10;
                }
            }
        }
        else
        {
            scores[color] += KING_SQUARE_MIDGAME[position->king_location[color] ^ rank_flip];
            bitboard b = SETS[position->king_location[color]].king_attacks;
            scores[color] += PopCount(pawns & b) * 30;
            /* penalty for enemy pieces attacking close to our king */
            while (b)
            {
                const int locn = FindAndClearLsb(&b);
                if (IsAttacked(position, locn, ENEMY(color)))
                {
                    scores[color] -= 25;
                }
            }
        }
    }
    score += position->state_flags & IS_BLACK_TO_MOVE ?
        scores[BLACK] - scores[WHITE] :
        scores[WHITE] - scores[BLACK];
    return score;
    (void)alpha;
    (void)beta;
}
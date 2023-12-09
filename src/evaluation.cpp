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
    -5,  0,  0,  5,  5,  0,  0, -5
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
     20, 30, 10,  0,  0, 10, 30, 20
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
    NULL,
    PAWN_SQUARE,
    KNIGHT_SQUARE,
    BISHOP_SQUARE,
    ROOK_SQUARE,
    QUEEN_SQUARE,
    KING_SQUARE_MIDGAME
};

static const int KING_HOME_SQUARE[2]    = { E1, E8 };
static const Bitboard FILES_BB[8]       = { FILE_A, FILE_B, FILE_C, FILE_D, FILE_E, FILE_F, FILE_G, FILE_H };

typedef struct EvalHash
{
    uint64_t hash;
    int score;
} EvalHash;

#define EVAL_HASH_SIZE 100003 /* Prime number */

static EvalHash eval_hash_table[EVAL_HASH_SIZE];

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
    EvalHash* const eval_hash = &eval_hash_table[position->hash % EVAL_HASH_SIZE];
    if (eval_hash->hash == position->hash)
    {
        INCREMENT("eval hash table hits");
        return eval_hash->score;
    }
    if (IsDrawByMaterial(position))
    {
        eval_hash->hash = position->hash;
        eval_hash->score = DRAW_SCORE;
        return DRAW_SCORE;
    }

    int scores[2] = { 0 };
    /* Phase 1: Evaluate material values alone. */
    for (int color = WHITE; color <= BLACK; ++color)
    {
        const Bitboard friendly_pieces  = position->pieces_of_color[color];
        const Bitboard pawns            = position->pawns   & friendly_pieces;
        const Bitboard knights          = position->knights & friendly_pieces;
        const Bitboard bishops          = position->bishops & friendly_pieces;
        const Bitboard rooks            = position->rooks   & friendly_pieces;
        const Bitboard queens           = position->queens  & friendly_pieces;
        scores[color] = 
            PopCount(pawns)    * 100 +
            PopCount(knights)  * 325 +
            PopCount(bishops)  * 325 +
            PopCount(rooks)    * 525 +
            PopCount(queens)   * 1000;
        if ((bishops & WHITE_SQUARES) && (bishops & BLACK_SQUARES))
        {
            scores[color] += 50;
        }
    }
    /* 
    See if material score alone causes alpha or beta cutoff. 
    This saves time doing more expensive evaluation when clearly it's
    not a PV node so evaluation accuracy is not as important. 
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
        const int rank_flip = color == WHITE ? RANK_FLIP : 0;
        const Bitboard friendly_pieces = position->pieces_of_color[color];
        const int king_location = position->king_location[color];
        for (int piece = PAWN; piece <= QUEEN; ++piece)
        {
            const int* const pst = PIECE_SQUARE_SCORES[piece];
            Bitboard p = position->piece[piece - 1] & friendly_pieces;
            while (p)
            {
                const int locn = FindAndClearLsb(p);
                scores[color] += pst[locn ^ rank_flip];
            }
        }        
        if (position->queens == NO_SQUARES || PopCount(position->white_pieces | position->black_pieces) < 8)
        {
            scores[color] += KING_SQUARE_ENDGAME[king_location ^ rank_flip];
            const Bitboard friendly_pawns = position->pawns & friendly_pieces;
            const Bitboard enemy_pawns = position->pawns & ~friendly_pieces;
            Bitboard p = friendly_pawns;
            while (p)
            {
                const int pawn_locn = FindAndClearLsb(p);
                const PawnSets* sets = &PAWN_SETS[color][pawn_locn];
                if ((sets->passed_pawn_mask & enemy_pawns) == NO_SQUARES)
                {
                    scores[color] += 20;
                }
                if (sets->doubled_pawn_mask & friendly_pawns)
                {
                    scores[color] -= 5;
                }
                if ((sets->isolated_pawn_mask & friendly_pawns) == NO_SQUARES)
                {
                    scores[color] -= 10;
                }
                if (sets->supported_pawn_mask & friendly_pawns)
                {
                    scores[color] += 5;
                }
            }
        }
        else
        {
            scores[color] += KING_SQUARE_MIDGAME[king_location ^ rank_flip];
            if (king_location != KING_HOME_SQUARE[color])
            {
                const Bitboard friendly_pawns = position->pawns & friendly_pieces;
                scores[color] +=  
                    10 * PopCount(SETS[king_location].king_attacks & friendly_pieces) + 
                    10 * PopCount(SETS[king_location].king_attacks & friendly_pawns);
                if ((FILES_BB[FILE_OF(king_location)] & friendly_pawns) == NO_SQUARES)
                {
                    scores[color] -= 50;
                }
            }
            Bitboard b = SETS[king_location].king_attacks;
            while (b)
            {
                const int locn = FindAndClearLsb(b);
                scores[color] -= 5 * PopCount(AttacksTo(position, locn, ENEMY(color)));
            }
        }
    }
    score = position->state_flags & IS_BLACK_TO_MOVE ?
        scores[BLACK] - scores[WHITE] :
        scores[WHITE] - scores[BLACK];
    eval_hash->hash = position->hash;
    eval_hash->score = score;
    return score;
}
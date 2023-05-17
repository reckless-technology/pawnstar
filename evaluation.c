#include "pawnstar.h"

#define SCORE_PAWN_ISOLATED    -15
#define SCORE_PAWN_DOUBLED     -10
#define SCORE_PAWN_BLOCKED     -10
#define SCORE_PAWN_SUPPORTED    5
#define SCORE_PAWN_DEFENDED     10
#define SCORE_PAWN_PASSED       20



static const int PAWN_SQUARE[64] = {
     0,  0,  0,  0,  0,  0,  0,  0,
    50, 50, 50, 50, 50, 50, 50, 50,
    30, 30, 30, 30, 30, 30, 30, 30,
    25, 25, 25, 25, 25, 25, 25, 25,
     0,  0,  0, 20, 20,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,-20,-20,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0
};

static const int KNIGHT_SQUARE[64] = {
    -50,-40,-30,-30,-30,-30,-40,-50,
    -40,-20,  0,  0,  0,  0,-20,-40,
    -30,  0, 10, 15, 15, 10,  0,-30,
    -30,  5, 15, 20, 20, 15,  5,-30,
    -30,  0, 15, 20, 20, 15,  0,-30,
    -30,  5, 10, 15, 15, 10,  5,-30,
    -40,-20,  0,  5,  5,  0,-20,-40,
    -50,-40,-30,-30,-30,-30,-40,-50
};
static const int BISHOP_SQUARE[64] = {
    -20,-10,-10,-10,-10,-10,-10,-20,
    -10,  0,  0,  0,  0,  0,  0,-10,
    -10,  0,  5, 10, 10,  5,  0,-10,
    -10,  5,  5, 10, 10,  5,  5,-10,
    -10,  0, 10, 10, 10, 10,  0,-10,
    -10, 10, 10, 10, 10, 10, 10,-10,
    -10,  5,  0,  0,  0,  0,  5,-10,
    -20,-10,-10,-10,-10,-10,-10,-20
};
static const int ROOK_SQUARE[64] = {
      0,  0,  0,  0,  0,  0,  0,  0,
      5, 10, 10, 10, 10, 10, 10,  5,
     -5,  0,  0,  0,  0,  0,  0, -5,
     -5,  0,  0,  0,  0,  0,  0, -5,
     -5,  0,  0,  0,  0,  0,  0, -5,
     -5,  0,  0,  0,  0,  0,  0, -5,
     -5,  0,  0,  0,  0,  0,  0, -5,
      0,  0,  0,  5,  5,  0,  0,  0
};
static const int QUEEN_SQUARE[64] = {
    -20,-10,-10, -5, -5,-10,-10,-20,
    -10,  0,  0,  0,  0,  0,  0,-10,
    -10,  0,  5,  5,  5,  5,  0,-10,
     -5,  0,  5,  5,  5,  5,  0, -5,
      0,  0,  5,  5,  5,  5,  0, -5,
    -10,  5,  5,  5,  5,  5,  0,-10,
    -10,  0,  5,  0,  0,  0,  0,-10,
    -20,-10,-10, -5, -5,-10,-10,-20
};
static const int KING_SQUARE_MIDGAME[64] = {
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -20,-30,-30,-40,-40,-30,-30,-20,
    -10,-20,-20,-20,-20,-20,-20,-10,
     20, 20,  0,  0,  0,  0, 20, 20,
     20, 30, 10,  0,  0, 10, 30, 20
};
static const int KING_SQUARE_ENDGAME[64] = {
      0, 10, 20, 30, 30, 20, 10,  0,
     10, 20, 30, 40, 40, 30, 20, 10,
     20, 30, 40, 50, 50, 40, 30, 20,
     30, 40, 50, 60, 60, 50, 40, 30,
     30, 40, 50, 60, 60, 50, 40, 30,
     20, 30, 40, 50, 50, 40, 30, 20,
     10, 20, 30, 40, 40, 30, 20, 10,
      0, 10, 20, 30, 30, 20, 10,  0,
};
static const int* const PIECE_SQUARES[7] = {
    NULL,
    PAWN_SQUARE,
    KNIGHT_SQUARE,
    BISHOP_SQUARE,
    ROOK_SQUARE,
    QUEEN_SQUARE,
    KING_SQUARE_MIDGAME,
};


static const int MATERIAL_VALUES[7] = {
    0,
    100, // pawn
    325, // knight
    350, // bishop
    600, // rook
    1000, // queen
    0, // king
};


int         piece_square_scores[2][6][64];
static int  king_endgame_delta[2][64];

/*
Set up the piece square tables
*/
void 
InitializeEval()
{
    for (int location = A1; location <= H8; ++location)
    {
        for (int piece = PAWN; piece <= KING; ++piece)
        {
            piece_square_scores[WHITE][piece - 1][location] =  MATERIAL_VALUES[piece] + PIECE_SQUARES[piece][location ^ RANK_FLIP];
            piece_square_scores[BLACK][piece - 1][location] = -MATERIAL_VALUES[piece] - PIECE_SQUARES[piece][location];
        }
        king_endgame_delta[WHITE][location] =  KING_SQUARE_ENDGAME[location ^ RANK_FLIP] - KING_SQUARE_MIDGAME[location ^ RANK_FLIP];
        king_endgame_delta[BLACK][location] = -KING_SQUARE_ENDGAME[location            ] + KING_SQUARE_MIDGAME[location            ];
    }
}

static int EvaluatePawns(const Position* position, int alpha, int beta)
{
    INCREMENT("eval king pawn");
    int scores[2] = { 0 };
    for (int color = WHITE; color <= BLACK; ++color)
    {
        const bitboard friendly_pawns  = position->pawns & position->pieces_of_color[      color ];
        const bitboard enemy_pawns     = position->pawns & position->pieces_of_color[ENEMY(color)];
        bitboard pawns = friendly_pawns;
        while (pawns)
        {
            const int location          = FindAndClearLsb(&pawns);
            const Sets* sets            = &SETS[location];
            const PawnSets* pawn_sets   = &PAWN_SETS[color][location];

            if (pawn_sets->doubled_pawn_mask & enemy_pawns)
            {
                scores[color] += SCORE_PAWN_BLOCKED;
            }

            if (pawn_sets->doubled_pawn_mask & friendly_pawns)
            {
                scores[color] += SCORE_PAWN_DOUBLED;
            }
            else if (!(pawn_sets->passed_pawn_mask & enemy_pawns))
            {
                scores[color] += SCORE_PAWN_PASSED;
            }

            if (sets->pawn_attacks[ENEMY(color)] & friendly_pawns)
            {
                scores[color] += SCORE_PAWN_DEFENDED;
            }
            else if (pawn_sets->supported_pawn_mask & friendly_pawns)
            {
                scores[color] += SCORE_PAWN_SUPPORTED;
            }
            else if (!(pawn_sets->isolated_pawn_mask & friendly_pawns))
            {
                scores[color] += SCORE_PAWN_ISOLATED;
            }
        }
    }
    return scores[WHITE] - scores[BLACK];
    (void)alpha;
    (void)beta;
}

static const int CLASSICAL_MATERIAL[7] =
{
    0,
    1,
    3,
    3,
    5,
    9,
    0
};

/* 4 x 3 (N) + 4 x 3 (B) + 4 x 5 (R) + 2 x 9 (Q) = 62 */
#define INITIAL_CLASSICAL_MATERIAL 62

/**
 * @brief Determine the percentage of non pawn material remaining.
 * @param position the position
 * @return percentage of non pawn left on the board
*/
static int ClassicalMaterialRemaining(const Position* position)
{
    int material = 0;
    for (int piece = KNIGHT; piece <= QUEEN; ++piece)
    {
        material += CLASSICAL_MATERIAL[piece] * PopCount(position->pieces[piece]);
    }
    return material;
}

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
    int score = position->score + EvaluatePawns(position, alpha, beta);
    /* Give a bonus for friendly players near our king */
    score += 5 * PopCount(SETS[position->king_location[WHITE]].king_attacks & position->white_pieces)
           - 5 * PopCount(SETS[position->king_location[BLACK]].king_attacks & position->black_pieces);

    const int endgame_delta = king_endgame_delta[WHITE][position->king_location[WHITE]] 
                            + king_endgame_delta[BLACK][position->king_location[BLACK]];
    
    const int early_game_delta = 20 * PopCount(SETS[position->king_location[WHITE]].king_attacks & position->white_pieces & position->pawns)
                               - 20 * PopCount(SETS[position->king_location[BLACK]].king_attacks & position->black_pieces & position->pawns);

    const int material_remaining = ClassicalMaterialRemaining(position);

    score += (early_game_delta * material_remaining + endgame_delta * (INITIAL_CLASSICAL_MATERIAL - material_remaining)) / INITIAL_CLASSICAL_MATERIAL;
    score /= 5;
    score *= 5;
    
    return position->state_flags & IS_BLACK_TO_MOVE ? -score : score;
    (void)alpha;
    (void)beta;
}



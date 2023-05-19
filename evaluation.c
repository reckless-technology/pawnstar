#include "pawnstar.h"

static const int PAWN_SQUARE[64] = 
{
     0,  0,  0,  0,  0,  0,  0,  0,
    50, 50, 50, 50, 50, 50, 50, 50,
    10, 10, 20, 30, 30, 20, 10, 10,
     5,  5, 10, 25, 25, 10,  5,  5,
     0,  0,  0, 20, 20,  0,  0,  0,
     5, -5,-10,  0,  0,-10, -5,  5,
     5, 10, 10,-20,-20, 10, 10,  5,
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

static const int BISHOP_SQUARE[64] = 
{
    -20,-10,-10,-10,-10,-10,-10,-20,
    -10,  0,  0,  0,  0,  0,  0,-10,
    -10,  0,  5, 10, 10,  5,  0,-10,
    -10,  5,  5, 10, 10,  5,  5,-10,
    -10,  0, 10, 10, 10, 10,  0,-10,
    -10, 10, 10, 10, 10, 10, 10,-10,
    -10,  5,  0,  0,  0,  0,  5,-10,
    -20,-10,-10,-10,-10,-10,-10,-20
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

static const int* const PIECE_SQUARES[7] = 
{
    NULL,
    PAWN_SQUARE,
    KNIGHT_SQUARE,
    BISHOP_SQUARE,
    ROOK_SQUARE,
    QUEEN_SQUARE,
    KING_SQUARE_MIDGAME,
};

static const int MATERIAL_VALUES[7] = 
{
    [PAWN]      = 100,
    [KNIGHT]    = 320,
    [BISHOP]    = 350,
    [ROOK]      = 600,
    [QUEEN]     = 1200,
};

/* 4x3 (N) + 4x3 (B) + 4x5 (R) + 2x9 (Q) = 62 (ignoring pawns) */
#define INITIAL_CLASSICAL_MATERIAL 62

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
    int score = position->score;
    const int material_remaining =
        3 * PopCount(position->knights)
      + 3 * PopCount(position->bishops)
      + 5 * PopCount(position->rooks)
      + 9 * PopCount(position->queens);
    if (material_remaining < 16)
    {
        const int endgame_delta = king_endgame_delta[WHITE][position->king_location[WHITE]]
                                + king_endgame_delta[BLACK][position->king_location[BLACK]];
        score += endgame_delta;
    }
    else if (material_remaining < 32)
    {
        const int endgame_delta = king_endgame_delta[WHITE][position->king_location[WHITE]]
                                + king_endgame_delta[BLACK][position->king_location[BLACK]];
        score += (endgame_delta * (32 - material_remaining)) / 16;
    }
    return position->state_flags & IS_BLACK_TO_MOVE ? -score : score;
    (void)alpha;
    (void)beta;
}
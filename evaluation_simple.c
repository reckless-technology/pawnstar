#include "pawnstar.h"
#if !DO_EVALUATION_FULL
/******************************************************************************
This is a very simple evaluation function as described by Tomasz Michniewski
Refer to http://chessprogramming.wikispaces.com/Simplified+evaluation+function

The evaluation funtion is based on material and piece-square tables. 

The idea is that what it lacks in sophistication (a lot) it makes up for in 
speed, thus hopefully allowing greater search depth. It's useful for regression
testing of the "full" evaluation function, and performs surprisingly well in
actual game tests.
*******************************************************************************/
static const int PAWN_SQUARE[64] = {
     0,  0,  0,  0,  0,  0,  0,  0,
    50, 50, 50, 50, 50, 50, 50, 50,
    10, 10, 20, 30, 30, 20, 10, 10,
     5,  5, 10, 25, 25, 10,  5,  5,
     0,  0,  0, 20, 20,  0,  0,  0,
     5, -5,-10,  0,  0,-10, -5,  5,
     5, 10, 10,-20,-20, 10, 10,  5,
     0,  0,  0,  0,  0,  0,  0,  0,
};
static const int KNIGHT_SQUARE[64] = {
    -50,-40,-30,-30,-30,-30,-40,-50,
    -40,-20,  0,  0,  0,  0,-20,-40,
    -30,  0, 10, 15, 15, 10,  0,-30,
    -30,  5, 15, 20, 20, 15,  5,-30,
    -30,  0, 15, 20, 20, 15,  0,-30,
    -30,  5, 10, 15, 15, 10,  5,-30,
    -40,-20,  0,  5,  5,  0,-20,-40,
    -50,-40,-30,-30,-30,-30,-40,-50,
};
static const int BISHOP_SQUARE[64] = {
    -20,-10,-10,-10,-10,-10,-10,-20,
    -10,  0,  0,  0,  0,  0,  0,-10,
    -10,  0,  5, 10, 10,  5,  0,-10,
    -10,  5,  5, 10, 10,  5,  5,-10,
    -10,  0, 10, 10, 10, 10,  0,-10,
    -10, 10, 10, 10, 10, 10, 10,-10,
    -10,  5,  0,  0,  0,  0,  5,-10,
    -20,-10,-10,-10,-10,-10,-10,-20,
};
static const int ROOK_SQUARE[64] = {
      0,  0,  0,  0,  0,  0,  0,  0,
      5, 10, 10, 10, 10, 10, 10,  5,
     -5,  0,  0,  0,  0,  0,  0, -5,
     -5,  0,  0,  0,  0,  0,  0, -5,
     -5,  0,  0,  0,  0,  0,  0, -5,
     -5,  0,  0,  0,  0,  0,  0, -5,
     -5,  0,  0,  0,  0,  0,  0, -5,
      0,  0,  0,  5,  5,  0,  0,  0,
};
static const int QUEEN_SQUARE[64] = {
    -20,-10,-10, -5, -5,-10,-10,-20,
    -10,  0,  0,  0,  0,  0,  0,-10,
    -10,  0,  5,  5,  5,  5,  0,-10,
     -5,  0,  5,  5,  5,  5,  0, -5,
      0,  0,  5,  5,  5,  5,  0, -5,
    -10,  5,  5,  5,  5,  5,  0,-10,
    -10,  0,  5,  0,  0,  0,  0,-10,
    -20,-10,-10, -5, -5,-10,-10,-20,
};
static const int KING_SQUARE_MIDGAME[64] = {
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -20,-30,-30,-40,-40,-30,-30,-20,
    -10,-20,-20,-20,-20,-20,-20,-10,
     20, 20,  0,  0,  0,  0, 20, 20,
     20, 30, 10,  0,  0, 10, 30, 20,
};
static const int KING_SQUARE_ENDGAME[64] = {
    -50,-40,-30,-20,-20,-30,-40,-50,
    -30,-20,-10,  0,  0,-10,-20,-30,
    -30,-10, 20, 30, 30, 20,-10,-30,
    -30,-10, 30, 40, 40, 30,-10,-30,
    -30,-10, 30, 40, 40, 30,-10,-30,
    -30,-10, 20, 30, 30, 20,-10,-30,
    -30,-30,  0,  0,  0,  0,-30,-30,
    -50,-30,-30,-30,-30,-30,-30,-50,
};
static const int* const PIECE_SQUARES[8] = {
    NULL,
    PAWN_SQUARE,
    KNIGHT_SQUARE,
    BISHOP_SQUARE,
    ROOK_SQUARE,
    QUEEN_SQUARE,
    KING_SQUARE_MIDGAME,
    NULL,
};
static const int MATERIAL_VALUES[8] = {
      0,
    100, // pawn
    320, // knight
    330, // bishop
    500, // rook
    900, // queen
      0, // king
      0,
};

static int piece_square_values[2][8][64];
static int king_endgame_values[2][64];

/******************************************************************************
Set up the piece square tables
*******************************************************************************/
void InitializeEval()
{
    int location;
    for (location = A1; location <= H8; ++location)
    {
        int piece;
        for (piece = PAWN; piece <= KING; ++piece)
        {
            piece_square_values[WHITE][piece][location] = MATERIAL_VALUES[piece] + PIECE_SQUARES[piece][location ^ RANK_FLIP];
            piece_square_values[BLACK][piece][location] = MATERIAL_VALUES[piece] + PIECE_SQUARES[piece][location];
        }
        king_endgame_values[WHITE][location] = KING_SQUARE_ENDGAME[location ^ RANK_FLIP];
        king_endgame_values[BLACK][location] = KING_SQUARE_ENDGAME[location];
    }
}

/******************************************************************************
Evaluate the current position, assuming neither king is in check and the 
position is quiet.
*******************************************************************************/
int EvaluatePosition(const Position* position, int alpha, int beta)
{    
    int score = 0;
    int piece;
    if (IsDrawByMaterial(position))
    {
        return DRAW_SCORE;
    }
    for (piece = PAWN; piece <= QUEEN; ++piece)
    {
        bitboard b = position->pieces[piece] & position->white_pieces;
        while (b)
        {
            const int locn = FindAndClearLsb(&b);
            score += piece_square_values[WHITE][piece][locn];
        }
        b = position->pieces[piece] & position->black_pieces;
        while (b)
        {
            const int locn = FindAndClearLsb(&b);
            score -= piece_square_values[BLACK][piece][locn];
        }
    }    
    /**************************************************************************
    Endgame is simply classified as:
    # no queens on the board, OR
    # fewer than 8 non-pawn pieces on the board
    ***************************************************************************/
    if (!position->queens || 
        PopCount(position->occupied_squares ^ position->pawns) < 8)
    {
        score += king_endgame_values[WHITE][position->king_location[WHITE]];
        score -= king_endgame_values[BLACK][position->king_location[BLACK]];
    }
    else
    {
        score += piece_square_values[WHITE][KING][position->king_location[WHITE]];
        score -= piece_square_values[BLACK][KING][position->king_location[BLACK]];
    }
    return position->state_flags & IS_BLACK_TO_MOVE ? -score : score;
    (void)alpha;
    (void)beta;
}
#endif

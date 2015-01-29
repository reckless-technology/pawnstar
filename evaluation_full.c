#include "pawnstar.h"
#if DO_EVALUATION_FULL
static const int PAWN_SQUARE[64] = {
     0,  0,  0,  0,  0,  0,  0,  0,
    50, 50, 50, 50, 50, 50, 50, 50,
    10, 15, 20, 30, 30, 20, 15, 10,
     5, 10, 10, 25, 25, 10, 10,  5,
     0,  0,  0, 20, 20,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,-20,-20,  0,  0,  0,
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
    400, // knight
    400, // bishop
    600, // rook
   1200, // queen
      0, // king
      0,
};

#define SCORE_BISHOP_PAIR           50  // bonus for the bishop pair
#define SCORE_KNIGHT_PAIR          -20  // penalty for the knight pair
#define SCORE_PAWN_KING_ADJ1        20  // bonus for each pawn standing directly in front of the king after castling
#define SCORE_PAWN_KING_ADJ2        10  // bonus for each pawn standing on the 3rd rank in front of the king
#define SCORE_ISOLATED_PAWN        -20  // penalty for an isolated pawn
#define SCORE_DOUBLED_PAWN         -10  // penalty for doubled or triped pawn
#define SCORE_FORFEIT_CASTLING     -40  // penalty for forfeiting castling rights without castling
#define SCORE_MATERIAL_THRESHOLD   150  // threshold for eval cutoff on material balance only

/******************************************************************************
Added to the winning side based on the total number of knights, bishops, rooks
and queens on the board. Encourages exchange of pieces but not pawns.
*******************************************************************************/
static const int PIECE_COUNT_VALUES[32] = { 70, 65, 60, 55, 50, 45, 40, 35, 30, 25, 20, 15, 10, 5, 0, /* ... */ };
/******************************************************************************
Set up the piece square tables
*******************************************************************************/
void InitializeEval()
{
}
/******************************************************************************
Evaluate the current position, assuming neither king is in check and the 
position is quiet.
*******************************************************************************/
int EvaluatePosition(const Position* position, int alpha, int beta)
{    
    int score = 0;
    int piece;
    const int score_sign = position->state_flags & IS_BLACK_TO_MOVE ? -1 : 1;
    if (IsDrawByMaterial(position))
    {
        return DRAW_SCORE;
    }
    /**************************************************************************
    Material
    ***************************************************************************/
    score = 
        MATERIAL_VALUES[PAWN]   * (PopCount(position->pawns   & position->white_pieces) - PopCount(position->pawns   & position->black_pieces)) +
        MATERIAL_VALUES[KNIGHT] * (PopCount(position->knights & position->white_pieces) - PopCount(position->knights & position->black_pieces)) +
        MATERIAL_VALUES[BISHOP] * (PopCount(position->bishops & position->white_pieces) - PopCount(position->bishops & position->black_pieces)) +
        MATERIAL_VALUES[ROOK]   * (PopCount(position->rooks   & position->white_pieces) - PopCount(position->rooks   & position->black_pieces)) +
        MATERIAL_VALUES[QUEEN]  * (PopCount(position->queens  & position->white_pieces) - PopCount(position->queens  & position->black_pieces));
    if (PopCount(position->bishops & position->white_pieces) >= 2)
    {
        score += SCORE_BISHOP_PAIR;
    }
    if (PopCount(position->bishops & position->black_pieces) >= 2)
    {
        score -= SCORE_BISHOP_PAIR;
    }
    if (PopCount(position->knights & position->white_pieces) >= 2)
    {
        score += SCORE_KNIGHT_PAIR;
    }
    if (PopCount(position->knights & position->black_pieces) >= 2)
    {
        score -= SCORE_KNIGHT_PAIR;
    }   
    if (score > 0)
    {
        score += PIECE_COUNT_VALUES[PopCount(position->knights | position->bishops | position->rooks | position->queens)];
    }
    else if (score < 0)
    {
        score -= PIECE_COUNT_VALUES[PopCount(position->knights | position->bishops | position->rooks | position->queens)];
    }
    if (score * score_sign > beta + SCORE_MATERIAL_THRESHOLD)
    {
        INCREMENT("eval beta cutoffs");
        return beta;
    }
    if (score * score_sign < alpha - SCORE_MATERIAL_THRESHOLD)
    {
        INCREMENT("eval alpha cutoffs");
        return alpha;
    }
    /**************************************************************************
    Piece square tables
    ***************************************************************************/
    for (piece = PAWN; piece <= QUEEN; ++piece)
    {
        bitboard b = position->pieces[piece] & position->white_pieces;
        while (b)
        {
            const int locn = FindAndClearLsb(&b);
            score += PIECE_SQUARES[piece][locn ^ RANK_FLIP];
        }
        b = position->pieces[piece] & position->black_pieces;
        while (b)
        {
            const int locn = FindAndClearLsb(&b);
            score -= PIECE_SQUARES[piece][locn];
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
        score += KING_SQUARE_ENDGAME[position->king_location[WHITE] ^ RANK_FLIP];
        score -= KING_SQUARE_ENDGAME[position->king_location[BLACK]];
    }
    else
    {
        /**********************************************************************
        Midgame
        ***********************************************************************/
        score += KING_SQUARE_MIDGAME[position->king_location[WHITE] ^ RANK_FLIP];
        score -= KING_SQUARE_MIDGAME[position->king_location[BLACK]];
        /**********************************************************************
        King pawn shield
        ***********************************************************************/
        const bitboard white_pawns = position->pawns & position->white_pieces;
        const bitboard black_pawns = position->pawns & position->black_pieces;
        score += SCORE_PAWN_KING_ADJ1 * PopCount(KING_PAWN_SHIELD_WHITE  [position->king_location[WHITE]] & white_pawns);
        score += SCORE_PAWN_KING_ADJ2 * PopCount(KING_PAWN_SHIELD_WHITE_2[position->king_location[WHITE]] & white_pawns);
        score -= SCORE_PAWN_KING_ADJ1 * PopCount(KING_PAWN_SHIELD_BLACK  [position->king_location[BLACK]] & black_pawns);
        score -= SCORE_PAWN_KING_ADJ2 * PopCount(KING_PAWN_SHIELD_BLACK_2[position->king_location[BLACK]] & black_pawns);
        /**********************************************************************
        Pawns
        ***********************************************************************/
        const bitboard white_pawn_files     = FillNorthAndSouth(white_pawns);
        const bitboard black_pawn_files     = FillNorthAndSouth(black_pawns);
        const bitboard white_isolated_pawns = white_pawns & ~(SHIFT_WEST(white_pawn_files) | SHIFT_EAST(white_pawn_files));
        const bitboard black_isolated_pawns = black_pawns & ~(SHIFT_WEST(black_pawn_files) | SHIFT_EAST(black_pawn_files));
        const bitboard white_doubled_pawns  = white_pawns & FillSouth(SHIFT_SOUTH(white_pawns));
        const bitboard black_doubled_pawns  = black_pawns & FillNorth(SHIFT_NORTH(black_pawns));
        score += SCORE_ISOLATED_PAWN * PopCount(white_isolated_pawns);
        score -= SCORE_ISOLATED_PAWN * PopCount(black_isolated_pawns);
        score += SCORE_DOUBLED_PAWN  * PopCount(white_doubled_pawns);
        score -= SCORE_DOUBLED_PAWN  * PopCount(black_doubled_pawns);
        /**********************************************************************
        Forfeit of castling rights
        ***********************************************************************/
        if (!(position->castle_flags & (MAY_WHITE_K | MAY_WHITE_Q | HAS_WHITE_CASTLED)))
        {
            score += SCORE_FORFEIT_CASTLING;
        }
        if (!(position->castle_flags & (MAY_BLACK_K | MAY_BLACK_Q | HAS_BLACK_CASTLED)))
        {
            score -= SCORE_FORFEIT_CASTLING;
        }
    }
    return score * score_sign;
}
#endif
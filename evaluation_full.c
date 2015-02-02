#include "pawnstar.h"
#if DO_EVALUATION_FULL
static const int PAWN_SQUARE_MIDGAME[64] = {
     0,  0,  0,  0,  0,  0,  0,  0,
    35, 35, 35, 35, 35, 35, 35, 35, 
    10, 20, 20, 30, 30, 20, 20, 10,
     5, 15, 15, 25, 25, 15, 15,  5,
     0,  0,  0, 20, 20,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,-20,-20,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,
};
static const int PAWN_SQUARE_ENDGAME[64] = {
     0,  0,  0,  0,  0,  0,  0,  0,
    70, 70, 70, 70, 70, 70, 70, 70,  
    50, 50, 50, 50, 50, 50, 50, 50, 
    40, 40, 40, 40, 40, 40, 40, 40, 
    30, 30, 30, 30, 30, 30, 30, 30, 
    20, 20, 20, 20, 20, 20, 20, 20, 
    10, 10, 10, 10, 10, 10, 10, 10, 
     0,  0,  0,  0,  0,  0,  0,  0,
};
static const int KNIGHT_SQUARE[64] = {
    -40,-30,-20,-10,-10,-20,-30,-40,
    -30,-20,-10,  0,  0,-10,-20,-30,
    -20,-10,  0, 10, 10,  0,-10,-20,
    -10,  0, 10, 20, 20, 10,  0,-10,
    -10,  0, 10, 20, 20, 10,  0,-10,
    -20,-10,  0, 10, 10,  0,-10,-20,
    -30,-20,-10,  0,  0,-10,-20,-30,
    -40,-30,-20,-10,-10,-20,-30,-40,   
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
#define SCORE_KNIGHT_PAIR          -30  // penalty for the knight pair
#define SCORE_PAWN_KING_ADJ1        25  // bonus for each pawn standing directly in front of the king after castling
#define SCORE_PAWN_KING_ADJ2        10  // bonus for each pawn standing on the 3rd rank in front of the king
#define SCORE_ISOLATED_PAWN        -20  // penalty for an isolated pawn
#define SCORE_DOUBLED_PAWN         -10  // penalty for doubled or triped pawn
#define SCORE_FORFEIT_CASTLING     -30  // penalty for forfeiting castling rights without castling
#define SCORE_EIGHT_PAWNS          -20  // penalty for not having exchanged at least one pawn
#define SCORE_PROTECTED_PAWN        10  // bonus for pawn protected by a friendly pawn
#define SCORE_MATERIAL_THRESHOLD   150  // threshold for eval cutoff on material balance only

/******************************************************************************
Added to the materially ahead side, indexed by the total number of knights, 
bishops, rooks and queens on the board. Encourages the side which is ahead on
material to exchange pieces but not pawns, i.e. trade down to a hopefully won
endgame.
*******************************************************************************/
static const int TRADE_DOWN_BONUS[32] = { 70, 65, 60, 55, 50, 45, 40, 35, 30, 25, 20, 15, 10, 5, 0, /* ... */ };
/******************************************************************************
Bonus / penalty for bishops based on number of available psuedo legal target 
squares. The rest should be handled by search.
*******************************************************************************/
static const int BISHOP_MOBILITY[14] = { -30, -20, 0, 5, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10 };
/******************************************************************************
Set up the piece square tables
*******************************************************************************/
void InitializeEval()
{
    // nothing to do in this version
}
/******************************************************************************
Evaluate the current position, assuming neither king is in check and the 
position is quiet. This returns alpha or beta in the case of material cutoff
and thus requires a fail hard alpha beta framework in which to operate 
correctly.
*******************************************************************************/
int EvaluatePosition(const Position* position, int alpha, int beta)
{    
    int score = 0;
    if (IsDrawByMaterial(position))
    {
        return DRAW_SCORE;
    }
    const int score_sign  = position->state_flags & IS_BLACK_TO_MOVE ? -1 : 1;
    const bool is_endgame = !position->queens || PopCount(position->occupied_squares ^ position->pawns) < 8;
    /**************************************************************************
    Material
    ***************************************************************************/
    const bitboard white_pawns   = position->pawns   & position->white_pieces;
    const bitboard black_pawns   = position->pawns   & position->black_pieces;
    const bitboard white_knights = position->knights & position->white_pieces;
    const bitboard black_knights = position->knights & position->black_pieces;
    const bitboard white_bishops = position->bishops & position->white_pieces;
    const bitboard black_bishops = position->bishops & position->black_pieces;
    const bitboard white_rooks   = position->rooks   & position->white_pieces;
    const bitboard black_rooks   = position->rooks   & position->black_pieces;
    const bitboard white_queens  = position->queens  & position->white_pieces;
    const bitboard black_queens  = position->queens  & position->black_pieces;
    score = 
        MATERIAL_VALUES[PAWN]   * (PopCount(white_pawns)   - PopCount(black_pawns))   +
        MATERIAL_VALUES[KNIGHT] * (PopCount(white_knights) - PopCount(black_knights)) +
        MATERIAL_VALUES[BISHOP] * (PopCount(white_bishops) - PopCount(black_bishops)) +
        MATERIAL_VALUES[ROOK]   * (PopCount(white_rooks)   - PopCount(black_rooks))   +
        MATERIAL_VALUES[QUEEN]  * (PopCount(white_queens)  - PopCount(black_queens));
    if (PopCount(white_bishops) >= 2)
    {
        score += SCORE_BISHOP_PAIR;
    }
    if (PopCount(black_bishops) >= 2)
    {
        score -= SCORE_BISHOP_PAIR;
    }
    if (PopCount(white_knights) >= 2)
    {
        score += SCORE_KNIGHT_PAIR;
    }
    if (PopCount(black_knights) >= 2)
    {
        score -= SCORE_KNIGHT_PAIR;
    }  
    if (PopCount(white_pawns) == 8)
    {
        score += SCORE_EIGHT_PAWNS;
    }
    if (PopCount(black_pawns) == 8)
    {
        score -= SCORE_EIGHT_PAWNS;
    }
    if (score > 0)
    {
        score += TRADE_DOWN_BONUS[PopCount(position->knights | position->bishops | position->rooks | position->queens)];
    }
    else if (score < 0)
    {
        score -= TRADE_DOWN_BONUS[PopCount(position->knights | position->bishops | position->rooks | position->queens)];
    }
    if (score * score_sign > beta + SCORE_MATERIAL_THRESHOLD)
    {
        INCREMENT("eval beta cutoffs");
        return score * score_sign;
    }
    if (score * score_sign < alpha - SCORE_MATERIAL_THRESHOLD)
    {
        INCREMENT("eval alpha cutoffs");
        return score * score_sign;
    }
    /**************************************************************************
    Piece square tables
    ***************************************************************************/
    const int* const piece_square_table[8] = 
    {
        NULL,
        is_endgame ? PAWN_SQUARE_ENDGAME : PAWN_SQUARE_MIDGAME,
        KNIGHT_SQUARE,
        BISHOP_SQUARE,
        ROOK_SQUARE,
        QUEEN_SQUARE,
        is_endgame ? KING_SQUARE_ENDGAME : KING_SQUARE_MIDGAME,
        NULL,
    };
    for (int piece = PAWN; piece <= KING; ++piece)
    {
        const int* const table = piece_square_table[piece];
        bitboard b = position->pieces[piece] & position->white_pieces;        
        while (b)
        {
            score += table[FindAndClearLsb(&b) ^ RANK_FLIP];
        }
        b = position->pieces[piece] & position->black_pieces;
        while (b)
        {
            score -= table[FindAndClearLsb(&b)];
        }
    }
    /**************************************************************************
    Pawns
    ***************************************************************************/
    const bitboard white_pawn_attacks   = SHIFT_NORTHWEST(white_pawns) | SHIFT_NORTHEAST(white_pawns);
    const bitboard black_pawn_attacks   = SHIFT_SOUTHWEST(black_pawns) | SHIFT_SOUTHEAST(black_pawns);
    const bitboard white_pawn_files     = FillNorthAndSouth(white_pawns);
    const bitboard black_pawn_files     = FillNorthAndSouth(black_pawns);
    const bitboard white_isolated_pawns = white_pawns & ~(SHIFT_WEST(white_pawn_files) | SHIFT_EAST(white_pawn_files));
    const bitboard black_isolated_pawns = black_pawns & ~(SHIFT_WEST(black_pawn_files) | SHIFT_EAST(black_pawn_files));
    const bitboard white_doubled_pawns  = white_pawns & FillSouth(SHIFT_SOUTH(white_pawns));
    const bitboard black_doubled_pawns  = black_pawns & FillNorth(SHIFT_NORTH(black_pawns));
    score += SCORE_ISOLATED_PAWN  * PopCount(white_isolated_pawns);
    score -= SCORE_ISOLATED_PAWN  * PopCount(black_isolated_pawns);
    score += SCORE_DOUBLED_PAWN   * PopCount(white_doubled_pawns);
    score -= SCORE_DOUBLED_PAWN   * PopCount(black_doubled_pawns);
    score += SCORE_PROTECTED_PAWN * PopCount(white_pawns & white_pawn_attacks);
    score -= SCORE_PROTECTED_PAWN * PopCount(black_pawns & black_pawn_attacks);
    /**************************************************************************
    Bishop mobility - very primitive heuristic which penalizes poor bishop
    mobility. Requires improvement!
    ***************************************************************************/
    bitboard b = white_bishops;
    while (b)
    {
        score += BISHOP_MOBILITY[PopCount(BishopAttacks(position->occupied_squares, FindAndClearLsb(&b)) & ~position->white_pieces)];
    }
    b = black_bishops;
    while (b)
    {
        score -= BISHOP_MOBILITY[PopCount(BishopAttacks(position->occupied_squares, FindAndClearLsb(&b)) & ~position->black_pieces)];
    }
    if (!is_endgame)
    {
        /**********************************************************************
        King pawn shield
        ***********************************************************************/
        score += SCORE_PAWN_KING_ADJ1 * PopCount(KING_PAWN_SHIELD_WHITE  [position->king_location[WHITE]] & white_pawns);
        score += SCORE_PAWN_KING_ADJ2 * PopCount(KING_PAWN_SHIELD_WHITE_2[position->king_location[WHITE]] & white_pawns);
        score -= SCORE_PAWN_KING_ADJ1 * PopCount(KING_PAWN_SHIELD_BLACK  [position->king_location[BLACK]] & black_pawns);
        score -= SCORE_PAWN_KING_ADJ2 * PopCount(KING_PAWN_SHIELD_BLACK_2[position->king_location[BLACK]] & black_pawns);       
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
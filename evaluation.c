#include "pawnstar.h"
#if DO_EXTRA_EVAL

static const int PAWN_SQUARE[64] = {
    0,  0,  0,  0,  0,  0,  0,  0,
   50, 50, 50, 50, 50, 50, 50, 50,
   10, 10, 20, 30, 30, 20, 10, 10,
    5,  5, 10, 25, 25, 10,  5,  5,
    0,  0,  0, 20, 20,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,-20,-20,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0
};

#else

static const int PAWN_SQUARE[64] = {
     0,  0,  0,  0,  0,  0,  0,  0,
    50, 50, 50, 50, 50, 50, 50, 50,
    10, 10, 20, 30, 30, 20, 10, 10,
     5,  5, 10, 25, 25, 10,  5,  5,
     0,  0,  0, 20, 20,  0,  0,  0,
     5, -5,-10,  0,  0,-10, -5,  5,
     5, 10, 10,-20,-20, 10, 10,  5,
     0,  0,  0,  0,  0,  0,  0,  0
};

#endif

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
static const int RANDOM_FLUCTUATION[8] = {
    -5, 0, 0, 0, 0, 0, 0, 5
};

#if DO_EXTRA_EVAL

static const int MATERIAL_VALUES[7] = {
      0,
    100, // pawn
    325, // knight
    325, // bishop
    500, // rook
   1000, // queen
      0, // king
};

#else

static const int MATERIAL_VALUES[7] = {
    0,
    100, // pawn
    320, // knight
    330, // bishop
    500, // rook
    900, // queen
    0, // king
};

#endif

int         piece_square_values[2][8][64];
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
            piece_square_values[WHITE][piece][location] =  MATERIAL_VALUES[piece] + PIECE_SQUARES[piece][location ^ RANK_FLIP];
            piece_square_values[BLACK][piece][location] = -MATERIAL_VALUES[piece] - PIECE_SQUARES[piece][location];
        }
        king_endgame_delta[WHITE][location] =  KING_SQUARE_ENDGAME[location ^ RANK_FLIP] - KING_SQUARE_MIDGAME[location ^ RANK_FLIP];
        king_endgame_delta[BLACK][location] = -KING_SQUARE_ENDGAME[location] + KING_SQUARE_MIDGAME[location];
    }
}

/*
Evaluate the current position, assuming neither king is in check and the 
position is quiet.
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
    const bool is_endgame = !position->queens || PopCount(position->occupied_squares ^ position->pawns) < 8;
    if (is_endgame)
    {
        score += king_endgame_delta[WHITE][position->king_location[WHITE]] + 
                 king_endgame_delta[BLACK][position->king_location[BLACK]];
        INCREMENT("eval endgames");
    }

#if DO_EXTRA_EVAL
    // Defended pawns
    const bitboard white_pawns = position->pawns & position->white_pieces;
    const bitboard black_pawns = position->pawns & position->black_pieces;
    const bitboard white_pawn_attacks = SHIFT_NORTHWEST(white_pawns) | SHIFT_NORTHEAST(white_pawns);
    const bitboard black_pawn_attacks = SHIFT_SOUTHWEST(black_pawns) | SHIFT_SOUTHEAST(black_pawns);
    score +=  5 * (PopCount(white_pawns & white_pawn_attacks) - 
                   PopCount(black_pawns & black_pawn_attacks)); 
    // Bishop pair
    const bitboard white_bishops = position->bishops & position->white_pieces;
    const bitboard black_bishops = position->bishops & position->black_pieces;
    if ((white_bishops & WHITE_SQUARES) && (white_bishops & BLACK_SQUARES))
    {
        score += 40;
    }
    if ((black_bishops & WHITE_SQUARES) && (black_bishops & BLACK_SQUARES))
    {
        score -= 40;
    }
    // Mid game features only
    if (!is_endgame)
    {
        // King pawn shelter
        score += 15 * (PopCount(KING_PAWN_SHIELD_WHITE  [position->king_location[WHITE]] & white_pawns) - 
                       PopCount(KING_PAWN_SHIELD_BLACK  [position->king_location[BLACK]] & black_pawns));
        score +=  5 * (PopCount(KING_PAWN_SHIELD_WHITE_2[position->king_location[WHITE]] & white_pawns) - 
                       PopCount(KING_PAWN_SHIELD_BLACK_2[position->king_location[BLACK]] & black_pawns));
        // King on open file
        if (!(NORTH_OF[position->king_location[WHITE]] & white_pawns))
        {
            score -= 20;
        }
        if (!(SOUTH_OF[position->king_location[BLACK]] & black_pawns))
        {
            score += 20;
        }
        // Forfeit of castling rights
        if (!(position->castle_flags & (MAY_WHITE_K | MAY_WHITE_Q)))
        {
            score -= 20;
        }
        if (!(position->castle_flags & (MAY_BLACK_K | MAY_BLACK_Q)))
        {
            score += 20;
        }
    }
#endif
    score += RANDOM_FLUCTUATION[NextRandom() & 7];
    return position->state_flags & IS_BLACK_TO_MOVE ? -score + 10 : score + 10; // tempo bonus
    (void)alpha;
    (void)beta;
}

#include "pawnstar.h"
#if DO_EVALUATION_FULL
/******************************************************************************
The more experimental "advanced" evaluation function: in practice this does not
seem to be much better than the simplified evaluation function, so it clearly 
needs some refinement.

The evaluation unit is centipawns (1/100 of a pawn)
*******************************************************************************/

#define MANHATTAN_DISTANCE(x, y) (abs(FILE_OF(x) - FILE_OF(y)) + abs(RANK_OF(x) - RANK_OF(y)))

typedef struct
{
    uint64  hash;
    int     score;
} EvalHashEntry;

static EvalHashEntry eval_hashtable[EVAL_HASHTABLE_SIZE];

static int EvaluateMaterial         (const Position* position, bitboard friendly_pieces);
static int EvaluatePieceSquare      (const Position* position, int color, bitboard friendly_pieces, bool is_endgame);
static int EvaluateMobility         (const Position* position, bitboard friendly_pieces);
static int EvaluateKingSafety       (const Position* position, int color, bitboard friendly_pieces);
static int EvaluatePawnStructure    (const PawnStructure* ps, int material_percent);

static const bitboard* const FORWARD_OF[2] = { NORTH_OF,                  SOUTH_OF                  };
static const bitboard* const KPS1[2]       = { KING_PAWN_SHIELD_WHITE,    KING_PAWN_SHIELD_BLACK    };
static const bitboard* const KPS2[2]       = { KING_PAWN_SHIELD_WHITE_2,  KING_PAWN_SHIELD_BLACK_2  };
static const uchar CASTLE_RIGHTS_MASK[2]   = { MAY_WHITE_K | MAY_WHITE_Q, MAY_BLACK_K | MAY_BLACK_Q };

static const int PAWN_SQUARE_MIDGAME[64] = 
{
     0,  0,  0,  0,  0,  0,  0,  0,
    40, 40, 40, 40, 40, 40, 40, 40, 
    20, 25, 25, 30, 30, 25, 25, 20,
    10, 15, 15, 20, 20, 15, 15, 10,
     0,  0,  0, 10, 10,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,-10,-10,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,
};
static const int PAWN_SQUARE_ENDGAME[64] = 
{
     0,  0,  0,  0,  0,  0,  0,  0,  
    50, 50, 50, 50, 50, 50, 50, 50, 
    40, 40, 40, 40, 40, 40, 40, 40, 
    30, 30, 30, 30, 30, 30, 30, 30, 
    20, 20, 20, 20, 20, 20, 20, 20, 
    10, 10, 10, 10, 10, 10, 10, 10, 
     0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,
};
static const int KNIGHT_SQUARE[64] = 
{
    -40,-30,-20,-10,-10,-20,-30,-40,
    -30,-20,-10,  0,  0,-10,-20,-30,
    -20,-10,  0, 10, 10,  0,-10,-20,
    -10,  0, 10, 25, 25, 10,  0,-10,
    -10,  0, 10, 20, 20, 10,  0,-10,
    -20,-10,  0, 10, 10,  0,-10,-20,
    -30,-20,-10,  0,  0,-10,-20,-30,
    -40,-30,-20,-10,-10,-20,-30,-40,   
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
      0,  0,  0,  5,  5,  0,  0,  0,
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
    -20,-10,-10, -5, -5,-10,-10,-20,
};
static const int KING_SQUARE_MIDGAME[64] = 
{
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -20,-30,-30,-40,-40,-30,-30,-20,
    -10,-20,-20,-20,-20,-20,-20,-10,
     10, 10,  0,  0,  0,  0, 10, 10,
     10, 30, 10,  0,  0, 10, 30, 10,
};
static const int KING_SQUARE_ENDGAME[64] = 
{
    -50,-40,-30,-20,-20,-30,-40,-50,
    -30,-20,-10,  0,  0,-10,-20,-30,
    -30,-10, 20, 30, 30, 20,-10,-30,
    -30,-10, 30, 40, 40, 30,-10,-30,
    -30,-10, 30, 40, 40, 30,-10,-30,
    -30,-10, 20, 30, 30, 20,-10,-30,
    -30,-30,  0,  0,  0,  0,-30,-30,
    -50,-30,-30,-30,-30,-30,-30,-50,
};

static const int* const PIECE_SQUARE_TABLE[8] = 
{
    NULL,
    PAWN_SQUARE_MIDGAME,
    KNIGHT_SQUARE,
    BISHOP_SQUARE,
    ROOK_SQUARE,
    QUEEN_SQUARE,
    KING_SQUARE_MIDGAME,
    NULL,
};

/******************************************************************************
Small random fluctuations in eval add variety to playing style
*******************************************************************************/
static const int RANDOM_FACTOR[16] = { -10, -5, -5, -5, 0, 0, 0, 0, 0, 0, 0, 0, 5, 5, 5, 10 };

#define SCORE_PAWN                         100
#define SCORE_KNIGHT                       380
#define SCORE_BISHOP                       420
#define SCORE_ROOK                         600
#define SCORE_QUEEN                       1200
#define TOTAL_MATERIAL_SUM                9600  // 2 x Q + 4 x R + 4 x B + 4 x N + 16 x P
#define SCORE_MATERIAL_THRESHOLD           400  // threshold for eval cutoff on material balance only
#define SCORE_CASTLING_RIGHTS               50  // bonus for retaining the right to castle
#define SCORE_PAWN_KING_ADJ1                25  // bonus for each pawn standing directly in front of the king after castling
#define SCORE_PAWN_KING_ADJ2                15  // bonus for each pawn standing on the 3rd rank in front of the king
#define SCORE_KING_OPEN_FILE               -30  // penalty for king on a file with no friendly pawns
#define SCORE_KING_ADJ_OPEN_FILE           -20  // penalty for king adjacent file with no friendly pawns
#define SCORE_ISOLATED_PAWN                -20  // penalty for an isolated pawn in the midgame
#define SCORE_ISOLATED_PAWN_END            -30  // penalty for isolated pawn in the endgame
#define SCORE_DOUBLED_PAWN                 -10  // penalty for doubled or triped pawn in the midgame
#define SCORE_PASSED_PAWN                   10  // bonus for passed pawn in the midgame
#define SCORE_PASSED_PAWN_END               20  // bonus for a passed pawn in the endgame
#define SCORE_PROTECTED_PAWN                10  // bonus for pawn protected by a friendly pawn in the midgame
#define SCORE_PROTECTED_PAWN_END            20  // bonus for pawn protected by a friendly pawn in the endgame
#define SCORE_PROTECTED_PASSED_PAWN_END     25  // additional bonus for a passed pawn protected by a friendly pawn in the endgame
/******************************************************************************
Added to the materially ahead side, indexed by the total number of knights, 
bishops, rooks and queens on the board. Encourages the side which is ahead on
material to exchange pieces but not pawns, i.e. try to trade down to a won 
endgame.
*******************************************************************************/
static const int TRADE_DOWN_BONUS[32] = { 140, 130, 120, 110, 100, 90, 80, 70, 60, 50, 40, 30, 20, 10, 0, /* ... */ };
/******************************************************************************
Bonus / penalty for bishops and rooks based on number of available psuedo legal 
target squares. The rest should be handled by search.
*******************************************************************************/
static const int MOBILITY[15] = { -30, -10, 0, 5, 10, 15, 20, 20, 20, 20, 20, 20, 20, 20, 20 };
/******************************************************************************
Penalty for enemy queen proximity (Manhattan distance) to friendly king
*******************************************************************************/
static const int QUEEN_KING_PROXIMITY[15] = { -60, -60, -60, -50, -40, -30, -20, -10, 0, 0, 0, 0, 0, 0, 0 };
/******************************************************************************
Bonus / penalty for number of enemy pieces within 2 squares of the friendly 
king
*******************************************************************************/
static const int ENEMY_PIECES_NEAR_KING[17] = { 20, 10, 0, -10, -20, -30, -40, -50, -60, -70, -70, -70, -70, -70, -70, -70, -70 };
/******************************************************************************
Bonus / penalty for number of friendly pieces within 2 squares of the friendly 
king
*******************************************************************************/
static const int FRIENDLY_PIECES_NEAR_KING[17] = { -30, -20, -10, 0, 10, 20, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30 };

void InitializeEval()
{
    /* nothing to do in this version */
}

/******************************************************************************
Evaluate the current position, assuming neither king is in check and the 
position is quiet. 
*******************************************************************************/
int EvaluatePosition(const Position* position, int alpha, int beta)
{    
    EvalHashEntry* const hash_entry = &eval_hashtable[position->hash % EVAL_HASHTABLE_SIZE];
    if (hash_entry->hash == position->hash)
    {
        INCREMENT("eval hash hits");
        return hash_entry->score;
    }
    if (IsDrawByMaterial(position))
    {
        return DRAW_SCORE;
    }
    const int score_sign = position->state_flags & IS_BLACK_TO_MOVE ? -1 : 1;
    /**************************************************************************
    Material
    ***************************************************************************/
    int material_white   = EvaluateMaterial(position, position->white_pieces);
    int material_black   = EvaluateMaterial(position, position->black_pieces);
    int material_percent = ((material_white + material_black) * 100) / TOTAL_MATERIAL_SUM;
    if (material_white > material_black)
    {
        material_white += TRADE_DOWN_BONUS[PopCount(position->occupied_squares ^ position->kings ^ position->pawns)];
    }
    else if (material_black > material_white)
    {
        material_black += TRADE_DOWN_BONUS[PopCount(position->occupied_squares ^ position->kings ^ position->pawns)];
    }   
    /**************************************************************************
    If we are hugely ahead or behind on material (sufficient for cutoff with a
    comfortable margin for error) then don't bother doing the full evaluation
    as this is clearly not a PV node and thus the score won't propagate up the
    search tree.
    ***************************************************************************/
    const int material_score = (material_white - material_black) * score_sign;
    if (material_score > beta + SCORE_MATERIAL_THRESHOLD)
    {
        INCREMENT("eval beta cutoffs");
        return material_score;
    }
    if (material_score < alpha - SCORE_MATERIAL_THRESHOLD)
    {
        INCREMENT("eval alpha cutoffs");
        return material_score;
    }
    /**************************************************************************
    We define the end game rather simply as:
    # No queens on the board, OR
    # Fewer than 8 non pawn pieces on the board
    ***************************************************************************/
    const bool is_endgame = !position->queens || PopCount(position->occupied_squares ^ position->pawns) < 8;
    PawnStructure ps[2];
    DeterminePawnStructure(position, ps);    
    const int piece_square_score = 
        EvaluatePieceSquare(position, WHITE, position->white_pieces, is_endgame) -
        EvaluatePieceSquare(position, BLACK, position->black_pieces, is_endgame);
    const int king_safety_score = is_endgame ? 0 : 
        EvaluateKingSafety(position, WHITE, position->white_pieces) - 
        EvaluateKingSafety(position, BLACK, position->black_pieces);
    const int mobility_score = 
        EvaluateMobility(position, position->white_pieces) -
        EvaluateMobility(position, position->black_pieces);
    const int pawn_structure_score =
        EvaluatePawnStructure(&ps[WHITE], material_percent) - 
        EvaluatePawnStructure(&ps[BLACK], material_percent);
    int final_score = 
        material_white - material_black + 
        piece_square_score              + 
        mobility_score                  + 
        king_safety_score               + 
        pawn_structure_score            +
        RANDOM_FACTOR[NextRandom() & 0x0F];
    final_score *= score_sign;
    final_score /= 10;
    final_score *= 10; /* quantize score to 1/10 of a pawn value - helps search stability */
    hash_entry->hash  = position->hash;
    hash_entry->score = final_score;
    return final_score;
}

static int EvaluateMaterial(const Position* position, bitboard friendly_pieces)
{
    int score = 
        SCORE_PAWN   * PopCount(position->pawns   & friendly_pieces) +
        SCORE_KNIGHT * PopCount(position->knights & friendly_pieces) +
        SCORE_BISHOP * PopCount(position->bishops & friendly_pieces) +
        SCORE_ROOK   * PopCount(position->rooks   & friendly_pieces) +
        SCORE_QUEEN  * PopCount(position->queens  & friendly_pieces);
    return score;
}

static int EvaluatePieceSquare(const Position* position, int color, bitboard friendly_pieces, bool is_endgame)
{ 
    const int rank_flip = color == WHITE ? RANK_FLIP : 0; /* Flip the piece square tables in the Y axis when evaluating white pieces */
    int score = is_endgame ? KING_SQUARE_ENDGAME[position->king_location[color] ^ rank_flip] : KING_SQUARE_MIDGAME[position->king_location[color] ^ rank_flip];
    const int* const pawn_table = is_endgame ? PAWN_SQUARE_ENDGAME : PAWN_SQUARE_MIDGAME;
    bitboard b = position->pawns & friendly_pieces;
    while (b)
    {
        score += pawn_table[FindAndClearLsb(&b) ^ rank_flip];
    }
    for (int piece = KNIGHT; piece <= QUEEN; ++piece)
    {
        const int* const table = PIECE_SQUARE_TABLE[piece];
        b = position->pieces[piece] & friendly_pieces;       
        while (b)
        {
            score += table[FindAndClearLsb(&b) ^ rank_flip];
        }
    }
    return score;
}

static int EvaluateMobility(const Position* position, bitboard friendly_pieces)
{
    int score = 0;
    /**************************************************************************
    Bishop mobility
    ***************************************************************************/
    bitboard b = position->bishops & friendly_pieces;
    while (b)
    {
        score += MOBILITY[PopCount(BishopAttacks(position->occupied_squares, FindAndClearLsb(&b)) & ~friendly_pieces)];
    }
    /**************************************************************************
    Rook mobility
    ***************************************************************************/
    b = position->rooks & friendly_pieces;
    while (b)
    {
        score += MOBILITY[PopCount(RookAttacks(position->occupied_squares, FindAndClearLsb(&b)) & ~friendly_pieces)];
    }
    return score;
}

static int EvaluateKingSafety(const Position* position, int color, bitboard friendly_pieces)
{
    int score = 0;
    const bitboard enemy_pieces = position->occupied_squares ^ friendly_pieces;
    /**************************************************************************
    King on a file with no friendly pawns in front of him, or on
    adjacent files
    ***************************************************************************/
    const int king_locn           = position->king_location[color];
    const bitboard friendly_pawns = position->pawns & friendly_pieces;
    const bitboard king_file      = FORWARD_OF[color][king_locn];
    const bitboard king_file_west = SHIFT_WEST(king_file);
    const bitboard king_file_east = SHIFT_EAST(king_file);
    if (!(king_file & friendly_pawns))
    {
        score += SCORE_KING_OPEN_FILE;
    }
    if (king_file_west && !(king_file_west & friendly_pawns))
    {
        score += SCORE_KING_ADJ_OPEN_FILE;
    }
    if (king_file_east && !(king_file_east & friendly_pawns))
    {
        score += SCORE_KING_ADJ_OPEN_FILE;
    }
    /**************************************************************************
    King safety - pawn shield - treat the bishop as a pseudo pawn for the 
    purpose of the king pawn shield, to allow for fianchetto bishop to protect
    the king.
    ***************************************************************************/
    const bitboard friendly_pawns_bishops = friendly_pieces & (position->pawns | position->bishops);
    score += SCORE_PAWN_KING_ADJ1 * PopCount(KPS1[color][king_locn] & friendly_pawns_bishops);
    score += SCORE_PAWN_KING_ADJ2 * PopCount(KPS2[color][king_locn] & friendly_pawns_bishops);     
    /**************************************************************************
    King safety - penalize the enemy queen being too close to our king
    ***************************************************************************/
    bitboard q = position->queens & ~friendly_pieces;
    while (q)
    {
        const int queen_locn = FindAndClearLsb(&q);
        score += QUEEN_KING_PROXIMITY[MANHATTAN_DISTANCE(queen_locn, king_locn)];
    }
    /**************************************************************************
    King safety - number of enemy pieces close to our king
    ***************************************************************************/
    score += ENEMY_PIECES_NEAR_KING[PopCount(KING_ATTACKS_2[king_locn] & enemy_pieces)];
    /**************************************************************************
    King safety - number of friendly pieces close to our king
    ***************************************************************************/
    score += FRIENDLY_PIECES_NEAR_KING[PopCount(KING_ATTACKS_2[king_locn] & friendly_pieces & ~position->kings)];  
    /**************************************************************************
    Bonus for retaining the right to castle
    ***************************************************************************/
    if (position->castle_flags & CASTLE_RIGHTS_MASK[color])
    {
        score += SCORE_CASTLING_RIGHTS;
    }
    return score;
}

static int EvaluatePawnStructure(const PawnStructure* ps, int material_percent)
{
    const int midgame_score =
        SCORE_ISOLATED_PAWN  * PopCount(ps->isolated_pawns) +
        SCORE_DOUBLED_PAWN   * PopCount(ps->doubled_pawns)  +
        SCORE_PROTECTED_PAWN * PopCount(ps->defended_pawns) +
        SCORE_PASSED_PAWN    * PopCount(ps->passed_pawns);
    const int endgame_score =
        SCORE_PROTECTED_PAWN_END        * PopCount(ps->defended_pawns)                    +
        SCORE_PASSED_PAWN_END           * PopCount(ps->passed_pawns)                      +
        SCORE_PROTECTED_PASSED_PAWN_END * PopCount(ps->passed_pawns & ps->defended_pawns) +
        SCORE_ISOLATED_PAWN_END         * PopCount(ps->isolated_pawns);
    return (midgame_score * material_percent + endgame_score * (100 - material_percent)) / 100;
}
#endif
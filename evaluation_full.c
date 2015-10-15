#include "pawnstar.h"
#if DO_EVALUATION_FULL
/******************************************************************************
The more experimental "advanced" evaluation function: in practice this does not
seem to be much better than the simplified evaluation function, so it clearly 
needs some refinement.

The evaluation unit is centipawns (1/100 of a pawn)
*******************************************************************************/

typedef struct
{
    uint64          hash;
    int             score;
    volatile long   lock;  
} EvalHashEntry;

static EvalHashEntry eval_hashtable[EVAL_HASHTABLE_SIZE];

static int EvaluateMaterial         (const Position* position, bitboard friendly_pieces);
static int EvaluatePieceSquare      (const Position* position, int color, bitboard friendly_pieces, int material_percent);
static int EvaluateMobility         (const Position* position, bitboard friendly_pieces);
static int EvaluateEnPrise          (const Position* position, bitboard friendly_pieces);
static int EvaluateKingSafety       (const Position* position, int color, bitboard friendly_pieces, int material_percent);
static int EvaluatePawnStructure    (const Position* position, const PawnStructure* ps, bitboard friendly_pieces, int color, int material_percent);

static const bitboard* const FORWARD_OF[2] = { NORTH_OF,                  SOUTH_OF                  };
static const bitboard* const KPS1[2]       = { KING_PAWN_SHIELD_WHITE,    KING_PAWN_SHIELD_BLACK    };
static const bitboard* const KPS2[2]       = { KING_PAWN_SHIELD_WHITE_2,  KING_PAWN_SHIELD_BLACK_2  };
static const uint8 CASTLE_RIGHTS_MASK[2]   = { MAY_WHITE_K | MAY_WHITE_Q, MAY_BLACK_K | MAY_BLACK_Q };
static const uint8 FLIP_BOARD[2]           = { RANK_FLIP,                 0 };
static const bitboard OPPOSITE_HALF[2]     = { RANK_5 | RANK_6 | RANK_7 | RANK_8, 
                                               RANK_1 | RANK_2 | RANK_3 | RANK_4 };
static const bitboard PAWN_RANKS[2][5]     = { { RANK_3, RANK_4, RANK_5, RANK_6, RANK_7 }, { RANK_6, RANK_5, RANK_4, RANK_3, RANK_2 } };

static const int KING_SQUARE_MIDGAME[64] = 
{
      0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,
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

#define SCORE_PAWN                         100
#define SCORE_KNIGHT                       350
#define SCORE_BISHOP                       350
#define SCORE_ROOK                         550
#define SCORE_QUEEN                       1000
#define TOTAL_MATERIAL_SUM                8600  // 2 x Q + 4 x R + 4 x B + 4 x N + 16 x P
#define SCORE_MATERIAL_THRESHOLD           600  // threshold for eval cutoff on material balance only
#define SCORE_NON_MATERIAL_LIMIT           300  // max contribution of positional features to eval score 
#define SCORE_BISHOP_PAIR                   50  // bonus for the bishop pair
#define SCORE_KNIGHT_CENTER                 20  // bonus for a knight standing in the central 16 squares
#define SCORE_KNIGHT_EDGE                  -20  // penalty for a knight standing at the edge of the board
#define SCORE_CASTLING_RIGHTS               60  // bonus for retaining the right to castle
#define SCORE_PAWN_IN_OPPONENTS_HALF        20  // bonus for a pawn in enemy half of the board
#define SCORE_PAWN_KING_ADJ1                30  // bonus for each pawn standing directly in front of the king after castling
#define SCORE_PAWN_KING_ADJ2                10  // bonus for each pawn standing on the 3rd rank in front of the king
#define SCORE_KING_OPEN_FILE               -20  // penalty for king on a file with no friendly pawns
#define SCORE_KING_ADJ_OPEN_FILE           -10  // penalty for king adjacent file with no friendly pawns
#define SCORE_ISOLATED_PAWN                -20  // penalty for an isolated pawn in the midgame
#define SCORE_ISOLATED_PAWN_END            -20  // penalty for isolated pawn in the endgame
#define SCORE_DOUBLED_PAWN                 -10  // penalty for doubled or triped pawn
#define SCORE_PASSED_PAWN                   10  // bonus for passed pawn in the midgame
#define SCORE_PASSED_PAWN_END               20  // bonus for a passed pawn in the endgame
#define SCORE_PROTECTED_PAWN                 5  // bonus for pawn protected by a friendly pawn in the midgame
#define SCORE_PROTECTED_PAWN_END            10  // bonus for pawn protected by a friendly pawn in the endgame
#define SCORE_PROTECTED_PASSED_PAWN_END     10  // additional bonus for a passed pawn protected by a friendly pawn in the endgame
#define SCORE_ROOK_BEHIND_PASSED_PAWN_END   10  // bonus for a rook behind a passed pawn in the end game
#define SCORE_PAWN_ADVANCEMENT               5  // bonus for pawn advancing a rank
/******************************************************************************
Bonus / penalty for bishops and rooks based on number of available pseudo legal 
target squares. The rest should be handled by search.
*******************************************************************************/
static const int BISHOP_ROOK_MOBILITY[15] = 
{ -30, -15, 0, 10, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20 };
/******************************************************************************
Bonus / penalty for knights based on number of available pseudo legal 
target squares. The rest should be handled by search.
*******************************************************************************/
static const int KNIGHT_MOBILITY[9] =
{ -40, -30, -20, -10, 0, 10, 20, 30, 40 };
/******************************************************************************
Penalty for enemy queen proximity (Manhattan distance) to friendly king
*******************************************************************************/
static const int QUEEN_KING_PROXIMITY[15] = 
{ -25, -25, -25, -20, -15, -10, -5, 0, 5, 10, 15, 20, 20, 20, 20 };
/******************************************************************************
Bonus / penalty for number of enemy pieces within 2 squares of the friendly 
king
*******************************************************************************/
static const int ENEMY_PIECES_NEAR_KING[17] = 
{ 20, 15, 10, 5, 0, -5, -10, -15, -20, -20, -20, -20, -20, -20, -20, -20, -20 };
/******************************************************************************
Bonus / penalty for number of friendly non pawn pieces within 2 squares of the 
friendly king
*******************************************************************************/
static const int FRIENDLY_PIECES_NEAR_KING[17] = 
{ -15, -10, -5, 0, 5, 10, 15, 20, 25, 30, 30, 30, 30, 30, 30, 30, 30 };
/******************************************************************************
Penalty for having a piece en prise
*******************************************************************************/
static const int EN_PRISE_PIECE[8] = 
/*    P    N    B    R    Q        */
{ 0,  0, -10, -10, -20, -30, 0, 0 };
/******************************************************************************
Bonus awarded to the side ahead on material based on the total number of
knights, bishops, rooks and queens on the board. Encourages the side ahead to 
trade down pieces but not pawns.
*******************************************************************************/
static const int TRADE_DOWN_BONUS[33] = 
{ 50, 45, 40, 35, 30, 25, 20, 15, 10, 5, 0, /* ... */ };
/******************************************************************************
Bonus awarded to the side ahead on material based on the total number of
pawns on the board. Encourages the side ahead to trade down pieces but not 
pawns.
*******************************************************************************/
static const int TRADE_DOWN_PAWN_BONUS[17] = 
{ 0, 5, 10, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 65, 70, 75, 80 };

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
    INCREMENT("eval calls");
    EvalHashEntry* const hash_entry = &eval_hashtable[position->hash % EVAL_HASHTABLE_SIZE];
    while (_InterlockedCompareExchange(&hash_entry->lock, 1, 0) != 0)
    {
        INCREMENT("lock contention eval retrieve");
    }
    if (hash_entry->hash == position->hash)
    {
        INCREMENT("eval hash hits");
        const int result = hash_entry->score;
        _InterlockedExchange(&hash_entry->lock, 0);
        return result;
    }
    _InterlockedExchange(&hash_entry->lock, 0);
    if (IsDrawByMaterial(position))
    {
        return DRAW_SCORE;
    }
    const int score_sign = position->state_flags & IS_BLACK_TO_MOVE ? -1 : 1;
    /**************************************************************************
    Material
    ***************************************************************************/
    int material_white = EvaluateMaterial(position, position->white_pieces);
    int material_black = EvaluateMaterial(position, position->black_pieces);
    int material_score = (material_white - material_black) * score_sign;
    if (material_score > beta + SCORE_MATERIAL_THRESHOLD)
    {
        INCREMENT("eval cutoff beta");
        return material_score;
    }
    if (material_score + SCORE_MATERIAL_THRESHOLD < alpha)
    {
        INCREMENT("eval cutoff alpha");
        return material_score;
    }
    int material_percent = ((material_white + material_black) * 100) / TOTAL_MATERIAL_SUM;
    if (material_percent >= 90)
    {
        material_percent = 100;
    }
    else if (material_percent <= 10)
    {
        material_percent = 0;
    }
    if (material_white > material_black)
    {
        material_white += 
            TRADE_DOWN_BONUS     [PopCount(position->knights | position->bishops | position->rooks | position->queens)] +
            TRADE_DOWN_PAWN_BONUS[PopCount(position->pawns)];
    }
    else if (material_black > material_white)
    {
        material_black += 
            TRADE_DOWN_BONUS     [PopCount(position->knights | position->bishops | position->rooks | position->queens)] + 
            TRADE_DOWN_PAWN_BONUS[PopCount(position->pawns)];
    }
    if ((position->bishops & position->white_pieces & WHITE_SQUARES) &&
        (position->bishops & position->white_pieces & BLACK_SQUARES))
    {
        material_white += SCORE_BISHOP_PAIR;
    }
    if ((position->bishops & position->black_pieces & WHITE_SQUARES) &&
        (position->bishops & position->black_pieces & BLACK_SQUARES))
    {
        material_black += SCORE_BISHOP_PAIR;
    }
    PawnStructure ps[2];
    DeterminePawnStructure(position, ps);    
    const int piece_square_score = 
        EvaluatePieceSquare(position, WHITE, position->white_pieces, material_percent) - 
        EvaluatePieceSquare(position, BLACK, position->black_pieces, material_percent);
    const int king_safety_score = 
        EvaluateKingSafety(position, WHITE, position->white_pieces, material_percent) - 
        EvaluateKingSafety(position, BLACK, position->black_pieces, material_percent);
    const int mobility_score = 
        EvaluateMobility(position, position->white_pieces) - 
        EvaluateMobility(position, position->black_pieces);
    const int pawn_structure_score = 
        EvaluatePawnStructure(position, &ps[WHITE], position->white_pieces, WHITE, material_percent) - 
        EvaluatePawnStructure(position, &ps[BLACK], position->black_pieces, BLACK, material_percent);
    int non_material_score = 
        (piece_square_score   +
         mobility_score       + 
         king_safety_score    + 
         pawn_structure_score) / 2;
    INCREMENT_IF(abs(non_material_score) > SCORE_PAWN, "eval non material exceeds 100");
    if (abs(non_material_score) > SCORE_NON_MATERIAL_LIMIT)
    {
        INCREMENT("eval non material exceeds 300");
        non_material_score = non_material_score > 0 ? SCORE_NON_MATERIAL_LIMIT : -SCORE_NON_MATERIAL_LIMIT;
    }
    int final_score = (material_white - material_black) + non_material_score;
    final_score /= 5;
    final_score *= 5 * score_sign; /* quantize score - helps search stability */
    while (_InterlockedCompareExchange(&hash_entry->lock, 1, 0) != 0)
    {
        INCREMENT("lock contention eval record");
    }
    hash_entry->hash  = position->hash;
    hash_entry->score = final_score;
    _InterlockedExchange(&hash_entry->lock, 0);
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

static int EvaluatePieceSquare(const Position* position, int color, bitboard friendly_pieces, int material_percent)
{ 
    const int king_index = position->king_location[color] ^ FLIP_BOARD[color];     
    int score = 
        ((KING_SQUARE_MIDGAME[king_index] *        material_percent) + 
         (KING_SQUARE_ENDGAME[king_index] * (100 - material_percent))) / 100;   
    const bitboard friendly_knights = position->knights & friendly_pieces;
    score += SCORE_KNIGHT_CENTER          * PopCount(friendly_knights & CTR_16_SQUARES);
    score += SCORE_KNIGHT_EDGE            * PopCount(friendly_knights & BORDER_SQUARES);
    /* Pawn advancement bonus */
    const bitboard friendly_pawns = position->pawns & friendly_pieces;
    int pawn_advancement_bonus = SCORE_PAWN_ADVANCEMENT;
    const bitboard* pawn_rank = &PAWN_RANKS[color][0];
    for (int i = 5; i != 0; --i)
    {
        score += pawn_advancement_bonus * PopCount(friendly_pawns & *pawn_rank);
        ++pawn_rank;
        pawn_advancement_bonus += SCORE_PAWN_ADVANCEMENT;
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
        score += BISHOP_ROOK_MOBILITY[PopCount(BishopAttacks(position->occupied_squares, FindAndClearLsb(&b)) & ~friendly_pieces)];
    }
    /**************************************************************************
    Rook mobility
    ***************************************************************************/
    b = position->rooks & friendly_pieces;
    while (b)
    {
        score += BISHOP_ROOK_MOBILITY[PopCount(RookAttacks(position->occupied_squares, FindAndClearLsb(&b)) & ~friendly_pieces)];
    }
    /**************************************************************************
    Knight mobility
    ***************************************************************************/
    b = position->knights & friendly_pieces;
    while (b)
    {
        score += KNIGHT_MOBILITY[PopCount(KNIGHT_ATTACKS[FindAndClearLsb(&b)] & ~friendly_pieces)];
    }
    return score;
}

static int EvaluateKingSafety(const Position* position, int color, bitboard friendly_pieces, int material_percent)
{
    if (material_percent == 0)
    {
        return 0;
    }
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
    if (king_file && !(king_file & friendly_pawns))
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
        score += QUEEN_KING_PROXIMITY[MANHATTAN_DISTANCE[king_locn][FindAndClearLsb(&q)]];
    }
    /**************************************************************************
    King safety - number of enemy pieces close to our king
    ***************************************************************************/
    score += ENEMY_PIECES_NEAR_KING[PopCount(KING_ATTACKS_2[king_locn] & enemy_pieces)];
    /**************************************************************************
    King safety - number of friendly pieces close to our king
    ***************************************************************************/
    score += FRIENDLY_PIECES_NEAR_KING[PopCount(KING_ATTACKS_2[king_locn] & friendly_pieces & ~(position->kings | position->pawns))];  
    /**************************************************************************
    Bonus for retaining the right to castle
    ***************************************************************************/
    if (position->castle_flags & CASTLE_RIGHTS_MASK[color])
    {
        score += SCORE_CASTLING_RIGHTS;
    }
    return (score * material_percent) / 100;
}

static int EvaluatePawnStructure(const Position* position, const PawnStructure* ps, bitboard friendly_pieces, int color, int material_percent)
{
    const int midgame_score =
        SCORE_ISOLATED_PAWN  * PopCount(ps->isolated_pawns) +
        SCORE_DOUBLED_PAWN   * PopCount(ps->doubled_pawns)  +
        SCORE_PROTECTED_PAWN * PopCount(ps->defended_pawns) +
        SCORE_PASSED_PAWN    * PopCount(ps->passed_pawns);
    int endgame_score =
        SCORE_DOUBLED_PAWN              * PopCount(ps->doubled_pawns)                     +
        SCORE_PROTECTED_PAWN_END        * PopCount(ps->defended_pawns)                    +
        SCORE_PASSED_PAWN_END           * PopCount(ps->passed_pawns)                      +
        SCORE_PROTECTED_PASSED_PAWN_END * PopCount(ps->passed_pawns & ps->defended_pawns) +
        SCORE_ISOLATED_PAWN_END         * PopCount(ps->isolated_pawns);
    bitboard friendly_rooks = position->rooks & friendly_pieces;
    while (friendly_rooks)
    {
        const int rook_locn = FindAndClearLsb(&friendly_rooks);
        bitboard pp = FORWARD_OF[color][rook_locn] & ps->passed_pawns;
        while (pp)
        {
            if (!(INTERVENING_SQUARES[rook_locn][FindAndClearLsb(&pp)] & position->occupied_squares))
            {
                endgame_score += SCORE_ROOK_BEHIND_PASSED_PAWN_END;
            }
        }
    }
    return (midgame_score * material_percent + endgame_score * (100 - material_percent)) / 100;
}

static int 
EvaluateEnPrise(const Position* position, 
                bitboard friendly_pieces)
{
    int score = 0;
    bitboard b = friendly_pieces & ~(position->pawns | position->kings);
    while (b)
    {
        const int locn = FindAndClearLsb(&b);
        const bitboard attacks_to = AttacksToSquare(position, locn);
        if (attacks_to && !(attacks_to & friendly_pieces))
        {
            score += EN_PRISE_PIECE[PIECE_AT(position, locn)];
        }
    }
    return score;
}
#endif
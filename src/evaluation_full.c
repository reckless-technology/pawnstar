#include "pawnstar.h"
#if DO_EVALUATION_FULL
/*
The more experimental "advanced" evaluation function: in practice this does not
seem to be much better than the simplified evaluation function, so it clearly 
needs some refinement.

The evaluation unit is centipawns (1/100 of a pawn)
*/

typedef struct
{
    uint64          hash;
    int             score;
    volatile long   lock;  
} EvalHashEntry;

typedef struct
{  
    int     white;
    int     black;
    int     percent;
} Material;

static EvalHashEntry eval_hashtable[EVAL_HASHTABLE_SIZE];

static void EvaluateMaterial        (const Position* position, Material* material);
static int  EvaluateMobility        (const Position* position, bitboard friendly_pieces);
static int  EvaluatePieceSquare     (const Position* position, bitboard friendly_pieces, int color, int material_percent);
static int  EvaluateKingSafety      (const Position* position, bitboard friendly_pieces, int color, int material_percent);
static int  EvaluatePawnStructure   (const Position* position, bitboard friendly_pieces, int color, int material_percent, const PawnStructure* ps);

static const bitboard* const FORWARD_OF[2] = { NORTH_OF,                  SOUTH_OF                  };
static const bitboard* const KPS1[2]       = { KING_PAWN_SHIELD_WHITE,    KING_PAWN_SHIELD_BLACK    };
static const bitboard* const KPS2[2]       = { KING_PAWN_SHIELD_WHITE_2,  KING_PAWN_SHIELD_BLACK_2  };
static const uint8 CASTLE_RIGHTS_MASK[2]   = { MAY_WHITE_K | MAY_WHITE_Q, MAY_BLACK_K | MAY_BLACK_Q };
static const uint8 FLIP_BOARD[2]           = { RANK_FLIP,                 0 };
static const bitboard PAWN_RANKS[2][6]     = { { RANK_3, RANK_4, RANK_5, RANK_6, RANK_7, NO_SQUARES }, 
                                               { RANK_6, RANK_5, RANK_4, RANK_3, RANK_2, NO_SQUARES } };

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
#define SCORE_MATERIAL_SUM                8600  // 2 x Q + 4 x R + 4 x B + 4 x N + 16 x P
#define SCORE_MATERIAL_THRESHOLD           400  // threshold for eval cutoff on material balance only
#define SCORE_NON_MATERIAL_LIMIT           300  // max contribution of positional features to eval score 
#define SCORE_BISHOP_PAIR                   50  // bonus for the bishop pair
#define SCORE_KNIGHT_CENTER                 20  // bonus for a knight standing in the central 16 squares
#define SCORE_KNIGHT_EDGE                  -20  // penalty for a knight standing at the edge of the board
#define SCORE_CASTLING_RIGHTS               40  // bonus for retaining the right to castle
#define SCORE_PAWN_KING_ADJ1                35  // bonus for each pawn standing directly in front of the king after castling
#define SCORE_PAWN_KING_ADJ2                15  // bonus for each pawn standing on the 3rd rank in front of the king
#define SCORE_ISOLATED_PAWN                -20  // penalty for an isolated pawn
#define SCORE_DOUBLED_PAWN                 -10  // penalty for doubled or triped pawn
#define SCORE_PASSED_PAWN                   10  // bonus for passed pawn
#define SCORE_PROTECTED_PAWN                 5  // bonus for pawn protected by a friendly pawn
#define SCORE_PROTECTED_PASSED_PAWN         10  // additional bonus for a passed pawn protected by a friendly pawn
#define SCORE_ROOK_BEHIND_PASSED_PAWN       10  // bonus for a rook behind a passed pawn
#define SCORE_PAWN_ADVANCEMENT               5  // bonus for pawn advancing a rank
/*
Bonus / penalty for bishops and rooks based on number of available pseudo legal 
target squares. The rest should be handled by search.
*/
static const int BISHOP_ROOK_MOBILITY[15] = 
{ -30, -15, 0, 10, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20 };
/*
Bonus / penalty for knights based on number of available pseudo legal 
target squares. The rest should be handled by search.
*/
static const int KNIGHT_MOBILITY[9] =
{ -40, -30, -20, -10, 0, 10, 20, 30, 40 };
/*
Penalty for enemy queen proximity (Manhattan distance) to friendly king
*/
static const int QUEEN_KING_PROXIMITY[15] = 
{ -25, -25, -25, -20, -15, -10, -5, 0, 5, 10, 15, 20, 20, 20, 20 };
/*
Bonus / penalty for number of enemy pieces within 3 squares of the friendly 
king
*/
static const int ENEMY_PIECES_NEAR_KING[17] = 
{ 20, 15, 10, 5, 0, -5, -10, -15, -20, -25, -30, -30, -30, -30, -30, -30, -30 };
/*
Bonus / penalty for number of friendly non pawn pieces within 2 squares of the 
friendly king
*/
static const int FRIENDLY_PIECES_NEAR_KING[17] = 
{ -15, -10, -5, 0, 5, 10, 15, 20, 25, 30, 30, 30, 30, 30, 30, 30, 30 };
/*
Bonus / penalty based on the number of open attacks to the friendly king,
determined by how many moves a queen could make to empty squares if she were
standing in the king's location.
*/
static const int ATTACK_RAYS_TO_KING[28] = 
{ 
    -10,   0,   0,   0,  -5,  -5, -10, -10, -15, -15, -20, -20, -25, -25,  
    -25, -25, -25, -25, -25, -25, -25, -25, -25, -25, -25, -25, -25, -25,
};
/*
Bonus awarded to the side ahead on material based on the total number of
knights, bishops, rooks and queens on the board. Encourages the side ahead to 
trade down pieces but not pawns.
*/
static const int TRADE_DOWN_BONUS[33] = 
{ 50, 45, 40, 35, 30, 25, 20, 15, 10, 5, 0, /* ... */ };
/*
Bonus awarded to the side ahead on material based on number of friendly
pawns on the board. Encourages the side ahead to trade down pieces but not 
pawns.
*/
static const int FRIENDLY_PAWN_BONUS[9] = 
{ 0, 10, 20, 30, 40, 40, 40, 40, 30 };

void InitializeEval()
{
    /* nothing to do in this version */
}

/*
Evaluate the current position, assuming neither king is in check and the 
position is quiet. 
*/
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
    const int score_polarity = (position->state_flags & IS_BLACK_TO_MOVE) ? -1 : 1;
    Material material;
    EvaluateMaterial(position, &material);
    const int material_score = (material.white - material.black) * score_polarity;
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
    PawnStructure ps[2];
    DeterminePawnStructure(position, ps);    
    const int piece_square_score = 
        EvaluatePieceSquare(position, position->white_pieces, WHITE, material.percent) - 
        EvaluatePieceSquare(position, position->black_pieces, BLACK, material.percent);
    const int king_safety_score = 
        EvaluateKingSafety(position, position->white_pieces, WHITE, material.percent) - 
        EvaluateKingSafety(position, position->black_pieces, BLACK, material.percent);
    const int mobility_score = 
        EvaluateMobility(position, position->white_pieces) - 
        EvaluateMobility(position, position->black_pieces);
    const int pawn_structure_score = 
        EvaluatePawnStructure(position, position->white_pieces, WHITE, material.percent, &ps[WHITE]) - 
        EvaluatePawnStructure(position, position->black_pieces, BLACK, material.percent, &ps[BLACK]);
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
    int evaluation_score = (material.white - material.black) + non_material_score;
    evaluation_score /= 5;
    evaluation_score *= 5 * score_polarity; /* quantize score - helps search stability */
    while (_InterlockedCompareExchange(&hash_entry->lock, 1, 0) != 0)
    {
        INCREMENT("lock contention eval record");
    }
    hash_entry->hash  = position->hash;
    hash_entry->score = evaluation_score;
    _InterlockedExchange(&hash_entry->lock, 0);
    return evaluation_score;
}

static void
EvaluateMaterial(const Position* position, 
                 Material*       material)
{
    material->white = 
        SCORE_PAWN   * PopCount(position->pawns   & position->white_pieces) +
        SCORE_KNIGHT * PopCount(position->knights & position->white_pieces) +
        SCORE_BISHOP * PopCount(position->bishops & position->white_pieces) +
        SCORE_ROOK   * PopCount(position->rooks   & position->white_pieces) +
        SCORE_QUEEN  * PopCount(position->queens  & position->white_pieces);
    material->black = 
        SCORE_PAWN   * PopCount(position->pawns   & position->black_pieces) +
        SCORE_KNIGHT * PopCount(position->knights & position->black_pieces) +
        SCORE_BISHOP * PopCount(position->bishops & position->black_pieces) +
        SCORE_ROOK   * PopCount(position->rooks   & position->black_pieces) +
        SCORE_QUEEN  * PopCount(position->queens  & position->black_pieces);
    material->percent = ((material->white + material->black) * 100) / SCORE_MATERIAL_SUM;
    if (material->percent >= 90)
    {
        material->percent = 100;
    }
    else if (material->percent <= 10)
    {
        material->percent = 0;
    }
    if (material->white > material->black)
    {
        material->white += 
            TRADE_DOWN_BONUS[PopCount(position->knights | position->bishops | position->rooks | position->queens)] +
            FRIENDLY_PAWN_BONUS[PopCount(position->pawns & position->white_pieces)];
    }
    else if (material->black > material->white)
    {
        material->black += 
            TRADE_DOWN_BONUS[PopCount(position->knights | position->bishops | position->rooks | position->queens)] + 
            FRIENDLY_PAWN_BONUS[PopCount(position->pawns & position->black_pieces)];
    }
    if ((position->bishops & position->white_pieces & WHITE_SQUARES) &&
        (position->bishops & position->white_pieces & BLACK_SQUARES))
    {
        material->white += SCORE_BISHOP_PAIR;
    }
    if ((position->bishops & position->black_pieces & WHITE_SQUARES) &&
        (position->bishops & position->black_pieces & BLACK_SQUARES))
    {
        material->black += SCORE_BISHOP_PAIR;
    }
}

static int 
EvaluatePieceSquare(const Position* position, 
                    bitboard        friendly_pieces, 
                    int             color, 
                    int             material_percent)
{ 
    /* King placement */
    const int king_index = position->king_location[color] ^ FLIP_BOARD[color];     
    int score = 
        ((KING_SQUARE_MIDGAME[king_index] *        material_percent) + 
         (KING_SQUARE_ENDGAME[king_index] * (100 - material_percent))) / 100;  
    /* Knight placement */
    const bitboard friendly_knights = position->knights & friendly_pieces;
    score += SCORE_KNIGHT_CENTER * PopCount(friendly_knights & CTR_16_SQUARES);
    score += SCORE_KNIGHT_EDGE   * PopCount(friendly_knights & BORDER_SQUARES);
    /* Pawn advancement */
    const bitboard friendly_pawns = position->pawns & friendly_pieces;
    int pawn_advancement_bonus    = SCORE_PAWN_ADVANCEMENT;
    const bitboard* pawn_rank     = &PAWN_RANKS[color][0];
    while (*pawn_rank)
    {
        score += pawn_advancement_bonus * PopCount(friendly_pawns & *pawn_rank);       
        pawn_advancement_bonus += SCORE_PAWN_ADVANCEMENT;
        ++pawn_rank;
    }
    return score;
}

static int 
EvaluateMobility(const Position* position, 
                 bitboard        friendly_pieces)
{
    int score = 0;
    /*
    Bishop mobility
    */
    bitboard b = position->bishops & friendly_pieces;
    while (b)
    {
        score += BISHOP_ROOK_MOBILITY[PopCount(BishopAttacks(position->occupied_squares, FindAndClearLsb(&b)) & ~friendly_pieces)];
    }
    /*
    Rook mobility
    */
    b = position->rooks & friendly_pieces;
    while (b)
    {
        score += BISHOP_ROOK_MOBILITY[PopCount(RookAttacks(position->occupied_squares, FindAndClearLsb(&b)) & ~friendly_pieces)];
    }
    /*
    Knight mobility
    */
    b = position->knights & friendly_pieces;
    while (b)
    {
        score += KNIGHT_MOBILITY[PopCount(KNIGHT_ATTACKS[FindAndClearLsb(&b)] & ~friendly_pieces)];
    }
    return score;
}

static int 
EvaluateKingSafety(const Position* position, 
                   bitboard        friendly_pieces, 
                   int             color, 
                   int             material_percent)
{
    if (material_percent == 0)
    {
        return 0;
    }
    int score = 0;    
    /*
    King safety - pawn shield - treat the bishop as a pseudo pawn for the 
    purpose of the king pawn shield, to allow for fianchetto bishop to protect
    the king.
    */
    const int king_locn                   = position->king_location[color];
    const bitboard friendly_pawns_bishops = friendly_pieces & (position->pawns | position->bishops);
    const bitboard enemy_pieces           = position->occupied_squares ^ friendly_pieces;
    score += SCORE_PAWN_KING_ADJ1 * PopCount(KPS1[color][king_locn] & friendly_pawns_bishops);
    score += SCORE_PAWN_KING_ADJ2 * PopCount(KPS2[color][king_locn] & friendly_pawns_bishops);     
    /*
    King safety - penalize the enemy queen being too close to our king
    */
    bitboard q = position->queens & ~friendly_pieces;
    while (q)
    {
        score += QUEEN_KING_PROXIMITY[MANHATTAN_DISTANCE[king_locn][FindAndClearLsb(&q)]];
    }
    /*
    King safety - how many lines of attack are open to the king
    */
    score += ATTACK_RAYS_TO_KING[PopCount(QueenAttacks(position->occupied_squares, king_locn) & ~position->occupied_squares)];
    /*
    King safety - number of enemy pieces close to our king
    */
    score += ENEMY_PIECES_NEAR_KING[PopCount(KING_ATTACKS_3[king_locn] & enemy_pieces)];
    /*
    King safety - number of friendly pieces close to our king
    */
    score += FRIENDLY_PIECES_NEAR_KING[PopCount(KING_ATTACKS_2[king_locn] & friendly_pieces & ~(position->kings | position->pawns))];  
    /*
    Bonus for retaining the right to castle (don't move the king prematurely)
    */
    if (position->castle_flags & CASTLE_RIGHTS_MASK[color])
    {
        score += SCORE_CASTLING_RIGHTS;
    }
    return (score * material_percent) / 100;
}

static int 
EvaluatePawnStructure(const Position*      position, 
                      bitboard             friendly_pieces, 
                      int                  color, 
                      int                  material_percent, 
                      const PawnStructure* ps)
{
    int score =
        SCORE_ISOLATED_PAWN         * PopCount(ps->isolated_pawns)                    +
        SCORE_DOUBLED_PAWN          * PopCount(ps->doubled_pawns)                     +
        SCORE_PROTECTED_PAWN        * PopCount(ps->defended_pawns)                    +
        SCORE_PROTECTED_PASSED_PAWN * PopCount(ps->defended_pawns & ps->passed_pawns) +
        SCORE_PASSED_PAWN           * PopCount(ps->passed_pawns);
    bitboard friendly_rooks = position->rooks & friendly_pieces;
    while (friendly_rooks)
    {
        const int rook_locn = FindAndClearLsb(&friendly_rooks);
        bitboard pp = FORWARD_OF[color][rook_locn] & ps->passed_pawns;
        while (pp)
        {
            if (!(INTERVENING_SQUARES[rook_locn][FindAndClearLsb(&pp)] & position->occupied_squares))
            {
                score += SCORE_ROOK_BEHIND_PASSED_PAWN;
            }
        }
    }
    return score;
    (void)material_percent;
}
#endif
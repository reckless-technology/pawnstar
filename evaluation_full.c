#include "pawnstar.h"
#if DO_EVALUATION_FULL

#define MANHATTAN_DISTANCE(x, y) (abs(FILE_OF(x) - FILE_OF(y)) + abs(RANK_OF(x) - RANK_OF(y)))

typedef struct
{
    bitboard white_pawns;  
    bitboard black_pawns;  
    bitboard white_knights;
    bitboard black_knights;
    bitboard white_bishops;
    bitboard black_bishops;
    bitboard white_rooks;  
    bitboard black_rooks;  
    bitboard white_queens; 
    bitboard black_queens; 
    PawnStructure ps[2];
} EvalCtx;


static int EvaluateMidgame    (const Position* position, const EvalCtx* ctx);
static int EvaluateEndgame    (const Position* position, const EvalCtx* ctx);
static int EvaluatePieceSquare(const Position* position, int material_percent);

static const int PAWN_SQUARE_MIDGAME[64] = 
{
     0,  0,  0,  0,  0,  0,  0,  0,
    35, 35, 35, 35, 35, 35, 35, 35, 
    10, 20, 20, 30, 30, 20, 20, 10,
     5, 15, 15, 25, 25, 15, 15,  5,
     0,  0,  0, 20, 20,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,-20,-20,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,
};
static const int PAWN_SQUARE_ENDGAME[64] = 
{
     0,  0,  0,  0,  0,  0,  0,  0,
    60, 60, 60, 60, 60, 60, 60, 60,  
    50, 50, 50, 50, 50, 50, 50, 50, 
    40, 40, 40, 40, 40, 40, 40, 40, 
    30, 30, 30, 30, 30, 30, 30, 30, 
    20, 20, 20, 20, 20, 20, 20, 20, 
    10, 10, 10, 10, 10, 10, 10, 10, 
     0,  0,  0,  0,  0,  0,  0,  0,
};
static const int KNIGHT_SQUARE[64] = 
{
    -40,-30,-20,-10,-10,-20,-30,-40,
    -30,-20,-10,  0,  0,-10,-20,-30,
    -20,-10,  0, 10, 10,  0,-10,-20,
    -10,  0, 10, 20, 20, 10,  0,-10,
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

/******************************************************************************
Small random variations to eval add variety to play style
*******************************************************************************/
static const int RANDOM_FACTOR[16] = { -10, -5, -5, -5, 0, 0, 0, 0, 0, 0, 0, 0, 5, 5, 5, 10 };
/******************************************************************************
Evaluation weights are all in centipawns
*******************************************************************************/
#define SCORE_PAWN                         100
#define SCORE_KNIGHT                       400
#define SCORE_BISHOP                       400
#define SCORE_ROOK                         600
#define SCORE_QUEEN                       1200
#define TOTAL_MATERIAL_SUM                9600  // 2 x Q + 4 x R + 4 x B + 4 x N + 16 x P
#define SCORE_BISHOP_PAIR                   50  // bonus for the bishop pair
#define SCORE_KNIGHT_PAIR                  -20  // penalty for the knight pair
#define SCORE_PAWN_KING_ADJ1                25  // bonus for each pawn standing directly in front of the king after castling
#define SCORE_PAWN_KING_ADJ2                15  // bonus for each pawn standing on the 3rd rank in front of the king
#define SCORE_KING_OPEN_FILE               -30  // penalty for king on a file with no friendly pawns
#define SCORE_KING_ADJ_OPEN_FILE           -20  // penalty for king adjacent file with no friendly pawns
#define SCORE_ISOLATED_PAWN                -20  // penalty for an isolated pawn
#define SCORE_ISOLATED_PAWN_END            -10  // penalty for isolated pawn in the endgame
#define SCORE_DOUBLED_PAWN                 -10  // penalty for doubled or triped pawn
#define SCORE_CASTLING_RIGHTS               50  // bonus for retaining the right to castle
#define SCORE_PROTECTED_PAWN                 5  // bonus for pawn protected by a friendly pawn
#define SCORE_PROTECTED_PAWN_END            10  // bonus for pawn protected by a friendly pawn in the endgame
#define SCORE_PASSED_PAWN_END               20  // bonus for a passed pawn in the endgame
#define SCORE_PROTECTED_PASSED_PAWN_END     25  // additional bonus for a passed pawn protected by a friendly pawn in the endgame
#define SCORE_MATERIAL_THRESHOLD           400  // threshold for eval cutoff on material balance only
/******************************************************************************
Added to the materially ahead side, indexed by the total number of knights, 
bishops, rooks and queens on the board. Encourages the side which is ahead on
material to exchange pieces but not pawns, i.e. trade down to a won endgame.
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
static const int ENEMY_PIECES_NEAR_KING[17] = { 20, 10, 0, -10, -20, -30, -40, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50 };

void InitializeEval()
{
    /* nothing to do in this version */
}

typedef struct
{
    uint64  hash;
    int     score;
} EvalHashEntry;

static EvalHashEntry eval_hashtable[EVAL_HASHTABLE_SIZE];
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
    int material[2];
    if (IsDrawByMaterial(position))
    {
        return DRAW_SCORE;
    }
    const int score_sign = position->state_flags & IS_BLACK_TO_MOVE ? -1 : 1;
    /**************************************************************************
    Material
    ***************************************************************************/
    EvalCtx ctx;
    ctx.white_pawns   = position->pawns   & position->white_pieces;
    ctx.black_pawns   = position->pawns   & position->black_pieces;
    ctx.white_knights = position->knights & position->white_pieces;
    ctx.black_knights = position->knights & position->black_pieces;
    ctx.white_bishops = position->bishops & position->white_pieces;
    ctx.black_bishops = position->bishops & position->black_pieces;
    ctx.white_rooks   = position->rooks   & position->white_pieces;
    ctx.black_rooks   = position->rooks   & position->black_pieces;
    ctx.white_queens  = position->queens  & position->white_pieces;
    ctx.black_queens  = position->queens  & position->black_pieces;
    
    material[WHITE] = 
        SCORE_PAWN   * PopCount(ctx.white_pawns)   +
        SCORE_KNIGHT * PopCount(ctx.white_knights) +
        SCORE_BISHOP * PopCount(ctx.white_bishops) +
        SCORE_ROOK   * PopCount(ctx.white_rooks)   +
        SCORE_QUEEN  * PopCount(ctx.white_queens);
    material[BLACK] =
        SCORE_PAWN   * PopCount(ctx.black_pawns)   +
        SCORE_KNIGHT * PopCount(ctx.black_knights) +
        SCORE_BISHOP * PopCount(ctx.black_bishops) +
        SCORE_ROOK   * PopCount(ctx.black_rooks)   +
        SCORE_QUEEN  * PopCount(ctx.black_queens);
    
    int material_percent = ((material[WHITE] + material[BLACK]) * 100) / TOTAL_MATERIAL_SUM;
    if (material_percent >= 90)
    {
        material_percent = 100;
    }
    else if (material_percent <= 10)
    {
        material_percent = 0;
    }
    if (material[WHITE] > material[BLACK])
    {
        material[WHITE] += TRADE_DOWN_BONUS[PopCount(position->knights | position->bishops | position->rooks | position->queens)];
    }
    else if (material[BLACK] > material[WHITE])
    {
        material[BLACK] += TRADE_DOWN_BONUS[PopCount(position->knights | position->bishops | position->rooks | position->queens)];
    }   
    if (PopCount(ctx.white_bishops) == 2)
    {
        material[WHITE] += SCORE_BISHOP_PAIR;
    }
    if (PopCount(ctx.black_bishops) == 2)
    {
        material[BLACK] += SCORE_BISHOP_PAIR;
    }
    if (PopCount(ctx.white_knights) == 2)
    {
        material[WHITE] += SCORE_KNIGHT_PAIR;
    }
    if (PopCount(ctx.black_knights) == 2)
    {
        material[BLACK] += SCORE_KNIGHT_PAIR;
    }  
    /**************************************************************************
    If we are hugely ahead or behind on material (sufficient for cutoff with a
    comfortable margin for error) then don't bother doing the full evaluation
    ***************************************************************************/
    const int material_score = (material[WHITE] - material[BLACK]) * score_sign;
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
    DeterminePawnStructure(position, ctx.ps);
    const int piece_square_score = EvaluatePieceSquare(position, material_percent);   
    const int midgame_score = material_percent == 0   ? 0 : EvaluateMidgame(position, &ctx);
    const int endgame_score = material_percent == 100 ? 0 : EvaluateEndgame(position, &ctx);
    const int phase_score = (midgame_score * material_percent + endgame_score * (100 - material_percent)) / 100;
    int final_score = material_score + (piece_square_score + phase_score) * score_sign + RANDOM_FACTOR[NextRandom() & 0x0F];
    final_score /= 10;
    final_score *= 10; /* quantize score to 1/10 of a pawn value - helps search stability */
    hash_entry->hash  = position->hash;
    hash_entry->score = final_score;
    return final_score;
}

static const int* const PIECE_SQUARE_TABLE[8] = 
{
    NULL,
    NULL,
    KNIGHT_SQUARE,
    BISHOP_SQUARE,
    ROOK_SQUARE,
    QUEEN_SQUARE,
    NULL,
    NULL,
};

static int EvaluatePieceSquare(const Position* position, int material_percent)
{
    int pawn_score = 0;
    bitboard b = position->pawns & position->white_pieces;
    while (b)
    {
        const int table_index = FindAndClearLsb(&b) ^ RANK_FLIP;
        pawn_score += 
            PAWN_SQUARE_MIDGAME[table_index] * material_percent + 
            PAWN_SQUARE_ENDGAME[table_index] * (100 - material_percent);
    }
    b = position->pawns & position->black_pieces;
    while (b)
    {
        const int table_index = FindAndClearLsb(&b);
        pawn_score -= 
            PAWN_SQUARE_MIDGAME[table_index] * material_percent + 
            PAWN_SQUARE_ENDGAME[table_index] * (100 - material_percent);
    }
    int king_score = 
        KING_SQUARE_MIDGAME[position->king_location[WHITE] ^ RANK_FLIP] * material_percent         + 
        KING_SQUARE_ENDGAME[position->king_location[WHITE] ^ RANK_FLIP] * (100 - material_percent) -
        KING_SQUARE_MIDGAME[position->king_location[BLACK]] * material_percent                     -
        KING_SQUARE_ENDGAME[position->king_location[BLACK]] * (100 - material_percent);
    int score = (pawn_score + king_score) / 100;
    for (int piece = KNIGHT; piece <= QUEEN; ++piece)
    {
        const int* const table = PIECE_SQUARE_TABLE[piece];
        b = position->pieces[piece] & position->white_pieces;       
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
    return score;
}

static int EvaluateMidgame(const Position* position, const EvalCtx* ctx)
{
    int score = 0;
    /**************************************************************************
    Castling rights
    ***************************************************************************/
    if (position->castle_flags & (MAY_WHITE_K | MAY_WHITE_Q))
    {
        score += SCORE_CASTLING_RIGHTS;
    }
    if (position->castle_flags & (MAY_BLACK_K | MAY_BLACK_Q))
    {
        score -= SCORE_CASTLING_RIGHTS;
    }
    /**************************************************************************
    Pawn structure
    ***************************************************************************/
    score += SCORE_ISOLATED_PAWN  * PopCount(ctx->ps[WHITE].isolated_pawns);
    score -= SCORE_ISOLATED_PAWN  * PopCount(ctx->ps[BLACK].isolated_pawns);
    score += SCORE_DOUBLED_PAWN   * PopCount(ctx->ps[WHITE].doubled_pawns);
    score -= SCORE_DOUBLED_PAWN   * PopCount(ctx->ps[BLACK].doubled_pawns);
    score += SCORE_PROTECTED_PAWN * PopCount(ctx->ps[WHITE].defended_pawns);
    score -= SCORE_PROTECTED_PAWN * PopCount(ctx->ps[BLACK].defended_pawns);
    /**************************************************************************
    King on a file with no friendly pawns in front of him
    ***************************************************************************/
    bitboard king_file      = NORTH_OF[position->king_location[WHITE]];
    bitboard king_file_west = SHIFT_WEST(king_file);
    bitboard king_file_east = SHIFT_EAST(king_file);
    if (!(king_file & ctx->white_pawns))
    {
        score += SCORE_KING_OPEN_FILE;
    }
    if (king_file_west && !(king_file_west & ctx->white_pawns))
    {
        score += SCORE_KING_ADJ_OPEN_FILE;
    }
    if (king_file_east && !(king_file_east & ctx->white_pawns))
    {
        score += SCORE_KING_ADJ_OPEN_FILE;
    }
    king_file      = SOUTH_OF[position->king_location[BLACK]];
    king_file_west = SHIFT_WEST(king_file);
    king_file_east = SHIFT_EAST(king_file);
    if (!(king_file & ctx->black_pawns))
    {
        score -= SCORE_KING_OPEN_FILE;
    }
    if (king_file_west && !(king_file_west & ctx->black_pawns))
    {
        score -= SCORE_KING_ADJ_OPEN_FILE;
    }
    if (king_file_east && !(king_file_east & ctx->black_pawns))
    {
        score -= SCORE_KING_ADJ_OPEN_FILE;
    }
    /**************************************************************************
    King safety - pawn shield - treat the bishop as a pseudo pawn for the 
    purpose of the king pawn shield, to allow for fianchetto bishop to protect
    the king.
    ***************************************************************************/
    score += SCORE_PAWN_KING_ADJ1 * PopCount(KING_PAWN_SHIELD_WHITE  [position->king_location[WHITE]] & (ctx->white_pawns | ctx->white_bishops));
    score -= SCORE_PAWN_KING_ADJ1 * PopCount(KING_PAWN_SHIELD_BLACK  [position->king_location[BLACK]] & (ctx->black_pawns | ctx->black_bishops));
    score += SCORE_PAWN_KING_ADJ2 * PopCount(KING_PAWN_SHIELD_WHITE_2[position->king_location[WHITE]] & (ctx->white_pawns | ctx->white_bishops));        
    score -= SCORE_PAWN_KING_ADJ2 * PopCount(KING_PAWN_SHIELD_BLACK_2[position->king_location[BLACK]] & (ctx->black_pawns | ctx->black_bishops)); 
    /**************************************************************************
    King safety - penalize the enemy queen being too close to our king
    ***************************************************************************/
    bitboard q = ctx->black_queens;
    while (q)
    {
        const int queen_locn = FindAndClearLsb(&q);
        score += QUEEN_KING_PROXIMITY[MANHATTAN_DISTANCE(queen_locn, position->king_location[WHITE])];
    }
    q = ctx->white_queens;
    while (q)
    {
        const int queen_locn = FindAndClearLsb(&q);
        score -= QUEEN_KING_PROXIMITY[MANHATTAN_DISTANCE(queen_locn, position->king_location[BLACK])];
    }
    /**************************************************************************
    King safety - number of enemy pieces close to our king
    ***************************************************************************/
    score += ENEMY_PIECES_NEAR_KING[PopCount(KING_ATTACKS_2[position->king_location[WHITE]] & position->black_pieces)];
    score -= ENEMY_PIECES_NEAR_KING[PopCount(KING_ATTACKS_2[position->king_location[BLACK]] & position->white_pieces)];
    /**************************************************************************
    Bishop mobility
    ***************************************************************************/
    bitboard b = ctx->white_bishops;
    while (b)
    {
        const int locn = FindAndClearLsb(&b);
        score += MOBILITY[PopCount(BishopAttacks(position->occupied_squares, locn) & ~position->white_pieces)];
    }
    b = ctx->black_bishops;
    while (b)
    {
        const int locn = FindAndClearLsb(&b);
        score -= MOBILITY[PopCount(BishopAttacks(position->occupied_squares, locn) & ~position->black_pieces)];
    }
    /**************************************************************************
    Rook mobility
    ***************************************************************************/
    b = ctx->white_rooks;
    while (b)
    {
        const int locn = FindAndClearLsb(&b);
        score += MOBILITY[PopCount(RookAttacks(position->occupied_squares, locn) & ~position->white_pieces)];
    }
    b = ctx->black_rooks;
    while (b)
    {
        const int locn = FindAndClearLsb(&b);
        score -= MOBILITY[PopCount(RookAttacks(position->occupied_squares, locn) & ~position->black_pieces)];
    }
    return score;
}

static int EvaluateEndgame(const Position* position, const EvalCtx* ctx)
{
    int score = 0;
    /**************************************************************************
    Pawn structure
    ***************************************************************************/
    score += SCORE_PROTECTED_PAWN_END        * PopCount(ctx->ps[WHITE].defended_pawns);
    score -= SCORE_PROTECTED_PAWN_END        * PopCount(ctx->ps[BLACK].defended_pawns);
    score += SCORE_PASSED_PAWN_END           * PopCount(ctx->ps[WHITE].passed_pawns);
    score -= SCORE_PASSED_PAWN_END           * PopCount(ctx->ps[BLACK].passed_pawns);
    score += SCORE_PROTECTED_PASSED_PAWN_END * PopCount(ctx->ps[WHITE].passed_pawns & ctx->ps[WHITE].defended_pawns);
    score -= SCORE_PROTECTED_PASSED_PAWN_END * PopCount(ctx->ps[BLACK].passed_pawns & ctx->ps[BLACK].defended_pawns);
    score += SCORE_ISOLATED_PAWN_END         * PopCount(ctx->ps[WHITE].isolated_pawns);
    score -= SCORE_ISOLATED_PAWN_END         * PopCount(ctx->ps[BLACK].isolated_pawns);
    return score;
    (void)position;
}
#endif
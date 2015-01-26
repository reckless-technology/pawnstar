#include "pawnstar.h"
#if DO_EVALUATION_FULL
/******************************************************************************
Individual evaluation feature weights
*******************************************************************************/
#define SCORE_CENTER_SQUARE_CONTROL       10    // bonus for controlling a center square
#define SCORE_NO_PAWNS                   -25    // penalty for having no pawns left
#define SCORE_PAWN_FORK                   15    // bonus for a pawn fork    
#define SCORE_PASSED_PAWN                 10    // bonus for a passed pawn
#define SCORE_ISOLATED_PAWN              -15    // penalty for an isolated pawn
#define SCORE_BLOCKED_PAWN                -5    // penalty for a blocked pawn (enemy pawn ahead but not on adjacent files)
#define SCORE_DOUBLED_PAWN               -10    // penalty for a doubled pawn
#define SCORE_DEFENDED_PAWN                5    // bonus for a pawn defended by a friendly pawn
#define SCORE_KNIGHT_FORK                 15    // bonus for a knight fork
#define SCORE_KNIGHT_PAWN_HOLE            10    // bonus for a knight standing on an enemy pawn hole
#define SCORE_KNIGHT_PAIR                -20    // penalty for a pair of knight (weakest pair of minors)
#define SCORE_BISHOP_PAIR                 50    // bonus for having the bishop pair
#define SCORE_BISHOP_FORK                 15    // bonus for a bishop fork
#define SCORE_BISHOP_ENEMY_PIECES          5    // when we have a single bishop, bonus per enemy piece standing on the same color square
#define SCORE_KING_OPEN_FILE             -25    // penalty for no friendly pawns on the king file
#define SCORE_KING_ADJ_OPEN_FILE         -20    // penalty for no friendly pawns on files adjacent to the king file
#define SCORE_KING_PAWN_SHIELD_1          20    // bonus for each pawn shield directly in front of the king (only when the king is in safe squares)
#define SCORE_KING_PAWN_SHIELD_2          15    // bonus for each pawn shield one square further forward
#define SCORE_FORFEIT_CASTLING_RIGHT     -30    // penalty for forfeiting the right to castle without castling
#define SCORE_CASTLED_BONUS               10    // bonus for having castled
#define SCORE_NON_MATERIAL_LIMIT          95    // maximum non material score evaluation
/******************************************************************************
Endgame specific feature weights
*******************************************************************************/
#define SCORE_EG_PAWN_PASSED_DEFENDED     50    // bonus for a pawn which is passed, defended and not blocked
#define SCORE_EG_PAWN_PASSED              20    // bonus for a passed pawn
#define SCORE_EG_PAWN_DEFENDED            20    // bonus for a defended pawn
#define SCORE_EG_PAWN_BLOCKED            -10    // penalty for a blocked pawn
#define SCORE_EG_PAWN_FREE_RUN            20    // bonus for a pawn where we control all the squares on its path to promotion
#define SCORE_EG_PAWN_PATH_OPEN           20    // bonus for a pawn when all squares on its path to promotion are unoccupied
#define SCORE_EG_KING_NEAR_PASSED_PAWN    10    // bonus for king proximity to passed pawns (within 2 moves of a passed pawn)
/******************************************************************************
Basic material values in centipawns (indexed by piece type)
*******************************************************************************/
static const int SCORE_MATERIAL_VALUES[7] = {
      0,    // not used
    100,    // pawn
    325,    // knight
    325,    // bishop
    500,    // rook
   1000,    // queen
      0,    // king (N/A)
};
/******************************************************************************
Penalties for undefended material (indexed by piece type)
*******************************************************************************/
static const int SCORE_UNDEFENDED_MATERIAL[7] = {
      0,    // not used
    -10,    // pawn
    -15,    // knight
    -15,    // bishop
    -20,    // rook
    -25,    // queen
      0,    // king
};
/******************************************************************************
Penalties for en prise material (indexed by piece type)
*******************************************************************************/
static const int SCORE_EN_PRISE_MATERIAL[7] = {
      0,    // not used
    -15,    // pawn
    -20,    // knight
    -20,    // bishop
    -25,    // rook
    -30,    // queen
      0,    // king
};
/******************************************************************************
Penalties for pinned material (indexed by piece type)
*******************************************************************************/
static const int SCORE_PINNED_MATERIAL[7] = {
      0,    // not used
     -5,    // pawn
    -10,    // knight
    -10,    // bishop
    -10,    // rook
      0,    // queen
      0,    // king
};
/******************************************************************************
Penalties for attacks adjacent to the king by enemy pieces for each square 
attacked (indexed by attacking piece type)
*******************************************************************************/
static const int SCORE_KING_ADJ_ATTACKED[7] = {
      0,    // not used
    -15,    // pawn
    -15,    // knight
    -15,    // bishop
    -20,    // rook
    -20,    // queen
      0,    // king
};
/******************************************************************************
Piece square table for pawns during the midgame
*******************************************************************************/
static const int SCORE_PAWN_SQUARE[64] = {
      0,  0,  0,  0,  0,  0,  0,  0,
     15, 15, 15, 15, 15, 15, 15, 15,
     10, 10, 10, 10, 10, 10, 10, 10,
      5,  5,  5,  5,  5,  5,  5,  5,
      0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0, -5, -5,  0,  0,  0,
      0,  0,  0,-10,-10,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,
};
/******************************************************************************
Piece square table for pawns during the endgame
*******************************************************************************/
static const int SCORE_PAWN_SQUARE_ENDGAME[64] = {
      0,  0,  0,  0,  0,  0,  0,  0,
     50, 50, 50, 50, 50, 50, 50, 50, 
     40, 40, 40, 40, 40, 40, 40, 40, 
     30, 30, 30, 30, 30, 30, 30, 30, 
     20, 20, 20, 20, 20, 20, 20, 20, 
     10, 10, 10, 10, 10, 10, 10, 10,
      0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,
};
/******************************************************************************
Piece square table for knights
*******************************************************************************/
static const int SCORE_KNIGHT_SQUARE[64] = {
    -15,-10,-10,-10,-10,-10,-10,-15,
    -10,-10,  0,  0,  0,  0,-10,-10,
    -10,  0, 10, 10, 10, 10,  0,-10,
    -10,  5, 10, 10, 10, 10,  5,-10,
    -10,  0, 10, 10, 10, 10,  0,-10,
    -10,  5, 10, 10, 10, 10,  5,-10,
    -10,-10,  0,  5,  5,  0,-10,-10,
    -15,-10,-10,-10,-10,-10,-10,-15,
};
/******************************************************************************
Piece square table for king during the midgame
*******************************************************************************/
static const int SCORE_KING_SQUARE[64] = {
    -20,-20,-20,-20,-20,-20,-20,-20,
    -20,-20,-20,-20,-20,-20,-20,-20,
    -20,-20,-20,-20,-20,-20,-20,-20,
    -20,-20,-20,-20,-20,-20,-20,-20,
    -20,-20,-20,-20,-20,-20,-20,-20,
    -10,-10,-10,-10,-10,-10,-10,-10,
      0,  0,  0,  0,  0,  0,  0,  0,
     10, 15,  5,  0,  0,  5, 15, 10,
};
/******************************************************************************
Piece square table for king during the endgame
*******************************************************************************/
static const int SCORE_KING_SQUARE_ENDGAME[64] = {
    -30,-20,-10,  0,  0,-10,-20,-30,
    -20,-10,  0, 10, 10,  0,-10,-20,
    -10,  0, 10, 20, 20, 10,  0,-10,
      0, 10, 20, 30, 30, 20, 10,  0,
      0, 10, 20, 30, 30, 20, 10,  0,
    -10,  0, 10, 20, 20, 10,  0,-10,
    -20,-10,  0, 10, 10,  0,-10,-20,
    -30,-20,-10,  0,  0,-10,-20,-30,
};
/******************************************************************************
Penalties for poor bishop mobility (indexed by number of available pseudo-legal 
moves)
*******************************************************************************/
static const int SCORE_BISHOP_MOBILITY[14] = { -20,-15,-10, -5, /* ... */ };
/******************************************************************************
Penalties for poor rook mobility (indexed by number of available pseudo-legal 
moves)
*******************************************************************************/
static const int SCORE_ROOK_MOBILITY[15]   = { -15,-10, -5,  0, /* ... */ };
/******************************************************************************
Penalties for poor queen mobility (indexed by number of available pseudo-legal 
moves)
*******************************************************************************/
static const int SCORE_QUEEN_MOBILITY[28]  = { -15,-10, -5,  0, /* ... */ };
/******************************************************************************
Penalties for poor endgame king mobility (indexed by number of available legal 
moves)
*******************************************************************************/
static const int SCORE_KING_MOBILITY_ENDGAME[9] = { -30,-20,-10,  0, 10, 20, 20, 20, 20 };
/******************************************************************************
Penalties (and bonus) for open rays to the king (indexed by the number of
squares a queen standing where the king is could attack)
*******************************************************************************/
static const int SCORE_KING_RAY_OPEN[28]   = {  
    20, 20, 20, 10,  0,-10,-20,-30,-30,-30,-30,-30,-30,-30,
   -30,-30,-30,-30,-30,-30,-30,-30,-30,-30,-30,-30,-30,-30,
};

extern const bitboard* const PAWN_ATTACKS[2];
static const bitboard* const KING_PAWN_SHIELD[2]   = { KING_PAWN_SHIELD_WHITE,   KING_PAWN_SHIELD_BLACK };
static const bitboard* const KING_PAWN_SHIELD_2[2] = { KING_PAWN_SHIELD_WHITE_2, KING_PAWN_SHIELD_BLACK_2 };
static const bitboard* const FORWARD[2]            = { NORTH_OF,                 SOUTH_OF };

/******************************************************************************
Estimate non-material endgame evaluation with respect to white.
*******************************************************************************/
static int EvaluateEndgame(const Position* position, const PawnStructure ps[2])
{    
    int non_material_scores[2] = { 0 };
    int color;
    for (color = WHITE; color <= BLACK; ++color)
    {         
        bitboard b;
        int score                        = 0; 
        bitboard king_targets            = NO_SQUARES;
        const int king_location          = position->king_location[color];
        const bitboard friendly_pieces   = position->occupied_squares & position->pieces_of_color[color];
        const bitboard enemy_pieces      = position->occupied_squares ^ friendly_pieces;
        const bitboard friendly_pawns    = position->pawns & friendly_pieces;
        const int      rank_flip         = color == WHITE ? RANK_FLIP : 0;
        const PawnStructure* const pstr = &ps[color];
        INCREMENT("eval calls endgame");
        /**********************************************************************
        Pawns
        # bonus for forks
        # bonus for advancement
        # bonus for open path to promotion
        # bonus if we control squares on path to promotion
        # bonus for defended
        # bonus if passed
        # penalty if blocked
        ***********************************************************************/       
        b = friendly_pawns;
        while (b)
        {
            const int locn           = FindAndClearLsb(&b);
            const bitboard square    = BITBOARD(locn);
            bitboard forward_squares = FORWARD[color][locn];
            score += SCORE_PAWN_SQUARE_ENDGAME[locn ^ rank_flip];
            if (PopCount(PAWN_ATTACKS[color][locn] & enemy_pieces & ~position->pawns) == 2)
            {
                EVAL_INCREMENT("EG pawn fork");
                score += SCORE_PAWN_FORK;
            }
            if (!(forward_squares & position->occupied_squares))
            {       
                EVAL_INCREMENT("EG pawn forward path open");
                score += SCORE_EG_PAWN_PATH_OPEN;               
                while (forward_squares)
                {
                    const bitboard attacks_to_sq = AttacksToSquare(position, FindAndClearLsb(&forward_squares));
                    if (PopCount(attacks_to_sq & enemy_pieces) > PopCount(attacks_to_sq & friendly_pieces))
                    {
                        goto NotFreeRun;
                    }
                }
                EVAL_INCREMENT("EG pawn free run");
                score += SCORE_EG_PAWN_FREE_RUN;
            }
NotFreeRun:
            if (square & pstr->passed_pawns & pstr->defended_pawns)
            {
                EVAL_INCREMENT("EG pawn passed and defended");
                score += SCORE_EG_PAWN_PASSED_DEFENDED;
            }
            else
            {
                if (square & pstr->passed_pawns)
                {
                    EVAL_INCREMENT("EG pawn passed");
                    score += SCORE_EG_PAWN_PASSED;
                }
                if (square & pstr->defended_pawns)
                {
                    EVAL_INCREMENT("EG pawn defended");
                    score += SCORE_EG_PAWN_DEFENDED;
                }
                if (square & pstr->blocked_pawns)
                {
                    EVAL_INCREMENT("EG pawn blocked");
                    score += SCORE_EG_PAWN_BLOCKED;
                }
            }
        }
        /**********************************************************************
        Knights
        # piece placement
        # bonus for forks
        ***********************************************************************/
        b = position->knights & friendly_pieces;
        while (b)
        {
            const int locn = FindAndClearLsb(&b);            
            const bitboard attacks = KNIGHT_ATTACKS[locn] & enemy_pieces & (position->rooks | position->queens | position->kings);
            if (PopCount(attacks) >= 2)
            {
                EVAL_INCREMENT("EG knight fork");
                score += SCORE_KNIGHT_FORK;
            }
            score += SCORE_KNIGHT_SQUARE[locn ^ rank_flip];
        }
        /**********************************************************************
        Bishops
        # single bishop - reward on same color squares as enemy pieces
        # penalty for poor mobility
        # bonus for bishop forks
        ***********************************************************************/
        b = position->bishops & friendly_pieces;
        if (HAS_SINGLE_BIT_SET(b))
        {
            if (b & WHITE_SQUARES)
            {
                EVAL_INCREMENT("EG bishop single white squares");
                score += SCORE_BISHOP_ENEMY_PIECES * PopCount(enemy_pieces & WHITE_SQUARES);
            }
            else
            {
                EVAL_INCREMENT("EG bishop single black squares");
                score += SCORE_BISHOP_ENEMY_PIECES * PopCount(enemy_pieces & BLACK_SQUARES);
            }
        }
        while (b)
        {
            const bitboard attacks = BishopAttacks(position->occupied_squares, FindAndClearLsb(&b)) & ~friendly_pieces;
            score += SCORE_BISHOP_MOBILITY[PopCount(attacks)];
            if (PopCount(attacks & (position->rooks | position->queens | position->kings)) >= 2)
            {
                EVAL_INCREMENT("EG bishop fork");
                score += SCORE_BISHOP_FORK;
            }
        }
        /**********************************************************************
        King
        # piece placement - encourage king to come into center
        # bonus for proximity to passed pawns (of either color)
        # bonus for good safe king mobility
        ***********************************************************************/
        score += SCORE_KING_SQUARE_ENDGAME[king_location ^ rank_flip];
        score += SCORE_EG_KING_NEAR_PASSED_PAWN * PopCount(KingFill(KING_ATTACKS[king_location]) & (ps[WHITE].passed_pawns | ps[BLACK].passed_pawns));
        b = KING_ATTACKS[king_location] & ~friendly_pieces;
        while (b)
        {
            const int locn = FindAndClearLsb(&b);
            if (!(AttacksToSquare(position, locn) & enemy_pieces))
            {
                king_targets |= BITBOARD(locn);
            }
        }
        score += SCORE_KING_MOBILITY_ENDGAME[PopCount(king_targets)];
        /**********************************************************************
        Undefended material
        # penalize undefended pieces
        ***********************************************************************/
        b = friendly_pieces;
        while (b)
        {
            const int locn = FindAndClearLsb(&b);
            if (!(AttacksToSquare(position, locn) & friendly_pieces))
            {
                EVAL_INCREMENT("EG undefended piece");
                score += SCORE_UNDEFENDED_MATERIAL[PIECE_AT(position, locn)];
            }
        }
        non_material_scores[color] = score;
    }
    return non_material_scores[WHITE] - non_material_scores[BLACK];
}
/******************************************************************************
Estimate non-material midgame evaluation with respect to white.
*******************************************************************************/
static int EvaluateMidgame(const Position* position, const PawnStructure ps[2])
{
    int non_material_scores[2] = { 0 };
    Pins pins;
    int color;
    DeterminePins(position, &pins);
    for (color = WHITE; color <= BLACK; ++color)
    {    
        int score = 0;
        bitboard b, file;
        const bitboard friendly_pieces   = position->occupied_squares & position->pieces_of_color[color];
        const bitboard enemy_pieces      = position->occupied_squares ^ friendly_pieces;
        const bitboard friendly_pawns    = position->pawns & friendly_pieces;
        const int      king_location     = position->king_location[color];
        const bitboard king_file         = FILE_BITBOARD(king_location);
        const int      rank_flip         = color == WHITE ? RANK_FLIP : 0;  
        const PawnStructure* const pstr = &ps[color];
        INCREMENT("eval calls midgame");
        /**********************************************************************
        Piece placement, pawn and knight forks
        # Pawns should advance
        # Bonus for pawn forks
        # Knights should stick to the center
        # Bonus for knights standing in enemy pawn holes
        # Bonus for knight forks
        # Kings should not come out until the endgame       
        ***********************************************************************/
        b = friendly_pawns;
        while (b)
        {
            const int locn = FindAndClearLsb(&b);
            if (PopCount(PAWN_ATTACKS[color][locn] & enemy_pieces & ~position->pawns) == 2)
            {
                EVAL_INCREMENT("MG pawn fork");
                score += SCORE_PAWN_FORK;
            }
            score += SCORE_PAWN_SQUARE[locn ^ rank_flip];
        }
        b = position->knights & friendly_pieces;
        score += SCORE_KNIGHT_PAWN_HOLE * PopCount(b & ps[ENEMY(color)].pawn_holes);
        while (b)
        {
            const int locn = FindAndClearLsb(&b);            
            if (PopCount(KNIGHT_ATTACKS[locn] & enemy_pieces & (position->rooks | position->queens | position->kings)) >= 2)
            {
                EVAL_INCREMENT("MG knight fork");
                score += SCORE_KNIGHT_FORK;
            }
            score += SCORE_KNIGHT_SQUARE[locn ^ rank_flip];
        }
        score += SCORE_KING_SQUARE[king_location ^ rank_flip];
        /**********************************************************************
        King safety
        # Kings should be protected by a wall of friendly pawns, preferably 
          directly in front of the king
        # Penalize enemy attacks to squares adjacent to the king
        # Penalize open files on or adjacent to the king
        # Penalize open ray attacks to the king
        ***********************************************************************/
        /* King on open file */
        if (!(king_file & friendly_pawns))
        {
            EVAL_INCREMENT("MG king open file");
            score += SCORE_KING_OPEN_FILE;
        }
        /* File(s) adjacent to king open */
        file = SHIFT_WEST(king_file);
        if (file && !(file & friendly_pawns))
        {
            EVAL_INCREMENT("MG king open file west");
            score += SCORE_KING_ADJ_OPEN_FILE;
        }
        file = SHIFT_EAST(king_file);
        if (file && !(file & friendly_pawns))
        {
            EVAL_INCREMENT("MG king open file east");
            score += SCORE_KING_ADJ_OPEN_FILE;
        }
        /* Friendly pawn shield */
        score += SCORE_KING_PAWN_SHIELD_1 * PopCount(friendly_pawns & KING_PAWN_SHIELD  [color][king_location]);
        score += SCORE_KING_PAWN_SHIELD_2 * PopCount(friendly_pawns & KING_PAWN_SHIELD_2[color][king_location]);
        /* Enemy attacks to squares adjacent to the king */
        b = KING_ATTACKS[king_location];
        while (b)
        {
            int locn = FindAndClearLsb(&b);
            bitboard attackers = AttacksToSquare(position, locn) & enemy_pieces;
            while (attackers)
            {
                score += SCORE_KING_ADJ_ATTACKED[PIECE_AT(position, FindAndClearLsb(&attackers))];
            }          
        }
        /* Open ray attacks to the king */
        score += SCORE_KING_RAY_OPEN[PopCount(QueenAttacks(position->occupied_squares, king_location))];
        /**********************************************************************
        Mobility and bishop forks
        # If we only have a single bishop, reward it for being on the same 
          color square as enemy pawns
        # Penalize poor mobility of sliding pieces, especially bishops
        # Bonus for bishop forks       
        ***********************************************************************/
        b = position->bishops & friendly_pieces;
        if (HAS_SINGLE_BIT_SET(b))
        {
            if (b & WHITE_SQUARES)
            {
                EVAL_INCREMENT("MG bishop single white");
                score += SCORE_BISHOP_ENEMY_PIECES * PopCount(enemy_pieces & WHITE_SQUARES);
            }
            else
            {
                EVAL_INCREMENT("MG bishop single black");
                score += SCORE_BISHOP_ENEMY_PIECES * PopCount(enemy_pieces & BLACK_SQUARES);
            }
        }
        while (b)
        {
            const bitboard attacks = BishopAttacks(position->occupied_squares, FindAndClearLsb(&b)) & ~friendly_pieces;
            score += SCORE_BISHOP_MOBILITY[PopCount(attacks)];
            if (PopCount(attacks & (position->rooks | position->queens | position->kings)) >= 2)
            {
                EVAL_INCREMENT("MG bishop fork");
                score += SCORE_BISHOP_FORK;
            }
        }
        /* Rook mobility */
        b = position->rooks & friendly_pieces;
        while (b)
        {
            score += SCORE_ROOK_MOBILITY[PopCount(RookAttacks(position->occupied_squares, FindAndClearLsb(&b)) & ~friendly_pieces)];
        }
        /* Queen mobility */
        b = position->queens & friendly_pieces;
        while (b)
        {
            score += SCORE_QUEEN_MOBILITY[PopCount(QueenAttacks(position->occupied_squares, FindAndClearLsb(&b)) & ~friendly_pieces)];
        }
        /**********************************************************************
        Control of the center
        # Reward control of the center 4 squares d4, e4, d5, e5
          (here, control is rather naievely defined as having more pieces
          attacking a square, regardless of the static exchange evaluation)
        ***********************************************************************/
        b = CTR_4_SQUARES;
        while(b)
        {
            const int locn = FindAndClearLsb(&b);
            const bitboard attacks_to_sq = AttacksToSquare(position, locn);
            if (PopCount(attacks_to_sq & friendly_pieces) > PopCount(attacks_to_sq & enemy_pieces))
            {
                EVAL_INCREMENT("MG center control");
                score += SCORE_CENTER_SQUARE_CONTROL;
            }
        }
        /**********************************************************************
        Castling
        # Bonus for having castled
        # Penalty for forfeiting the right to castle
        ***********************************************************************/
        if (color == WHITE)
        {
            if (position->castle_flags & HAS_WHITE_CASTLED)
            {
                EVAL_INCREMENT("MG castling bonus white");
                score += SCORE_CASTLED_BONUS;
            }
            else if (!(position->castle_flags & (MAY_WHITE_K | MAY_WHITE_Q)))
            {
                EVAL_INCREMENT("MG castling forfeit white");
                score += SCORE_FORFEIT_CASTLING_RIGHT;
            }
        }
        else
        {
            if (position->castle_flags & HAS_BLACK_CASTLED)
            {
                EVAL_INCREMENT("MG castling bonus black");
                score += SCORE_CASTLED_BONUS;
            }
            else if (!(position->castle_flags & (MAY_BLACK_K | MAY_BLACK_Q)))
            {
                EVAL_INCREMENT("MG castling forfeit black");
                score += SCORE_FORFEIT_CASTLING_RIGHT;
            }
        }
        /**********************************************************************
        Pawn structure
        # Reward pawns which defend one another
        # Reward passed pawns
        # Penalize isolated pawns
        # Penalize blocked pawns
        # Penalize doubled pawns
        
        This is pretty primitive and should definitely be improved...
        ***********************************************************************/
        score += 
            SCORE_DEFENDED_PAWN * PopCount(pstr->defended_pawns) +
            SCORE_ISOLATED_PAWN * PopCount(pstr->isolated_pawns) +
            SCORE_BLOCKED_PAWN  * PopCount(pstr->blocked_pawns)  +
            SCORE_DOUBLED_PAWN  * PopCount(pstr->doubled_pawns)  +
            SCORE_PASSED_PAWN   * PopCount(pstr->passed_pawns);
        /**********************************************************************
        Pins
        # Penalize pieces which are pinned to the king
        ***********************************************************************/
        b = pins.pinned_pieces & friendly_pieces;
        while (b)
        {
            EVAL_INCREMENT("MG pinned piece");
            score += SCORE_PINNED_MATERIAL[PIECE_AT(position,FindAndClearLsb(&b))];
        }
        /**********************************************************************
        Undefended pieces and en prise pieces
        # Penalize friendly pieces which are not defended
        # Penalize more heavily pieces which are en prise
        ***********************************************************************/
        b = friendly_pieces;
        while (b)
        {
            const int locn = FindAndClearLsb(&b);
            const bitboard attacks_to_sq = AttacksToSquare(position, locn);
            if (!(attacks_to_sq & friendly_pieces))
            {
                if (attacks_to_sq & enemy_pieces)
                {
                    EVAL_INCREMENT("MG en prise");
                    score += SCORE_EN_PRISE_MATERIAL[PIECE_AT(position, locn)];
                }
                else
                {
                    EVAL_INCREMENT("MG undefended");
                    score += SCORE_UNDEFENDED_MATERIAL[PIECE_AT(position, locn)];
                }
            }
        }
        non_material_scores[color] = score;
    }
    return non_material_scores[WHITE] - non_material_scores[BLACK];
}
/******************************************************************************
A basic evaluation function which takes the following factors into 
consideration:

# Material
# King safety
    ## pawn shield
    ## open files near the king
    ## enemy attacks to squares adjacent to the king
    ## open rays directly in view of the king
# Piece placement
# Mobility of sliding pieces
# Control of the center
# Castling
# Pawn structure
    ## passed pawns
    ## isolated pawns
    ## doubled pawns
    ## blocked pawns
    ## defended pawns
# Pawn forks
# Knight forks
# Pins
# Undefended pieces
# En prise pieces
*******************************************************************************/
int EvaluatePosition(const Position* position, int alpha, int beta)
{
    bool is_endgame;
    int piece;
    bitboard b;   
    int score;
    PawnStructure ps[2];
    int material_scores[2] = { 0 };  
    INCREMENT("eval calls");
    /**************************************************************************
    First pass: do a quick material only evaluation to see if it's even worth 
    computing the real score. If we are above beta or below alpha by the non 
    material limit then just return the limit value. This saves a lot of time 
    in positions where there is no way we could be on the principal variation 
    and the exact score doesn't matter since we are going to cutoff anyway.
    ***************************************************************************/
    for (piece = PAWN; piece <= QUEEN; ++piece)
    {
        b = position->pieces[piece];
        material_scores[WHITE] += SCORE_MATERIAL_VALUES[piece] * PopCount(b & position->white_pieces);
        material_scores[BLACK] += SCORE_MATERIAL_VALUES[piece] * PopCount(b & position->black_pieces);
    }
    if ((position->white_pieces & position->bishops & WHITE_SQUARES) &&
        (position->white_pieces & position->bishops & BLACK_SQUARES))
    {
        material_scores[WHITE] += SCORE_BISHOP_PAIR;
    }
    if ((position->black_pieces & position->bishops & WHITE_SQUARES) &&
        (position->black_pieces & position->bishops & BLACK_SQUARES))
    {
        material_scores[BLACK] += SCORE_BISHOP_PAIR;
    }
    if (!(position->pawns & position->white_pieces))
    {
        material_scores[WHITE] += SCORE_NO_PAWNS;
    }
    if (!(position->pawns & position->black_pieces))
    {
        material_scores[BLACK] += SCORE_NO_PAWNS;
    }
    if (PopCount(position->knights & position->white_pieces) == 2)
    {
        material_scores[WHITE] += SCORE_KNIGHT_PAIR;
    }
    if (PopCount(position->knights & position->black_pieces) == 2)
    {
        material_scores[BLACK] += SCORE_KNIGHT_PAIR;
    }
    score = position->state_flags & IS_BLACK_TO_MOVE ? 
        material_scores[BLACK] - material_scores[WHITE] : material_scores[WHITE] - material_scores[BLACK];
    if (score >= beta + SCORE_NON_MATERIAL_LIMIT)
    {
        INCREMENT("eval pos material only");
        return beta;
    }
    if (score <= alpha - SCORE_NON_MATERIAL_LIMIT)
    {
        INCREMENT("eval neg material only");
        return alpha;
    }
    if (IsDrawByMaterial(position))
    {
        return DRAW_SCORE;
    }
    DeterminePawnStructure(position, ps);
    /**************************************************************************
    Limit the total effect of non material evaluation features to avoid  
    throwing away material in exchange for a probably dubious positional 
    advantage (we know our positional knowledge is poor so don't rely too 
    heavily on it)
    ***************************************************************************/
    is_endgame = ((material_scores[WHITE] < 1500 && material_scores[BLACK] < 1500) ||
                  material_scores[WHITE] < 1000                                  ||
                  material_scores[BLACK] < 1000);
    score = is_endgame ? EvaluateEndgame(position, ps) : EvaluateMidgame(position, ps);
    score = score > SCORE_NON_MATERIAL_LIMIT ? SCORE_NON_MATERIAL_LIMIT : (score < -SCORE_NON_MATERIAL_LIMIT ? -SCORE_NON_MATERIAL_LIMIT : score);
    score += material_scores[WHITE] - material_scores[BLACK];
    return position->state_flags & IS_BLACK_TO_MOVE ? -score : score;
}
#endif
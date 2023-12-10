#pragma once

#include "pawnstar.h"

static const int PAWN_SQUARE[64] =
{
     0,  0,  0,  0,  0,  0,  0,  0,
     5, 10, 15, 20, 20, 15, 10,  5,
     4,  8, 12, 16, 16, 12,  8,  4,
     3,  6,  9, 12, 12,  9,  6,  3,
     2,  4,  6,  8,  8,  6,  4,  2,
     1,  2,  3,-10,-10,  3,  2,  1,
     0,  0,  0,-30,-30,  0,  0,  0,
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
   -50,-40,-30,-30,-30,-30,-40,-50,
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
    -5,  0,  0,  5,  5,  0,  0, -5,
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
   -40,-40,-40,-40,-40,-40,-40,-40,
   -40,-40,-40,-40,-40,-40,-40,-40,
   -40,-40,-40,-40,-40,-40,-40,-40,
   -40,-40,-40,-40,-40,-40,-40,-40,
   -40,-40,-40,-40,-40,-40,-40,-40,
   -40,-40,-40,-40,-40,-40,-40,-40,
   -20,-20,-20,-20,-20,-20,-20,-20,
     0, 20, 40,-20,  0,-20, 40, 20
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
     0, 10, 20, 30, 30, 20, 10,  0
};

static const int PASSED_PAWN_SQUARE[64] = 
{
     0,  0,  0,  0,  0,  0,  0,  0,
    50, 50, 50, 50, 50, 50, 50, 50, 
    50, 50, 50, 50, 50, 50, 50, 50,
    30, 30, 30, 30, 30, 30, 30, 30, 
    20, 20, 20, 20, 20, 20, 20, 20,
    15, 15, 15, 15, 15, 15, 15, 15, 
    10, 10, 10, 10, 10, 10, 10, 10, 
     0,  0,  0,  0,  0,  0,  0,  0,
};

static const Bitboard FILES_BB[8] = { FILE_A, FILE_B, FILE_C, FILE_D, FILE_E, FILE_F, FILE_G, FILE_H };

template<int color> void
DeterminePawnStructure(const Position& position, PawnStructure& s)
{
    const Bitboard friendly_pawns = position.pawns & position.pieces_of_color[color];
    const Bitboard enemy_pawns    = position.pawns ^ friendly_pawns;
    memset(&s, 0, sizeof(s));
    Bitboard p = friendly_pawns;
    while (p)
    {
        const uint8_t locn = FindAndClearLsb(p);
        const PawnSets& sets = PAWN_SETS[color][locn];
        if ((sets.passed_pawn_mask & enemy_pawns) == NO_SQUARES)
        {
            s.passed_pawns |= BITBOARD(locn);
        }
        if (sets.doubled_pawn_mask & friendly_pawns)
        {
            s.doubled_pawns |= BITBOARD(locn);
        }
        if ((sets.isolated_pawn_mask & friendly_pawns) == NO_SQUARES)
        {
            s.isolated_pawns |= BITBOARD(locn);
        }
        if ((sets.supported_pawn_mask & friendly_pawns) == NO_SQUARES)
        {
            if constexpr (color == WHITE)
            {
                const Bitboard enemy_pawn_attacks = ShiftSouthwest(enemy_pawns) | ShiftSoutheast(enemy_pawns);
                const uint8_t forward_locn = locn + 8;
                if (enemy_pawn_attacks & BITBOARD(forward_locn))
                {
                    s.backward_pawns |= BITBOARD(locn);
                }
            }
            else
            {
                const Bitboard enemy_pawn_attacks = ShiftNorthwest(enemy_pawns) | ShiftNortheast(enemy_pawns);
                const uint8_t forward_locn = locn - 8;
                if (enemy_pawn_attacks & BITBOARD(forward_locn))
                {
                    s.backward_pawns |= BITBOARD(locn);
                }
            }
        }
    }
    /* Doubled pawns are not passed pawns. */
    s.passed_pawns &= ~s.doubled_pawns;
}

template<int color> int
EvaluateMaterial(const Position& position)
{
    const Bitboard friendly_pieces = position.pieces_of_color[color];
    const Bitboard pawns   = position.pawns   & friendly_pieces;
    const Bitboard knights = position.knights & friendly_pieces;
    const Bitboard bishops = position.bishops & friendly_pieces;
    const Bitboard rooks   = position.rooks   & friendly_pieces;
    const Bitboard queens  = position.queens  & friendly_pieces;
    int score = 
        PopCount(pawns)   * 100 +
        PopCount(knights) * 300 +
        PopCount(bishops) * 300 +
        PopCount(rooks)   * 500 +
        PopCount(queens)  * 900;
    /* Bonus for the bishop pair. */
    if ((bishops & WHITE_SQUARES) && (bishops & BLACK_SQUARES))
    {
        score += 50;
    }
    /* Penalty for no pawns */
    if (pawns == NO_SQUARES)
    {
        score -= 50;
    }
    /* Bonus for knights based on number of friendly pawns */
    score += PopCount(knights) * (PopCount(pawns) - 4) * 5;
    return score;
}

/**
 * @brief Evaluate piece square scores excluding the king.
 * @tparam color color to evaluate for
 * @param position Position
 * @return 
 */
template<int color> int
EvaluatePieceSquare(const Position& position)
{
    constexpr int rank_flip = (color == WHITE ? RANK_FLIP : 0);
    const Bitboard friendly_pieces = position.pieces_of_color[color];
    int score = 0;
    Bitboard b = position.pawns & friendly_pieces;
    while (b)
    {
        score += PAWN_SQUARE[FindAndClearLsb(b) ^ rank_flip];
    }
    b = position.knights & friendly_pieces;
    while (b)
    {
        score += KNIGHT_SQUARE[FindAndClearLsb(b) ^ rank_flip];
    }
    b = position.bishops & friendly_pieces;
    while (b)
    {
        score += BISHOP_SQUARE[FindAndClearLsb(b) ^ rank_flip];
    }
    b = position.rooks & friendly_pieces;
    while (b)
    {
        score += ROOK_SQUARE[FindAndClearLsb(b) ^ rank_flip];
    }
    b = position.queens & friendly_pieces;
    while (b)
    {
        score += QUEEN_SQUARE[FindAndClearLsb(b) ^ rank_flip];
    }
    return score;
}

template<int color> int
EvaluatePawnStructure(const PawnStructure& ps)
{
    constexpr int rank_flip = (color == WHITE ? RANK_FLIP : 0);
    Bitboard b = ps.passed_pawns;
    int score = 0;
    while (b)
    {
        score += PASSED_PAWN_SQUARE[FindAndClearLsb(b) ^ rank_flip];
    }
    score -= PopCount(ps.backward_pawns) * 10;
    score -= PopCount(ps.doubled_pawns)  * 10;
    score -= PopCount(ps.isolated_pawns) * 20;
    return score;
}

/**
 * @brief Evaluate king score based on positional factors and enemy material.
 * @tparam color Color to evaluate
 * @param position Position
 * @return score
 */
template<int color> int
EvaluateKing(const Position& position)
{
    constexpr int rank_flip         = (color == WHITE ? RANK_FLIP : 0);
    const Bitboard friendly_pieces  = position.pieces_of_color[color];
    const Bitboard enemy_pieces     = position.pieces_of_color[EnemyOf(color)];
    const Bitboard friendly_pawns   = friendly_pieces & position.pawns;
    const Bitboard enemy_pawns      = enemy_pieces    & position.pawns;
    /* 
    Determine enemy non pawn material value. 
    This starts out at the beginning of the game as 31
    (2 x N + 2 x B + 2 x R + 1 x Q)
    */
    const int enemy_material =
        3 * PopCount((position.knights | position.bishops) & enemy_pieces) +
        5 * PopCount( position.rooks                       & enemy_pieces) +
        9 * PopCount( position.queens                      & enemy_pieces);
    int score;
    /* First do piece square table */
    if (enemy_material >= 16)
    {
        score = KING_SQUARE_MIDGAME[position.king_location[color] ^ rank_flip];
    }
    else if (enemy_material <= 11)
    {
        score = KING_SQUARE_ENDGAME[position.king_location[color] ^ rank_flip];
    }
    else
    {
        const int early_score = KING_SQUARE_MIDGAME[position.king_location[color] ^ rank_flip];
        const int late_score  = KING_SQUARE_ENDGAME[position.king_location[color] ^ rank_flip];
        score = early_score * (enemy_material - 11) + late_score * (16 - enemy_material);
        score /= 5;
    }
    /* Consider pawns in front of the king and approaching enemy pawns */
    if constexpr (color == WHITE)
    {
        const Bitboard king_bb = BITBOARD(position.king_location[color]);
        
        /* Bonus for pawns which are directly in front of the king */
        Bitboard pawn_shelter_squares = 
            ShiftNorthwest(king_bb) | ShiftNorth(king_bb) | ShiftNortheast(king_bb);
        score += 25 * PopCount(friendly_pawns & pawn_shelter_squares);
        
        /* Smaller bonus for pawns which have moved forward one square */
        pawn_shelter_squares = ShiftNorth(pawn_shelter_squares);
        score += 10 * PopCount(friendly_pawns & pawn_shelter_squares) 
               - 20 * PopCount(enemy_pawns & pawn_shelter_squares);
        
        /* Penalty for enemy pawns close to our king */
        pawn_shelter_squares = ShiftNorth(pawn_shelter_squares);
        score -= 10 * PopCount(enemy_pawns & pawn_shelter_squares);
       
        /* Penalty for open files near our king */
        Bitboard king_file = FILES_BB[FileOf(position.king_location[color])];
        if ((king_file & friendly_pawns) == NO_SQUARES)
        {
            score -= 30;
        }
        Bitboard adjacent_file = ShiftWest(king_file);
        if (adjacent_file && ((adjacent_file & friendly_pawns) == NO_SQUARES))
        {
            score -= 20;
        }
        adjacent_file = ShiftEast(king_file);
        if (adjacent_file && ((adjacent_file & friendly_pawns) == NO_SQUARES))
        {
            score -= 20;
        }
        
    }
    else
    {
        const Bitboard king_bb = BITBOARD(position.king_location[color]);
        
        /* Bonus for pawns which are directly in front of the king */
        Bitboard pawn_shelter_squares = 
            ShiftSouthwest(king_bb) | ShiftSouth(king_bb) | ShiftSoutheast(king_bb);
        score += 25 * PopCount(friendly_pawns & pawn_shelter_squares);
        
        /* Smaller bonus for pawns which have moved forward one square */
        pawn_shelter_squares = ShiftSouth(pawn_shelter_squares);
        score += 10 * PopCount(friendly_pawns & pawn_shelter_squares) 
               - 20 * PopCount(enemy_pawns & pawn_shelter_squares);
        
        /* Penalty for enemy pawns close to our king */
        pawn_shelter_squares = ShiftSouth(pawn_shelter_squares);
        score -= 10 * PopCount(enemy_pawns & pawn_shelter_squares);
       
        /* Penalty for open files near our king */
        Bitboard king_file = FILES_BB[FileOf(position.king_location[color])];
        if ((king_file & friendly_pawns) == NO_SQUARES)
        {
            score -= 30;
        }
        Bitboard adjacent_file = ShiftWest(king_file);
        if (adjacent_file && ((adjacent_file & friendly_pawns) == NO_SQUARES))
        {
            score -= 20;
        }
        adjacent_file = ShiftEast(king_file);
        if (adjacent_file && ((adjacent_file & friendly_pawns) == NO_SQUARES))
        {
            score -= 20;
        }
    }
    return score;
}
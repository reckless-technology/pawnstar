#pragma once
#include <stdint.h>
#include <memory.h>

#include "bitboard.h"
#include "function_prototypes.h"

/**
 * @brief Pawn structure for one color.
 * A passed pawn has no enemy pawns in front of it either on its file or on adjacent files.
 * 
 * An isolated pawn has no friendly pawns on either adjacent file.
 * 
 * A backward pawn has no pawns to support it and its forward square is 
 * attacked by an enemy pawn.
 * 
 * A doubled pawn has a friendly pawn in front of it on the same file.
 */
struct PawnStructure
{
    Bitboard passed_pawns;
    Bitboard isolated_pawns;
    Bitboard backward_pawns;
    Bitboard doubled_pawns;
};

const int PAWN_SQUARE[64] =
{
     0,  0,  0,  0,  0,  0,  0,  0,
     5, 10, 15, 20, 20, 15, 10,  5,
     4,  8, 12, 16, 16, 12,  8,  4,
     3,  6,  9, 12, 12,  9,  6,  3,
     2,  4,  6,  8,  8,  6,  4,  2,
     1,  2,  3,-10,-10,  3,  2,  1,
     0,  0,  0,-40,-40,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0
};

const int KNIGHT_SQUARE[64] =
{
  -10,-10,-10,-10,-10,-10,-10, -10,
  -10,  0,  0,  0,  0,  0,  0, -10,
  -10,  0,  5,  5,  5,  5,  0, -10,
  -10,  0,  5, 10, 10,  5,  0, -10,
  -10,  0,  5, 10, 10,  5,  0, -10,
  -10,  0,  5,  5,  5,  5,  0, -10,
  -10,  0,  0,  0,  0,  0,  0, -10,
  -10,-30,-10,-10,-10,-10,-30, -10
};

const int BISHOP_SQUARE[64] = 
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

const int ROOK_SQUARE[64] = 
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

const int QUEEN_SQUARE[64] = 
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

const int KING_SQUARE_MIDGAME[64] =
{
   -40,-40,-40,-40,-40,-40,-40,-40,
   -40,-40,-40,-40,-40,-40,-40,-40,
   -40,-40,-40,-40,-40,-40,-40,-40,
   -40,-40,-40,-40,-40,-40,-40,-40,
   -40,-40,-40,-40,-40,-40,-40,-40,
   -40,-40,-40,-40,-40,-40,-40,-40,
   -20,-20,-20,-20,-20,-20,-20,-20,
     0, 40, 20,-20,  0,-20, 40, 20
};

const int KING_SQUARE_ENDGAME[64] =
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

const int PASSED_PAWN_SQUARE[64] = 
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

const Bitboard FILE_BITBOARDS[8] = { FILE_A, FILE_B, FILE_C, FILE_D, FILE_E, FILE_F, FILE_G, FILE_H };

template<int color> void
DeterminePawnStructure(const Position& position, PawnStructure& s)
{
    const Bitboard friendly_pawns = position.pawns_ & position.pieces_of_color_[color];
    const Bitboard enemy_pawns    = position.pawns_ ^ friendly_pawns;
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
    const Bitboard friendly_pieces = position.pieces_of_color_[color];
    const Bitboard pawns   = position.pawns_   & friendly_pieces;
    const Bitboard knights = position.knights_ & friendly_pieces;
    const Bitboard bishops = position.bishops_ & friendly_pieces;
    const Bitboard rooks   = position.rooks_   & friendly_pieces;
    const Bitboard queens  = position.queens_  & friendly_pieces;
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
 * @tparam color Color to evaluate for
 * @param position Position
 * @return 
 */
template<int color> int
EvaluatePieceSquare(const Position& position)
{
    constexpr int rank_flip = (color == WHITE ? RANK_FLIP : 0);
    const Bitboard friendly_pieces = position.pieces_of_color_[color];
    int score = 0;
    Bitboard b = position.pawns_ & friendly_pieces;
    while (b)
    {
        score += PAWN_SQUARE[FindAndClearLsb(b) ^ rank_flip];
    }
    b = position.knights_ & friendly_pieces;
    while (b)
    {
        score += KNIGHT_SQUARE[FindAndClearLsb(b) ^ rank_flip];
    }
    b = position.bishops_ & friendly_pieces;
    while (b)
    {
        score += BISHOP_SQUARE[FindAndClearLsb(b) ^ rank_flip];
    }
    b = position.rooks_ & friendly_pieces;
    while (b)
    {
        score += ROOK_SQUARE[FindAndClearLsb(b) ^ rank_flip];
    }
    b = position.queens_ & friendly_pieces;
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
    const Bitboard friendly_pieces  = position.pieces_of_color_[color];
    const Bitboard enemy_pieces     = position.pieces_of_color_[EnemyOf(color)];
    const Bitboard friendly_pawns   = friendly_pieces & position.pawns_;
    const Bitboard enemy_pawns      = enemy_pieces    & position.pawns_;
    /* 
    Determine enemy non pawn material value. 
    This starts out at the beginning of the game as 31
    (2 x N + 2 x B + 2 x R + 1 x Q)
    */
    const int enemy_material =
        3 * PopCount((position.knights_ | position.bishops_) & enemy_pieces) +
        5 * PopCount( position.rooks_                       & enemy_pieces) +
        9 * PopCount( position.queens_                      & enemy_pieces);
    int piece_square_score;
    /* First do piece square table */
    if (enemy_material >= 16)
    {
        piece_square_score = KING_SQUARE_MIDGAME[position.king_location_[color] ^ rank_flip];
    }
    else if (enemy_material <= 11)
    {
        piece_square_score = KING_SQUARE_ENDGAME[position.king_location_[color] ^ rank_flip];
    }
    else
    {
        const int early_score = KING_SQUARE_MIDGAME[position.king_location_[color] ^ rank_flip];
        const int late_score  = KING_SQUARE_ENDGAME[position.king_location_[color] ^ rank_flip];
        piece_square_score = early_score * (enemy_material - 11) + late_score * (16 - enemy_material);
        piece_square_score /= 5;
    }
    
    /* Consider pawns in front of the king and approaching enemy pawns */
    const Bitboard pawn_shelter_1 = SETS[position.king_location_[color]].king_pawn_shelter[color];
    Bitboard pawn_shelter_2;
    Bitboard pawn_shelter_3;
    if constexpr (color == WHITE)
    {
        pawn_shelter_2 = ShiftNorth(pawn_shelter_1);
        pawn_shelter_3 = ShiftNorth(pawn_shelter_2);
    }
    else
    {
        pawn_shelter_2 = ShiftSouth(pawn_shelter_1);
        pawn_shelter_3 = ShiftSouth(pawn_shelter_2);
    }
    int safety_score = 25 * PopCount(friendly_pawns & pawn_shelter_1)
                     + 10 * PopCount(friendly_pawns & pawn_shelter_2) 
                     - 20 * PopCount(enemy_pawns    & pawn_shelter_2)
                     - 10 * PopCount(enemy_pawns    & pawn_shelter_3);
    
    /* Penalty for open files near our king */
    const Bitboard king_file = FILE_BITBOARDS[FileOf(position.king_location_[color])];
    if ((king_file & friendly_pawns) == NO_SQUARES)
    {
        safety_score -= 30;
    }
    Bitboard adjacent_file = ShiftWest(king_file);
    if (adjacent_file && ((adjacent_file & friendly_pawns) == NO_SQUARES))
    {
        safety_score -= 20;
    }
    adjacent_file = ShiftEast(king_file);
    if (adjacent_file && ((adjacent_file & friendly_pawns) == NO_SQUARES))
    {
        safety_score -= 20;
    }
    /* Scale the king safety score according to the enemy's material. */
    safety_score = (safety_score * enemy_material) / 31;
    return piece_square_score + safety_score;
}
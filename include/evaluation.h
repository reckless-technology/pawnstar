#pragma once
#include <cstdint>
#include <cstring>

#include "bitboard.h"
#include "position.h"

int EvaluatePosition(const Position &position, int alpha, int beta);

// clang-format off

/// @brief Piece square table for pawns.
constexpr int PAWN_SQUARE[64] =
{
     0,  0,  0,  0,  0,  0,  0,  0,
    25, 25, 25, 25, 25, 25, 25, 25,
    15, 15, 15, 20, 20, 15, 15, 15,
    10, 10, 10, 15, 15, 10, 10, 10,
     5,  5,  5, 10, 10,  5,  5,  5,
     0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,-20,-20,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0
};

/// @brief Piece square table for knights.
constexpr int KNIGHT_SQUARE[64] =
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

/// @brief Piece square table for bishops.
constexpr int BISHOP_SQUARE[64] = 
{
   -20,-10,-10,-10,-10,-10,-10,-20,
   -10,  0,  0,  0,  0,  0,  0,-10,
   -10,  0,  5, 10, 10,  5,  0,-10,
   -10,  5,  5, 10, 10,  5,  5,-10,
   -10,  0, 10, 10, 10, 10,  0,-10,
   -10, 10, 10, 10, 10, 10, 10,-10,
   -10,  5,  0,  0,  0,  0,  5,-10,
   -20,-10,-20,-10,-10,-20,-10,-20,
};

constexpr int BISHOP_ATTACK_SCORES[14] = {-40, -30, -20, -10, 0, 5, 10, 15, 20, 25, 30, 35, 40, 45};

/// @brief Piece square table for rooks.
constexpr int ROOK_SQUARE[64] = 
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

constexpr int ROOK_ATTACK_SCORES[15] = {-40, -30, -20,-10, 0, 5, 10, 15, 20, 25, 30, 35, 40, 45, 50};

/// @brief Piece square table for queens.
constexpr int QUEEN_SQUARE[64] = 
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

/// @brief Piece square table for kings in the mid game.
constexpr int KING_SQUARE_MIDGAME[64] =
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

/// @brief Piece square table for kings in the end game.
constexpr int KING_SQUARE_ENDGAME[64] =
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

/// @brief Piece square table for passed pawn bonuses./
constexpr int PASSED_PAWN_SQUARE[64] = 
{
     0,  0,  0,  0,  0,  0,  0,  0,
    50, 50, 50, 50, 50, 50, 50, 50, // All pawns are "passed" at 7th rank
    50, 50, 50, 50, 50, 50, 50, 50,
    30, 30, 30, 30, 30, 30, 30, 30, 
    20, 20, 20, 20, 20, 20, 20, 20,
    15, 15, 15, 15, 15, 15, 15, 15, 
    10, 10, 10, 10, 10, 10, 10, 10, 
     0,  0,  0,  0,  0,  0,  0,  0,
};

// clang-format on

constexpr Bitboard FILE_BITBOARDS[8] = {FILE_A, FILE_B, FILE_C, FILE_D, FILE_E, FILE_F, FILE_G, FILE_H};

/// @brief Pawn structure for one color.
/// A passed pawn has no enemy pawns in front of it either on its file or on adjacent files.
/// An isolated pawn has no friendly pawns on either adjacent file.
/// A backward pawn has no pawns to support it and its forward square is attacked by an enemy pawn.
/// A doubled pawn has a friendly pawn in front of it on the same file.
struct PawnStructure
{
    Bitboard passed_pawns;
    Bitboard isolated_pawns;
    Bitboard backward_pawns;
    Bitboard doubled_pawns;
    PawnStructure()
        : passed_pawns(NO_SQUARES), isolated_pawns(NO_SQUARES), backward_pawns(NO_SQUARES), doubled_pawns(NO_SQUARES)
    {
    }
};

/// @brief Determine the pawn structure
/// @tparam color color to evaluate
/// @param position position to evaluate
/// @return Pawn structure
template <Color color> PawnStructure DeterminePawnStructure(const Position &position)
{
    const Bitboard friendly_pawns = position.Pawns() & position.PiecesOfColor(color);
    const Bitboard enemy_pawns    = position.Pawns() ^ friendly_pawns;
    PawnStructure  ps;
    Bitboard       fp = friendly_pawns;
    for (Square locn : fp)
    {
        const Bitboard  bb   = Bitboard(locn);
        const PawnSets &sets = PAWN_SETS[color][locn];
        if ((sets.passed_pawn_mask & enemy_pawns) == NO_SQUARES)
        {
            ps.passed_pawns |= bb;
        }
        if ((sets.doubled_pawn_mask & friendly_pawns) != NO_SQUARES)
        {
            ps.doubled_pawns |= bb;
        }
        if ((sets.isolated_pawn_mask & friendly_pawns) == NO_SQUARES)
        {
            ps.isolated_pawns |= bb;
        }
        if ((sets.supported_pawn_mask & friendly_pawns) == NO_SQUARES)
        {
            if constexpr (color == WHITE)
            {
                const Bitboard enemy_pawn_attacks = ShiftSouthwest(enemy_pawns) | ShiftSoutheast(enemy_pawns);
                const Square   forward_locn       = (Square)(locn + 8);
                if ((enemy_pawn_attacks & Bitboard(forward_locn)) != NO_SQUARES)
                {
                    ps.backward_pawns |= bb;
                }
            }
            else
            {
                const Bitboard enemy_pawn_attacks = ShiftNorthwest(enemy_pawns) | ShiftNortheast(enemy_pawns);
                const Square   forward_locn       = (Square)(locn - 8);
                if ((enemy_pawn_attacks & Bitboard(forward_locn)) != NO_SQUARES)
                {
                    ps.backward_pawns |= bb;
                }
            }
        }
    }
    // Doubled pawns cannot be passed pawns.
    ps.passed_pawns &= ~ps.doubled_pawns;
    return ps;
}

/// @brief Evaluate material on the board.
/// @tparam color color to evaluate
/// @param position position to evaluate
/// @return material score
template <Color color> int EvaluateMaterial(const Position &position)
{
    const Bitboard friendly_pieces = position.PiecesOfColor(color);
    const Bitboard pawns           = position.Pawns() & friendly_pieces;
    const Bitboard knights         = position.Knights() & friendly_pieces;
    const Bitboard bishops         = position.Bishops() & friendly_pieces;
    const Bitboard rooks           = position.Rooks() & friendly_pieces;
    const Bitboard queens          = position.Queens() & friendly_pieces;
    int score = pawns.PopCount() * 100 + knights.PopCount() * 400 + bishops.PopCount() * 400 + rooks.PopCount() * 600 +
                queens.PopCount() * 1200;
    // Bonus for the bishop pair.
    if ((bishops & WHITE_SQUARES) != NO_SQUARES && (bishops & BLACK_SQUARES) != NO_SQUARES)
    {
        score += 100;
    }
    // Penalty for no pawns.
    if (pawns == NO_SQUARES)
    {
        score -= 50;
    }
    // Adjustment for knights based on number of friendly pawns.
    score += knights.PopCount() * (pawns.PopCount() - 4) * 5;
    return score;
}

/// @brief Evaluate bishop and rook mobility scores
/// @tparam color color to evaluate
/// @param position current position
/// @return mobility score
template <Color color> int EvaluateMobility(const Position &position)
{

    const Bitboard friendly_pieces  = position.PiecesOfColor(color);
    const Bitboard occupied_squares = position.OccupiedSquares();
    int            score            = 0;
    Bitboard       b                = position.Bishops() & friendly_pieces;
    for (Square locn : b)
    {
        const Bitboard attacks     = BishopAttacks(occupied_squares, locn) & ~friendly_pieces;
        const int      num_attacks = attacks.PopCount();
        score += BISHOP_ATTACK_SCORES[num_attacks];
    }
    b = position.Rooks() & friendly_pieces;
    for (Square locn : b)
    {
        const Bitboard attacks     = RookAttacks(occupied_squares, locn) & ~friendly_pieces;
        const int      num_attacks = attacks.PopCount();
        score += ROOK_ATTACK_SCORES[num_attacks];
    }
    return score;
}

/// @brief Evaluate piece square scores excluding the king.
/// @tparam color Color to evaluate for
/// @param position Position
/// @return score
template <Color color> int EvaluatePieceSquare(const Position &position)
{
    constexpr uint8_t rank_flip       = (color == WHITE ? RANK_FLIP : 0);
    const Bitboard    friendly_pieces = position.PiecesOfColor(color);
    int               score           = 0;
    Bitboard          b               = position.Pawns() & friendly_pieces;
    for (Square locn : b)
    {
        score += PAWN_SQUARE[locn ^ rank_flip];
    }
    b = position.Knights() & friendly_pieces;
    for (Square locn : b)
    {
        score += KNIGHT_SQUARE[locn ^ rank_flip];
    }
    b = position.Bishops() & friendly_pieces;
    for (Square locn : b)
    {
        score += BISHOP_SQUARE[locn ^ rank_flip];
    }
    b = position.Rooks() & friendly_pieces;
    for (Square locn : b)
    {
        score += ROOK_SQUARE[locn ^ rank_flip];
    }
    b = position.Queens() & friendly_pieces;
    for (Square locn : b)
    {
        score += QUEEN_SQUARE[locn ^ rank_flip];
    }
    return score;
}

/// @brief Evaluate pawn structure
/// @tparam color color tp evaluate
/// @param ps pawn structure
/// @return score for ps
template <Color color> int EvaluatePawnStructure(const PawnStructure &ps)
{
    constexpr uint8_t rank_flip = (color == WHITE ? RANK_FLIP : 0);
    Bitboard          b         = ps.passed_pawns;
    int               score     = 0;
    for (Square locn : b)
    {
        score += PASSED_PAWN_SQUARE[locn ^ rank_flip];
    }
    score -= ps.backward_pawns.PopCount() * 20;
    score -= ps.doubled_pawns.PopCount() * 10;
    score -= ps.isolated_pawns.PopCount() * 20;
    return score;
}

/// @brief Evaluate king score based on positional factors and enemy material.
/// @tparam color Color to evaluate
/// @param position Position
/// @return score
template <Color color> int EvaluateKing(const Position &position)
{
    constexpr uint8_t rank_flip       = (color == WHITE ? RANK_FLIP : 0);
    constexpr Square  king_home_locn  = (color == WHITE ? E1 : E8);
    const Bitboard    friendly_pieces = position.PiecesOfColor(color);
    const Bitboard    enemy_pieces    = position.PiecesOfColor(EnemyOf(color));
    const Bitboard    friendly_pawns  = friendly_pieces & position.Pawns();
    const Bitboard    enemy_pawns     = enemy_pieces & position.Pawns();

    // Determine enemy non-pawn material value. This starts out at the beginning of the game as 31
    // (2 x N + 2 x B + 2 x R + 1 x Q)
    const int enemy_material = 3 * ((position.Knights() | position.Bishops()) & enemy_pieces).PopCount() +
                               5 * (position.Rooks() & enemy_pieces).PopCount() +
                               9 * (position.Queens() & enemy_pieces).PopCount();
    int piece_square_score;
    // First do piece square table.
    if (enemy_material > 15)
    {
        // Sufficient material to remain behind the pawn shelter
        piece_square_score = KING_SQUARE_MIDGAME[position.KingLocation(color) ^ rank_flip];
    }
    else if (enemy_material < 9)
    {
        // Material low enough to come to the middle of the board
        piece_square_score = KING_SQUARE_ENDGAME[position.KingLocation(color) ^ rank_flip];
    }
    else
    {
        // Interpolate the 2 based on enemy material value of (9,10,11,12,13,14,15)
        const int a        = enemy_material - 8;
        const int b        = 16 - enemy_material;
        piece_square_score = KING_SQUARE_ENDGAME[position.KingLocation(color) ^ rank_flip] * b +
                             KING_SQUARE_MIDGAME[position.KingLocation(color) ^ rank_flip] * a;
        piece_square_score /= 8;
    }
    // Consider pawns in front of the king and approaching enemy pawns.
    int safety_score = 0;
    if (position.KingLocation(color) != king_home_locn)
    {
        const Bitboard pawn_shelter_1 = SETS[position.KingLocation(color)].KingPawnShelter(color);
        Bitboard       pawn_shelter_2;
        Bitboard       pawn_shelter_3;

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
        safety_score =
            25 * (friendly_pawns & pawn_shelter_1).PopCount() + 10 * (friendly_pawns & pawn_shelter_2).PopCount() +
            5 * (friendly_pawns & pawn_shelter_3).PopCount() - 25 * (enemy_pawns & pawn_shelter_1).PopCount() -
            10 * (enemy_pawns & pawn_shelter_2).PopCount() - 5 * (enemy_pawns & pawn_shelter_3).PopCount();
    }
    // Penalty for open files near our king.
    const Bitboard king_file = FILE_BITBOARDS[FileOf(position.KingLocation(color))];
    if ((king_file & friendly_pawns) == NO_SQUARES)
    {
        safety_score -= 30;
    }
    Bitboard adjacent_file = ShiftWest(king_file);
    if (adjacent_file != NO_SQUARES && ((adjacent_file & friendly_pawns) == NO_SQUARES))
    {
        safety_score -= 20;
    }
    adjacent_file = ShiftEast(king_file);
    if (adjacent_file != NO_SQUARES && ((adjacent_file & friendly_pawns) == NO_SQUARES))
    {
        safety_score -= 20;
    }
    // Scale the king safety score according to the enemy's material.
    safety_score = (safety_score * enemy_material) / 31;
    return piece_square_score + safety_score;
}
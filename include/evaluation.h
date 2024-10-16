#pragma once
/// @file Data and functions to evaluate a chess position.

#include <array>
#include <cstdint>

#include "position.h"

class Game;

int EvaluatePosition(const Game &game, int alpha, int beta);

// clang-format off

/// @brief Piece square table for pawns.
constexpr std::array<int, 64> PAWN_SQUARE
{
     0,   0,   0,   0,   0,   0,   0,   0,
    25,  25,  25,  25,  25,  25,  25,  25,
    15,  15,  15,  20,  20,  15,  15,  15,
    10,  10,  10,  15,  15,  10,  10,  10,
     0,   0,   0,  10,  10,   0,   0,   0,
     0,   0,   0,   0,   0,   0,   0,   0,
     0,   0,   0, -40, -40,   0,   0,   0,
     0,   0,   0,   0,   0,   0,   0,   0
};

/// @brief Piece square table for knights.
constexpr std::array<int, 64> KNIGHT_SQUARE
{
   -20, -10, -10, -10, -10, -10, -10, -20,
   -10,   0,   0,   0,   0,   0,   0, -10,
   -10,   0,   5,   5,   5,   5,   0, -10,
   -10,   0,   5,  10,  10,   5,   0, -10,
   -10,   0,   5,  10,  10,   5,   0, -10,
   -10,   0,   5,   5,   5,   5,   0, -10,
   -10,   0,   0,   0,   0,   0,   0, -10,
   -20, -20, -10, -10, -10, -10, -20, -20
};

/// @brief Piece square table for bishops.
constexpr std::array<int, 64> BISHOP_SQUARE
{
   -10, -10, -10, -10, -10, -10, -10, -10,
   -10,   0,   0,   0,   0,   0,   0, -10,
   -10,   0,   5,   5,   5,   5,   0, -10,
   -10,   0,   5,  10,  10,   5,   0, -10,
   -10,   0,   5,  10,  10,   5,   0, -10,
   -10,   0,   5,   5,   5,   5,   0, -10,
   -10,   0,   0,   0,   0,   0,   0, -10,
   -10, -10, -20, -10, -10, -20, -10, -10
};


/// @brief Piece square table for rooks.
constexpr std::array<int, 64> ROOK_SQUARE
{
     0,   0,   0,   0,   0,   0,   0,   0,
     5,  10,  10,  10,  10,  10,  10,   5,
    -5,   0,   0,   0,   0,   0,   0,  -5,
    -5,   0,   0,   0,   0,   0,   0,  -5,
    -5,   0,   0,   0,   0,   0,   0,  -5,
    -5,   0,   0,   0,   0,   0,   0,  -5,
    -5,   0,   0,   0,   0,   0,   0,  -5,
    -5,   0,   5,   5,   5,   5,   0,  -5,
};


/// @brief Piece square table for queens.
constexpr std::array<int, 64> QUEEN_SQUARE
{
   -10, -10, -10,  -5,  -5, -10, -10, -10,
   -10,   0,   0,   0,   0,   0,   0, -10,
   -10,   0,   5,   5,   5,   5,   0, -10,
    -5,   0,   5,   5,   5,   5,   0,  -5,
     0,   0,   5,   5,   5,   5,   0,  -5,
   -10,   5,   5,   5,   5,   5,   0, -10,
   -10,   0,   5,   0,   0,   0,   0, -10,
   -10, -10, -10,  -5,  -5, -10, -10, -10
};

/// @brief Piece square table for kings in the mid game.
constexpr std::array<int, 64> KING_SQUARE_MIDGAME
{
   -40, -40, -40, -40, -40, -40, -40, -40,
   -40, -40, -40, -40, -40, -40, -40, -40,
   -40, -40, -40, -40, -40, -40, -40, -40,
   -40, -40, -40, -40, -40, -40, -40, -40,
   -40, -40, -40, -40, -40, -40, -40, -40,
   -40, -40, -40, -40, -40, -40, -40, -40,
   -20, -20, -20, -20, -20, -20, -20, -20,
    20,  40,  40, -20,   0, -20,  40,  20
};

/// @brief Piece square table for kings in the end game.
constexpr std::array<int, 64> KING_SQUARE_ENDGAME
{
     0,  10,  20,  30,  30,  20,  10,   0,
    10,  20,  30,  40,  40,  30,  20,  10,
    20,  30,  40,  50,  50,  40,  30,  20,
    30,  40,  50,  60,  60,  50,  40,  30,
    30,  40,  50,  60,  60,  50,  40,  30,
    20,  30,  40,  50,  50,  40,  30,  20,
    10,  20,  30,  40,  40,  30,  20,  10,
     0,  10,  20,  30,  30,  20,  10,   0
};

/// @brief Piece square table for passed pawn bonuses.
constexpr std::array<int, 64> PASSED_PAWN_SQUARE 
{
     0,   0,   0,   0,   0,   0,   0,   0,
    30,  30,  30,  30,  30,  30,  30,  30, // All pawns are "passed" at 7th rank
    30,  30,  30,  30,  30,  30,  30,  30,
    20,  20,  20,  20,  20,  20,  20,  20, 
    15,  15,  15,  15,  15,  15,  15,  15,
    10,  10,  10,  10,  10,  10,  10,  10, 
     5,   5,   5,   5,   5,   5,   5,   5, 
     0,   0,   0,   0,   0,   0,   0,   0,
};

/// @brief Scores for bishop and rook mobility based on number of squares attacked.
constexpr std::array<int, 14>     BISHOP_ATTACK_SCORES  {-20,-10, -5, -5,  0,  5, 10, 15, 20, 20, 20, 20, 20, 20    };
constexpr std::array<int, 15>     ROOK_ATTACK_SCORES    {-10,-10, -5, -5,  0,  5, 10, 15, 20, 20, 20, 20, 20, 20, 20};
constexpr std::array<Bitboard, 8> FILE_BITBOARDS        {FILE_A, FILE_B, FILE_C, FILE_D, FILE_E, FILE_F, FILE_G, FILE_H};

// clang-format on

/// @brief Pawn structure for one color.
struct PawnStructure
{
    Bitboard passed_pawns;   ///< A passed pawn has no enemy pawns whicn can prevent it promoting.
    Bitboard isolated_pawns; ///< An isolated pawn has no friendly pawns on either adjacent file.
    Bitboard backward_pawns; ///< A backward pawn has no pawns to support it.
    Bitboard doubled_pawns;  ///< A doubled pawn has a friendly pawn in front of it.
    Bitboard defended_pawns; ///< A defended pawn has a friendly pawn defending it.
    PawnStructure()
        : passed_pawns(NO_SQUARES), isolated_pawns(NO_SQUARES), backward_pawns(NO_SQUARES), doubled_pawns(NO_SQUARES),
          defended_pawns(NO_SQUARES)
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
    if constexpr (color == WHITE)
    {
        ps.defended_pawns = friendly_pawns & (friendly_pawns.ShiftNorthwest() | friendly_pawns.ShiftNortheast());
    }
    else
    {
        ps.defended_pawns = friendly_pawns & (friendly_pawns.ShiftSouthwest() | friendly_pawns.ShiftSoutheast());
    }
    for (Square s : friendly_pawns)
    {
        const Bitboard  b    = Bitboard(s);
        const PawnSets &sets = PAWN_SETS[color][s];
        if ((sets.passed_pawn_mask & enemy_pawns).IsEmpty())
        {
            ps.passed_pawns |= b;
        }
        if ((sets.doubled_pawn_mask & friendly_pawns).IsNotEmpty())
        {
            ps.doubled_pawns |= b;
        }
        if ((sets.isolated_pawn_mask & friendly_pawns).IsEmpty())
        {
            ps.isolated_pawns |= b;
        }
        if ((sets.supported_pawn_mask & friendly_pawns).IsEmpty())
        {
            if constexpr (color == WHITE)
            {
                const Bitboard enemy_pawn_attacks = enemy_pawns.ShiftSouthwest() | enemy_pawns.ShiftSoutheast();
                const Square   forward_locn       = (Square)(s + 8);
                if ((enemy_pawn_attacks & Bitboard(forward_locn)).IsNotEmpty())
                {
                    ps.backward_pawns |= b;
                }
            }
            else
            {
                const Bitboard enemy_pawn_attacks = enemy_pawns.ShiftNorthwest() | enemy_pawns.ShiftNortheast();
                const Square   forward_locn       = (Square)(s - 8);
                if ((enemy_pawn_attacks & Bitboard(forward_locn)).IsNotEmpty())
                {
                    ps.backward_pawns |= b;
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
    // clang-format off
    int score = pawns.PopCount()   *  100 + 
                knights.PopCount() *  400 + 
                bishops.PopCount() *  400 + 
                rooks.PopCount()   *  600 +
                queens.PopCount()  * 1200;
    // clang-format on
    // Bonus for the bishop pair.
    if ((bishops & WHITE_SQUARES).IsNotEmpty() && (bishops & BLACK_SQUARES).IsNotEmpty())
    {
        score += 50;
    }
    return score;
}

/// @brief Evaluate bishop and rook mobility scores
/// @tparam color color to evaluate
/// @param position current position
/// @return mobility score
template <Color color> int EvaluateMobility(const Position &position, const std::array<PawnStructure, 2> &ps)
{

    const Bitboard friendly_pieces  = position.PiecesOfColor(color);
    const Bitboard occupied_squares = position.OccupiedSquares();
    int            score            = 0;
    for (Square s : position.Bishops() & friendly_pieces)
    {
        Bitboard attacks = BishopAttacks(occupied_squares, s) & ~friendly_pieces;
        // Exclude pawns defended by a pawn from the bishop move targets, since they always result in the loss of the
        // bishop, and are a frequent cause of "blocked bishops".
        if constexpr (color == WHITE)
        {
            attacks &= ~ps[BLACK].defended_pawns;
        }
        else
        {
            attacks &= ~ps[WHITE].defended_pawns;
        }
        score += BISHOP_ATTACK_SCORES[attacks.PopCount()];
    }
    for (Square s : position.Rooks() & friendly_pieces)
    {
        const Bitboard attacks = RookAttacks(occupied_squares, s) & ~friendly_pieces;
        score += ROOK_ATTACK_SCORES[attacks.PopCount()];
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
    for (Square s : position.Pawns() & friendly_pieces)
    {
        score += PAWN_SQUARE[s ^ rank_flip];
    }
    for (Square s : position.Knights() & friendly_pieces)
    {
        score += KNIGHT_SQUARE[s ^ rank_flip];
    }
    for (Square s : position.Bishops() & friendly_pieces)
    {
        score += BISHOP_SQUARE[s ^ rank_flip];
    }
    for (Square s : position.Rooks() & friendly_pieces)
    {
        score += ROOK_SQUARE[s ^ rank_flip];
    }
    for (Square s : position.Queens() & friendly_pieces)
    {
        score += QUEEN_SQUARE[s ^ rank_flip];
    }
    return score;
}

/// @brief Evaluate pawn structure
/// @tparam color color to evaluate
/// @param ps pawn structure
/// @return score for pawn structure
template <Color color> int EvaluatePawnStructure(const PawnStructure &ps)
{
    constexpr uint8_t rank_flip = (color == WHITE ? RANK_FLIP : 0);
    int               score     = 0;
    for (Square s : ps.passed_pawns)
    {
        score += PASSED_PAWN_SQUARE[s ^ rank_flip];
    }
    score += ps.defended_pawns.PopCount() * 5;
    score -= ps.backward_pawns.PopCount() * 10;
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
    const Bitboard    friendly_pieces = position.PiecesOfColor(color);
    const Bitboard    enemy_pieces    = position.PiecesOfColor(EnemyOf(color));
    const Bitboard    friendly_pawns  = friendly_pieces & position.Pawns();
    const Bitboard    enemy_pawns     = enemy_pieces & position.Pawns();
    const bool        is_endgame =
        position.Queens().IsEmpty() && (position.OccupiedSquares() ^ position.Pawns()).PopCount() < 8;

    const int piece_square_score = is_endgame ? KING_SQUARE_ENDGAME[position.KingLocation(color) ^ rank_flip]
                                              : KING_SQUARE_MIDGAME[position.KingLocation(color) ^ rank_flip];

    int safety_score = 0;
    if (!is_endgame)
    {
        // Consider pawns in front of the king and approaching enemy pawns.
        const Bitboard pawn_shelter_1 = SETS[position.KingLocation(color)].KingPawnShelter(color);
        Bitboard       pawn_shelter_2;
        Bitboard       pawn_shelter_3;
        if constexpr (color == WHITE)
        {
            pawn_shelter_2 = pawn_shelter_1.ShiftNorth();
            pawn_shelter_3 = pawn_shelter_2.ShiftNorth();
        }
        else
        {
            pawn_shelter_2 = pawn_shelter_1.ShiftSouth();
            pawn_shelter_3 = pawn_shelter_2.ShiftSouth();
        }
        // clang-format off
        safety_score =
            15 * (friendly_pawns & pawn_shelter_1).PopCount() + 
            10 * (friendly_pawns & pawn_shelter_2).PopCount() +
             5 * (friendly_pawns & pawn_shelter_3).PopCount() - 
            15 * (enemy_pawns & pawn_shelter_1).PopCount() -
            10 * (enemy_pawns & pawn_shelter_2).PopCount() - 
             5 * (enemy_pawns & pawn_shelter_3).PopCount();
        // clang-format on

        // Penalty for open files near our king.
        const Bitboard king_file = FILE_BITBOARDS[FileOf(position.KingLocation(color))];
        if ((king_file & friendly_pawns).IsEmpty())
        {
            safety_score -= 15;
        }
        Bitboard adjacent_file = king_file.ShiftWest();
        if (adjacent_file.IsNotEmpty() && (adjacent_file & friendly_pawns).IsEmpty())
        {
            safety_score -= 10;
        }
        adjacent_file = king_file.ShiftEast();
        if (adjacent_file.IsNotEmpty() && (adjacent_file & friendly_pawns).IsEmpty())
        {
            safety_score -= 10;
        }
    }
    // Bonus for friendly pieces near to our king
    const Bitboard adjacent1 = SETS[position.KingLocation(color)].king_attacks;
    const Bitboard adjacent2 = SETS[position.KingLocation(color)].king_attacks2 ^ adjacent1;
    const Bitboard non_pawns = friendly_pieces ^ friendly_pawns;
    safety_score += 10 * (adjacent1 & non_pawns).PopCount() + 5 * (adjacent2 & non_pawns).PopCount();
    return piece_square_score + safety_score;
}
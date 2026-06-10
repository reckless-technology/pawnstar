#pragma once
/// @file evaluation.h Data and functions to evaluate a chess position.

#include <array>
#include <cstdint>

#include "position.h"

class SearchState;

/// @brief Evaluate the current position from the side-to-move's perspective (see definition for details).
int EvaluatePosition(const SearchState &state, int alpha, int beta);

// clang-format off

/// @brief Piece square table for pawns.
constexpr std::array<int, 64> kPawnSquare
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
constexpr std::array<int, 64> kKnightSquare
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
constexpr std::array<int, 64> kBishopSquare
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
constexpr std::array<int, 64> kRookSquare
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
constexpr std::array<int, 64> kQueenSquare
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
constexpr std::array<int, 64> kKingSquareMidgame
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
constexpr std::array<int, 64> kKingSquareEndgame
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
constexpr std::array<int, 64> kPassedPawnSquare 
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

/// @brief Bishop mobility scores indexed by number of squares attacked.
constexpr std::array<int, 14>     kBishopAttackScores   {-20,-10, -5, -5,  0,  5, 10, 15, 20, 20, 20, 20, 20, 20    };
/// @brief Rook mobility scores indexed by number of squares attacked.
constexpr std::array<int, 15>     kRookAttackScores     {-10,-10, -5, -5,  0,  5, 10, 15, 20, 20, 20, 20, 20, 20, 20};
/// @brief Bitboard of each file, indexed by file number (0 = a-file).
constexpr std::array<Bitboard, 8> kFileBitboards        {kFileA, kFileB, kFileC, kFileD, kFileE, kFileF, kFileG, kFileH};

// clang-format on

/// @brief Pawn structure for one color.
struct PawnStructure
{
    Bitboard passed_pawns;      ///< A passed pawn has no enemy pawns whicn can prevent it promoting.
    Bitboard isolated_pawns;    ///< An isolated pawn has no friendly pawns on either adjacent file.
    Bitboard unsupported_pawns; ///< An unsupported pawn has no friendly pawns in its support window.
    Bitboard doubled_pawns;     ///< A doubled pawn has a friendly pawn in front of it.
    Bitboard defended_pawns;    ///< A defended pawn has a friendly pawn defending it.
    /// @brief Construct with all pawn-structure bitboards empty.
    PawnStructure()
        : passed_pawns(kNoSquares), isolated_pawns(kNoSquares), unsupported_pawns(kNoSquares),
          doubled_pawns(kNoSquares), defended_pawns(kNoSquares)
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
    // clang-format off
    const std::array<Bitboard, 64> &passed_pawn_mask    = color == kWhite ? kPassedPawnMaskWhite    : kPassedPawnMaskBlack;
    const std::array<Bitboard, 64> &isolated_pawn_mask  = color == kWhite ? kIsolatedPawnMaskWhite  : kIsolatedPawnMaskBlack;
    const std::array<Bitboard, 64> &supported_pawn_mask = color == kWhite ? kSupportedPawnMaskWhite : kSupportedPawnMaskBlack;
    const std::array<Bitboard, 64> &doubled_pawn_mask   = color == kWhite ? kDoubledPawnMaskWhite   : kDoubledPawnMaskBlack;
    // clang-format on
    if constexpr (color == kWhite)
    {
        ps.defended_pawns = friendly_pawns & (friendly_pawns.ShiftNorthwest() | friendly_pawns.ShiftNortheast());
    }
    else
    {
        ps.defended_pawns = friendly_pawns & (friendly_pawns.ShiftSouthwest() | friendly_pawns.ShiftSoutheast());
    }
    for (Square s : friendly_pawns)
    {
        const Bitboard b = Bitboard(s);
        if ((passed_pawn_mask[s] & enemy_pawns).IsEmpty())
        {
            ps.passed_pawns |= b;
        }
        if ((doubled_pawn_mask[s] & friendly_pawns).IsNotEmpty())
        {
            ps.doubled_pawns |= b;
        }
        if ((isolated_pawn_mask[s] & friendly_pawns).IsEmpty())
        {
            ps.isolated_pawns |= b;
        }
        if ((supported_pawn_mask[s] & friendly_pawns).IsEmpty())
        {
            ps.unsupported_pawns |= b;
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
    if ((bishops & kWhiteSquares).IsNotEmpty() && (bishops & kBlackSquares).IsNotEmpty())
    {
        score += 50;
    }
    return score;
}

/// @brief Evaluate bishop and rook mobility scores
/// @tparam color color to evaluate
/// @param position current position
/// @param ps Pawn structure for both colors (used to exclude pawn-defended targets).
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
        if constexpr (color == kWhite)
        {
            attacks &= ~ps[kBlack].defended_pawns;
        }
        else
        {
            attacks &= ~ps[kWhite].defended_pawns;
        }
        score += kBishopAttackScores[attacks.PopCount()];
    }
    for (Square s : position.Rooks() & friendly_pieces)
    {
        const Bitboard attacks = RookAttacks(occupied_squares, s) & ~friendly_pieces;
        score += kRookAttackScores[attacks.PopCount()];
    }
    return score;
}

/// @brief Evaluate piece square scores excluding the king.
/// @tparam color Color to evaluate for
/// @param position Position
/// @return score
template <Color color> int EvaluatePieceSquare(const Position &position)
{
    constexpr uint8_t rank_flip       = (color == kWhite ? kRankFlip : 0);
    const Bitboard    friendly_pieces = position.PiecesOfColor(color);
    int               score           = 0;
    for (Square s : position.Pawns() & friendly_pieces)
    {
        score += kPawnSquare[s ^ rank_flip];
    }
    for (Square s : position.Knights() & friendly_pieces)
    {
        score += kKnightSquare[s ^ rank_flip];
    }
    for (Square s : position.Bishops() & friendly_pieces)
    {
        score += kBishopSquare[s ^ rank_flip];
    }
    for (Square s : position.Rooks() & friendly_pieces)
    {
        score += kRookSquare[s ^ rank_flip];
    }
    for (Square s : position.Queens() & friendly_pieces)
    {
        score += kQueenSquare[s ^ rank_flip];
    }
    return score;
}

/// @brief Evaluate pawn structure
/// @tparam color color to evaluate
/// @param ps pawn structure
/// @return score for pawn structure
template <Color color> int EvaluatePawnStructure(const PawnStructure &ps)
{
    constexpr uint8_t rank_flip = (color == kWhite ? kRankFlip : 0);
    int               score     = 0;
    for (Square s : ps.passed_pawns)
    {
        score += kPassedPawnSquare[s ^ rank_flip];
    }
    score += ps.defended_pawns.PopCount() * 5;
    score -= ps.unsupported_pawns.PopCount() * 10;
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
    constexpr uint8_t rank_flip       = (color == kWhite ? kRankFlip : 0);
    const Bitboard    friendly_pieces = position.PiecesOfColor(color);
    const Bitboard    enemy_pieces    = position.PiecesOfColor(EnemyOf(color));
    const Bitboard    friendly_pawns  = friendly_pieces & position.Pawns();
    const Bitboard    enemy_pawns     = enemy_pieces & position.Pawns();
    const bool        is_endgame =
        position.Queens().IsEmpty() && (position.OccupiedSquares() ^ position.Pawns()).PopCount() < 8;

    const int piece_square_score = is_endgame ? kKingSquareEndgame[position.KingLocation(color) ^ rank_flip]
                                              : kKingSquareMidgame[position.KingLocation(color) ^ rank_flip];

    int safety_score = 0;
    if (!is_endgame)
    {
        // Consider pawns in front of the king and approaching enemy pawns.
        const Bitboard pawn_shelter_1 = color == kWhite ? kKingPawnShelterWhite[position.KingLocation(color)]
                                                        : kKingPawnShelterBlack[position.KingLocation(color)];
        Bitboard       pawn_shelter_2, pawn_shelter_3;
        if constexpr (color == kWhite)
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
        const Bitboard king_file = kFileBitboards[position.KingLocation(color).File()];
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
    const Bitboard adjacent1 = kKingAttacks[position.KingLocation(color)];
    const Bitboard adjacent2 = kKingAttacks2[position.KingLocation(color)] ^ adjacent1;
    const Bitboard non_pawns = friendly_pieces ^ friendly_pawns;
    safety_score += 10 * (adjacent1 & non_pawns).PopCount() + 5 * (adjacent2 & non_pawns).PopCount();
    return piece_square_score + safety_score;
}
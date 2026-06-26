#pragma once
/// @file constants.h Program configuration constants and core enums.

#include <cstdint>

/// @brief Chess pieces.
enum Piece : uint8_t
{
    kNone,   ///< No piece / empty square.
    kPawn,   ///< Pawn.
    kKnight, ///< Knight.
    kBishop, ///< Bishop.
    kRook,   ///< Rook.
    kQueen,  ///< Queen.
    kKing,   ///< King.
};

/// @brief Piece colors.
enum Color : uint8_t
{
    kWhite, ///< White pieces / side.
    kBlack, ///< Black pieces / side.
};

/// @brief Return the enemy of a color.
/// @param color the color
/// @return The opposite color
constexpr Color EnemyOf(Color color)
{
    return color == Color::kWhite ? Color::kBlack : Color::kWhite;
}

constexpr int kHashtableMegabytes  = 64;  ///< default transposition table size in MB
constexpr int kEvalCacheMb         = 8;   ///< NNUE evaluation cache size in MB (lockless)
constexpr int kMaxMovesPerPosition = 256; ///< maximum possible number of moves for a chess position
// Score band (all comfortably within int16, so eval-cache packing and mate scores never overflow):
//   |eval| <= kMaxEvaluation (30000)  <  kWinThreshold (31000)  <  |mate| in [31836,31900]  <  kBeta (32000)  <  32767.
constexpr int kBeta                      = 32000;  ///< greater than any possible evaluation score
constexpr int kAlpha                     = -32000; ///< smaller than any possible evaluation score
constexpr int kCheckmatedScore           = -31900; ///< score for losing the game (mate scores = this + ply)
constexpr int kWinThreshold              = 31000;  ///< score threshold for winning at any ply (above any clamped eval)
constexpr int kLoseThreshold             = -31000; ///< score threshold for losing at any ply
constexpr int kMaxEvaluation             = 30000;  ///< NNUE eval clamp magnitude; keeps |eval| below kWinThreshold
constexpr int kDrawScore                 = 0;      ///< score for achieving a draw
constexpr int kMaxPly                    = 64;     ///< terminate the search at this ply no matter what
constexpr int kRankFlip                  = 0x38;   ///< used as an XOR mask to flip the board horizontally
constexpr int kSearchCancelledScore      = -12345678; ///< illegal value returned when search is cancelled
constexpr int kScoreInstabilityThreshold = 50;        ///< window of score variability
constexpr int kStartDepth                = 3;         ///< depth to do full width search
constexpr int kRfpMaxDepth               = 7;         ///< reverse-futility pruning: max depth to apply it
constexpr int kRfpMargin                 = 80;        ///< reverse-futility pruning: centipawn margin per ply
constexpr int kLmpMaxDepth               = 8;         ///< late-move (move-count) pruning: max depth to apply it
constexpr int kLmpBase                   = 3;         ///< quiet skipped once move index >= kLmpBase + depth*depth
constexpr int kNoTTReduceMinDepth        = 4;         ///< min depth at which a node lacking a TT move is reduced
constexpr int kMegabyte                  = 1 << 20;   ///< An old school megabyte ;-)
constexpr int kMaxSearchThreads          = 256;       ///< upper bound on Lazy SMP search threads
constexpr int kContKeys                  = 7 * 64;    ///< Index by [piece, to] for continuation scores.
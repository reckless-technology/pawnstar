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

constexpr int kHashtableMegabytes        = 64;        ///< default transposition table size in MB
constexpr int kEvalCacheMb               = 8;         ///< NNUE evaluation cache size in MB (lockless)
constexpr int kMaxMovesPerPosition       = 256;       ///< maximum possible number of moves for a chess position
constexpr int kBeta                      = 11000;     ///< greater than any possible evaluation score
constexpr int kAlpha                     = -11000;    ///< smaller than any possible evaluation score
constexpr int kCheckmatedScore           = -10000;    ///< score for losing the game
constexpr int kWinThreshold              = 9000;      ///< score threshold for winning at any ply
constexpr int kLoseThreshold             = -9000;     ///< score threshold for losing at any ply
constexpr int kDrawScore                 = 0;         ///< score for achieving a draw
constexpr int kMaxPly                    = 64;        ///< terminate the search at this ply no matter what
constexpr int kRankFlip                  = 0x38;      ///< used as an XOR mask to flip the board horizontally
constexpr int kSearchCancelledScore      = -12345678; ///< illegal value returned when search is cancelled
constexpr int kScoreInstabilityThreshold = 50;        ///< window of score variability
constexpr int kStartDepth                = 3;         ///< depth to do full width search
constexpr int kRfpMaxDepth               = 7;         ///< reverse-futility pruning: max depth to apply it
constexpr int kRfpMargin                 = 80;        ///< reverse-futility pruning: centipawn margin per ply
constexpr int kMegabyte                  = 1 << 20;
constexpr int kMaxSearchThreads          = 256;       ///< upper bound on Lazy SMP search threads
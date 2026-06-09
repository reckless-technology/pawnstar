#pragma once
/// @file Program configuration constants.

constexpr int kHashtableMegabytes        = 64;        ///< default transposition table size in MB
constexpr int kQHashtableMb              = 8;         ///< quiescent hash table default size
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
constexpr int kMegabyte                  = 1 << 20;
#pragma once
#include <stdint.h>
/**
 * @file Functions for constructing and using chess moves.
 *
 * Moves are contained within a 64 bit integer.
 *
 *   BITS   INTERPRETATION
 *  0 -  5  To (destination square index)
 *  6 - 11  From (source square index)
 * 12 - 14  Moving piece type
 * 15 - 17  Captured piece type in the case of capture moves
 * 18 - 20  Promoted piece type in the case of pawn promotions
 * 21 - 21  Castling move flag
 * 22 - 22  En passant capture flag
 * 23 - 23  Pawn double push flag
 * 32 - 63  May contain move score (as signed 32 bit int) 
 *          after move has been evaluated / scored
 * 
 * A value of 0 terminates a move list.
*/

typedef int64_t Move;

/**
 * @brief Chess pieces.
*/
enum Piece
{
    NO_PIECE,
    PAWN,
    KNIGHT,
    BISHOP,
    ROOK,
    QUEEN,
    KING,
};

/**
 * @brief Chess piece colors.
*/
enum Color
{
    WHITE,
    BLACK,
    NEITHER_COLOR,
};

constexpr Move PromotionMove    (int from, int to, int captured, int promoted)  { return (to|(from<<6)|(PAWN <<12)|(captured<<15)|(promoted<<18)); }
constexpr Move CastlingMove     (int from, int to)                              { return (to|(from<<6)|(KING <<12)|               (1<<21));        }
constexpr Move EpCaptureMove    (int from, int to)                              { return (to|(from<<6)|(PAWN <<12)|(PAWN    <<15)|(1<<22));        }
constexpr Move DoublePushMove   (int from, int to)                              { return (to|(from<<6)|(PAWN <<12)|               (1<<23));        }
constexpr Move CaptureMove      (int from, int to, int piece, int captured)     { return (to|(from<<6)|(piece<<12)|(captured<<15));                }
constexpr Move NonCaptureMove   (int from, int to, int piece)                   { return (to|(from<<6)|(piece<<12));                               }

constexpr uint8_t   MoveTo(Move m)                  { return  m        & 0x3F;      }
constexpr uint8_t   MoveFrom(Move m)                { return (m >>  6) & 0x3F;      }
constexpr uint8_t   MovePiece(Move m)               { return (m >> 12) & 0x07;      }
constexpr uint8_t   MoveCaptured(Move m)            { return (m >> 15) & 0x07;      }
constexpr uint8_t   MovePromoted(Move m)            { return (m >> 18) & 0x07;      }
constexpr bool      IsCastlingMove(Move m)          { return  m        & (1 << 21); }
constexpr bool      IsEpCaptureMove(Move m)         { return  m        & (1 << 22); }
constexpr bool      IsPawnDoublePushMove(Move m)    { return  m        & (1 << 23); }
constexpr int       MoveScore(Move m)               { return  (int)(m >> 32);       }
constexpr uint32_t  MoveBits(Move m)                { return  m & 0xFFFFFFFF;       }
constexpr Move      ScoredMove(Move m, int score)   { return (m & 0xFFFFFFFF) | ((int64_t)score << 32); }
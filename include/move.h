#pragma once
#include <cstdint>
#include <vector>

#include "constants.h"
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
 * 24 - 24  Is checking move flag (move gives check)
 * 32 - 63  Contains the move score (as signed 32 bit int)
 *          after move has been evaluated / sorted
 *
 * A value of 0 terminates a move list.
 */

/* clang-format off */

typedef int64_t Move;

enum Piece : uint8_t
{
    NONE,
    PAWN,
    KNIGHT,
    BISHOP,
    ROOK,
    QUEEN,
    KING,
};

enum Color : uint8_t
{
    WHITE,
    BLACK,
    NEITHER_COLOR,
};

enum Square : uint8_t
{
    A1, B1, C1, D1, E1, F1, G1, H1,
    A2, B2, C2, D2, E2, F2, G2, H2,
    A3, B3, C3, D3, E3, F3, G3, H3,
    A4, B4, C4, D4, E4, F4, G4, H4,
    A5, B5, C5, D5, E5, F5, G5, H5,
    A6, B6, C6, D6, E6, F6, G6, H6,
    A7, B7, C7, D7, E7, F7, G7, H7,
    A8, B8, C8, D8, E8, F8, G8, H8,
    NO_SQUARE = 0,
};

constexpr Move PromotionMove (Square from, Square to, Piece captured, Piece promoted)   { return to|(from<<6)|(PAWN <<12)|(captured<<15)|(promoted<<18);    }
constexpr Move CastlingMove  (Square from, Square to)                                   { return to|(from<<6)|(KING <<12)|               (1<<21);           }
constexpr Move EpCaptureMove (Square from, Square to)                                   { return to|(from<<6)|(PAWN <<12)|(PAWN    <<15)|(1<<22);           }
constexpr Move DoublePushMove(Square from, Square to)                                   { return to|(from<<6)|(PAWN <<12)|               (1<<23);           }
constexpr Move CaptureMove   (Square from, Square to, Piece piece, Piece captured)      { return to|(from<<6)|(piece<<12)|(captured<<15);                   }
constexpr Move NonCaptureMove(Square from, Square to, Piece piece)                      { return to|(from<<6)|(piece<<12);                                  }

constexpr Square    MoveTo(Move m)                          { return (Square) (m        & 0x3F);                            }
constexpr Square    MoveFrom(Move m)                        { return (Square)((m >>  6) & 0x3F);                            }
constexpr Piece     MovePiece(Move m)                       { return (Piece) ((m >> 12) & 0x07);                            }
constexpr Piece     MoveCaptured(Move m)                    { return (Piece) ((m >> 15) & 0x07);                            }
constexpr Piece     MovePromoted(Move m)                    { return (Piece) ((m >> 18) & 0x07);                            }
constexpr bool      IsCastlingMove(Move m)                  { return  m & (1 << 21);                                        }
constexpr bool      IsEpCaptureMove(Move m)                 { return  m & (1 << 22);                                        }
constexpr bool      IsPawnDoublePushMove(Move m)            { return  m & (1 << 23);                                        }
constexpr bool      IsCheckingMove(Move m)                  { return  m & (1 << 24);                                        }
constexpr int       MoveScore(Move m)                       { return  (int)(m >> 32);                                       }
constexpr uint32_t  MoveBits(Move m)                        { return  m & 0x00FFFFFF;                                       }
constexpr Move      ScoredMove(Move m, int score)           { return (m & 0x00FFFFFF) |             ((int64_t)score << 32); }
constexpr Move      ScoredCheckingMove(Move m, int score)   { return (m & 0x00FFFFFF) | (1 << 24) | ((int64_t)score << 32); }

typedef std::vector<Move> Variation;
typedef std::vector<Move> MoveList;

/* clang-format on */

constexpr void CopyVariation(Variation &dst, const Variation &src, Move best_move)
{
    dst.clear();
    dst.push_back(best_move);
    dst.insert(dst.end(), src.begin(), src.end());
}

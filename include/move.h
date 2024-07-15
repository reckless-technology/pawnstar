#pragma once
#include <cstdint>
#include <vector>

#include "constants.h"
#include "stack_list.h"
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

/* clang-format off */
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
/* clang-format on */

class Move
{
  public:
    Move()
    {
    }

    Move(const Move &that) : m(that.m)
    {
    }

    Move &operator=(const Move &that)
    {
        m = that.m;
        return *this;
    }

    bool operator==(const Move &that) const
    {
        return (m & 0xFFFFFFFF) == (that.m & 0xFFFFFFFF);
    }

    Piece piece() const
    {
        return (Piece)((m >> 12) & 0x07);
    }

    Square from() const
    {
        return (Square)((m >> 6) & 0x3F);
    }

    Square to() const
    {
        return (Square)(m & 0x3F);
    }

    Piece captured() const
    {
        return (Piece)((m >> 15) & 0x07);
    }

    Piece promoted() const
    {
        return (Piece)((m >> 18) & 0x07);
    }

    int score() const
    {
        return (int)(m >> 32);
    }

    bool IsCastlingMove() const
    {
        return m & IS_CASTLING;
    }

    bool IsEpCaptureMove() const
    {
        return m & IS_EP_CAPTURE;
    }

    bool IsPawnDoublePushMove() const
    {
        return m & IS_DOUBLE_PUSH;
    }

    bool IsCheckingMove() const
    {
        return m & IS_CHECKING;
    }

    void AssignScore(int score)
    {
        m = (m & 0xFFFFFFFF) | ((int64_t)score << 32);
    }

    void GivesCheck()
    {
        m |= IS_CHECKING;
    }

    operator bool() const
    {
        return m != 0;
    }

    static Move NoMove()
    {
        return Move{0};
    }

    static Move PromotionMove(Square from, Square to, Piece captured, Piece promoted)
    {
        return Move{to | (from << 6) | (PAWN << 12) | (captured << 15) | (promoted << 18)};
    }

    static Move CastlingMove(Square from, Square to)
    {
        return Move{to | (from << 6) | (KING << 12) | IS_CASTLING};
    }

    static Move EpCaptureMove(Square from, Square to)
    {
        return Move{to | (from << 6) | (PAWN << 12) | (PAWN << 15) | IS_EP_CAPTURE};
    }

    static Move DoublePushMove(Square from, Square to)
    {
        return Move{to | (from << 6) | (PAWN << 12) | IS_DOUBLE_PUSH};
    }

    static Move CaptureMove(Square from, Square to, Piece piece, Piece captured)
    {
        return Move{to | (from << 6) | (piece << 12) | (captured << 15)};
    }

    static Move NonCaptureMove(Square from, Square to, Piece piece)
    {
        return Move{to | (from << 6) | (piece << 12)};
    }

    std::size_t operator()(const Move &m) const
    {
        return std::hash<int64_t>()(m.m);
    }

  private:
    int64_t m;

    Move(int64_t v) : m(v)
    {
    }

    enum MoveFlags
    {
        IS_CASTLING    = 1 << 21,
        IS_EP_CAPTURE  = 1 << 22,
        IS_DOUBLE_PUSH = 1 << 23,
        IS_CHECKING    = 1 << 24,
    };
};

typedef std::vector<Move>       Variation;
typedef StackVector<Move, 1024> MoveList;

static inline void CopyVariation(Variation &dst, const Variation &src, Move best_move)
{
    dst.clear();
    dst.push_back(best_move);
    dst.insert(dst.end(), src.begin(), src.end());
}

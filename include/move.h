#pragma once
#include <cstdint>
#include <vector>

#include "constants.h"
#include "stack_vector.h"
/**
 * @file Functions for constructing and using chess moves.
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

constexpr Color EnemyOf(Color color)
{
    return color == WHITE ? BLACK : WHITE;
}

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

constexpr uint8_t FileOf(Square locn)
{
    return locn & 7;
}
constexpr uint8_t RankOf(Square locn)
{
    return locn >> 3;
}
constexpr char FileChar(Square locn)
{
    return 'a' + FileOf(locn);
}
constexpr char RankChar(Square locn)
{
    return '1' + RankOf(locn);
}

/**
 * @brief Class for containing a chess move.
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
class Move
{
  public:
    constexpr Move()
    {
    }

    constexpr Move(const Move &that) : m(that.m)
    {
    }

    constexpr Move &operator=(const Move &that)
    {
        m = that.m;
        return *this;
    }

    constexpr bool operator==(const Move &that) const
    {
        return (m & 0xFFFFFFFF) == (that.m & 0xFFFFFFFF); // Ignore score when comparing moves.
    }

    constexpr Piece piece() const
    {
        return (Piece)((m >> 12) & 0x07);
    }

    constexpr Square from() const
    {
        return (Square)((m >> 6) & 0x3F);
    }

    constexpr Square to() const
    {
        return (Square)(m & 0x3F);
    }

    constexpr Piece captured() const
    {
        return (Piece)((m >> 15) & 0x07);
    }

    constexpr Piece promoted() const
    {
        return (Piece)((m >> 18) & 0x07);
    }

    constexpr int score() const
    {
        return (int)(m >> 32);
    }

    constexpr bool IsCastlingMove() const
    {
        return m & IS_CASTLING;
    }

    constexpr bool IsEpCaptureMove() const
    {
        return m & IS_EP_CAPTURE;
    }

    constexpr bool IsPawnDoublePushMove() const
    {
        return m & IS_DOUBLE_PUSH;
    }

    constexpr bool IsCheckingMove() const
    {
        return m & IS_CHECKING;
    }

    constexpr void AssignScore(int score)
    {
        m = (m & 0xFFFFFFFF) | ((int64_t)score << 32);
    }

    constexpr void GivesCheck()
    {
        m |= IS_CHECKING;
    }

    constexpr operator bool() const
    {
        return m != 0;
    }

    constexpr static Move NoMove()
    {
        return Move{0};
    }

    constexpr static Move PromotionMove(Square from, Square to, Piece captured, Piece promoted)
    {
        return Move{to | (from << 6) | (PAWN << 12) | (captured << 15) | (promoted << 18)};
    }

    constexpr static Move CastlingMove(Square from, Square to)
    {
        return Move{to | (from << 6) | (KING << 12) | IS_CASTLING};
    }

    constexpr static Move EpCaptureMove(Square from, Square to)
    {
        return Move{to | (from << 6) | (PAWN << 12) | (PAWN << 15) | IS_EP_CAPTURE};
    }

    constexpr static Move DoublePushMove(Square from, Square to)
    {
        return Move{to | (from << 6) | (PAWN << 12) | IS_DOUBLE_PUSH};
    }

    constexpr static Move CaptureMove(Square from, Square to, Piece piece, Piece captured)
    {
        return Move{to | (from << 6) | (piece << 12) | (captured << 15)};
    }

    constexpr static Move NonCaptureMove(Square from, Square to, Piece piece)
    {
        return Move{to | (from << 6) | (piece << 12)};
    }

    constexpr std::size_t operator()(const Move &m) const
    {
        return (std::size_t)m; // Used for hashing moves in std containers.
    }

  private:
    int64_t m;

    constexpr Move(int64_t v) : m(v)
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

typedef StackVector<Move, MAX_PLY>                Variation;
typedef StackVector<Move, MAX_MOVES_PER_POSITION> MoveList;

static inline void CopyVariation(Variation &dst, const Variation &src, Move best_move)
{
    dst.clear();
    dst.push_back(best_move);
    for (auto move : src)
    {
        dst.push_back(move);
    }
}

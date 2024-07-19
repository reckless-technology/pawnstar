#pragma once
/// @file Types and functions for constructing and using chess pieces, colors and moves on the board.

#include <cstdint>
#include <string>

#include "constants.h"
#include "stack_vector.h"

/// @brief Chess pieces.
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

/// @brief Piece colors.
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

// clang-format off
/// @brief Square indices.
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
// clang-format on

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

///
/// @brief Class for representing a chess move.
/// Moves are represented by a 64 bit integer with the following bit fields:
///   BITS      INTERPRETATION
///  0 -  5     To (destination square index)
///  6 - 11     From (source square index)
/// 12 - 14     Moving piece type
/// 15 - 17     Captured piece type in the case of capture moves
/// 18 - 20     Promoted piece type in the case of pawn promotions
/// 21 - 21     Castling move flag
/// 22 - 22     En passant capture flag
/// 23 - 23     Pawn double push flag
/// 24 - 24     Is checking move flag (this move gives check)
/// 32 - 63     Contains the move score (as signed int) after move has been evaluated / sorted
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
        // Just consider from, to, piece, captured, promoted fields when testing moves for equality.
        return (m & 0x1FFFFF) == (that.m & 0x1FFFFF);
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

    constexpr int piece_from_to_mask() const
    {
        return m & 0x7FFF; // Lower 15 bits contain (piece, from, to): index into killer move table.
    }

    constexpr bool IsCastling() const
    {
        return m & IS_CASTLING;
    }

    constexpr bool IsEpCapture() const
    {
        return m & IS_EP_CAPTURE;
    }

    constexpr bool IsPawnDoublePush() const
    {
        return m & IS_DOUBLE_PUSH;
    }

    constexpr bool IsChecking() const
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

    constexpr static Move None()
    {
        return Move{0};
    }

    constexpr static Move PromotionCaptureMove(Square from, Square to, Piece captured, Piece promoted)
    {
        return Move{to | (from << 6) | (PAWN << 12) | (captured << 15) | (promoted << 18)};
    }

    constexpr static Move PromotionMove(Square from, Square to, Piece promoted)
    {
        return Move{to | (from << 6) | (PAWN << 12) | (promoted << 18)};
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

    constexpr const std::string ToString() const
    {
        std::string result;
        result.push_back(FileChar(from()));
        result.push_back(RankChar(from()));
        result.push_back(FileChar(to()));
        result.push_back(RankChar(to()));
        if (promoted() != NONE)
        {
            result.push_back(" pnbrqk"[promoted()]);
        }
        return result;
    }

    constexpr std::string DebugString() const
    {
        std::string result;
        result.push_back(" PNBRQK"[piece()]);
        result.push_back(FileChar(from()));
        result.push_back(RankChar(from()));
        result.push_back(FileChar(to()));
        result.push_back(RankChar(to()));
        if (captured())
        {
            result.push_back('x');
            result.push_back(" PNBRQK"[captured()]);
        }
        if (promoted())
        {
            result.push_back('=');
            result.push_back(" PNBRQK"[promoted()]);
        }
        if (IsCastling())
        {
            result += " O-O";
        }
        if (IsEpCapture())
        {
            result += " e.p.";
        }
        if (IsPawnDoublePush())
        {
            result += " (dp)";
        }
        if (IsChecking())
        {
            result.push_back('+');
        }
        return result;
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

typedef StackVector<Move, MAX_MOVES_PER_POSITION> MoveList;
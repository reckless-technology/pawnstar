#pragma once
/// @file Types and functions for constructing and using chess pieces, colors and moves on the board.

#include <cstdint>
#include <string>

#include "constants.h"
#include "stack_vector.h"

/// @brief Chess pieces.
enum Piece : uint8_t
{
    NO_PIECE,
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
};

/// @brief Return the enemy of a color
/// @param color the color
/// @return The opposite color
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

/// @brief Class for representing a chess move.
/// Moves are represented by a 64 bit integer with the following bit fields:
///   BITS      INTERPRETATION
///  0 -  5     To (destination square index)
///  6 - 11     From (source square index)
/// 12 - 14     MoveType
/// 15 - 15     Is checking flag (move gives check)
/// 32 - 63     score
class Move
{
  public:
    enum MoveType : uint8_t
    {
        REGULAR,
        PAWN_DOUBLE_PUSH,
        PROMOTION_KNIGHT = KNIGHT,
        PROMOTION_BISHOP = BISHOP,
        PROMOTION_ROOK   = ROOK,
        PROMOTION_QUEEN  = QUEEN,
        EP_CAPTURE,
        CASTLING,
    };

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
        /// Only consider from, to and type fields for equality.
        return (m & 0x7FFF) == (that.m & 0x7FFF);
    }

    constexpr Square to() const
    {
        return (Square)(m & 0x3F);
    }

    constexpr Square from() const
    {
        return (Square)((m >> 6) & 0x3F);
    }

    constexpr MoveType type() const
    {
        return (MoveType)((m >> 12) & 0x07);
    }

    constexpr Piece promoted() const
    {
        switch (type())
        {
        case REGULAR:
        case PAWN_DOUBLE_PUSH:
        case CASTLING:
        case EP_CAPTURE:
            return NO_PIECE;
        case PROMOTION_KNIGHT:
            return KNIGHT;
        case PROMOTION_BISHOP:
            return BISHOP;
        case PROMOTION_ROOK:
            return ROOK;
        case PROMOTION_QUEEN:
            return QUEEN;
        }
    }

    constexpr int score() const
    {
        return (int)(m >> 32);
    }

    constexpr int from_to_mask() const
    {
        // Lower 12 bits contain (from, to)
        return m & 0xFFF;
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

    constexpr static Move Regular(Square from, Square to)
    {
        return Move{to | (from << 6)};
    }

    constexpr static Move Promotion(Square from, Square to, Piece promoted)
    {
        return Move{to | (from << 6) | (promoted << 12)};
    }

    constexpr static Move Castling(Square from, Square to)
    {
        return Move{to | (from << 6) | (CASTLING << 12)};
    }

    constexpr static Move EpCapture(Square from, Square to)
    {
        return Move{to | (from << 6) | (EP_CAPTURE << 12)};
    }

    constexpr static Move DoublePush(Square from, Square to)
    {
        return Move{to | (from << 6) | (PAWN_DOUBLE_PUSH << 12)};
    }

    constexpr std::size_t operator()(const Move &move) const
    {
        return (std::size_t)move.m; // Used for hashing moves in std containers.
    }

    constexpr const std::string ToString() const
    {
        std::string result;
        result.push_back(FileChar(from()));
        result.push_back(RankChar(from()));
        result.push_back(FileChar(to()));
        result.push_back(RankChar(to()));
        if (promoted() != NO_PIECE)
        {
            result.push_back(" pnbrqk"[promoted()]);
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
        IS_CHECKING = 1 << 15,
    };
};

typedef StackVector<Move, MAX_MOVES_PER_POSITION> MoveList;

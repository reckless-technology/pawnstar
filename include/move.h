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
class alignas(8) Move
{
  public:
    /// @brief Move types.
    enum MoveType : uint8_t
    {
        REGULAR,                   ///< Regular move type including captures.
        PAWN_DOUBLE_PUSH,          ///< Pawn double push (affects en passant).
        PROMOTION_KNIGHT = KNIGHT, ///< Promotion to knight.
        PROMOTION_BISHOP = BISHOP, ///< Promotion to bishop.
        PROMOTION_ROOK   = ROOK,   ///< Promotion to rook.
        PROMOTION_QUEEN  = QUEEN,  ///< Promotion to queen.
        EP_CAPTURE,                ///< En passant capture.
        CASTLING,                  ///< Castling.
    };

    /// @brief Default constructor: does NOT initialize the move object (for speed).
    constexpr Move()
    {
    }

    constexpr Move(const Move &that)
    {
        *(uint64_t *)this = *(uint64_t *)(&that);
    }

    constexpr Move &operator=(const Move &that)
    {
        *(uint64_t *)this = *(uint64_t *)(&that);
        return *this;
    }

    /// @brief Equality operator. Only consider from, to and type when testing equality.
    /// @param that Other move to compare.
    /// @return true if moves are "equivalent".
    constexpr bool operator==(const Move &that) const
    {
        return to_ == that.to_ && from_ == that.from_ && type_ == that.type_;
    }

    constexpr Square to() const
    {
        return to_;
    }

    constexpr Square from() const
    {
        return from_;
    }

    constexpr MoveType type() const
    {
        return type_;
    }

    constexpr Piece promoted() const
    {
        switch (type_)
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
        return score_;
    }

    /// @brief Create a from-to bitmask for indexing into the good moves hash table.
    /// @return 12 bit from-to combination.
    constexpr int from_to_mask() const
    {
        return to_ | ((int)from_ << 6);
    }

    constexpr bool IsChecking() const
    {
        return flags_ & IS_CHECKING;
    }

    constexpr void AssignScore(int score)
    {
        score_ = score;
    }

    constexpr void GivesCheck()
    {
        flags_ = IS_CHECKING;
    }

    constexpr operator bool() const
    {
        return to_ != 0 || from_ != 0;
    }

    constexpr static Move None()
    {
        return Move{(Square)0, (Square)0};
    }

    constexpr static Move Regular(Square from, Square to)
    {
        return Move{from, to};
    }

    constexpr static Move Promotion(Square from, Square to, Piece promoted)
    {
        return Move{from, to, (MoveType)promoted};
    }

    constexpr static Move Castling(Square from, Square to)
    {
        return Move{from, to, CASTLING};
    }

    constexpr static Move EpCapture(Square from, Square to)
    {
        return Move{from, to, EP_CAPTURE};
    }

    constexpr static Move DoublePush(Square from, Square to)
    {
        return Move{from, to, PAWN_DOUBLE_PUSH};
    }

    constexpr std::size_t operator()(const Move &move) const
    {
        return (std::size_t) * (uint64_t *)&move;
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
    enum MoveFlags : uint8_t
    {
        NO_FLAG     = 0,
        IS_CHECKING = 1,
    };

    Square    to_;
    Square    from_;
    MoveType  type_;
    MoveFlags flags_;
    int       score_;

    constexpr Move(Square from, Square to) : to_(to), from_(from), type_(REGULAR), flags_(NO_FLAG), score_(0)
    {
    }

    constexpr Move(Square from, Square to, MoveType type)
        : to_(to), from_(from), type_(type), flags_(NO_FLAG), score_(0)
    {
    }
};

static_assert(sizeof(Move) == sizeof(uint64_t));

typedef StackVector<Move, MAX_MOVES_PER_POSITION> MoveList;

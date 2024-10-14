#pragma once
/// @file Types and functions for using chess pieces, colors and moves on the board.

#include <algorithm>
#include <cstdint>
#include <string>

#include "constants.h"
#include "stack_list.h"

/// @brief Chess pieces.
enum Piece : uint8_t
{
    NO_PIECE, ///< Used to indicate absence of a piece.
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
/// @brief Chess board square indices.
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

/// @brief Return the file of a square.
/// @param locn Square index.
/// @return File number (0,7)
constexpr uint8_t FileOf(Square locn)
{
    return locn & 7;
}
/// @brief Return the rank of a square.
/// @param locn Square index.
/// @return Rank number (0,7)
constexpr uint8_t RankOf(Square locn)
{
    return locn >> 3;
}
/// @brief Return the name of a file.
/// @param locn Square index.
/// @return Square file name.
constexpr char FileChar(Square locn)
{
    return 'a' + FileOf(locn);
}
/// @brief Return the name of a rank.
/// @param locn Square index.
/// @return Square rank name.
constexpr char RankChar(Square locn)
{
    return '1' + RankOf(locn);
}
/// @brief Return the name of a square.
/// @param locn Square index.
/// @return Square name.
constexpr std::string SquareName(Square locn)
{
    return {FileChar(locn), RankChar(locn)};
}

/// @brief Class for representing a chess move.
/// A move is 64 bits in size.
class alignas(8) Move
{
  public:
    /// @brief Move types.
    enum Type : uint8_t
    {
        NON_CAPTURE,               ///< Regular non capture move.
        CAPTURE,                   ///< Capture move.
        PROMOTION_KNIGHT = KNIGHT, ///< Promotion to knight.
        PROMOTION_BISHOP = BISHOP, ///< Promotion to bishop.
        PROMOTION_ROOK   = ROOK,   ///< Promotion to rook.
        PROMOTION_QUEEN  = QUEEN,  ///< Promotion to queen.
        PAWN_DOUBLE_PUSH,          ///< Pawn double push (affects en passant).
        EP_CAPTURE,                ///< En passant capture.
        CASTLING,                  ///< Castling.
    };

    /// @brief Default constructor; does not initialize the move object, so that arrays of moves can be quickly created
    /// without having to construct initialize each move.
    constexpr Move()
    {
    }

    /// @brief Copy constructor. Treat the move object as a 64 bit int for efficiency.
    /// @param that Move to construct from.
    constexpr Move(const Move &that)
    {
        *(uint64_t *)this = *(uint64_t *)(&that);
    }

    /// @brief Assignment. Treat the move object as a 64 bit int for efficiency.
    /// @param that Value to be assigned.
    /// @return Assignee move.
    constexpr Move &operator=(const Move &that)
    {
        *(uint64_t *)this = *(uint64_t *)(&that);
        return *this;
    }

    /// @brief Equality operator; only consider from, to and move type when testing for equality.
    /// @param that Other move to compare.
    /// @return true if moves are equivalent.
    constexpr bool operator==(const Move &that) const
    {
        return to_ == that.to_ && from_ == that.from_ && type_ == that.type_;
    }

    /// @brief Destination square.
    /// @return Square index of move to.
    constexpr Square to() const
    {
        return to_;
    }

    /// @brief Source square.
    /// @return Square index of move from.
    constexpr Square from() const
    {
        return from_;
    }

    /// @brief Type.
    /// @return Move type.
    constexpr Type type() const
    {
        return type_;
    }

    /// @brief Promoted piece.
    /// @return In the case of a pawn promotion, the piece to be promoted.
    constexpr Piece promoted() const
    {
        switch (type_)
        {
        case NON_CAPTURE:
        case CAPTURE:
        case PAWN_DOUBLE_PUSH:
        case EP_CAPTURE:
        case CASTLING:
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

    /// @brief The move score.
    /// @return Move score (usually in centipawns).
    constexpr int score() const
    {
        return score_;
    }

    /// @brief Create a from-to bits value for indexing into history tables.
    /// @return 12 bit from-to combination.
    constexpr int from_to() const
    {
        return to_ | ((int)from_ << 6);
    }

    /// @brief Does this move give check?
    /// @return true if move is checking.
    constexpr bool IsChecking() const
    {
        return flags_ & IS_CHECKING;
    }

    /// @brief Assign the score to the move.
    /// @param score Score to be assigned.
    constexpr void AssignScore(int score)
    {
        score_ = score;
    }

    /// @brief Set the flag that indicates this move gives check.
    constexpr void GivesCheck()
    {
        flags_ = IS_CHECKING;
    }

    /// @brief Return true if this is an actual move, false if it is a list terminator, null move, or no move.
    constexpr operator bool() const
    {
        return to_ != 0 || from_ != 0;
    }

    /// @brief Null move.
    /// @return Special sentinel move that indicates not a move.
    constexpr static Move None()
    {
        return Move{(Square)0, (Square)0};
    }

    /// @brief Create a non capture move.
    /// @param from From square.
    /// @param to To square.
    /// @return The move.
    constexpr static Move NonCapture(Square from, Square to)
    {
        return Move{from, to};
    }

    /// @brief Create a capture move.
    /// @param from From square.
    /// @param to To square.
    /// @return The move.
    constexpr static Move Capture(Square from, Square to)
    {
        return Move{from, to, CAPTURE};
    }

    /// @brief Create a pawn promotion move.
    /// @param from From square.
    /// @param to To square.
    /// @param promoted Piece to be promoted to.
    /// @return The move.
    constexpr static Move Promotion(Square from, Square to, Piece promoted)
    {
        return Move{from, to, (Type)promoted};
    }

    /// @brief Create a castling move.
    /// @param from King source square.
    /// @param to King destination square.
    /// @return The move.
    constexpr static Move Castling(Square from, Square to)
    {
        return Move{from, to, CASTLING};
    }

    /// @brief Create an en passant capture move.
    /// @param from Capturing pawn from square.
    /// @param to Capturing pawn to square.
    /// @return The move.
    constexpr static Move EpCapture(Square from, Square to)
    {
        return Move{from, to, EP_CAPTURE};
    }

    /// @brief Create a pawn double push move.
    /// @param from From square.
    /// @param to To square.
    /// @return The move.
    constexpr static Move DoublePush(Square from, Square to)
    {
        return Move{from, to, PAWN_DOUBLE_PUSH};
    }

    /// @brief Used for putting moves in std library hashed containers.
    /// @param move Move to hash.
    /// @return Hashed value of move.
    constexpr std::size_t operator()(const Move &move) const
    {
        return (std::size_t) * (uint64_t *)&move;
    }

    /// @brief Convert a move to an algebraic notation string, e.g. "e1g1", "a7a8q".
    /// @return Move string.
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
    /// @brief Move flags.
    enum MoveFlags : uint8_t
    {
        NO_FLAG     = 0,
        IS_CHECKING = 1, ///< Move gives check.
    };

    Square    to_;    ///< To square.
    Square    from_;  ///< From square.
    Type      type_;  ///< Move type.
    MoveFlags flags_; ///< Move flags.
    int       score_; ///< Assigned score, if any.

    /// @brief Construct a non capture move.
    /// @param from From square.
    /// @param to To square.
    constexpr Move(Square from, Square to) : to_(to), from_(from), type_(NON_CAPTURE), flags_(NO_FLAG), score_(0)
    {
    }

    /// @brief Construct a move with the specified type.
    /// @param from From square.
    /// @param to To Square.
    /// @param type The move type.
    constexpr Move(Square from, Square to, Type type) : to_(to), from_(from), type_(type), flags_(NO_FLAG), score_(0)
    {
    }
};

static_assert(sizeof(Move) == sizeof(uint64_t));

/// @brief A list of moves, typically used to hold the list of legal moves available from a given position.
/// A StackList is much faster than a std::vector, and more convenient than a std::array.
typedef StackList<Move, MAX_MOVES_PER_POSITION> MoveList;

/// @brief Sort moves best first, i.e. in descending score order.
/// @tparam is_stable_sort True for stable (slower) sort, only required when processing the root node of the search.
/// @param moves List of moves to be sorted.
template <bool is_stable_sort> constexpr void SortMoves(MoveList &moves)
{
    if constexpr (is_stable_sort)
    {
        std::stable_sort(moves.begin(), moves.end(),
                         [](const Move &a, const Move &b) { return a.score() > b.score(); });
    }
    else
    {
        std::sort(moves.begin(), moves.end(), [](const Move &a, const Move &b) { return a.score() > b.score(); });
    }
}

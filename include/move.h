#pragma once
/// @file Types and functions for using chess pieces, colors and moves on the board.

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>

#include "constants.h"
#include "stack_list.h"

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

/// @brief Return the enemy of a color.
/// @param color the color
/// @return The opposite color
constexpr Color EnemyOf(Color color)
{
    return color == WHITE ? BLACK : WHITE;
}

/// @brief A square on the chess board.
class Square
{
  public:
    /// @brief Default constructor - does nothing.
    constexpr Square()
    {
    }
    /// @brief Constructor
    /// @param x Square index in LERF mapping.
    constexpr Square(uint8_t x) : s(x)
    {
    }
    /// @brief Constructor
    /// @param x file
    /// @param y rank
    constexpr Square(uint8_t x, uint8_t y) : s(x + 8 * y)
    {
    }
    /// @brief Constructor
    /// @param str Name of square e.g. "e4"
    constexpr Square(const char *str) : s((str[0] | 0x20) - 'a' + 8 * (str[1] - '1'))
    {
    }
    /// @brief Convert to square index.
    constexpr operator uint8_t() const
    {
        return s;
    }
    /// @brief File.
    /// @return File on board (0 thru 7 -> a thru h)
    constexpr uint8_t File() const
    {
        return s & 7;
    }
    /// @brief Rank.
    /// @return Rank on board (zero indexed 0 thru 7).
    constexpr uint8_t Rank() const
    {
        return s >> 3;
    }
    /// @brief Square name.
    /// @return String containing name of square e.g. "e4"
    constexpr std::string ToString() const
    {
        return std::string{(char)('a' + File()), (char)('1' + Rank())};
    }

  private:
    uint8_t s;
};

/// @brief Class for representing a chess move.
/// A move is 64 bits in size.
/// Bits are as follows:
///  0 -  5: to square
///  6 - 11: from square
/// 12 - 14: moving piece
/// 15 - 17: captured piece (in the case of capture moves)
/// 18 - 21: move type
/// 22 - 22: is checking flag (move gives check)
/// 32 - 63: score (when move has score assigned)
class alignas(8) Move
{
  private:
    static const int64_t IS_CHECKING = 1 << 22;
    int64_t              val;

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

    /// @brief Default constructor; does not initialize the move object, so that arrays of moves can be quickly
    /// created without having to construct initialize each move.
    constexpr Move()
    {
    }

    /// @brief Copy constructor. Treat the move object as a 64 bit int for efficiency.
    /// @param that Move to construct from.
    constexpr Move(const Move &that) : val(that.val)
    {
    }

    /// @brief Assignment. Treat the move object as a 64 bit int for efficiency.
    /// @param that Value to be assigned.
    /// @return Assignee move.
    constexpr Move &operator=(const Move &that)
    {
        val = that.val;
        return *this;
    }

    /// @brief Equality operator. Compare from, to, piece, captured, type fields (22 bits).
    /// @param that Other move to compare.
    /// @return true if moves are equivalent.
    constexpr bool operator==(const Move &that) const
    {
        return (val & 0x3FFFFF) == (that.val & 0x3FFFFF);
    }

    /// @brief Destination square.
    /// @return Square index of move to.
    constexpr Square to() const
    {
        return Square{(uint8_t)(val & 0x3F)};
    }

    /// @brief Source square.
    /// @return Square index of move from.
    constexpr Square from() const
    {
        return Square{(uint8_t)((val >> 6) & 0x3F)};
    }

    /// @brief Type.
    /// @return Move type.
    constexpr Type type() const
    {
        return (Type)((val >> 18) & 0x0F);
    }

    /// @brief Moving piece
    /// @return the piece.
    constexpr Piece piece() const
    {
        return (Piece)((val >> 12) & 0x07);
    }

    /// @brief Captured piece.
    /// @return the captured piece or NO_PIECE if not a capture move.
    constexpr Piece captured() const
    {
        return (Piece)((val >> 15) & 0x07);
    }

    /// @brief Promoted piece.
    /// @return In the case of a pawn promotion, the piece to be promoted.
    constexpr Piece promoted() const
    {
        switch (type())
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
        return (int)(val >> 32);
    }

    /// @brief Create a from-to bits value for indexing into history tables.
    /// @return 12 bit from-to combination.
    constexpr int from_to() const
    {
        return val & 0xFFF;
    }

    /// @brief Does this move give check?
    /// @return true if move is checking.
    constexpr bool IsChecking() const
    {
        return !!(val & IS_CHECKING);
    }

    /// @brief Assign the score to the move.
    /// @param score Score to be assigned.
    constexpr void AssignScore(int score)
    {
        val = (val & 0xFFFFFFFF) | (int64_t)(score) << 32;
    }

    /// @brief Set the flag that indicates this move gives check.
    constexpr void GivesCheck()
    {
        val |= IS_CHECKING;
    }

    /// @brief Return true if this is an actual move, false if it is a list terminator, null move, or no move.
    constexpr operator bool() const
    {
        return val != 0;
    }

    /// @brief Null move.
    /// @return Special sentinel move that indicates not a move.
    constexpr static Move None()
    {
        return Move{0};
    }

    /// @brief Create a non capture move.
    /// @param from From square.
    /// @param to To square.
    /// @return The move.
    constexpr static Move NonCapture(Square from, Square to, Piece piece)
    {
        return Move{from, to, piece};
    }

    /// @brief Create a capture move.
    /// @param from From square.
    /// @param to To square.
    /// @return The move.
    constexpr static Move Capture(Square from, Square to, Piece piece, Piece captured)
    {
        return Move{from, to, piece, Type::CAPTURE, captured};
    }

    /// @brief Create a pawn promotion capture move.
    /// @param from From square.
    /// @param to To square.
    /// @param promoted Piece to be promoted to.
    /// @return The move.
    constexpr static Move Promotion(Square from, Square to, Piece promoted, Piece captured)
    {
        return Move{from, to, PAWN, (Type)promoted, captured};
    }

    /// @brief Create a pawn promotion non capture move.
    /// @param from From square.
    /// @param to To square.
    /// @param promoted Piece to be promoted to.
    /// @return The move.
    constexpr static Move Promotion(Square from, Square to, Piece promoted)
    {
        return Move{from, to, PAWN, (Type)promoted};
    }

    /// @brief Create a castling move.
    /// @param from King source square.
    /// @param to King destination square.
    /// @return The move.
    constexpr static Move Castling(Square from, Square to)
    {
        return Move{from, to, KING, Type::CASTLING};
    }

    /// @brief Create an en passant capture move.
    /// @param from Capturing pawn from square.
    /// @param to Capturing pawn to square.
    /// @return The move.
    constexpr static Move EpCapture(Square from, Square to)
    {
        return Move{from, to, PAWN, Type::EP_CAPTURE, PAWN};
    }

    /// @brief Create a pawn double push move.
    /// @param from From square.
    /// @param to To square.
    /// @return The move.
    constexpr static Move DoublePush(Square from, Square to)
    {
        return Move{from, to, PAWN, Type::PAWN_DOUBLE_PUSH};
    }

    /// @brief Used for putting moves in std library hashed containers.
    /// @param move Move to hash.
    /// @return Hashed value of move.
    constexpr std::size_t operator()(const Move &move) const
    {
        return (std::size_t)move.val;
    }

    /// @brief Convert a move to an algebraic notation string, e.g. "e1g1", "a7a8q".
    /// @return Move string.
    constexpr const std::string ToString() const
    {
        std::string result = from().ToString() + to().ToString();
        if (promoted() != NO_PIECE)
        {
            result.push_back(" pnbrqk"[promoted()]);
        }
        return result;
    }

  private:
    constexpr Move(Square from, Square to, Piece piece) : val(to | (from << 6) | (piece << 12))
    {
    }

    constexpr Move(Square from, Square to, Piece piece, Type type)
        : val(to | (from << 6) | (piece << 12) | (type << 18))
    {
    }

    constexpr Move(Square from, Square to, Piece piece, Type type, Piece captured)
        : val(to | (from << 6) | (piece << 12) | (captured << 15) | (type << 18))
    {
    }

    constexpr Move(int64_t v) : val(v)
    {
    }
};

static_assert(sizeof(Move) == sizeof(uint64_t));

/// @brief A list of moves, typically used to hold the list of legal moves available from a given position.
/// A StackList is much faster than a std::vector, and more convenient than a raw std::array.
typedef StackList<Move, MAX_MOVES_PER_POSITION> MoveList;

/// @brief Sort moves best first, i.e. in descending score order.
/// @tparam is_stable_sort True for stable (slower) sort, only required when processing the root node of the search.
/// @param moves List of moves to be sorted.
template <bool is_stable_sort> constexpr void SortMoves(MoveList &moves)
{
    if constexpr (is_stable_sort)
    {
        std::ranges::stable_sort(moves, [](const Move &a, const Move &b) { return a.score() > b.score(); });
    }
    else
    {
        std::ranges::sort(moves, [](const Move &a, const Move &b) { return a.score() > b.score(); });
    }
}

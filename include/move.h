#pragma once
/// @file Types and functions for using chess moves.

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>

#include "color.h"
#include "constants.h"
#include "piece.h"
#include "square.h"
#include "stack_list.h"

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
class Move
{
  private:
    static const int64_t kIsChecking = 1 << 22;
    int64_t              val;

  public:
    /// @brief Move types.
    enum Type : uint8_t
    {
        kNonCapture,                ///< Regular non capture move.
        kCapture,                   ///< Capture move.
        kPromotionKnight = kKnight, ///< Promotion to knight.
        kPromotionBishop = kBishop, ///< Promotion to bishop.
        kPromotionRook   = kRook,   ///< Promotion to rook.
        kPromotionQueen  = kQueen,  ///< Promotion to queen.
        kPawnDoublePush,            ///< Pawn double push (affects en passant).
        kEpCapture,                 ///< En passant capture.
        kCastling,                  ///< Castling.
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

    /// @brief Less than operator. Used to put moves into a set.
    /// @param that Other move to compare.
    /// @return true if move is less than that.
    constexpr bool operator<(const Move &that) const
    {
        return val < that.val;
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
        return Type{(uint8_t)((val >> 18) & 0x0F)};
    }

    /// @brief Moving piece
    /// @return the piece.
    constexpr Piece piece() const
    {
        return Piece{(uint8_t)((val >> 12) & 0x07)};
    }

    /// @brief Captured piece.
    /// @return the captured piece or kNone if not a capture move.
    constexpr Piece captured() const
    {
        return Piece{(uint8_t)((val >> 15) & 0x07)};
    }

    /// @brief Promoted piece.
    /// @return In the case of a pawn promotion, the piece to be promoted.
    constexpr Piece promoted() const
    {
        switch (type())
        {
        case kNonCapture:
        case kCapture:
        case kPawnDoublePush:
        case kEpCapture:
        case kCastling:
            return Piece::kNone;
        case kPromotionKnight:
            return kKnight;
        case kPromotionBishop:
            return kBishop;
        case kPromotionRook:
            return kRook;
        case kPromotionQueen:
            return kQueen;
        }
        return Piece::kNone;
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
        return !!(val & kIsChecking);
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
        val |= kIsChecking;
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
        return Move{from, to, piece, Type::kCapture, captured};
    }

    /// @brief Create a pawn promotion capture move.
    /// @param from From square.
    /// @param to To square.
    /// @param promoted Piece to be promoted to.
    /// @return The move.
    constexpr static Move Promotion(Square from, Square to, Piece promoted, Piece captured)
    {
        return Move{from, to, kPawn, (Type)promoted, captured};
    }

    /// @brief Create a pawn promotion non capture move.
    /// @param from From square.
    /// @param to To square.
    /// @param promoted Piece to be promoted to.
    /// @return The move.
    constexpr static Move Promotion(Square from, Square to, Piece promoted)
    {
        return Move{from, to, kPawn, (Type)promoted};
    }

    /// @brief Create a castling move.
    /// @param from King source square.
    /// @param to King destination square.
    /// @return The move.
    constexpr static Move Castling(Square from, Square to)
    {
        return Move{from, to, kKing, Type::kCastling};
    }

    /// @brief Create an en passant capture move.
    /// @param from Capturing pawn from square.
    /// @param to Capturing pawn to square.
    /// @return The move.
    constexpr static Move EpCapture(Square from, Square to)
    {
        return Move{from, to, kPawn, Type::kEpCapture, kPawn};
    }

    /// @brief Create a pawn double push move.
    /// @param from From square.
    /// @param to To square.
    /// @return The move.
    constexpr static Move DoublePush(Square from, Square to)
    {
        return Move{from, to, kPawn, Type::kPawnDoublePush};
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
        if (promoted() != Piece::kNone)
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
typedef StackList<Move, kMaxMovesPerPosition> MoveList;

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

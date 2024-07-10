#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "bitboard.h"
#include "move.h"

/**
 * @brief Bitset of position state flags.
 */
enum StateFlags : uint16_t
{
    MAY_WHITE_CASTLE_KINGSIDE  = 1 << 0, /**< white has the right to castle king side                                 */
    MAY_WHITE_CASTLE_QUEENSIDE = 1 << 1, /**< white has the right to castle queen side                                */
    MAY_BLACK_CASTLE_KINGSIDE  = 1 << 2, /**< black has the right to castle king side                                 */
    MAY_BLACK_CASTLE_QUEENSIDE = 1 << 3, /**< black has the right to castle queen side                                */
    IS_BLACK_TO_MOVE           = 1 << 4, /**< it is black's turn to move                                              */
    IS_CHECK                   = 1 << 5, /**< is the side to move in check                                            */
    HAS_MOVED_INTO_CHECK       = 1 << 6, /**< is the side not to move in check (illegal position)                     */
    IS_NULL_MOVE               = 1 << 7, /**< position was the result of a null move                                  */
    HAS_BEEN_REDUCED           = 1 << 8, /**< has late move reduction been applied in this position or its ancestors  */
    CASTLING_RIGHTS_MASK =
        (MAY_WHITE_CASTLE_KINGSIDE | MAY_WHITE_CASTLE_QUEENSIDE | MAY_BLACK_CASTLE_KINGSIDE | MAY_BLACK_CASTLE_QUEENSIDE),
};

/**
 * @brief A chess position.
 * The position comprises the pieces on the board, whose turn it is to
 * move, castling rights for each side, whether en passant capture is possible,
 * and the number of consecutive reversible half-moves.
 */
struct Position
{

  public:
    Position() {};
    Position(std::string_view fen_string);                           /**< Construct a position from a FEN string. */
    Position    MakeMove(Move move) const;                           /**< Construct a position from this one by making a move. */
    Position    MakeNullMove() const;                                /**< Construct a position from this one by making a null move. */
    void        AddPiece(Color color, Piece piece, Square to);       /**< Place a piece on the board. */
    Bitboard    AttacksTo(Square location, Color color) const;       /**< The set of attackers to a location on the board. */
    bool        IsAttacked(Square location, Color color) const;      /**< Determine if location is attacked by color. */
    bool        IsLegal() const;                                     /**< Is this a legal chess position. */
    bool        IsCheckmate() const;                                 /**< Is this position checkmate. */
    bool        IsStalemate() const;                                 /**< Is this position stalemate. */
    bool        IsDrawByFiftyMoves() const;                          /**< Is this position a draw by the 50 move rule. */
    bool        IsDrawByMaterial() const;                            /**< Is this position a draw by insufficient material. */
    bool        IsDrawByRepetition(bool is_search) const;            /**< Is this position a draw by repetition. */
    uint64_t    ComputeHash() const;                                 /**< Compute the Zobrist hash from scratch. */
    std::string ToString() const;                                    /**< Return the FEN string for this position. */
    std::string MoveToString(Move move) const;                       /**< Generate the SAN string for a specific legal move. */
    std::string VariationToString(const Variation &variation) const; /**< Generate SAN strings for a legal sequence of moves. */

    constexpr Piece PieceAt(Square location) const
    {
        const Bitboard square = BITBOARD(location);
        return (square & pawns_)     ? PAWN
               : (square & knights_) ? KNIGHT
               : (square & bishops_) ? BISHOP
               : (square & rooks_)   ? ROOK
               : (square & queens_)  ? QUEEN
               : (square & kings_)   ? KING
                                     : NO_PIECE;
    }

    constexpr uint8_t CastleFlags() const
    {
        return flags_ & CASTLING_RIGHTS_MASK;
    }

    constexpr Bitboard Pawns() const
    {
        return pawns_;
    }

    constexpr Bitboard Knights() const
    {
        return knights_;
    }

    constexpr Bitboard Bishops() const
    {
        return bishops_;
    }

    constexpr Bitboard Rooks() const
    {
        return rooks_;
    }

    constexpr Bitboard Queens() const
    {
        return queens_;
    }

    constexpr Bitboard Kings() const
    {
        return kings_;
    }

    constexpr Bitboard WhitePieces() const
    {
        return white_pieces_;
    }

    constexpr Bitboard BlackPieces() const
    {
        return black_pieces_;
    }

    constexpr Bitboard OccupiedSquares() const
    {
        return white_pieces_ | black_pieces_;
    }

    constexpr Bitboard VacantSquares() const
    {
        return ~(white_pieces_ | black_pieces_);
    }

    constexpr Bitboard PiecesOfColor(Color color) const
    {
        return pieces_of_color_[color];
    }

    constexpr Color ColorToMove() const
    {
        return flags_ & IS_BLACK_TO_MOVE ? BLACK : WHITE;
    }

    constexpr Square KingLocation(Color color) const
    {
        return king_location_[color];
    }

    constexpr uint64_t Hash() const
    {
        return hash_;
    }

    constexpr bool IsNullMove() const
    {
        return !!(flags_ & IS_NULL_MOVE);
    }

    constexpr bool IsInCheck() const
    {
        return !!(flags_ & IS_CHECK);
    }

    constexpr bool HasMovedIntoCheck() const
    {
        return !!(flags_ & HAS_MOVED_INTO_CHECK);
    }

    constexpr bool HasBeenReduced() const
    {
        return !!(flags_ & HAS_BEEN_REDUCED);
    }

    constexpr void Reduce()
    {
        flags_ |= HAS_BEEN_REDUCED;
    }

    constexpr uint8_t MoveCount() const
    {
        return full_move_count_;
    }

    constexpr Square EnPassantIndex() const
    {
        return en_passant_index_;
    }

  private:
    template <bool do_all_moves> MoveList GenerateMoves() const; /**< Prototype function to generate pseudo legal moves. */
    void                                  RemovePiece(Color color, Piece piece, Square from); /**< Remove a piece from the board. */
    void                                  MovePiece(Color color, Piece piece, Square from, Square to); /**< Move a piece on the board. */

    union {
        Bitboard pieces_[6]; /**< used to index pieces by [piece - 1] */
        struct
        {
            Bitboard pawns_;   /**< squares with a pawn on them        */
            Bitboard knights_; /**< squares with a knight on them      */
            Bitboard bishops_; /**< squares with a bishop on them      */
            Bitboard rooks_;   /**< squares with a rook on them        */
            Bitboard queens_;  /**< squares with a queen on them       */
            Bitboard kings_;   /**< squares with a king on them        */
        };
    };
    union {
        Bitboard pieces_of_color_[2]; /**< used to index pieces by color */
        struct
        {
            Bitboard white_pieces_; /**< squares with a white piece on them */
            Bitboard black_pieces_; /**< squares with a black piece on them */
        };
    };
    uint64_t        hash_;                  /**< Zobrist hash of this position, maintained incrementally    */
    const Position *previous_;              /**< position immediately prior to this in the line of play     */
    uint16_t        flags_;                 /**< game state-machine flags                                   */
    Square          king_location_[2];      /**< square index of white and black kings                      */
    Square          en_passant_index_;      /**< en passant capture availability square (0 if none)         */
    uint8_t         reversible_move_count_; /**< number of consecutive reversible half-moves (plies)        */
    uint8_t         full_move_count_;       /**< number of full moves (zero indexed)                        */
};
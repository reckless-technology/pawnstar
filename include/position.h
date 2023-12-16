#pragma once

#include <string>
#include <vector>
#include <string_view>

#include "bitboard.h"
#include "move.h"

/**
 * @brief Chess board square indices.
*/
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
};

/**
 * @brief Bitset of position state flags.
*/
enum StateFlags : uint8_t
{
    MAY_WHITE_CASTLE_KINGSIDE   = 1 << 0,  /**< white has the right to castle king side                    */
    MAY_WHITE_CASTLE_QUEENSIDE  = 1 << 1,  /**< white has the right to castle queen side                   */
    MAY_BLACK_CASTLE_KINGSIDE   = 1 << 2,  /**< black has the right to castle king side                    */
    MAY_BLACK_CASTLE_QUEENSIDE  = 1 << 3,  /**< black has the right to castle queen side                   */
    IS_BLACK_TO_MOVE            = 1 << 4,  /**< it is black's turn to move                                 */
    IS_CHECK                    = 1 << 5,  /**< is the side to move in check                               */
    IS_MOVED_INTO_CHECK         = 1 << 6,  /**< is the side not to move in check (illegal position)        */
    IS_NULL_MOVE                = 1 << 7,  /**< position was the result of a null move                     */
    CASTLING_RIGHTS_MASK        = (MAY_WHITE_CASTLE_KINGSIDE | MAY_WHITE_CASTLE_QUEENSIDE | 
                                   MAY_BLACK_CASTLE_KINGSIDE | MAY_BLACK_CASTLE_QUEENSIDE),
};

/**
 * @brief A chess position.
 * The position comprises the pieces on the board, whose turn it is to
 * move, castling rights for each side, whether en passant capture is possible,
 * and the number of consecutive reversible half-moves.
*/
struct Position
{    
    union
    {
        Bitboard pieces_[6];    /**< used to index pieces by [piece - 1] */
        struct
        {
            Bitboard pawns_;    /**< squares with a pawn on them        */
            Bitboard knights_;  /**< squares with a knight on them      */  
            Bitboard bishops_;  /**< squares with a bishop on them      */
            Bitboard rooks_;    /**< squares with a rook on them        */
            Bitboard queens_;   /**< squares with a queen on them       */
            Bitboard kings_;    /**< squares with a king on them        */
        };
    };
    union
    {
        Bitboard pieces_of_color_[2];   /**< used to index pieces by color */
        struct
        {
            Bitboard white_pieces_;     /**< squares with a white piece on them */
            Bitboard black_pieces_;     /**< squares with a black piece on them */
        };
    };    
    uint64_t        hash_;                  /**< Zobrist hash of this position, maintained incrementally    */
    const Position* previous_;              /**< position immediately prior to this in the line of play     */
    uint8_t         flags_;                 /**< game state-machine flags                                   */
    uint8_t         king_location_[2];      /**< square index of white and black kings                      */
    uint8_t         en_passant_index_;      /**< en passant capture availability square (0 if none)         */
    uint8_t         reversible_move_count_; /**< number of consecutive reversible half-moves (plies)        */
    uint8_t         full_move_count_;       /**< number of full moves (zero indexed)                        */

    Position() noexcept {};
    Position(std::string_view fen_string) noexcept;                         /**< Construct a position from a FEN string. */
    Position(const Position& previous, Move move) noexcept;                 /**< Construct a position from its predecessor and a move. */
    operator        std::string() const;                                    /**< Return the FEN string for this position. */
    void            MakeNullMove(Position& dst_position) const;             /**< Construct a position from this one making a null move. */
    void            AddPiece   (uint8_t color, uint8_t piece, uint8_t to);  /**< Place a piece on the board. */
    Bitboard        AttacksTo  (uint8_t location, uint8_t color) const;     /**< The set of attackers to a location on the board. */
    bool            IsAttacked (uint8_t location, uint8_t color) const;     /**< Determine if location is attacked by color. */
    bool            IsLegal() const;                                        /**< Is this a legal chess position. */
    bool            IsCheckmate() const;                                    /**< Is this position checkmate. */
    bool            IsDrawByFiftyMoves() const;                             /**< Is this position a dfraw by the 50 move rule. */
    bool            IsDrawByMaterial() const;                               /**< Is this position a draw by insufficient material. */
    bool            IsDrawByRepetition(bool is_search) const;               /**< Is this position a draw by repetition. */
    bool            IsStalemate() const;                                    /**< Is this position stalemate. */
    uint64_t        ComputeHash() const;                                    /**< Compute the Zobrist hash from scratch. */
    MoveList        GenerateLegalMoves() const;                             /**< Generate all legal moves (slow). */
    MoveList        GeneratePseudoLegalCaptures() const;                    /**< Generate pseudo legal captures and promotions. */
    MoveList        GeneratePseudoLegalMoves() const;                       /**< Generate all pseudo legal moves. */
    std::string     MoveToString(Move move) const;                          /**< Generate the SAN string for a specific legal move. */
    std::string     VariationToString(const Variation& variation) const;    /**< Generate SAN strings for a legal sequence of moves. */
    
    constexpr uint8_t PieceAt(uint8_t location) const
    {
        const Bitboard square = BITBOARD(location);
        return 
            (square & pawns_)   ? PAWN   :
            (square & knights_) ? KNIGHT :
            (square & bishops_) ? BISHOP :
            (square & rooks_)   ? ROOK   :
            (square & queens_)  ? QUEEN  :
            (square & kings_)   ? KING   : NO_PIECE;
    }                                                                           /**< Find the piece standing on location, if any. */

    constexpr uint8_t ColorToMove() const
    { 
        return flags_ & IS_BLACK_TO_MOVE ? BLACK : WHITE; 
    }                                                                           /**< Return whose turn it is to move. */

private:
    template <bool do_all_moves> MoveList GenerateMoves() const;                /**< Prototype function to generate pseudo legal moves. */
    void RemovePiece(uint8_t color, uint8_t piece, uint8_t from);               /**< Remove a piece from the board. */
    void MovePiece  (uint8_t color, uint8_t piece, uint8_t from, uint8_t to);   /**< Move a piece on the board. */
};
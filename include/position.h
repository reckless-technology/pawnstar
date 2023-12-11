#pragma once

#include <string>

#include "bitboard.h"
#include "move.h"

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
        Bitboard pieces_[6];             /**< used to index pieces by [piece - 1] */
        struct
        {
            Bitboard pawns_;             /**< squares with a pawn on them        */
            Bitboard knights_;           /**< squares with a knight on them      */  
            Bitboard bishops_;           /**< squares with a bishop on them      */
            Bitboard rooks_;             /**< squares with a rook on them        */
            Bitboard queens_;            /**< squares with a queen on them       */
            Bitboard kings_;             /**< squares with a king on them        */
        };
    };
    union
    {
        Bitboard pieces_of_color_[2];    /**< used to index pieces by color */
        struct
        {
            Bitboard white_pieces_;      /**< squares with a white piece on them */
            Bitboard black_pieces_;      /**< squares with a black piece on them */
        };
    };    
    uint64_t        hash_;                   /**< Zobrist hash of this position, maintained incrementally    */
    const Position* previous_;               /**< position immediately prior to this in the line of play     */
    uint16_t        flags_;                  /**< game state-machine flags                                   */
    uint8_t         king_location_[2];       /**< square index of white and black kings                      */
    uint8_t         en_passant_index_;       /**< en passant capture availability square (0 if none)         */
    uint8_t         reversible_move_count_;  /**< number of consecutive reversible half-moves (plies)        */
    uint8_t         full_move_count_;        /**< number of full moves (zero indexed)                        */

    Position(const char* fen_string) noexcept;
    Position(const Position& that, Move move) noexcept;
    Position() noexcept {};

    operator std::string() const;
    void AddPiece   (int color, int piece, int to);
    void RemovePiece(int color, int piece, int from);
    void MovePiece  (int color, int piece, int from, int to);
};
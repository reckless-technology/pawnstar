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
        Bitboard piece[6];              /**< used to index pieces by [piece - 1] */
        struct
        {
            Bitboard pawns;             /**< squares with a pawn on them        */
            Bitboard knights;           /**< squares with a knight on them      */  
            Bitboard bishops;           /**< squares with a bishop on them      */
            Bitboard rooks;             /**< squares with a rook on them        */
            Bitboard queens;            /**< squares with a queen on them       */
            Bitboard kings;             /**< squares with a king on them        */
        };
    };
    union
    {
        Bitboard pieces_of_color[2];    /**< used to index pieces by color */
        struct
        {
            Bitboard white_pieces;      /**< squares with a white piece on them */
            Bitboard black_pieces;      /**< squares with a black piece on them */
        };
    };    
    uint64_t        hash;                   /**< Zobrist hash of this position, maintained incrementally    */
    const Position* previous;               /**< position immediately prior to this in the line of play     */
    uint16_t        flags;                  /**< game state-machine flags                                   */
    uint8_t         king_location[2];       /**< square index of white and black kings                      */
    uint8_t         en_passant_index;       /**< en passant capture availability square (0 if none)         */
    uint8_t         reversible_move_count;  /**< number of consecutive reversible half-moves (plies)        */
    uint8_t         full_move_count;        /**< number of full moves (zero indexed)                        */

    Position(const char* fen_string) noexcept;
    Position(const Position& that, Move move) noexcept;
    Position() noexcept {};

    operator std::string() const;
};
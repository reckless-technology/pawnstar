#pragma once

#include <string>

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

    Position(const char* fen_string) noexcept;
    Position(const Position& that, Move move) noexcept;
    Position() noexcept {};

    operator    std::string() const;

    void        MakeNullMove(Position& dst_position) const;
    uint8_t     ColorToMove() const  { return flags_ & IS_BLACK_TO_MOVE ? BLACK : WHITE; }
    void        AddPiece   (int color, int piece, int to);
    void        RemovePiece(int color, int piece, int from);
    void        MovePiece  (int color, int piece, int from, int to);
    Bitboard    AttacksTo  (uint8_t location, int color) const;
    bool        IsAttacked (uint8_t location, int color) const;
    bool        IsLegal() const;
    bool        IsCheckmate() const;
    bool        IsDrawByFiftyMoves() const;
    bool        IsDrawByMaterial() const;
    bool        IsDrawByRepetition(bool is_search) const;
    bool        IsStalemate() const;
    uint64_t    ComputeHash() const;
    int         GenerateLegalMoves(MoveList& moves) const;
    void        GeneratePseudoLegalCaptures(MoveList& moves) const;
    void        GeneratePseudoLegalMoves(MoveList& moves) const;
    
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
    }

private:
    template <bool do_all_moves> void GenerateMoves(MoveList& moves) const;
};

#include "position_move_generation.h"
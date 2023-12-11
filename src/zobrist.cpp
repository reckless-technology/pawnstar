#include "pawnstar.h"

/**
 * @brief Compute the Zobrist hash for a chess position.
 * @param position the position to compute
 * @return the 64 bit hash
*/
uint64_t
ComputeHash(const Position& position)
{
    uint64_t hash = position.flags_ & IS_BLACK_TO_MOVE ? BLACK_MOVE_HASH : 0ull;
    hash ^= CASTLING_RIGHTS_HASHES[position.flags_ & CASTLING_RIGHTS_MASK];
    hash ^= EN_PASSANT_HASHES[position.en_passant_index_];
    for (int piece = PAWN - 1; piece <= KING - 1; ++piece) /* -1 because the arrays are zero indexed and PAWN value is 1 */
    {
        Bitboard b = position.pieces_[piece] & position.white_pieces_;
        while (b)
        {
            hash ^= PIECE_SQUARE_HASHES[WHITE][piece][FindAndClearLsb(b)];
        }
        b = position.pieces_[piece] & position.black_pieces_;
        while (b)
        {
            hash ^= PIECE_SQUARE_HASHES[BLACK][piece][FindAndClearLsb(b)];
        }
    }
    return hash;
}
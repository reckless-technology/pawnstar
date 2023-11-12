#include "pawnstar.h"

/**
 * @brief Compute the Zobrist hash for a chess position.
 * @param position the position to compute
 * @return the 64 bit hash
*/
uint64_t
ComputeHash(const Position* position)
{
    uint64_t hash = position->state_flags & IS_BLACK_TO_MOVE ? BLACK_MOVE_HASH : 0ull;
    hash += CASTLING_RIGHTS_HASHES[position->castle_flags];
    if (position->en_passant_index)
    {
        hash += EN_PASSANT_HASHES[FILE_OF(position->en_passant_index)];
    }
    for (int piece = PAWN; piece <= KING; ++piece)
    {
        bitboard b = position->pieces[piece] & position->white_pieces;
        while (b)
        {
            hash += PIECE_SQUARE_HASHES[WHITE][piece - 1][FindAndClearLsb(&b)];
        }
        b = position->pieces[piece] & position->black_pieces;
        while (b)
        {
            hash += PIECE_SQUARE_HASHES[BLACK][piece - 1][FindAndClearLsb(&b)];
        }
    }
    return hash;
}
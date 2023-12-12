#pragma once
#include <inttypes.h>

#include "types.h"

extern const char* const    OPENING_BOOK_MOVES;
extern const Sets           SETS[64];
extern const PawnSets       PAWN_SETS[2][64];
extern const Bitboard       INTERVENING_SQUARES[64][64];
extern const uint64_t       CASTLING_RIGHTS_HASHES[16];
extern const uint64_t       EN_PASSANT_HASHES[64];
extern const uint64_t       PIECE_SQUARE_HASHES[2][6][64];
extern const MagicMoveEntry ROOK_MAGICS[64];
extern const MagicMoveEntry BISHOP_MAGICS[64];

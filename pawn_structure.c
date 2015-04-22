#include "pawnstar.h"
/******************************************************************************
Identifies the pawn-related features of the current position
*******************************************************************************/
void DeterminePawnStructure(const Position* position, PawnStructure ps[2])
{
    const bitboard white_pawns = position->pawns & position->white_pieces;
    const bitboard black_pawns = position->pawns & position->black_pieces;

    ps[WHITE].pawn_attacks = SHIFT_NORTHWEST(white_pawns) | SHIFT_NORTHEAST(white_pawns);
    ps[BLACK].pawn_attacks = SHIFT_SOUTHWEST(black_pawns) | SHIFT_SOUTHEAST(black_pawns);

    const bitboard white_potential_attacks = FillNorth(ps[WHITE].pawn_attacks);
    const bitboard black_potential_attacks = FillSouth(ps[BLACK].pawn_attacks);
    
    const bitboard white_pawn_files = FillNorthAndSouth(white_pawns);
    const bitboard black_pawn_files = FillNorthAndSouth(black_pawns);
    
    ps[WHITE].doubled_pawns = white_pawns & FillSouth(SHIFT_SOUTH(white_pawns));
    ps[BLACK].doubled_pawns = black_pawns & FillNorth(SHIFT_NORTH(black_pawns));
    
    const bitboard white_potential_pushes = FillNorth(SHIFT_NORTH(white_pawns));
    const bitboard black_potential_pushes = FillSouth(SHIFT_SOUTH(black_pawns));

    ps[WHITE].defended_pawns = white_pawns & ps[WHITE].pawn_attacks;
    ps[BLACK].defended_pawns = black_pawns & ps[BLACK].pawn_attacks;

    ps[WHITE].isolated_pawns = white_pawns & ~(SHIFT_WEST(white_pawn_files) | SHIFT_EAST(white_pawn_files));
    ps[BLACK].isolated_pawns = black_pawns & ~(SHIFT_WEST(black_pawn_files) | SHIFT_EAST(black_pawn_files));

    ps[WHITE].passed_pawns = white_pawns & ~(ps[WHITE].doubled_pawns | black_potential_attacks | black_potential_pushes);
    ps[BLACK].passed_pawns = black_pawns & ~(ps[BLACK].doubled_pawns | white_potential_attacks | white_potential_pushes);
}
#include "pawnstar.h"

#if 1

bitboard BishopAttacks(bitboard occupied_squares, int location)
{
    const MagicMoveEntry* const m = BISHOP_MAGICS[location];
    return m->attacks[m->indices[((occupied_squares & m->occupancy_mask) * m->magic) >> m->shift]];
}

bitboard RookAttacks(bitboard occupied_squares, int location)
{
    const MagicMoveEntry* const m = ROOK_MAGICS[location];
    return m->attacks[m->indices[((occupied_squares & m->occupancy_mask) * m->magic) >> m->shift]];
}

#else

bitboard BishopAttacks(bitboard occupied_squares, int location)
{
    const Sets* sets= &SETS[location];
    bitboard attacks = sets->bishop_attacks;
    attacks ^= SETS[Lsb((sets->northeast & occupied_squares) | H8BB)].northeast;
    attacks ^= SETS[Lsb((sets->northwest & occupied_squares) | H8BB)].northwest;
    attacks ^= SETS[Msb((sets->southeast & occupied_squares) | A1BB)].southeast;
    attacks ^= SETS[Msb((sets->southwest & occupied_squares) | A1BB)].southwest;
    return attacks;
}

bitboard RookAttacks(bitboard occupied_squares, int location)
{
    const Sets* sets= &SETS[location];
    bitboard attacks = sets->rook_attacks;
    attacks ^= SETS[Lsb((sets->north & occupied_squares) | H8BB)].north;
    attacks ^= SETS[Lsb((sets->east  & occupied_squares) | H8BB)].east;
    attacks ^= SETS[Msb((sets->south & occupied_squares) | A1BB)].south;
    attacks ^= SETS[Msb((sets->west  & occupied_squares) | A1BB)].west;
    return attacks;
}

#endif

/**
 * @brief Determine queen attacks based on occupied squares.
 * @param occupied_squares set of occupied squares
 * @param location location where queen is
 * @return bitset of attacked squares
*/
bitboard QueenAttacks(bitboard occupied_squares, int location)
{
    return BishopAttacks(occupied_squares, location) | RookAttacks(occupied_squares, location);
}

/**
 * @brief Determine if a square is attacked by a color
 * @param position the position
 * @param location  location of attacked square
 * @param color attacking color
 * @return true if color attacks position, otherwise false
*/
bool IsAttacked(const Position* position, int location, int color)
{
    const Sets* const sets = &SETS[location];
    const bitboard* const intervening_squares = &INTERVENING_SQUARES[location][0];
    const bitboard attacking_pieces = position->pieces_of_color[color];
    const bitboard occupied_squares = position->white_pieces | position->black_pieces;
    /*
    Pawn, knight and king attacks can be done by direct lookup since blockers 
    do not affect their attack set.
    */
    if (attacking_pieces & 
        ((sets->pawn_attacks[ENEMY(color)] & position->pawns)   |
        (             sets->knight_attacks & position->knights) | 
        (               sets->king_attacks & position->kings)))
    {
        return true;
    }
    /*
    Rook and queen horizontal and vertical sliding attacks
    */
    bitboard sliding_attackers = (position->rooks | position->queens) & sets->rook_attacks & attacking_pieces;
    while (sliding_attackers)
    {
        if (!(intervening_squares[FindAndClearLsb(&sliding_attackers)] & occupied_squares))
        {
            return true;
        }
    }
    /*
    Bishop and queen diagonal and antidiagonal sliding attacks
    */
    sliding_attackers = (position->bishops | position->queens) & sets->bishop_attacks & attacking_pieces;
    while (sliding_attackers)
    {
        if (!(intervening_squares[FindAndClearLsb(&sliding_attackers)] & occupied_squares))
        {
            return true;
        }
    }
    return false;
}

#include "pawnstar.h"

/**
 * @brief Determine bishop attacks based on occupied squares.
 * @param occupied_squares set of occupied squares
 * @param location location where bishop is
 * @return bitset of attacked squares
*/
bitboard BishopAttacks(uint64_t occupied_squares, int location)
{
    const Sets* const sets = &SETS[location];
    bitboard result = sets->bishop_attacks;
    bitboard ray = sets->northeast & occupied_squares;
    result ^= SETS[Lsb(ray | H8BB)].northeast;
    ray = sets->northwest & occupied_squares;
    result ^= SETS[Lsb(ray | H8BB)].northwest;
    ray = sets->southwest & occupied_squares;
    result ^= SETS[Msb(ray | A1BB)].southwest;
    ray = sets->southeast & occupied_squares;
    result ^= SETS[Msb(ray | A1BB)].southeast;
    return result;
}

/**
 * @brief Determine rook attacks based on occupied squares.
 * @param occupied_squares set of occupied squares
 * @param location location where rook is
 * @return bitset of attacked squares
*/
bitboard RookAttacks(uint64_t occupied_squares, int location)
{
    const Sets* const sets = &SETS[location];
    bitboard result = sets->rook_attacks;
    bitboard ray = sets->north & occupied_squares;
    result ^= SETS[Lsb(ray | H8BB)].north;
    ray = sets->east & occupied_squares;
    result ^= SETS[Lsb(ray | H8BB)].east;
    ray = sets->south & occupied_squares;
    result ^= SETS[Msb(ray | A1BB)].south;
    ray = sets->west & occupied_squares;
    result ^= SETS[Msb(ray | A1BB)].west;
    return result;
}

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
        if (!(intervening_squares[FindAndClearLsb(&sliding_attackers)] & position->occupied_squares))
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
        if (!(intervening_squares[FindAndClearLsb(&sliding_attackers)] & position->occupied_squares))
        {
            return true;
        }
    }
    return false;
}
/*
Determine the set of squares attacked by the piece (if any) standing on 
location
*/
bitboard AttacksFromSquare(const Position* position, int location, int piece)
{
    switch (piece)
    {
    case NO_PIECE:
        return NO_SQUARES;
    case PAWN:
        return ColorAt(position, location) == WHITE ? SETS[location].pawn_attacks_white : SETS[location].pawn_attacks_black;
    case KNIGHT:
        return SETS[location].knight_attacks;
    case BISHOP:
        return BishopAttacks(position->occupied_squares, location);
    case ROOK:
        return RookAttacks(position->occupied_squares, location);
    case QUEEN:
        return QueenAttacks(position->occupied_squares, location);
    case KING:
        return SETS[location].king_attacks;
    }
    return NO_SQUARES;
}

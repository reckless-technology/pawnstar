#include "pawnstar.h"

const bitboard* const FRIENDLY_PAWN_ATTACKS[2]  = { PAWN_ATTACKS_WHITE, PAWN_ATTACKS_BLACK };
const bitboard* const ENEMY_PAWN_ATTACKS[2]     = { PAWN_ATTACKS_BLACK, PAWN_ATTACKS_WHITE };

/**
 * @brief Determine bishop attacks based on occupied squares.
 * @param occupied_squares set of occupied squares
 * @param location location where bishop is
 * @return bitset of attacked squares
*/
bitboard BishopAttacks(uint64 occupied_squares, int location)
{
    bitboard result = BISHOP_ATTACKS[location];
    bitboard ray = NORTHEAST_OF[location] & occupied_squares;
    result ^= NORTHEAST_OF[Lsb(ray | H8BB)];
    ray = NORTHWEST_OF[location] & occupied_squares;
    result ^= NORTHWEST_OF[Lsb(ray | H8BB)];
    ray = SOUTHWEST_OF[location] & occupied_squares;
    result ^= SOUTHWEST_OF[Msb(ray | A1BB)];
    ray = SOUTHEAST_OF[location] & occupied_squares;
    result ^= SOUTHEAST_OF[Msb(ray | A1BB)];
    return result;
}

/**
 * @brief Determine rook attacks based on occupied squares.
 * @param occupied_squares set of occupied squares
 * @param location location where rook is
 * @return bitset of attacked squares
*/
bitboard RookAttacks(uint64 occupied_squares, int location)
{
    bitboard result = ROOK_ATTACKS[location];
    bitboard ray = NORTH_OF[location] & occupied_squares;
    result ^= NORTH_OF[Lsb(ray | H8BB)];
    ray = EAST_OF[location] & occupied_squares;
    result ^= EAST_OF[Lsb(ray | H8BB)];
    ray = SOUTH_OF[location] & occupied_squares;
    result ^= SOUTH_OF[Msb(ray | A1BB)];
    ray = WEST_OF[location] & occupied_squares;
    result ^= WEST_OF[Msb(ray | A1BB)];
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
    const bitboard* const intervening_squares = &INTERVENING_SQUARES[location][0];
    const bitboard attacking_pieces = position->pieces_of_color[color];
    /*
    Pawn, knight and king attacks can be done by direct lookup since blockers 
    do not affect their attack set.
    */
    if (attacking_pieces & 
        ((ENEMY_PAWN_ATTACKS[color][location] & position->pawns  )  | 
        (            KNIGHT_ATTACKS[location] & position->knights)  | 
        (              KING_ATTACKS[location] & position->kings)))
    {
        return true;
    }
    /*
    Rook and queen horizontal and vertical sliding attacks
    */
    bitboard sliding_attackers = (position->rooks | position->queens) & ROOK_ATTACKS[location] & attacking_pieces;
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
    sliding_attackers = (position->bishops | position->queens) & BISHOP_ATTACKS[location] & attacking_pieces;
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
        return ColorAt(position, location) == WHITE ? PAWN_ATTACKS_WHITE[location] : PAWN_ATTACKS_BLACK[location];
    case KNIGHT:
        return KNIGHT_ATTACKS[location];
    case BISHOP:
        return BishopAttacks(position->occupied_squares, location);
    case ROOK:
        return RookAttacks(position->occupied_squares, location);
    case QUEEN:
        return QueenAttacks(position->occupied_squares, location);
    case KING:
        return KING_ATTACKS[location];
    }
    return NO_SQUARES;
}

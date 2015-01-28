#include "pawnstar.h"

const bitboard* const ENEMY_PAWN_ATTACKS[2] = { PAWN_ATTACKS_BLACK, PAWN_ATTACKS_WHITE };
const bitboard* const PAWN_ATTACKS[2]       = { PAWN_ATTACKS_WHITE, PAWN_ATTACKS_BLACK };

bitboard BishopAttacks(bitboard occupied_squares, int location)
{
    return 
        BISHOP_ATTACKS[location]                                                  ^
        FillNorthEast(SHIFT_NORTHEAST(NORTHEAST_OF[location] & occupied_squares)) ^
        FillSouthEast(SHIFT_SOUTHEAST(SOUTHEAST_OF[location] & occupied_squares)) ^
        FillSouthWest(SHIFT_SOUTHWEST(SOUTHWEST_OF[location] & occupied_squares)) ^
        FillNorthWest(SHIFT_NORTHWEST(NORTHWEST_OF[location] & occupied_squares));
}

bitboard RookAttacks(bitboard occupied_squares, int location)
{
    return
        ROOK_ATTACKS[location]                                        ^
        FillNorth(SHIFT_NORTH(NORTH_OF[location] & occupied_squares)) ^
        FillEast (SHIFT_EAST ( EAST_OF[location] & occupied_squares)) ^
        FillSouth(SHIFT_SOUTH(SOUTH_OF[location] & occupied_squares)) ^
        FillWest (SHIFT_WEST ( WEST_OF[location] & occupied_squares));
}

bitboard QueenAttacks(bitboard occupied_squares, int location)
{
    return BishopAttacks(occupied_squares, location) | RookAttacks(occupied_squares, location);
}
/******************************************************************************
Determine if location is attacked by color
*******************************************************************************/
bool IsAttacked(const Position* position, int location, int color)
{
    const bitboard* const intervening_squares = &INTERVENING_SQUARES[location][0];
    const bitboard attacking_pieces = position->pieces_of_color[color];
    bitboard sliding_attackers;
    /**************************************************************************
    Pawn, knight and king attacks can be done by direct lookup since blockers 
    do not affect their attack set.
    ***************************************************************************/
    if (attacking_pieces & 
        ((ENEMY_PAWN_ATTACKS[color][location] & position->pawns  )  | 
        (            KNIGHT_ATTACKS[location] & position->knights)  | 
        (              KING_ATTACKS[location] & position->kings)))
    {
        return true;
    }
    /**************************************************************************
    Rook and queen horizontal and vertical sliding attacks
    ***************************************************************************/
    sliding_attackers = (position->rooks | position->queens) & ROOK_ATTACKS[location] & attacking_pieces;
    while (sliding_attackers)
    {
        if (!(intervening_squares[FindAndClearLsb(&sliding_attackers)] & position->occupied_squares))
        {
            return true;
        }
    }
    /**************************************************************************
    Bishop and queen diagonal and antidiagonal sliding attacks
    ***************************************************************************/
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
/******************************************************************************
Determine the set of squares attacked by the piece (if any) standing on 
location
*******************************************************************************/
bitboard AttacksFromSquare(const Position* position, int location, int piece)
{
    switch (piece)
    {
    case NO_PIECE:
        return NO_SQUARES;
    case PAWN:
        return COLOR_AT(position, location) == WHITE ? PAWN_ATTACKS_WHITE[location] : PAWN_ATTACKS_BLACK[location];
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
/******************************************************************************
Generate a bitboard of all pieces of the specified color which directly attack 
location.
*******************************************************************************/
bitboard AttacksToSquareByColor(const Position* position, int location, int color)
{
    const bitboard* const intervening_squares = &INTERVENING_SQUARES[location][0];  
    const bitboard attacking_pieces           = position->pieces_of_color[color];
    bitboard sliding_attackers;
    /**************************************************************************
    Pawn, knight and king attacks can be done by lookup since blockers do not 
    affect their attacks
    ***************************************************************************/
    bitboard attackers = attacking_pieces & (
        (ENEMY_PAWN_ATTACKS[color][location] & position->pawns  ) |
        (           KNIGHT_ATTACKS[location] & position->knights) |
        (             KING_ATTACKS[location] & position->kings  ));
    /**************************************************************************
    Rook and queen horizontal and vertical sliding attacks
    ***************************************************************************/
    sliding_attackers = ROOK_ATTACKS[location] & (position->rooks | position->queens) & attacking_pieces;
    while (sliding_attackers)
    {
        const int locn = FindAndClearLsb(&sliding_attackers);
        if (!(intervening_squares[locn] & position->occupied_squares))
        {
            attackers |= BITBOARD(locn);
        }
    }
    /**************************************************************************
    Bishop and queen diagonal and antidiagonal sliding attacks
    ***************************************************************************/
    sliding_attackers = BISHOP_ATTACKS[location] & (position->bishops | position->queens) & attacking_pieces;
    while (sliding_attackers)
    {
        const int locn = FindAndClearLsb(&sliding_attackers);
        if (!(intervening_squares[locn] & position->occupied_squares))
        {
            attackers |= BITBOARD(locn);
        }
    }
    return attackers;
}
/******************************************************************************
Generate a bitboard of all pieces of the specifed type and color which attack
the target location.
*******************************************************************************/
bitboard AttacksToSquareByType(const Position* position, int location, int color, int piece)
{
    const bitboard attacking_pieces = position->pieces_of_color[color];
    bitboard attackers, sliding_attackers;
    const bitboard* intervening_squares;   
    switch (piece)
    {
    default:
    case NO_PIECE:
        return NO_SQUARES;

    case PAWN:
        return ENEMY_PAWN_ATTACKS[color][location] & position->pawns & attacking_pieces;

    case KNIGHT:
        return KNIGHT_ATTACKS[location] & position->knights & attacking_pieces;
    
    case BISHOP:
        intervening_squares = &INTERVENING_SQUARES[location][0]; 
        sliding_attackers   = BISHOP_ATTACKS[location] & position->bishops & attacking_pieces;
        attackers           = NO_SQUARES;
        while (sliding_attackers)
        {
            const int locn = FindAndClearLsb(&sliding_attackers);
            if (!(intervening_squares[locn] & position->occupied_squares))
            {
                attackers |= BITBOARD(locn);
            }
        }
        return attackers;
    
    case ROOK:
        intervening_squares = &INTERVENING_SQUARES[location][0]; 
        sliding_attackers   = ROOK_ATTACKS[location] & position->rooks & attacking_pieces;
        attackers           = NO_SQUARES;
        while (sliding_attackers)
        {
            const int locn = FindAndClearLsb(&sliding_attackers);
            if (!(intervening_squares[locn] & position->occupied_squares))
            {
                attackers |= BITBOARD(locn);
            }
        }
        return attackers;

    case QUEEN:
        intervening_squares = &INTERVENING_SQUARES[location][0]; 
        sliding_attackers   = QUEEN_ATTACKS[location] & position->queens & attacking_pieces;
        attackers           = NO_SQUARES;
        while (sliding_attackers)
        {
            const int locn = FindAndClearLsb(&sliding_attackers);
            if (!(intervening_squares[locn] & position->occupied_squares))
            {
                attackers |= BITBOARD(locn);
            }
        }
        return attackers;

    case KING:
        return KING_ATTACKS[location] & position->kings & attacking_pieces;
    }
}
/******************************************************************************
Generate a bitboard of all pieces of both colors which directly attack 
location.
*******************************************************************************/
bitboard AttacksToSquare(const Position* position, int location)
{
    const bitboard* const intervening_squares = &INTERVENING_SQUARES[location][0];
    bitboard sliding_attackers;
    /**************************************************************************
    Pawn, knight and king attacks can be done by lookup since blockers do not 
    affect their attacks
    ***************************************************************************/
    bitboard attackers =
        (PAWN_ATTACKS_WHITE[location] & position->pawns & position->black_pieces) |
        (PAWN_ATTACKS_BLACK[location] & position->pawns & position->white_pieces) |
        (    KNIGHT_ATTACKS[location] & position->knights)                        |
        (      KING_ATTACKS[location] & position->kings);
    /**************************************************************************
    Rook and queen horizontal and vertical sliding attacks
    ***************************************************************************/
    sliding_attackers = ROOK_ATTACKS[location] & (position->rooks | position->queens);
    while (sliding_attackers)
    {
        const int locn = FindAndClearLsb(&sliding_attackers);
        if (!(intervening_squares[locn] & position->occupied_squares))
        {
            attackers |= BITBOARD(locn);
        }
    }
    /**************************************************************************
    Bishop and queen diagonal and antidiagonal sliding attacks
    ***************************************************************************/
    sliding_attackers = BISHOP_ATTACKS[location] & (position->bishops | position->queens);
    while (sliding_attackers)
    {
        const int locn = FindAndClearLsb(&sliding_attackers);
        if (!(intervening_squares[locn] & position->occupied_squares))
        {
            attackers |= BITBOARD(locn);
        }
    }
    return attackers;
}

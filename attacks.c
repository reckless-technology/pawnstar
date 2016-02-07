#include "pawnstar.h"

const bitboard* const ENEMY_PAWN_ATTACKS[2] = { PAWN_ATTACKS_BLACK, PAWN_ATTACKS_WHITE };
const bitboard* const PAWN_ATTACKS[2]       = { PAWN_ATTACKS_WHITE, PAWN_ATTACKS_BLACK };

#if DO_MAGIC_BITBOARDS

bitboard BishopAttacks(bitboard occupied_squares, int location)
{
    const MagicMoveEntry* const m = &BISHOP_MAGICS[location];
    return m->attacks[m->attack_indices[((occupied_squares & m->occupancy_mask) * m->magic) >> m->shift]];
}

bitboard RookAttacks(bitboard occupied_squares, int location)
{
    const MagicMoveEntry* const m = &ROOK_MAGICS[location];
    return m->attacks[m->attack_indices[((occupied_squares & m->occupancy_mask) * m->magic) >> m->shift]];
}

#else

/******************************************************************************
The naive loop based attack generator performs almost as fast as the magic
bitboard attack generator when profiled in a release build...
*******************************************************************************/
bitboard BishopAttacks(uint64 occupied_squares, int location)
{
    bitboard result = NO_SQUARES;
    for (bitboard square = SHIFT_NORTHEAST(BITBOARD(location)); square; square = SHIFT_NORTHEAST(square))
    {
        result |= square;
        if (square & occupied_squares)
        {
            break;
        }
    }
    for (bitboard square = SHIFT_SOUTHEAST(BITBOARD(location)); square; square = SHIFT_SOUTHEAST(square))
    {
        result |= square;
        if (square & occupied_squares)
        {
            break;
        }
    }
    for (bitboard square = SHIFT_SOUTHWEST(BITBOARD(location)); square; square = SHIFT_SOUTHWEST(square))
    {
        result |= square;
        if (square & occupied_squares)
        {
            break;
        }
    }
    for (bitboard square = SHIFT_NORTHWEST(BITBOARD(location)); square; square = SHIFT_NORTHWEST(square))
    {
        result |= square;
        if (square & occupied_squares)
        {
            break;
        }
    }
    return result;
}

bitboard RookAttacks(uint64 occupied_squares, int location)
{
    bitboard result = NO_SQUARES;
    for (bitboard square = SHIFT_NORTH(BITBOARD(location)); square; square = SHIFT_NORTH(square))
    {
        result |= square;
        if (square & occupied_squares)
        {
            break;
        }
    }
    for (bitboard square = SHIFT_EAST(BITBOARD(location)); square; square = SHIFT_EAST(square))
    {
        result |= square;
        if (square & occupied_squares)
        {
            break;
        }
    }
    for (bitboard square = SHIFT_SOUTH(BITBOARD(location)); square; square = SHIFT_SOUTH(square))
    {
        result |= square;
        if (square & occupied_squares)
        {
            break;
        }
    }
    for (bitboard square = SHIFT_WEST(BITBOARD(location)); square; square = SHIFT_WEST(square))
    {
        result |= square;
        if (square & occupied_squares)
        {
            break;
        }
    }
    return result;
}

#endif // DO_MAGIC_BITBOARDS

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

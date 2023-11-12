#include "pawnstar.h"
/*
Functions to generate pseudo-legal and strictly-legal moves. Pseudo-legal moves
may leave our king in check; this is tested during search for improved 
efficiency.

Moves are contained within the least significant 22 bits of an integer

  Bits      Interpretation
  
 0 -  5     to (destination square index)
 6 - 11     from (source square index)
12 - 14     moving piece type
15 - 17     captured piece type in the case of capture moves
18 - 20     promoted piece type in the case of pawn promotions
21 - 21     special flag (castling or en passant capture move)

A value of 0 terminates a move list

Given a first pawn promotion move to queen, generate the under promotions to
knight, bishop and rook
*/
INLINE void GenerateUnderpromotions(int** pmoves)
{
    int* const moves = *pmoves;
    const int move   = *moves;
    moves[1] = move ^ ((ROOK   ^ QUEEN) << 18);
    moves[2] = move ^ ((BISHOP ^ QUEEN) << 18);
    moves[3] = move ^ ((KNIGHT ^ QUEEN) << 18);
    *pmoves += 4;
}
/*
Generate pseudo-legal moves for all pieces
If do_non_captures is false, just do captures and promotions (when not in check)
*/
void
GeneratePseudoLegalMoves(const Position* position, 
                         int             captures[], 
                         int             non_captures[])
{ 
    /*
    Pawn move variables
    */
    bitboard        pawns;
    bitboard        promotions_west;
    bitboard        promotions_east;
    bitboard        promotions;
    bitboard        captures_west;
    bitboard        captures_east;
    bitboard        double_pushes;
    bitboard        single_pushes;
    bitboard        en_passant_sources;
    int             push_delta;
    int             west_delta;
    int             east_delta;
    const int       color          = COLOR_TO_MOVE(position);
    const bitboard  vacant_squares = ~position->occupied_squares;
    /*
    Generate pawn moves
    */
    if (color == WHITE)
    {
        pawns              = position->pawns            & position->white_pieces;
        single_pushes      = SHIFT_NORTH(pawns)         & vacant_squares;
        double_pushes      = SHIFT_NORTH(single_pushes) & vacant_squares & RANK_4;
        captures_west      = SHIFT_NORTHWEST(pawns)     & position->black_pieces;
        captures_east      = SHIFT_NORTHEAST(pawns)     & position->black_pieces;
        en_passant_sources = position->en_passant_index ? SETS[position->en_passant_index].pawn_attacks_black & pawns : NO_SQUARES;
        promotions         = single_pushes & RANK_8;
        promotions_west    = captures_west & RANK_8;
        promotions_east    = captures_east & RANK_8;   
        captures_west     ^= promotions_west;
        captures_east     ^= promotions_east;
        single_pushes     ^= promotions;
        push_delta         = 8;
        west_delta         = 7;
        east_delta         = 9;
    }
    else
    {
        pawns              = position->pawns            & position->black_pieces;
        single_pushes      = SHIFT_SOUTH(pawns)         & vacant_squares;
        double_pushes      = SHIFT_SOUTH(single_pushes) & vacant_squares & RANK_5;
        captures_west      = SHIFT_SOUTHWEST(pawns)     & position->white_pieces;
        captures_east      = SHIFT_SOUTHEAST(pawns)     & position->white_pieces;
        en_passant_sources = position->en_passant_index ? SETS[position->en_passant_index].pawn_attacks_white & pawns : NO_SQUARES;
        promotions         = single_pushes & RANK_1;
        promotions_west    = captures_west & RANK_1;
        promotions_east    = captures_east & RANK_1;   
        captures_west     ^= promotions_west;
        captures_east     ^= promotions_east;
        single_pushes     ^= promotions;
        push_delta         = -8;
        west_delta         = -9;
        east_delta         = -7;
    }
    while (promotions_west)
    {
        const int to = FindAndClearLsb(&promotions_west);
        *captures = CONSTRUCT_PROMOTION_MOVE(to - west_delta, to, PieceAt(position, to), QUEEN);
        GenerateUnderpromotions(&captures);
    }
    while (promotions_east)
    {
        const int to = FindAndClearLsb(&promotions_east);
        *captures = CONSTRUCT_PROMOTION_MOVE(to - east_delta, to, PieceAt(position, to), QUEEN);
        GenerateUnderpromotions(&captures);
    }
    while (promotions)
    {
        const int to = FindAndClearLsb(&promotions);
        *captures = CONSTRUCT_PROMOTION_MOVE(to - push_delta, to, NO_PIECE, QUEEN);
        GenerateUnderpromotions(&captures);
    }
    while (captures_west)
    {
        const int to = FindAndClearLsb(&captures_west);
        *captures++ = CONSTRUCT_CAPTURE_MOVE(to - west_delta, to, PAWN, PieceAt(position, to));
    }
    while (captures_east)
    {
        const int to = FindAndClearLsb(&captures_east);
        *captures++ = CONSTRUCT_CAPTURE_MOVE(to - east_delta, to, PAWN, PieceAt(position, to));
    }
    while (en_passant_sources)
    {
        *captures++ = CONSTRUCT_EP_CAPTURE_MOVE(FindAndClearLsb(&en_passant_sources), position->en_passant_index);
    }
    if (non_captures)
    {
        push_delta <<= 1;
        while (double_pushes)
        {
            const int to = FindAndClearLsb(&double_pushes);
            *non_captures++ = CONSTRUCT_NON_CAPTURE_MOVE(to - push_delta, to, PAWN);
        }
        push_delta >>= 1;
        while (single_pushes)
        {
            const int to = FindAndClearLsb(&single_pushes);
            *non_captures++ = CONSTRUCT_NON_CAPTURE_MOVE(to - push_delta, to, PAWN);
        }
    }
    /*
    Generate non pawn piece moves
    */
    bitboard sources = (position->occupied_squares ^ position->pawns) & position->pieces_of_color[color];
    while (sources)
    {
        const int from  = FindAndClearLsb(&sources);
        const int piece = PieceAt(position, from);
        const bitboard targets   = AttacksFromSquare(position, from, piece);
        bitboard capture_targets = targets & position->pieces_of_color[ENEMY(color)];
        while (capture_targets)
        {
            const int to = FindAndClearLsb(&capture_targets);
            *captures++ = CONSTRUCT_CAPTURE_MOVE(from, to, piece, PieceAt(position, to));
        }
        if (non_captures)
        {
            bitboard non_capture_targets = targets & vacant_squares;
            while (non_capture_targets)
            {
                *non_captures++ = CONSTRUCT_NON_CAPTURE_MOVE(from, FindAndClearLsb(&non_capture_targets), piece);
            }
        }
    }
    if (non_captures && !(position->state_flags & IS_CHECK))
    {
        /*
        Generate castling moves
        */
        if (color == WHITE)
        {
            if ((position->castle_flags & MAY_WHITE_CASTLE_KINGSIDE)    &&  /* if white retains the right to castle kingside and */
                !(position->occupied_squares & (F1BB | G1BB))           &&  /* f1 and g1 are both vacant and                     */      
                !IsAttacked(position, F1, BLACK)                        &&  /* f1 is not attacked by black and                   */
                !IsAttacked(position, G1, BLACK))                           /* the king's destination is not attacked by black   */
            {
                *non_captures++ = CONSTRUCT_CASTLING_MOVE(E1, G1);
            }
            if ((position->castle_flags & MAY_WHITE_CASTLE_QUEENSIDE)   &&
                !(position->occupied_squares & (B1BB | C1BB | D1BB))    &&
                !IsAttacked(position, D1, BLACK)                        &&
                !IsAttacked(position, C1, BLACK))
            {
                *non_captures++ = CONSTRUCT_CASTLING_MOVE(E1, C1);
            }
        }
        else
        {
            if ((position->castle_flags & MAY_BLACK_CASTLE_KINGSIDE)    &&
                !(position->occupied_squares & (F8BB | G8BB))           && 
                !IsAttacked(position, F8, WHITE)                        &&
                !IsAttacked(position, G8, WHITE))
            {
                *non_captures++ = CONSTRUCT_CASTLING_MOVE(E8, G8);
            }
            if ((position->castle_flags & MAY_BLACK_CASTLE_QUEENSIDE)   &&
                !(position->occupied_squares & (B8BB | C8BB | D8BB))    &&
                !IsAttacked(position, D8, WHITE)                        &&
                !IsAttacked(position, C8, WHITE))
            {
                *non_captures++ = CONSTRUCT_CASTLING_MOVE(E8, C8);
            }
        }
    }
    *captures = 0;
    if (non_captures)
    {
        *non_captures = 0;
    }
}
/*
Generate all strictly legal moves for this position
This is relatively slow and is not used at each node of the search, since each 
move has to be tested to see if it leaves the king in check
Returns the number of moves generated
*/
#pragma warning(disable:4221)
int GenerateLegalMoves(const Position* position, int moves[])
{
    const int* const initial_ptr = moves;
    const int* move;
    int captures[MAX_MOVES_PER_POSITION];
    int non_captures[MAX_MOVES_PER_POSITION];
    int* phases[] = { captures, non_captures, NULL };
    Position dst_position[1];
    GeneratePseudoLegalMoves(position, captures, non_captures);
    for (int** phase = phases; *phase; ++phase)
    {
        for (move = *phase; *move; ++move)
        {
            MakeMove(dst_position, position, *move);
            if (!(dst_position->state_flags & IS_MOVED_INTO_CHECK))
            {
                *moves++ = *move;
            }
        }
    }
    *moves = 0;
    return (int)(moves - initial_ptr);
}

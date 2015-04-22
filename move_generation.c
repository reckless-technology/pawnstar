#include "pawnstar.h"
extern const bitboard* const ENEMY_PAWN_ATTACKS[2];
/******************************************************************************
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
*******************************************************************************/
INLINE void GenerateUnderpromotions(int** pmoves)
{
    int* const moves = *pmoves;
    const int move   = *moves;
    moves[1] = move ^ ((ROOK   ^ QUEEN) << 18);
    moves[2] = move ^ ((BISHOP ^ QUEEN) << 18);
    moves[3] = move ^ ((KNIGHT ^ QUEEN) << 18);
    *pmoves += 4;
}
/******************************************************************************
Generate pseudo-legal moves for all pieces
If do_all_moves is false, just do captures and promotions (when not in check)
Returns the end of the buffer (one past the last move generated)
*******************************************************************************/
int* GeneratePseudoLegalMoves(const Position* position, int moves[], bool do_all_moves)
{ 
    bitboard   pawns;
    bitboard   promotions_west;
    bitboard   promotions_east;
    bitboard   promotions;
    bitboard   captures_west;
    bitboard   captures_east;
    bitboard   double_pushes;
    bitboard   single_pushes;
    bitboard   en_passant_sources;
    int        push_delta;
    int        west_delta;
    int        east_delta;
    uchar      piece_type;
    uchar      board_pieces[64]   = { NO_PIECE };
    const int color               = COLOR_TO_MOVE(position);
    const bitboard vacant_squares = ~position->occupied_squares;
    /**************************************************************************
    Determine the pieces on the board once, to save continually looking them up
    as we generate moves.
    ***************************************************************************/
    for (piece_type = PAWN; piece_type <= KING; ++piece_type)
    {
        bitboard b = position->pieces[piece_type];
        while (b)
        {
            board_pieces[FindAndClearLsb(&b)] = piece_type;
        }
    }
    /**************************************************************************
    Generate pawn moves
    ***************************************************************************/
    if (color == WHITE)
    {
        pawns              = position->pawns            & position->white_pieces;
        single_pushes      = SHIFT_NORTH(pawns)         & vacant_squares;
        double_pushes      = SHIFT_NORTH(single_pushes) & vacant_squares & RANK_4;
        captures_west      = SHIFT_NORTHWEST(pawns)     & position->black_pieces;
        captures_east      = SHIFT_NORTHEAST(pawns)     & position->black_pieces;
        en_passant_sources = position->en_passant_index ? PAWN_ATTACKS_BLACK[position->en_passant_index] & pawns : NO_SQUARES;
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
        en_passant_sources = position->en_passant_index ? PAWN_ATTACKS_WHITE[position->en_passant_index] & pawns : NO_SQUARES;
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
        *moves = CONSTRUCT_PROMOTION_MOVE(to - west_delta, to, board_pieces[to], QUEEN);
        GenerateUnderpromotions(&moves);
    }
    while (promotions_east)
    {
        const int to = FindAndClearLsb(&promotions_east);
        *moves = CONSTRUCT_PROMOTION_MOVE(to - east_delta, to, board_pieces[to], QUEEN);
        GenerateUnderpromotions(&moves);
    }
    while (promotions)
    {
        const int to = FindAndClearLsb(&promotions);
        *moves = CONSTRUCT_PROMOTION_MOVE(to - push_delta, to, NO_PIECE, QUEEN);
        GenerateUnderpromotions(&moves);
    }
    while (captures_west)
    {
        const int to = FindAndClearLsb(&captures_west);
        *moves++ = CONSTRUCT_MOVE(to - west_delta, to, PAWN, board_pieces[to]);
    }
    while (captures_east)
    {
        const int to = FindAndClearLsb(&captures_east);
        *moves++ = CONSTRUCT_MOVE(to - east_delta, to, PAWN, board_pieces[to]);
    }
    while (en_passant_sources)
    {
        *moves++ = CONSTRUCT_SPECIAL_MOVE(FindAndClearLsb(&en_passant_sources), position->en_passant_index, PAWN, PAWN);
    }
    if (do_all_moves)
    {
        push_delta <<= 1;
        while (double_pushes)
        {
            const int to = FindAndClearLsb(&double_pushes);
            *moves++ = CONSTRUCT_NON_CAPTURE_MOVE(to - push_delta, to, PAWN);
        }
        push_delta >>= 1;
        while (single_pushes)
        {
            const int to = FindAndClearLsb(&single_pushes);
            *moves++ = CONSTRUCT_NON_CAPTURE_MOVE(to - push_delta, to, PAWN);
        }
    }
    /**************************************************************************
    Generate non pawn piece moves
    ***************************************************************************/
    bitboard sources = (position->occupied_squares ^ position->pawns) & position->pieces_of_color[color];
    const bitboard dests = do_all_moves ? ~position->pieces_of_color[color] : position->pieces_of_color[ENEMY(color)];
    while (sources)
    {
        const int from  = FindAndClearLsb(&sources);
        const int piece = board_pieces[from];
        bitboard targets = AttacksFromSquare(position, from, piece) & dests;
        while (targets)
        {
            const int to = FindAndClearLsb(&targets);
            *moves++ = CONSTRUCT_MOVE(from, to, piece, board_pieces[to]);
        }
    }
    if (do_all_moves && !(position->state_flags & IS_CHECK))
    {
        /**********************************************************************
        Generate castling moves
        ***********************************************************************/
        if (color == WHITE)
        {
            if ((position->castle_flags & MAY_WHITE_K)        &&  /* if white retains the right to castle kingside and */
                !(position->occupied_squares & (F1BB | G1BB)) &&  /* f1 and g1 are both vacant and                     */      
                !IsAttacked(position, F1, BLACK)              &&  /* f1 is not attacked by black and                   */
                !IsAttacked(position, G1, BLACK))                 /* the king's destination is not attacked by black   */
            {
                *moves++ = CONSTRUCT_SPECIAL_MOVE(E1, G1, KING, NO_PIECE);
            }
            if ((position->castle_flags & MAY_WHITE_Q)               &&
                !(position->occupied_squares & (B1BB | C1BB | D1BB)) &&                              
                !IsAttacked(position, D1, BLACK)                     &&
                !IsAttacked(position, C1, BLACK))
            {
                *moves++ = CONSTRUCT_SPECIAL_MOVE(E1, C1, KING, NO_PIECE);
            }
        }
        else
        {
            if ((position->castle_flags & MAY_BLACK_K)        &&
                !(position->occupied_squares & (F8BB | G8BB)) &&                                     
                !IsAttacked(position, F8, WHITE)              &&
                !IsAttacked(position, G8, WHITE))
            {
                *moves++ = CONSTRUCT_SPECIAL_MOVE(E8, G8, KING, NO_PIECE);
            }
            if ((position->castle_flags & MAY_BLACK_Q)               &&
                !(position->occupied_squares & (B8BB | C8BB | D8BB)) &&                              
                !IsAttacked(position, D8, WHITE)                     &&
                !IsAttacked(position, C8, WHITE))
            {
                *moves++ = CONSTRUCT_SPECIAL_MOVE(E8, C8, KING, NO_PIECE);
            }
        }
    }
    *moves = 0;
    return moves;
}
/******************************************************************************
Generate all strictly legal moves for this position
This is relatively slow and is not used at each node of the search, since each 
move has to be tested to see if it leaves the king in check
Returns the number of moves generated
*******************************************************************************/
int GenerateLegalMoves(const Position* position, int moves[])
{
    const int* const initial_ptr = moves;
    const int* move;
    int pseudo_legal_moves[MAX_MOVES_PER_POSITION];
    Position dst_position[1];
    GeneratePseudoLegalMoves(position, pseudo_legal_moves, true);
    for (move = pseudo_legal_moves; *move; ++move)
    {
        MakeMove(dst_position, position, *move);
        if (!(dst_position->state_flags & MOVED_INTO_CHECK))
        {
            *moves++ = *move;
        }
    }
    *moves = 0;
    return (int)(moves - initial_ptr);
}

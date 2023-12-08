/**
 * @file Used by move_generation.c to generate moves.
 * This file is a form of "template" and gets incluced twice, to generate 2 functions,
 * generating all moves and moves only (for quiescent search).
*/
{
    const int color                 = COLOR_TO_MOVE(position);
    const Bitboard occupied_squares = position->white_pieces | position->black_pieces;
    const Bitboard vacant_squares   = ~occupied_squares;
    const Bitboard friendly_pieces  = position->pieces_of_color[color];
    const Bitboard enemy_pieces     = occupied_squares ^ friendly_pieces;
    Pins pins;
    DeterminePins(position, &pins);
    /*
    Pawn move variables
    */
    Bitboard pawns;
    Bitboard promotions_west;
    Bitboard promotions_east;
    Bitboard promotions;
    Bitboard captures_west;
    Bitboard captures_east;
#if GENERATE_NON_CAPTURES
    Bitboard double_pushes;
#endif
    Bitboard single_pushes;
    Bitboard en_passant_sources;
    int push_delta;
    int west_delta;
    int east_delta;
    /*
    Generate pawn moves
    */
    if (color == WHITE)
    {
        pawns = position->pawns & position->white_pieces;
        single_pushes = SHIFT_NORTH(pawns) & vacant_squares;
#if GENERATE_NON_CAPTURES
        double_pushes = SHIFT_NORTH(single_pushes) & vacant_squares & RANK_4;
#endif
        captures_west = SHIFT_NORTHWEST(pawns) & position->black_pieces;
        captures_east = SHIFT_NORTHEAST(pawns) & position->black_pieces;
        en_passant_sources = position->en_passant_index ? SETS[position->en_passant_index].pawn_attacks_black & pawns : NO_SQUARES;
        promotions = single_pushes & RANK_8;
        promotions_west = captures_west & RANK_8;
        promotions_east = captures_east & RANK_8;
        captures_west ^= promotions_west;
        captures_east ^= promotions_east;
        single_pushes ^= promotions;
        push_delta = 8;
        west_delta = 7;
        east_delta = 9;
    }
    else
    {
        pawns = position->pawns & position->black_pieces;
        single_pushes = SHIFT_SOUTH(pawns) & vacant_squares;
#if GENERATE_NON_CAPTURES
        double_pushes = SHIFT_SOUTH(single_pushes) & vacant_squares & RANK_5;
#endif
        captures_west = SHIFT_SOUTHWEST(pawns) & position->white_pieces;
        captures_east = SHIFT_SOUTHEAST(pawns) & position->white_pieces;
        en_passant_sources = position->en_passant_index ? SETS[position->en_passant_index].pawn_attacks_white & pawns : NO_SQUARES;
        promotions = single_pushes & RANK_1;
        promotions_west = captures_west & RANK_1;
        promotions_east = captures_east & RANK_1;
        captures_west ^= promotions_west;
        captures_east ^= promotions_east;
        single_pushes ^= promotions;
        push_delta = -8;
        west_delta = -9;
        east_delta = -7;
    }
    while (promotions_west)
    {
        const int to = FindAndClearLsb(&promotions_west);
        const int from = to - west_delta;
        if (pins.pinned_pieces & BITBOARD(from))
        {
            if (!(pins.allowed_squares[from] & BITBOARD(to)))
            {
                continue;
            }
        }
        *moves = CONSTRUCT_PROMOTION_MOVE(from, to, PieceAt(position, to), QUEEN);
        GenerateUnderpromotions(&moves);
    }
    while (promotions_east)
    {
        const int to = FindAndClearLsb(&promotions_east);
        const int from = to - east_delta;
        if (pins.pinned_pieces & BITBOARD(from))
        {
            if (!(pins.allowed_squares[from] & BITBOARD(to)))
            {
                continue;
            }
        }
        *moves = CONSTRUCT_PROMOTION_MOVE(from, to, PieceAt(position, to), QUEEN);
        GenerateUnderpromotions(&moves);
    }
    while (promotions)
    {
        const int to = FindAndClearLsb(&promotions);
        const int from = to - push_delta;
        if (pins.pinned_pieces & BITBOARD(from))
        {
            if (!(pins.allowed_squares[from] & BITBOARD(to)))
            {
                continue;
            }
        }
        *moves = CONSTRUCT_PROMOTION_MOVE(from, to, NO_PIECE, QUEEN);
        GenerateUnderpromotions(&moves);
    }
    while (captures_west)
    {
        const int to = FindAndClearLsb(&captures_west);
        const int from = to - west_delta;
        if (pins.pinned_pieces & BITBOARD(from))
        {
            if (!(pins.allowed_squares[from] & BITBOARD(to)))
            {
                continue;
            }
        }
        *moves++ = CONSTRUCT_CAPTURE_MOVE(from, to, PAWN, PieceAt(position, to));
    }
    while (captures_east)
    {
        const int to = FindAndClearLsb(&captures_east);
        const int from = to - east_delta;
        if (pins.pinned_pieces & BITBOARD(from))
        {
            if (!(pins.allowed_squares[from] & BITBOARD(to)))
            {
                continue;
            }
        }
        *moves++ = CONSTRUCT_CAPTURE_MOVE(from, to, PAWN, PieceAt(position, to));
    }
    while (en_passant_sources)
    {
        const int from = FindAndClearLsb(&en_passant_sources);
        if (pins.pinned_pieces & BITBOARD(from))
        {
            if (!(pins.allowed_squares[from] & BITBOARD(position->en_passant_index)))
            {
                continue;
            }
        }
        *moves++ = CONSTRUCT_EP_CAPTURE_MOVE(from, position->en_passant_index);
    }
#if GENERATE_NON_CAPTURES
    push_delta <<= 1;
    while (double_pushes)
    {
        const int to = FindAndClearLsb(&double_pushes);
        const int from = to - push_delta;
        if (pins.pinned_pieces & BITBOARD(from))
        {
            if (!(pins.allowed_squares[from] & BITBOARD(to)))
            {
                continue;
            }
        }
        *moves++ = CONSTRUCT_PAWN_DOUBLE_PUSH_MOVE(from, to);
    }
    push_delta >>= 1;
    while (single_pushes)
    {
        const int to = FindAndClearLsb(&single_pushes);
        const int from = to - push_delta;
        if (pins.pinned_pieces & BITBOARD(from))
        {
            if (!(pins.allowed_squares[from] & BITBOARD(to)))
            {
                continue;
            }
        }
        *moves++ = CONSTRUCT_NON_CAPTURE_MOVE(from, to, PAWN);
    }
#endif
    /*
    Generate knight moves
    */
    Bitboard knights = position->knights & friendly_pieces;
    while (knights)
    {
        const int from = FindAndClearLsb(&knights);
        Bitboard targets = SETS[from].knight_attacks;
        if (pins.pinned_pieces & BITBOARD(from))
        {
            targets &= pins.allowed_squares[from];
        }
        Bitboard capture_targets = targets & enemy_pieces;
        while (capture_targets)
        {
            const int to = FindAndClearLsb(&capture_targets);
            *moves++ = CONSTRUCT_CAPTURE_MOVE(from, to, KNIGHT, PieceAt(position, to));
        }
#if GENERATE_NON_CAPTURES
        Bitboard non_capture_targets = targets & vacant_squares;
        while (non_capture_targets)
        {
            *moves++ = CONSTRUCT_NON_CAPTURE_MOVE(from, FindAndClearLsb(&non_capture_targets), KNIGHT);
        }
#endif
    }
    /*
    Generate bishop sliding moves
    */
    Bitboard bishops = position->bishops & friendly_pieces;
    while (bishops)
    {
        const int from = FindAndClearLsb(&bishops);
        Bitboard targets = BishopAttacks(occupied_squares, from);
        if (pins.pinned_pieces & BITBOARD(from))
        {
            targets &= pins.allowed_squares[from];
        }
        Bitboard capture_targets = targets & enemy_pieces;
        while (capture_targets)
        {
            const int to = FindAndClearLsb(&capture_targets);
            *moves++ = CONSTRUCT_CAPTURE_MOVE(from, to, BISHOP, PieceAt(position, to));
        }
#if GENERATE_NON_CAPTURES
        Bitboard non_capture_targets = targets & vacant_squares;
        while (non_capture_targets)
        {
            *moves++ = CONSTRUCT_NON_CAPTURE_MOVE(from, FindAndClearLsb(&non_capture_targets), BISHOP);
        }
#endif
    }
    /*
    Generate rook sliding moves
    */
    Bitboard rooks = position->rooks & friendly_pieces;
    while (rooks)
    {
        const int from = FindAndClearLsb(&rooks);
        Bitboard targets = RookAttacks(occupied_squares, from);
        if (pins.pinned_pieces & BITBOARD(from))
        {
            targets &= pins.allowed_squares[from];
        }
        Bitboard capture_targets = targets & enemy_pieces;
        while (capture_targets)
        {
            const int to = FindAndClearLsb(&capture_targets);
            *moves++ = CONSTRUCT_CAPTURE_MOVE(from, to, ROOK, PieceAt(position, to));
        }
#if GENERATE_NON_CAPTURES
        Bitboard non_capture_targets = targets & vacant_squares;
        while (non_capture_targets)
        {
            *moves++ = CONSTRUCT_NON_CAPTURE_MOVE(from, FindAndClearLsb(&non_capture_targets), ROOK);
        }
#endif
    }
    /*
    Generate Queen sliding moves
    */
    Bitboard queens = position->queens & friendly_pieces;
    while (queens)
    {
        const int from = FindAndClearLsb(&queens);
        Bitboard targets = QueenAttacks(occupied_squares, from);
        if (pins.pinned_pieces & BITBOARD(from))
        {
            targets &= pins.allowed_squares[from];
        }
        Bitboard capture_targets = targets & enemy_pieces;
        while (capture_targets)
        {
            const int to = FindAndClearLsb(&capture_targets);
            *moves++ = CONSTRUCT_CAPTURE_MOVE(from, to, QUEEN, PieceAt(position, to));
        }
#if GENERATE_NON_CAPTURES
        Bitboard non_capture_targets = targets & vacant_squares;
        while (non_capture_targets)
        {
            *moves++ = CONSTRUCT_NON_CAPTURE_MOVE(from, FindAndClearLsb(&non_capture_targets), QUEEN);
        }
#endif
    }
    /*
    Generate King Moves
    */
    const int king_location = Lsb(position->kings & friendly_pieces);
    Bitboard targets = SETS[king_location].king_attacks;
    Bitboard capture_targets = targets & enemy_pieces;
    while (capture_targets)
    {
        const int to = FindAndClearLsb(&capture_targets);
        *moves++ = CONSTRUCT_CAPTURE_MOVE(king_location, to, KING, PieceAt(position, to));
    }
    *moves = 0;
#if GENERATE_NON_CAPTURES
    Bitboard non_capture_targets = targets & vacant_squares;
    while (non_capture_targets)
    {
        *moves++ = CONSTRUCT_NON_CAPTURE_MOVE(king_location, FindAndClearLsb(&non_capture_targets), KING);
    }
    if (!(position->state_flags & IS_CHECK))
    {
        /*
        Generate castling moves
        */
        if (color == WHITE)
        {
            if ((position->state_flags & MAY_WHITE_CASTLE_KINGSIDE) &&  /* if white retains the right to castle kingside and */
                !(occupied_squares & (F1BB | G1BB)) &&                  /* f1 and g1 are both vacant and                     */
                !IsAttacked(position, F1, BLACK) &&                     /* f1 is not attacked by black and                   */
                !IsAttacked(position, G1, BLACK))                       /* the king's destination is not attacked by black   */
            {
                *moves++ = CONSTRUCT_CASTLING_MOVE(E1, G1);
            }
            if ((position->state_flags & MAY_WHITE_CASTLE_QUEENSIDE) &&
                !(occupied_squares & (B1BB | C1BB | D1BB)) &&
                !IsAttacked(position, D1, BLACK) &&
                !IsAttacked(position, C1, BLACK))
            {
                *moves++ = CONSTRUCT_CASTLING_MOVE(E1, C1);
            }
        }
        else
        {
            if ((position->state_flags & MAY_BLACK_CASTLE_KINGSIDE) &&
                !(occupied_squares & (F8BB | G8BB)) &&
                !IsAttacked(position, F8, WHITE) &&
                !IsAttacked(position, G8, WHITE))
            {
                *moves++ = CONSTRUCT_CASTLING_MOVE(E8, G8);
            }
            if ((position->state_flags & MAY_BLACK_CASTLE_QUEENSIDE) &&
                !(occupied_squares & (B8BB | C8BB | D8BB)) &&
                !IsAttacked(position, D8, WHITE) &&
                !IsAttacked(position, C8, WHITE))
            {
                *moves++ = CONSTRUCT_CASTLING_MOVE(E8, C8);
            }
        }
    }
    *moves = 0;
#endif
}
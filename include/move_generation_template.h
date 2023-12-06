/**
 * @file Used by move_generation.c to generate moves, both captures only and all moves.
*/
{
    const int color                 = COLOR_TO_MOVE(position);
    const bitboard occupied_squares = position->white_pieces | position->black_pieces;
    const bitboard vacant_squares   = ~occupied_squares;
    const bitboard friendly_pieces  = position->pieces_of_color[color];
    const bitboard enemy_pieces     = occupied_squares ^ friendly_pieces;
    /*
    Pawn move variables
    */
    bitboard pawns;
    bitboard promotions_west;
    bitboard promotions_east;
    bitboard promotions;
    bitboard captures_west;
    bitboard captures_east;
#if GENERATE_NON_CAPTURES
    bitboard double_pushes;
#endif
    bitboard single_pushes;
    bitboard en_passant_sources;
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
#if GENERATE_NON_CAPTURES
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
#endif
    /*
    Generate knight moves
    */
    bitboard knights = position->knights & friendly_pieces;
    while (knights)
    {
        const int from = FindAndClearLsb(&knights);
        bitboard capture_targets = SETS[from].knight_attacks & enemy_pieces;
        while (capture_targets)
        {
            const int to = FindAndClearLsb(&capture_targets);
            *captures++ = CONSTRUCT_CAPTURE_MOVE(from, to, KNIGHT, PieceAt(position, to));
        }
#if GENERATE_NON_CAPTURES
        bitboard non_capture_targets = SETS[from].knight_attacks & vacant_squares;
        while (non_capture_targets)
        {
            *non_captures++ = CONSTRUCT_NON_CAPTURE_MOVE(from, FindAndClearLsb(&non_capture_targets), KNIGHT);
        }
#endif
    }
    /*
    Generate bishop sliding moves
    */
    bitboard bishops = position->bishops & friendly_pieces;
    while (bishops)
    {
        const int from = FindAndClearLsb(&bishops);
        const bitboard attacks = BishopAttacks(occupied_squares, from);
        bitboard capture_targets = attacks & enemy_pieces;
        while (capture_targets)
        {
            const int to = FindAndClearLsb(&capture_targets);
            *captures++ = CONSTRUCT_CAPTURE_MOVE(from, to, BISHOP, PieceAt(position, to));
        }
#if GENERATE_NON_CAPTURES
        bitboard non_capture_targets = attacks & vacant_squares;
        while (non_capture_targets)
        {
            *non_captures++ = CONSTRUCT_NON_CAPTURE_MOVE(from, FindAndClearLsb(&non_capture_targets), BISHOP);
        }
#endif
    }
    /*
    Generate rook sliding moves
    */
    bitboard rooks = position->rooks & friendly_pieces;
    while (rooks)
    {
        const int from = FindAndClearLsb(&rooks);
        const bitboard attacks = RookAttacks(occupied_squares, from);
        bitboard capture_targets = attacks & enemy_pieces;
        while (capture_targets)
        {
            const int to = FindAndClearLsb(&capture_targets);
            *captures++ = CONSTRUCT_CAPTURE_MOVE(from, to, ROOK, PieceAt(position, to));
        }
#if GENERATE_NON_CAPTURES
        bitboard non_capture_targets = attacks & vacant_squares;
        while (non_capture_targets)
        {
            *non_captures++ = CONSTRUCT_NON_CAPTURE_MOVE(from, FindAndClearLsb(&non_capture_targets), ROOK);
        }
#endif
    }
    /*
    Generate Queen sliding moves
    */
    bitboard queens = position->queens & friendly_pieces;
    while (queens)
    {
        const int from = FindAndClearLsb(&queens);
        const bitboard attacks = QueenAttacks(occupied_squares, from);
        bitboard capture_targets = attacks & enemy_pieces;
        while (capture_targets)
        {
            const int to = FindAndClearLsb(&capture_targets);
            *captures++ = CONSTRUCT_CAPTURE_MOVE(from, to, QUEEN, PieceAt(position, to));
        }
#if GENERATE_NON_CAPTURES
        bitboard non_capture_targets = attacks & vacant_squares;
        while (non_capture_targets)
        {
            *non_captures++ = CONSTRUCT_NON_CAPTURE_MOVE(from, FindAndClearLsb(&non_capture_targets), QUEEN);
        }
#endif
    }
    /*
    Generate King Moves
    */
    const int king_location = Lsb(position->kings & position->pieces_of_color[color]);
    const bitboard targets = SETS[king_location].king_attacks;
    bitboard capture_targets = targets & enemy_pieces;
    while (capture_targets)
    {
        const int to = FindAndClearLsb(&capture_targets);
        *captures++ = CONSTRUCT_CAPTURE_MOVE(king_location, to, KING, PieceAt(position, to));
    }
    *captures = 0;
#if GENERATE_NON_CAPTURES
    bitboard non_capture_targets = targets & vacant_squares;
    while (non_capture_targets)
    {
        *non_captures++ = CONSTRUCT_NON_CAPTURE_MOVE(king_location, FindAndClearLsb(&non_capture_targets), KING);
    }
    if (!(position->state_flags & IS_CHECK))
    {
        /*
        Generate castling moves
        */
        if (color == WHITE)
        {
            if ((position->state_flags & MAY_WHITE_CASTLE_KINGSIDE) && /* if white retains the right to castle kingside and */
                !(occupied_squares & (F1BB | G1BB)) &&        /* f1 and g1 are both vacant and                     */
                !IsAttacked(position, F1, BLACK) &&                     /* f1 is not attacked by black and                   */
                !IsAttacked(position, G1, BLACK))                       /* the king's destination is not attacked by black   */
            {
                *non_captures++ = CONSTRUCT_CASTLING_MOVE(E1, G1);
            }
            if ((position->state_flags & MAY_WHITE_CASTLE_QUEENSIDE) &&
                !(occupied_squares & (B1BB | C1BB | D1BB)) &&
                !IsAttacked(position, D1, BLACK) &&
                !IsAttacked(position, C1, BLACK))
            {
                *non_captures++ = CONSTRUCT_CASTLING_MOVE(E1, C1);
            }
        }
        else
        {
            if ((position->state_flags & MAY_BLACK_CASTLE_KINGSIDE) &&
                !(occupied_squares & (F8BB | G8BB)) &&
                !IsAttacked(position, F8, WHITE) &&
                !IsAttacked(position, G8, WHITE))
            {
                *non_captures++ = CONSTRUCT_CASTLING_MOVE(E8, G8);
            }
            if ((position->state_flags & MAY_BLACK_CASTLE_QUEENSIDE) &&
                !(occupied_squares & (B8BB | C8BB | D8BB)) &&
                !IsAttacked(position, D8, WHITE) &&
                !IsAttacked(position, C8, WHITE))
            {
                *non_captures++ = CONSTRUCT_CASTLING_MOVE(E8, C8);
            }
        }
    }
    *non_captures = 0;
#endif
}

#include "pawnstar.h"
#include "move_generation_template.h"

#if 1

void GeneratePseudoLegalMoves(const Position& position,
                              Move            moves[])
{
    if (ColorToMove(position) == WHITE)
    {
        GenerateMoves<true, WHITE>(position, moves);
    }
    else
    {
        GenerateMoves<true, BLACK>(position, moves);
    }
}

void GeneratePseudoLegalCaptures(const Position& position,
                                 Move            moves[])
{
    if (ColorToMove(position) == WHITE)
    {
        GenerateMoves<false, WHITE>(position, moves);
    }
    else
    {
        GenerateMoves<false, BLACK>(position, moves);
    }
}

#else

static const int FINAL_RANK[2] = { 7, 0 };

void
GeneratePseudoLegalMoves(const Position& position, 
                         Move            captures[], 
                         Move            non_captures[])
{   
    Bitboard has_attacks_to = NO_SQUARES;
    Bitboard attacks_to[64];
    const int color = ColorToMove(position);
    const Bitboard friendly_pieces = position.pieces_of_color[color];
    const Bitboard enemy_pieces = position.pieces_of_color[EnemyOf(color)];
    const Bitboard occupied_squares = position.white_pieces | position.black_pieces;
    const Bitboard vacant_squares = ~occupied_squares;
    /* Generate pawn promotions. */
    Bitboard friendly_pawns;
    Bitboard pawn_promotions_capture_west;
    Bitboard pawn_promotions_capture_east;
    Bitboard pawn_promotions;
    int pawn_push_delta;
    int pawn_capture_west_delta;
    int pawn_capture_east_delta;
    if (color == WHITE)
    {
        friendly_pawns = position.pawns & position.white_pieces;
        pawn_promotions_capture_west = ShiftNorthwest(friendly_pawns) & enemy_pieces & RANK_8;
        pawn_promotions_capture_east = ShiftNortheast(friendly_pawns) & enemy_pieces & RANK_8;
        pawn_promotions = ShiftNorth(friendly_pawns) & vacant_squares & RANK_8;
        pawn_push_delta = 8;
        pawn_capture_west_delta = 7;
        pawn_capture_east_delta = 9;
    }
    else
    {
        friendly_pawns = position.pawns & position.black_pieces;
        pawn_promotions_capture_west = ShiftSouthwest(friendly_pawns) & enemy_pieces & RANK_1;
        pawn_promotions_capture_east = ShiftSoutheast(friendly_pawns) & enemy_pieces & RANK_1;
        pawn_promotions = ShiftSouth(friendly_pawns) & vacant_squares & RANK_1;
        pawn_push_delta = -8;
        pawn_capture_west_delta = -9;
        pawn_capture_east_delta = -7;
    }
    while (pawn_promotions_capture_west)
    {
        const int to = FindAndClearLsb(&pawn_promotions_capture_west);
        *captures = PromotionMove(to - pawn_capture_west_delta, to, PieceAt(position, to), QUEEN);
        GenerateUnderpromotions(&captures);
    }
    while (pawn_promotions_capture_east)
    {
        const int to = FindAndClearLsb(&pawn_promotions_capture_east);
        *captures = PromotionMove(to - pawn_capture_east_delta, to, PieceAt(position, to), QUEEN);
        GenerateUnderpromotions(&captures);
    }
    while (pawn_promotions)
    {
        const int to = FindAndClearLsb(&pawn_promotions);
        *captures = PromotionMove(to - pawn_push_delta, to, NO_PIECE, QUEEN);
        GenerateUnderpromotions(&captures);
    }
    /*
    Generate en passant capture moves (unlikely but typically valuable)
    */
    if (position.en_passant_index)
    {
        Bitboard en_passant_sources = SETS[position.en_passant_index].pawn_attacks[EnemyOf(color)] & friendly_pawns;
        while (en_passant_sources)
        {
            *captures++ = EpCaptureMove(FindAndClearLsb(&en_passant_sources), position.en_passant_index);
        }
    }
    /*
    Generate captures most valuable victim / least valuable attacker up to equal value.
    */
    for (int victim = QUEEN; victim >= PAWN; --victim)
    {
        Bitboard v = position.piece[victim - 1] & enemy_pieces;
        while (v)
        {
            const int victim_locn = FindAndClearLsb(&v);
            if (!(has_attacks_to & BITBOARD(victim_locn)))
            {
                /* Generate attacks to this square and save them for later use. */
                attacks_to[victim_locn] = AttacksTo(position, victim_locn, color);
                has_attacks_to |= BITBOARD(victim_locn);
            }
            for (int attacker = PAWN; attacker <= victim; ++attacker)
            {
                if (attacker == PAWN && RankOf(victim_locn) == FINAL_RANK[color])
                {
                    /* Pawn promotion captures: already handled. */
                    continue;
                }
                Bitboard a = position.piece[attacker - 1] & friendly_pieces & attacks_to[victim_locn];
                while (a)
                {
                    const int attacker_locn = FindAndClearLsb(&a);
                    *captures++ = CaptureMove(attacker_locn, victim_locn, attacker, victim);
                }
            }
        }
    }
    /* Generate captures MVV / LVA possible losing captures (where attacker > victim ) */
    for (int victim = QUEEN; victim >= PAWN; --victim)
    {
        Bitboard v = position.piece[victim - 1] & enemy_pieces;
        while (v)
        {
            const int victim_locn = FindAndClearLsb(&v);
            if (!(has_attacks_to & BITBOARD(victim_locn)))
            {
                /* Generate attacks to this square and save them for later use. */
                attacks_to[victim_locn] = AttacksTo(position, victim_locn, color);
                has_attacks_to |= BITBOARD(victim_locn);
            }
            for (int attacker = victim + 1; attacker <= KING; ++attacker)
            {
                if (attacker == PAWN && RankOf(victim_locn) == FINAL_RANK[color])
                {
                    /* Pawn promotion captures: already handled. */
                    continue;
                }
                Bitboard a = position.piece[attacker - 1] & friendly_pieces & attacks_to[victim_locn];
                while (a)
                {
                    const int attacker_locn = FindAndClearLsb(&a);
                    *captures++ = CaptureMove(attacker_locn, victim_locn, attacker, victim);
                }
            }
        }
    }
    /* Done with capture moves. */
     *captures = 0;
    if (non_captures == NULL)
    {
        return;
    }
    /* Pawn pushes */
    Bitboard pawn_single_pushes;
    Bitboard pawn_double_pushes;
    if (color == WHITE)
    {
        pawn_single_pushes = ShiftNorth(friendly_pawns) & vacant_squares & ~RANK_8;
        pawn_double_pushes = ShiftNorth(pawn_single_pushes) & vacant_squares & RANK_4;
    }
    else
    {
        pawn_single_pushes = ShiftSouth(friendly_pawns) & vacant_squares & ~RANK_1;
        pawn_double_pushes = ShiftSouth(pawn_single_pushes) & vacant_squares & RANK_5;
    }
    pawn_push_delta <<= 1;
    while (pawn_double_pushes)
    {
        const int to = FindAndClearLsb(&pawn_double_pushes);
        *non_captures++ = DoublePushMove(to - pawn_push_delta, to);
    }
    pawn_push_delta >>= 1;
    while (pawn_single_pushes)
    {
        const int to = FindAndClearLsb(&pawn_single_pushes);
        *non_captures++ = NonCaptureMove(to - pawn_push_delta, to, PAWN);
    }
    /* Knight non captures */
    Bitboard knights = position.knights & friendly_pieces;
    while (knights)
    {
        const int from = FindAndClearLsb(&knights);
        Bitboard targets = SETS[from].knight_attacks & vacant_squares;
        while (targets)
        {
            *non_captures++ = NonCaptureMove(from, FindAndClearLsb(&targets), KNIGHT);
        }
    }
    /* Bishop non captures */
    Bitboard bishops = position.bishops & friendly_pieces;
    while (bishops)
    {
        const int from = FindAndClearLsb(&bishops);
        Bitboard targets = BishopAttacks(occupied_squares, from) & vacant_squares;
        while (targets)
        {
            *non_captures++ = NonCaptureMove(from, FindAndClearLsb(&targets), BISHOP);
        }

    }
    /* Rook non captures */
    Bitboard rooks = position.rooks & friendly_pieces;
    while (rooks)
    {
        const int from = FindAndClearLsb(&rooks);
        Bitboard targets = RookAttacks(occupied_squares, from) & vacant_squares;
        while (targets)
        {
            *non_captures++ = NonCaptureMove(from, FindAndClearLsb(&targets), ROOK);
        }

    }
    /* Queen non captures */
    Bitboard queens = position.queens & friendly_pieces;
    while (queens)
    {
        const int from = FindAndClearLsb(&queens);
        Bitboard targets = QueenAttacks(occupied_squares, from) & vacant_squares;
        while (targets)
        {
            *non_captures++ = NonCaptureMove(from, FindAndClearLsb(&targets), QUEEN);
        }
    }
    /* King non captures */
    const int king_locn = position.king_location[color];
    Bitboard targets = SETS[king_locn].king_attacks & vacant_squares;
    while (targets)
    {
        const int to = FindAndClearLsb(&targets);
        if (!IsAttacked(position, to, EnemyOf(color)))
        {
            *non_captures++ = NonCaptureMove(king_locn, to, KING);
        }

    }
    /* Castling moves */
    if (!(position.state_flags & IS_CHECK))
    {
        /*
        Generate castling moves
        */
        if (color == WHITE)
        {
            if ((position.state_flags & MAY_WHITE_CASTLE_KINGSIDE) && /* if white retains the right to castle kingside and */
                !(occupied_squares & (F1BB | G1BB)) &&                 /* f1 and g1 are both vacant and                     */
                !IsAttacked(position, F1, BLACK) &&                    /* f1 is not attacked by black and                   */
                !IsAttacked(position, G1, BLACK))                      /* the king's destination is not attacked by black   */
            {
                *non_captures++ = CastlingMove(E1, G1);
            }
            if ((position.state_flags & MAY_WHITE_CASTLE_QUEENSIDE) &&
                !(occupied_squares & (B1BB | C1BB | D1BB)) &&
                !IsAttacked(position, D1, BLACK) &&
                !IsAttacked(position, C1, BLACK))
            {
                *non_captures++ = CastlingMove(E1, C1);
            }
        }
        else
        {
            if ((position.state_flags & MAY_BLACK_CASTLE_KINGSIDE) &&
                !(occupied_squares & (F8BB | G8BB)) &&
                !IsAttacked(position, F8, WHITE) &&
                !IsAttacked(position, G8, WHITE))
            {
                *non_captures++ = CastlingMove(E8, G8);
            }
            if ((position.state_flags & MAY_BLACK_CASTLE_QUEENSIDE) &&
                !(occupied_squares & (B8BB | C8BB | D8BB)) &&
                !IsAttacked(position, D8, WHITE) &&
                !IsAttacked(position, C8, WHITE))
            {
                *non_captures++ = CastlingMove(E8, C8);
            }
        }
    }
    *non_captures = 0;
}

#endif

/*
Generate all strictly legal moves for this position
This is relatively slow and is not used at each node of the search, since each
move has to be tested to see if it leaves the king in check
Returns the number of moves generated
*/
int GenerateLegalMoves(const Position& position, Move moves[])
{
    const Move *const initial_ptr = moves;
    Move pseudo_legal_moves[MAX_MOVES_PER_POSITION];
    GeneratePseudoLegalMoves(position, pseudo_legal_moves);
    for (const Move* move = pseudo_legal_moves; *move; ++move)
    {
        Position dst_position;
        MakeMove(dst_position, position, *move);
        if (!(dst_position.flags & IS_MOVED_INTO_CHECK))
        {
            *moves++ = *move;
        }
    }
    *moves = 0;
    return (int)(moves - initial_ptr);
}
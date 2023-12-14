#pragma once
#include "position.h"
#include "function_prototypes.h"

/**
 * @brief Generate pseudo legal moves for the side to move.
 * @tparam do_all_moves true for all moves, false for captures and promotions only.
 * @param position Position to generate moves for.
 * @param moves Where to store the zero terminated list of moves.
 */
template <bool do_all_moves> void 
Position::GenerateMoves(MoveList& m) const
{
    const uint8_t color             = ColorToMove();
    const Bitboard occupied_squares = white_pieces_ | black_pieces_;
    const Bitboard vacant_squares   = ~occupied_squares;
    const Bitboard friendly_pieces  = pieces_of_color_[color];
    const Bitboard enemy_pieces     = occupied_squares ^ friendly_pieces;
    /*
    Pawn move variables
    */
    Bitboard pawns;
    Bitboard promotions_west;
    Bitboard promotions_east;
    Bitboard promotions;
    Bitboard captures_west;
    Bitboard captures_east;
    Bitboard double_pushes;
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
        pawns = pawns_ & white_pieces_;
        single_pushes = ShiftNorth(pawns) & vacant_squares;
        double_pushes = ShiftNorth(single_pushes) & vacant_squares & RANK_4;
        captures_west = ShiftNorthwest(pawns) & black_pieces_;
        captures_east = ShiftNortheast(pawns) & black_pieces_;
        en_passant_sources = en_passant_index_ ? SETS[en_passant_index_].pawn_attacks_black & pawns : NO_SQUARES;
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
        pawns = pawns_ & black_pieces_;
        single_pushes = ShiftSouth(pawns) & vacant_squares;
        double_pushes = ShiftSouth(single_pushes) & vacant_squares & RANK_5;
        captures_west = ShiftSouthwest(pawns) & white_pieces_;
        captures_east = ShiftSoutheast(pawns) & white_pieces_;
        en_passant_sources = en_passant_index_ ? SETS[en_passant_index_].pawn_attacks_white & pawns : NO_SQUARES;
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
        const int to = FindAndClearLsb(promotions_west);
        const int from = to - west_delta;
        const uint8_t captured = PieceAt(to);
        m.Add(PromotionMove(from, to, captured, QUEEN));
        m.Add(PromotionMove(from, to, captured, ROOK));
        m.Add(PromotionMove(from, to, captured, BISHOP));
        m.Add(PromotionMove(from, to, captured, KNIGHT));
    }
    while (promotions_east)
    {
        const int to = FindAndClearLsb(promotions_east);
        const int from = to - east_delta;
        const uint8_t captured = PieceAt(to);
        m.Add(PromotionMove(from, to, captured, QUEEN));
        m.Add(PromotionMove(from, to, captured, ROOK));
        m.Add(PromotionMove(from, to, captured, BISHOP));
        m.Add(PromotionMove(from, to, captured, KNIGHT));
    }
    while (promotions)
    {
        const int to = FindAndClearLsb(promotions);
        const int from = to - push_delta;
        m.Add(PromotionMove(from, to, NO_PIECE, QUEEN));
        m.Add(PromotionMove(from, to, NO_PIECE, ROOK));
        m.Add(PromotionMove(from, to, NO_PIECE, BISHOP));
        m.Add(PromotionMove(from, to, NO_PIECE, KNIGHT));
    }
    while (captures_west)
    {
        const int to = FindAndClearLsb(captures_west);
        const int from = to - west_delta;
        m.Add(CaptureMove(from, to, PAWN, PieceAt(to)));
    }
    while (captures_east)
    {
        const int to = FindAndClearLsb(captures_east);
        const int from = to - east_delta;
        m.Add(CaptureMove(from, to, PAWN, PieceAt(to)));;
    }
    while (en_passant_sources)
    {
        const int from = FindAndClearLsb(en_passant_sources);
        m.Add(EpCaptureMove(from, en_passant_index_));
    }
    if constexpr (do_all_moves)
    {
        push_delta <<= 1;
        while (double_pushes)
        {
            const int to = FindAndClearLsb(double_pushes);
            const int from = to - push_delta;
            m.Add(DoublePushMove(from, to));
        }
        push_delta >>= 1;
        while (single_pushes)
        {
            const int to = FindAndClearLsb(single_pushes);
            const int from = to - push_delta;
            m.Add(NonCaptureMove(from, to, PAWN));
        }
    }
    /*
    Generate knight moves
    */
    Bitboard knights = knights_ & friendly_pieces;
    while (knights)
    {
        const int from = FindAndClearLsb(knights);
        Bitboard targets = SETS[from].knight_attacks;
        Bitboard capture_targets = targets & enemy_pieces;
        while (capture_targets)
        {
            const int to = FindAndClearLsb(capture_targets);
            m.Add(CaptureMove(from, to, KNIGHT, PieceAt(to)));
        }
        if constexpr (do_all_moves)
        {
            Bitboard non_capture_targets = targets & vacant_squares;
            while (non_capture_targets)
            {
                m.Add(NonCaptureMove(from, FindAndClearLsb(non_capture_targets), KNIGHT));
            }
        }
    }
    /*
    Generate bishop sliding moves
    */
    Bitboard bishops = bishops_ & friendly_pieces;
    while (bishops)
    {
        const int from = FindAndClearLsb(bishops);
        Bitboard targets = BishopAttacks(occupied_squares, from);
        Bitboard capture_targets = targets & enemy_pieces;
        while (capture_targets)
        {
            const int to = FindAndClearLsb(capture_targets);
            m.Add(CaptureMove(from, to, BISHOP, PieceAt(to)));
        }
        if constexpr (do_all_moves)
        {
            Bitboard non_capture_targets = targets & vacant_squares;
            while (non_capture_targets)
            {
                m.Add(NonCaptureMove(from, FindAndClearLsb(non_capture_targets), BISHOP));;
            }
        }
    }
    /*
    Generate rook sliding moves
    */
    Bitboard rooks = rooks_ & friendly_pieces;
    while (rooks)
    {
        const int from = FindAndClearLsb(rooks);
        Bitboard targets = RookAttacks(occupied_squares, from);
        Bitboard capture_targets = targets & enemy_pieces;
        while (capture_targets)
        {
            const int to = FindAndClearLsb(capture_targets);
            m.Add(CaptureMove(from, to, ROOK, PieceAt(to)));
        }
        if constexpr (do_all_moves)
        {
            Bitboard non_capture_targets = targets & vacant_squares;
            while (non_capture_targets)
            {
               m.Add(NonCaptureMove(from, FindAndClearLsb(non_capture_targets), ROOK));
            }
        }
    }
    /*
    Generate Queen sliding moves
    */
    Bitboard queens = queens_ & friendly_pieces;
    while (queens)
    {
        const int from = FindAndClearLsb(queens);
        Bitboard targets = QueenAttacks(occupied_squares, from);
        Bitboard capture_targets = targets & enemy_pieces;
        while (capture_targets)
        {
            const int to = FindAndClearLsb(capture_targets);
            m.Add(CaptureMove(from, to, QUEEN, PieceAt(to)));
        }
        if constexpr (do_all_moves)
        {
            Bitboard non_capture_targets = targets & vacant_squares;
            while (non_capture_targets)
            {
                m.Add(NonCaptureMove(from, FindAndClearLsb(non_capture_targets), QUEEN));
            }
        }
    }
    /*
    Generate King Moves
    */
    const int king_location = king_location_[color];
    Bitboard targets = SETS[king_location].king_attacks;
    Bitboard capture_targets = targets & enemy_pieces;
    while (capture_targets)
    {
        const int to = FindAndClearLsb(capture_targets);
        m.Add(CaptureMove(king_location, to, KING, PieceAt(to)));
    }
    if constexpr (do_all_moves)
    {
        Bitboard non_capture_targets = targets & vacant_squares;
        while (non_capture_targets)
        {
            m.Add(NonCaptureMove(king_location, FindAndClearLsb(non_capture_targets), KING));
        }
        if (!(flags_ & IS_CHECK))
        {
            /*
            Generate castling moves
            */
            if (color == WHITE)
            {  
                /* 
                Castling to g1 is possible if:
                - white retains the right to castle kingside AND 
                - f1 and g1 are both vacant AND
                - f1 is not attacked by black AND
                - the king's destination is not attacked by black
                */
                if ((flags_ & MAY_WHITE_CASTLE_KINGSIDE) && 
                    !(occupied_squares & (BITBOARD("f1") | BITBOARD("g1"))) &&
                    !IsAttacked(F1, BLACK) &&
                    !IsAttacked(G1, BLACK))
                {
                    m.Add(CastlingMove(E1, G1));
                }
                if ((flags_ & MAY_WHITE_CASTLE_QUEENSIDE) &&
                    !(occupied_squares & (BITBOARD("b1") | BITBOARD("c1") | BITBOARD("d1"))) &&
                    !IsAttacked(D1, BLACK) &&
                    !IsAttacked(C1, BLACK))
                {
                    m.Add(CastlingMove(E1, C1));
                }
            }
            else
            {
                if ((flags_ & MAY_BLACK_CASTLE_KINGSIDE) &&
                    !(occupied_squares & (BITBOARD("f8") | BITBOARD("g8"))) &&
                    !IsAttacked(F8, WHITE) &&
                    !IsAttacked(G8, WHITE))
                {
                    m.Add(CastlingMove(E8, G8));
                }
                if ((flags_ & MAY_BLACK_CASTLE_QUEENSIDE) &&
                    !(occupied_squares & (BITBOARD("b8") | BITBOARD("c8") | BITBOARD("d8"))) &&
                    !IsAttacked(D8, WHITE) &&
                    !IsAttacked(C8, WHITE))
                {
                    m.Add(CastlingMove(E8, C8));;
                }
            }
        }
       
    }
    m.Done();
}
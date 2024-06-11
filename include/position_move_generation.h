#pragma once
#include "function_prototypes.h"
#include "position.h"
#include <vector>

/**
 * @brief Generate pseudo legal moves for the side to move.
 * @return List of moves
 */
template <bool do_all_moves> MoveList Position::GenerateMoves() const
{
    MoveList moves{};
    moves.reserve(64);
    const Color    color            = ColorToMove();
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
    int8_t   push_delta;
    int8_t   west_delta;
    int8_t   east_delta;
    /*
    Generate pawn moves
    */
    if (color == WHITE)
    {
        pawns              = pawns_ & white_pieces_;
        single_pushes      = ShiftNorth(pawns) & vacant_squares;
        double_pushes      = ShiftNorth(single_pushes) & vacant_squares & RANK_4;
        captures_west      = ShiftNorthwest(pawns) & black_pieces_;
        captures_east      = ShiftNortheast(pawns) & black_pieces_;
        en_passant_sources = en_passant_index_ ? SETS[en_passant_index_].pawn_attacks_black & pawns : NO_SQUARES;
        promotions         = single_pushes & RANK_8;
        promotions_west    = captures_west & RANK_8;
        promotions_east    = captures_east & RANK_8;
        push_delta         = 8;
        west_delta         = 7;
        east_delta         = 9;
    }
    else
    {
        pawns              = pawns_ & black_pieces_;
        single_pushes      = ShiftSouth(pawns) & vacant_squares;
        double_pushes      = ShiftSouth(single_pushes) & vacant_squares & RANK_5;
        captures_west      = ShiftSouthwest(pawns) & white_pieces_;
        captures_east      = ShiftSoutheast(pawns) & white_pieces_;
        en_passant_sources = en_passant_index_ ? SETS[en_passant_index_].pawn_attacks_white & pawns : NO_SQUARES;
        promotions         = single_pushes & RANK_1;
        promotions_west    = captures_west & RANK_1;
        promotions_east    = captures_east & RANK_1;
        push_delta         = -8;
        west_delta         = -9;
        east_delta         = -7;
    }
    captures_west ^= promotions_west;
    captures_east ^= promotions_east;
    single_pushes ^= promotions;
    while (promotions_west)
    {
        const Square to       = FindAndClearLsb(promotions_west);
        const Square from     = (Square)(to - west_delta);
        const Piece  captured = PieceAt(to);
        moves.push_back(PromotionMove(from, to, captured, QUEEN));
        moves.push_back(PromotionMove(from, to, captured, ROOK));
        moves.push_back(PromotionMove(from, to, captured, BISHOP));
        moves.push_back(PromotionMove(from, to, captured, KNIGHT));
    }
    while (promotions_east)
    {
        const Square to       = FindAndClearLsb(promotions_east);
        const Square from     = (Square)(to - east_delta);
        const Piece  captured = PieceAt(to);
        moves.push_back(PromotionMove(from, to, captured, QUEEN));
        moves.push_back(PromotionMove(from, to, captured, ROOK));
        moves.push_back(PromotionMove(from, to, captured, BISHOP));
        moves.push_back(PromotionMove(from, to, captured, KNIGHT));
    }
    while (promotions)
    {
        const Square to   = FindAndClearLsb(promotions);
        const Square from = (Square)(to - push_delta);
        moves.push_back(PromotionMove(from, to, NO_PIECE, QUEEN));
        moves.push_back(PromotionMove(from, to, NO_PIECE, ROOK));
        moves.push_back(PromotionMove(from, to, NO_PIECE, BISHOP));
        moves.push_back(PromotionMove(from, to, NO_PIECE, KNIGHT));
    }
    while (captures_west)
    {
        const Square to   = FindAndClearLsb(captures_west);
        const Square from = (Square)(to - west_delta);
        moves.push_back(CaptureMove(from, to, PAWN, PieceAt(to)));
    }
    while (captures_east)
    {
        const Square to   = FindAndClearLsb(captures_east);
        const Square from = (Square)(to - east_delta);
        moves.push_back(CaptureMove(from, to, PAWN, PieceAt(to)));
        ;
    }
    while (en_passant_sources)
    {
        const Square from = FindAndClearLsb(en_passant_sources);
        moves.push_back(EpCaptureMove(from, en_passant_index_));
    }
    if constexpr (do_all_moves)
    {
        push_delta <<= 1;
        while (double_pushes)
        {
            const Square to   = FindAndClearLsb(double_pushes);
            const Square from = (Square)(to - push_delta);
            moves.push_back(DoublePushMove(from, to));
        }
        push_delta >>= 1;
        while (single_pushes)
        {
            const Square to   = FindAndClearLsb(single_pushes);
            const Square from = (Square)(to - push_delta);
            moves.push_back(NonCaptureMove(from, to, PAWN));
        }
    }
    /*
    Generate knight moves
    */
    Bitboard knights = knights_ & friendly_pieces;
    while (knights)
    {
        const Square   from            = FindAndClearLsb(knights);
        const Bitboard targets         = SETS[from].knight_attacks;
        Bitboard       capture_targets = targets & enemy_pieces;
        while (capture_targets)
        {
            const Square to = FindAndClearLsb(capture_targets);
            moves.push_back(CaptureMove(from, to, KNIGHT, PieceAt(to)));
        }
        if constexpr (do_all_moves)
        {
            Bitboard non_capture_targets = targets & vacant_squares;
            while (non_capture_targets)
            {
                moves.push_back(NonCaptureMove(from, FindAndClearLsb(non_capture_targets), KNIGHT));
            }
        }
    }
    /*
    Generate bishop sliding moves
    */
    Bitboard bishops = bishops_ & friendly_pieces;
    while (bishops)
    {
        const Square   from            = FindAndClearLsb(bishops);
        const Bitboard targets         = BishopAttacks(occupied_squares, from);
        Bitboard       capture_targets = targets & enemy_pieces;
        while (capture_targets)
        {
            const Square to = FindAndClearLsb(capture_targets);
            moves.push_back(CaptureMove(from, to, BISHOP, PieceAt(to)));
        }
        if constexpr (do_all_moves)
        {
            Bitboard non_capture_targets = targets & vacant_squares;
            while (non_capture_targets)
            {
                moves.push_back(NonCaptureMove(from, FindAndClearLsb(non_capture_targets), BISHOP));
            }
        }
    }
    /*
    Generate rook sliding moves
    */
    Bitboard rooks = rooks_ & friendly_pieces;
    while (rooks)
    {
        const Square   from            = FindAndClearLsb(rooks);
        const Bitboard targets         = RookAttacks(occupied_squares, from);
        Bitboard       capture_targets = targets & enemy_pieces;
        while (capture_targets)
        {
            const Square to = FindAndClearLsb(capture_targets);
            moves.push_back(CaptureMove(from, to, ROOK, PieceAt(to)));
        }
        if constexpr (do_all_moves)
        {
            Bitboard non_capture_targets = targets & vacant_squares;
            while (non_capture_targets)
            {
                moves.push_back(NonCaptureMove(from, FindAndClearLsb(non_capture_targets), ROOK));
            }
        }
    }
    /*
    Generate Queen sliding moves
    */
    Bitboard queens = queens_ & friendly_pieces;
    while (queens)
    {
        const Square   from            = FindAndClearLsb(queens);
        const Bitboard targets         = QueenAttacks(occupied_squares, from);
        Bitboard       capture_targets = targets & enemy_pieces;
        while (capture_targets)
        {
            const Square to = FindAndClearLsb(capture_targets);
            moves.push_back(CaptureMove(from, to, QUEEN, PieceAt(to)));
        }
        if constexpr (do_all_moves)
        {
            Bitboard non_capture_targets = targets & vacant_squares;
            while (non_capture_targets)
            {
                moves.push_back(NonCaptureMove(from, FindAndClearLsb(non_capture_targets), QUEEN));
            }
        }
    }
    /*
    Generate King Moves
    */
    const Square   king_location   = king_location_[color];
    const Bitboard targets         = SETS[king_location].king_attacks;
    Bitboard       capture_targets = targets & enemy_pieces;
    while (capture_targets)
    {
        const Square to = FindAndClearLsb(capture_targets);
        moves.push_back(CaptureMove(king_location, to, KING, PieceAt(to)));
    }
    if constexpr (do_all_moves)
    {
        Bitboard non_capture_targets = targets & vacant_squares;
        while (non_capture_targets)
        {
            moves.push_back(NonCaptureMove(king_location, FindAndClearLsb(non_capture_targets), KING));
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
                if ((flags_ & MAY_WHITE_CASTLE_KINGSIDE) && !(occupied_squares & (BITBOARD(F1) | BITBOARD(G1))) && !IsAttacked(F1, BLACK) &&
                    !IsAttacked(G1, BLACK))
                {
                    moves.push_back(CastlingMove(E1, G1));
                }
                if ((flags_ & MAY_WHITE_CASTLE_QUEENSIDE) && !(occupied_squares & (BITBOARD(B1) | BITBOARD(C1) | BITBOARD(D1))) &&
                    !IsAttacked(D1, BLACK) && !IsAttacked(C1, BLACK))
                {
                    moves.push_back(CastlingMove(E1, C1));
                }
            }
            else
            {
                if ((flags_ & MAY_BLACK_CASTLE_KINGSIDE) && !(occupied_squares & (BITBOARD(F8) | BITBOARD(G8))) && !IsAttacked(F8, WHITE) &&
                    !IsAttacked(G8, WHITE))
                {
                    moves.push_back(CastlingMove(E8, G8));
                }
                if ((flags_ & MAY_BLACK_CASTLE_QUEENSIDE) && !(occupied_squares & (BITBOARD(B8) | BITBOARD(C8) | BITBOARD(D8))) &&
                    !IsAttacked(D8, WHITE) && !IsAttacked(C8, WHITE))
                {
                    moves.push_back(CastlingMove(E8, C8));
                }
            }
        }
    }
    return moves;
}
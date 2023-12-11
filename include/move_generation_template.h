#pragma once
#include "pawnstar.h"

/**
 * @brief Generate underpromotions to knight, bishop and rook.
 * @param pmoves Pointer to move list.
 */
constexpr void GenerateUnderpromotions(Move **pmoves)
{
    Move *const moves = *pmoves;
    const Move move = *moves;
    moves[1] = move ^ ((ROOK   ^ QUEEN) << 18);
    moves[2] = move ^ ((BISHOP ^ QUEEN) << 18);
    moves[3] = move ^ ((KNIGHT ^ QUEEN) << 18);
    *pmoves += 4;
}

/**
 * @brief Generate pseudo legal moves for the side to move.
 * @tparam do_all_moves true for all moves, false for captures and promotions only.
 * @param position Position to generate moves for.
 * @param moves Where to store the zero terminated list of moves.
 */
template <bool do_all_moves, int color>
void GenerateMoves(const Position& position,
                   Move            moves[])
{
    const Bitboard occupied_squares = position.white_pieces_ | position.black_pieces_;
    const Bitboard vacant_squares = ~occupied_squares;
    const Bitboard friendly_pieces = position.pieces_of_color_[color];
    const Bitboard enemy_pieces = occupied_squares ^ friendly_pieces;
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
    if constexpr (color == WHITE)
    {
        pawns = position.pawns_ & position.white_pieces_;
        single_pushes = ShiftNorth(pawns) & vacant_squares;
        double_pushes = ShiftNorth(single_pushes) & vacant_squares & RANK_4;
        captures_west = ShiftNorthwest(pawns) & position.black_pieces_;
        captures_east = ShiftNortheast(pawns) & position.black_pieces_;
        en_passant_sources = position.en_passant_index_ ? SETS[position.en_passant_index_].pawn_attacks_black & pawns : NO_SQUARES;
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
        pawns = position.pawns_ & position.black_pieces_;
        single_pushes = ShiftSouth(pawns) & vacant_squares;
        double_pushes = ShiftSouth(single_pushes) & vacant_squares & RANK_5;
        captures_west = ShiftSouthwest(pawns) & position.white_pieces_;
        captures_east = ShiftSoutheast(pawns) & position.white_pieces_;
        en_passant_sources = position.en_passant_index_ ? SETS[position.en_passant_index_].pawn_attacks_white & pawns : NO_SQUARES;
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
        *moves = PromotionMove(from, to, PieceAt(position, to), QUEEN);
        GenerateUnderpromotions(&moves);
    }
    while (promotions_east)
    {
        const int to = FindAndClearLsb(promotions_east);
        const int from = to - east_delta;
        *moves = PromotionMove(from, to, PieceAt(position, to), QUEEN);
        GenerateUnderpromotions(&moves);
    }
    while (promotions)
    {
        const int to = FindAndClearLsb(promotions);
        const int from = to - push_delta;
        *moves = PromotionMove(from, to, NO_PIECE, QUEEN);
        GenerateUnderpromotions(&moves);
    }
    while (captures_west)
    {
        const int to = FindAndClearLsb(captures_west);
        const int from = to - west_delta;
        *moves++ = CaptureMove(from, to, PAWN, PieceAt(position, to));
    }
    while (captures_east)
    {
        const int to = FindAndClearLsb(captures_east);
        const int from = to - east_delta;
        *moves++ = CaptureMove(from, to, PAWN, PieceAt(position, to));
    }
    while (en_passant_sources)
    {
        const int from = FindAndClearLsb(en_passant_sources);
        *moves++ = EpCaptureMove(from, position.en_passant_index_);
    }
    if constexpr (do_all_moves)
    {
        push_delta <<= 1;
        while (double_pushes)
        {
            const int to = FindAndClearLsb(double_pushes);
            const int from = to - push_delta;
            *moves++ = DoublePushMove(from, to);
        }
        push_delta >>= 1;
        while (single_pushes)
        {
            const int to = FindAndClearLsb(single_pushes);
            const int from = to - push_delta;
            *moves++ = NonCaptureMove(from, to, PAWN);
        }
    }
    /*
    Generate knight moves
    */
    Bitboard knights = position.knights_ & friendly_pieces;
    while (knights)
    {
        const int from = FindAndClearLsb(knights);
        Bitboard targets = SETS[from].knight_attacks;
        Bitboard capture_targets = targets & enemy_pieces;
        while (capture_targets)
        {
            const int to = FindAndClearLsb(capture_targets);
            *moves++ = CaptureMove(from, to, KNIGHT, PieceAt(position, to));
        }
        if constexpr (do_all_moves)
        {
            Bitboard non_capture_targets = targets & vacant_squares;
            while (non_capture_targets)
            {
                *moves++ = NonCaptureMove(from, FindAndClearLsb(non_capture_targets), KNIGHT);
            }
        }
    }
    /*
    Generate bishop sliding moves
    */
    Bitboard bishops = position.bishops_ & friendly_pieces;
    while (bishops)
    {
        const int from = FindAndClearLsb(bishops);
        Bitboard targets = BishopAttacks(occupied_squares, from);
        Bitboard capture_targets = targets & enemy_pieces;
        while (capture_targets)
        {
            const int to = FindAndClearLsb(capture_targets);
            *moves++ = CaptureMove(from, to, BISHOP, PieceAt(position, to));
        }
        if constexpr (do_all_moves)
        {
            Bitboard non_capture_targets = targets & vacant_squares;
            while (non_capture_targets)
            {
                *moves++ = NonCaptureMove(from, FindAndClearLsb(non_capture_targets), BISHOP);
            }
        }
    }
    /*
    Generate rook sliding moves
    */
    Bitboard rooks = position.rooks_ & friendly_pieces;
    while (rooks)
    {
        const int from = FindAndClearLsb(rooks);
        Bitboard targets = RookAttacks(occupied_squares, from);
        Bitboard capture_targets = targets & enemy_pieces;
        while (capture_targets)
        {
            const int to = FindAndClearLsb(capture_targets);
            *moves++ = CaptureMove(from, to, ROOK, PieceAt(position, to));
        }
        if constexpr (do_all_moves)
        {
            Bitboard non_capture_targets = targets & vacant_squares;
            while (non_capture_targets)
            {
                *moves++ = NonCaptureMove(from, FindAndClearLsb(non_capture_targets), ROOK);
            }
        }
    }
    /*
    Generate Queen sliding moves
    */
    Bitboard queens = position.queens_ & friendly_pieces;
    while (queens)
    {
        const int from = FindAndClearLsb(queens);
        Bitboard targets = QueenAttacks(occupied_squares, from);
        Bitboard capture_targets = targets & enemy_pieces;
        while (capture_targets)
        {
            const int to = FindAndClearLsb(capture_targets);
            *moves++ = CaptureMove(from, to, QUEEN, PieceAt(position, to));
        }
        if constexpr (do_all_moves)
        {
            Bitboard non_capture_targets = targets & vacant_squares;
            while (non_capture_targets)
            {
                *moves++ = NonCaptureMove(from, FindAndClearLsb(non_capture_targets), QUEEN);
            }
        }
    }
    /*
    Generate King Moves
    */
    const int king_location = position.king_location_[color];
    Bitboard targets = SETS[king_location].king_attacks;
    Bitboard capture_targets = targets & enemy_pieces;
    while (capture_targets)
    {
        const int to = FindAndClearLsb(capture_targets);
        *moves++ = CaptureMove(king_location, to, KING, PieceAt(position, to));
    }
    *moves = 0;
    if constexpr (do_all_moves)
    {
        Bitboard non_capture_targets = targets & vacant_squares;
        while (non_capture_targets)
        {
            *moves++ = NonCaptureMove(king_location, FindAndClearLsb(non_capture_targets), KING);
        }
        if (!(position.flags_ & IS_CHECK))
        {
            /*
            Generate castling moves
            */
            if constexpr (color == WHITE)
            {  
                /* 
                Castling to g1 is possible if:
                - white retains the right to castle kingside AND 
                - f1 and g1 are both vacant AND
                - f1 is not attacked by black AND
                - the king's destination is not attacked by black
                */
                if ((position.flags_ & MAY_WHITE_CASTLE_KINGSIDE) && 
                    !(occupied_squares & (BITBOARD("f1") | BITBOARD("g1"))) &&
                    !IsAttacked(position, F1, BLACK) &&
                    !IsAttacked(position, G1, BLACK))
                {
                    *moves++ = CastlingMove(E1, G1);
                }
                if ((position.flags_ & MAY_WHITE_CASTLE_QUEENSIDE) &&
                    !(occupied_squares & (BITBOARD("b1") | BITBOARD("c1") | BITBOARD("d1"))) &&
                    !IsAttacked(position, D1, BLACK) &&
                    !IsAttacked(position, C1, BLACK))
                {
                    *moves++ = CastlingMove(E1, C1);
                }
            }
            else
            {
                if ((position.flags_ & MAY_BLACK_CASTLE_KINGSIDE) &&
                    !(occupied_squares & (BITBOARD("f8") | BITBOARD("g8"))) &&
                    !IsAttacked(position, F8, WHITE) &&
                    !IsAttacked(position, G8, WHITE))
                {
                    *moves++ = CastlingMove(E8, G8);
                }
                if ((position.flags_ & MAY_BLACK_CASTLE_QUEENSIDE) &&
                    !(occupied_squares & (BITBOARD("b8") | BITBOARD("c8") | BITBOARD("d8"))) &&
                    !IsAttacked(position, D8, WHITE) &&
                    !IsAttacked(position, C8, WHITE))
                {
                    *moves++ = CastlingMove(E8, C8);
                }
            }
        }
        *moves = 0;
    }
}
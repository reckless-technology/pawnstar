#pragma once
#include "function_prototypes.h"
#include "position.h"
#include <vector>

/**
 * @brief Generate pseudo legal moves for the side to move.
 * @return List of moves
 */
template <bool do_all_moves> MoveList GenerateMoves(const Position &position)
{
    MoveList moves{};
    moves.reserve(64);
    const Color    color            = position.ColorToMove();
    const Bitboard occupied_squares = position.OccupiedSquares();
    const Bitboard vacant_squares   = ~occupied_squares;
    const Bitboard friendly_pieces  = position.PiecesOfColor(color);
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
        pawns              = position.Pawns() & position.WhitePieces();
        single_pushes      = ShiftNorth(pawns) & vacant_squares;
        double_pushes      = ShiftNorth(single_pushes) & vacant_squares & RANK_4;
        captures_west      = ShiftNorthwest(pawns) & position.BlackPieces();
        captures_east      = ShiftNortheast(pawns) & position.BlackPieces();
        en_passant_sources = position.EnPassantIndex() ? SETS[position.EnPassantIndex()].pawn_attacks_black & pawns : NO_SQUARES;
        promotions         = single_pushes & RANK_8;
        promotions_west    = captures_west & RANK_8;
        promotions_east    = captures_east & RANK_8;
        push_delta         = 8;
        west_delta         = 7;
        east_delta         = 9;
    }
    else
    {
        pawns              = position.Pawns() & position.BlackPieces();
        single_pushes      = ShiftSouth(pawns) & vacant_squares;
        double_pushes      = ShiftSouth(single_pushes) & vacant_squares & RANK_5;
        captures_west      = ShiftSouthwest(pawns) & position.WhitePieces();
        captures_east      = ShiftSoutheast(pawns) & position.WhitePieces();
        en_passant_sources = position.EnPassantIndex() ? SETS[position.EnPassantIndex()].pawn_attacks_white & pawns : NO_SQUARES;
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
        const Piece  captured = position.PieceAt(to);
        moves.push_back(PromotionMove(from, to, captured, QUEEN));
        moves.push_back(PromotionMove(from, to, captured, ROOK));
        moves.push_back(PromotionMove(from, to, captured, BISHOP));
        moves.push_back(PromotionMove(from, to, captured, KNIGHT));
    }
    while (promotions_east)
    {
        const Square to       = FindAndClearLsb(promotions_east);
        const Square from     = (Square)(to - east_delta);
        const Piece  captured = position.PieceAt(to);
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
        moves.push_back(CaptureMove(from, to, PAWN, position.PieceAt(to)));
    }
    while (captures_east)
    {
        const Square to   = FindAndClearLsb(captures_east);
        const Square from = (Square)(to - east_delta);
        moves.push_back(CaptureMove(from, to, PAWN, position.PieceAt(to)));
        ;
    }
    while (en_passant_sources)
    {
        const Square from = FindAndClearLsb(en_passant_sources);
        moves.push_back(EpCaptureMove(from, position.EnPassantIndex()));
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
    Bitboard knights = position.Knights() & friendly_pieces;
    while (knights)
    {
        const Square   from            = FindAndClearLsb(knights);
        const Bitboard targets         = SETS[from].knight_attacks;
        Bitboard       capture_targets = targets & enemy_pieces;
        while (capture_targets)
        {
            const Square to = FindAndClearLsb(capture_targets);
            moves.push_back(CaptureMove(from, to, KNIGHT, position.PieceAt(to)));
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
    Bitboard bishops = position.Bishops() & friendly_pieces;
    while (bishops)
    {
        const Square   from            = FindAndClearLsb(bishops);
        const Bitboard targets         = BishopAttacks(occupied_squares, from);
        Bitboard       capture_targets = targets & enemy_pieces;
        while (capture_targets)
        {
            const Square to = FindAndClearLsb(capture_targets);
            moves.push_back(CaptureMove(from, to, BISHOP, position.PieceAt(to)));
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
    Bitboard rooks = position.Rooks() & friendly_pieces;
    while (rooks)
    {
        const Square   from            = FindAndClearLsb(rooks);
        const Bitboard targets         = RookAttacks(occupied_squares, from);
        Bitboard       capture_targets = targets & enemy_pieces;
        while (capture_targets)
        {
            const Square to = FindAndClearLsb(capture_targets);
            moves.push_back(CaptureMove(from, to, ROOK, position.PieceAt(to)));
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
    Bitboard queens = position.Queens() & friendly_pieces;
    while (queens)
    {
        const Square   from            = FindAndClearLsb(queens);
        const Bitboard targets         = QueenAttacks(occupied_squares, from);
        Bitboard       capture_targets = targets & enemy_pieces;
        while (capture_targets)
        {
            const Square to = FindAndClearLsb(capture_targets);
            moves.push_back(CaptureMove(from, to, QUEEN, position.PieceAt(to)));
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
    const Square   king_location   = position.KingLocation(color);
    const Bitboard targets         = SETS[king_location].king_attacks;
    Bitboard       capture_targets = targets & enemy_pieces;
    while (capture_targets)
    {
        const Square to = FindAndClearLsb(capture_targets);
        moves.push_back(CaptureMove(king_location, to, KING, position.PieceAt(to)));
    }
    if constexpr (do_all_moves)
    {
        Bitboard non_capture_targets = targets & vacant_squares;
        while (non_capture_targets)
        {
            moves.push_back(NonCaptureMove(king_location, FindAndClearLsb(non_capture_targets), KING));
        }
        if (!position.IsInCheck())
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
                if (position.MayWhiteCastleKingside() && !(occupied_squares & (BITBOARD(F1) | BITBOARD(G1))) &&
                    !position.IsAttacked(F1, BLACK) && !position.IsAttacked(G1, BLACK))
                {
                    moves.push_back(CastlingMove(E1, G1));
                }
                if (position.MayWhiteCastleQueenside() && !(occupied_squares & (BITBOARD(B1) | BITBOARD(C1) | BITBOARD(D1))) &&
                    !position.IsAttacked(D1, BLACK) && !position.IsAttacked(C1, BLACK))
                {
                    moves.push_back(CastlingMove(E1, C1));
                }
            }
            else
            {
                if (position.MayBlackCastleKingside() && !(occupied_squares & (BITBOARD(F8) | BITBOARD(G8))) &&
                    !position.IsAttacked(F8, WHITE) && !position.IsAttacked(G8, WHITE))
                {
                    moves.push_back(CastlingMove(E8, G8));
                }
                if (position.MayBlackCastleQueenside() && !(occupied_squares & (BITBOARD(B8) | BITBOARD(C8) | BITBOARD(D8))) &&
                    !position.IsAttacked(D8, WHITE) && !position.IsAttacked(C8, WHITE))
                {
                    moves.push_back(CastlingMove(E8, C8));
                }
            }
        }
    }
    return moves;
}

static inline MoveList GeneratePseudoLegalMoves(const Position &position)
{
    return GenerateMoves<true>(position);
}

static inline MoveList GeneratePseudoLegalCaptures(const Position &position)
{
    return GenerateMoves<false>(position);
}

static inline MoveList GenerateLegalMoves(const Position &position)
{
    MoveList pseudo_legal_moves = GeneratePseudoLegalMoves(position);
    MoveList result;
    for (auto move : pseudo_legal_moves)
    {
        Position dst_position{position.MakeMove(move)};
        if (!dst_position.HasMovedIntoCheck())
        {
            result.push_back(move);
        }
    }
    return result;
}
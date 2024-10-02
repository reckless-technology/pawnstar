#include "static_exchange_evaluation.h"
#include "debug_hashtable.h"
#include "position.h"
#include "transposition_table.h"

#include <algorithm>

/// @brief Use nominal 1-3-3-5-9 material values for SEE
constexpr int piece_values[7] = {0, 100, 300, 300, 500, 900, 10000};

/// @brief Bitboards of piece on the board used for static exchange evaluation.
struct SeeBoards
{
    Bitboard  pawns;
    Bitboard  knights;
    Bitboard  bishops;
    Bitboard  rooks;
    Bitboard  queens;
    Bitboard  kings;
    Bitboard  white_pieces;
    Bitboard  black_pieces;
    Bitboard &PiecesOfType(Piece piece)
    {
        return (&pawns)[piece - PAWN];
    }
    Bitboard &PiecesOfColor(Color color)
    {
        return (&white_pieces)[color];
    }
};

static int EvaluateSwapOff(SeeBoards &bb, Square location, Color color, Piece piece_on_square);

/// @brief Evaluate the SEE for a move.
/// @param src_position Position to evaluate.
/// @param move Move to evaluate.
/// @param is_checking On exit, set to true if the move gave check.
/// @return static exchange evaluation score for this move.
int EvaluateStaticExchange(const Position &src_position, Move move, bool &is_checking)
{
    Position dst_position{src_position.MakeMove(move)};
    is_checking = dst_position.IsInCheck();
    SeeBoards bb{.pawns        = dst_position.Pawns(),
                 .knights      = dst_position.Knights(),
                 .bishops      = dst_position.Bishops(),
                 .rooks        = dst_position.Rooks(),
                 .queens       = dst_position.Queens(),
                 .kings        = dst_position.Kings(),
                 .white_pieces = dst_position.WhitePieces(),
                 .black_pieces = dst_position.BlackPieces()};
    if (move.promoted() != NONE)
    {
        return piece_values[move.captured()] + piece_values[move.promoted()] - piece_values[PAWN] -
               EvaluateSwapOff(bb, move.to(), dst_position.ColorToMove(), move.promoted());
    }
    return piece_values[move.captured()] - EvaluateSwapOff(bb, move.to(), dst_position.ColorToMove(), move.piece());
}

/// @brief Determine the swap off value for a capture on a square.
/// @param bb set of bitboards of piece locations on the board
/// @param location target square
/// @param color color to move
/// @param piece_on_square piece currently standing on the target square
/// @return int swap off (static exchange) score
static int EvaluateSwapOff(SeeBoards &bb, Square location, Color color, Piece piece_on_square)
{
    const Sets    &sets     = SETS[location];
    const Bitboard square   = Bitboard(location);
    Bitboard       occupied = bb.white_pieces | bb.black_pieces;
    int            scores[32];
    int            ply;
    // First pass: perform all the captures onto the square least valuable piece first.
    for (ply = 0; ply != 32; ++ply)
    {
        // Find the least valuable piece of color which directly attacks location.
        Piece          capturing_piece;
        Bitboard       bishop_attacks;
        Bitboard       rook_attacks;
        const Bitboard attacking_pieces = bb.PiecesOfColor(color);
        Bitboard       attackers        = sets.PawnAttacks(EnemyOf(color)) & attacking_pieces & bb.pawns;
        if (attackers != NO_SQUARES)
        {
            capturing_piece = PAWN;
            goto FoundAttacker;
        }
        attackers = sets.knight_attacks & attacking_pieces & bb.knights;
        if (attackers != NO_SQUARES)
        {
            capturing_piece = KNIGHT;
            goto FoundAttacker;
        }
        bishop_attacks = BishopAttacks(occupied, location);
        attackers      = bishop_attacks & attacking_pieces & bb.bishops;
        if (attackers != NO_SQUARES)
        {
            capturing_piece = BISHOP;
            goto FoundAttacker;
        }
        rook_attacks = RookAttacks(occupied, location);
        attackers    = rook_attacks & attacking_pieces & bb.rooks;
        if (attackers != NO_SQUARES)
        {
            capturing_piece = ROOK;
            goto FoundAttacker;
        }
        attackers = (bishop_attacks | rook_attacks) & attacking_pieces & bb.queens;
        if (attackers != NO_SQUARES)
        {
            capturing_piece = QUEEN;
            goto FoundAttacker;
        }
        attackers = sets.king_attacks & attacking_pieces & bb.kings;
        if (attackers != NO_SQUARES)
        {
            capturing_piece = KING;
            goto FoundAttacker;
        }

        // No more attackers - finished first pass.
        scores[ply] = 0;
        break;

    FoundAttacker:
        attackers.IsolateLsb();
        bb.PiecesOfType(piece_on_square) ^= square;
        bb.PiecesOfColor(EnemyOf(color)) ^= square;
        bb.PiecesOfType(capturing_piece) ^= attackers | square;
        bb.PiecesOfColor(color) ^= attackers | square;
        occupied ^= attackers;
        scores[ply]     = piece_values[piece_on_square];
        piece_on_square = capturing_piece;
        color           = EnemyOf(color);
    }
    // Second pass: unwind the capture stack and propagate values back to the top for material winning sequences.
    for (--ply; ply >= 0; --ply)
    {
        scores[ply] -= scores[ply + 1];
        if (scores[ply] < 0)
        {
            scores[ply] = 0; // Player would not initiate a losing capture sequence.
        }
    }
    return scores[0];
}
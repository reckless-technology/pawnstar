#include "static_exchange_evaluation.h"
#include "debug_hashtable.h"
#include "position.h"
#include "transposition_table.h"

#include <algorithm>

/// @brief Use nominal 1-3-3-5-9 material values for SEE
constexpr int piece_values[7] = {0, 100, 300, 300, 500, 900, 10000};

/// @brief Bitboards of piece on the board used for static exchange evaluation.
struct SeeBoard
{
    Bitboard pawns;
    Bitboard knights;
    Bitboard bishops;
    Bitboard rooks;
    Bitboard queens;
    Bitboard kings;
    Bitboard white_pieces;
    Bitboard black_pieces;

    SeeBoard(const Position &p)
        : pawns(p.Pawns()), knights(p.Knights()), bishops(p.Bishops()), rooks(p.Rooks()), queens(p.Queens()),
          kings(p.Kings()), white_pieces(p.WhitePieces()), black_pieces(p.BlackPieces())
    {
    }

    Bitboard &PiecesOfType(Piece piece)
    {
        return (&pawns)[piece - PAWN];
    }

    Bitboard &PiecesOfColor(Color color)
    {
        return (&white_pieces)[color];
    }
};

static int EvaluateSwapOff(SeeBoard &bb, Square location, Color color, Piece piece_on_square);

/// @brief Evaluate the static exchange evaluation for a move.
/// @param src_position Position to evaluate.
/// @param move Move to evaluate.
/// @param is_checking On exit, set to true if the move gave check.
/// @return static exchange evaluation score for this move, and whether move gives check.
std::pair<int, bool> EvaluateStaticExchange(const Position &src_position, Move move)
{
    Position    dst_position{src_position.MakeMove(move)};
    const bool  is_checking = dst_position.IsInCheck();
    SeeBoard    bb{dst_position};
    const Piece piece    = src_position.PieceAt(move.from());
    const Piece captured = move.type() == Move::EP_CAPTURE ? PAWN : src_position.PieceAt(move.to());
    const Piece promoted = move.promoted();
    if (promoted != NO_PIECE)
    {
        return {piece_values[captured] + piece_values[promoted] - piece_values[PAWN] -
                    EvaluateSwapOff(bb, move.to(), dst_position.ColorToMove(), move.promoted()),
                is_checking};
    }
    return {piece_values[captured] - EvaluateSwapOff(bb, move.to(), dst_position.ColorToMove(), piece), is_checking};
}

/// @brief Determine the swap off value for a capture on a square.
/// @param bb set of bitboards of piece locations on the board
/// @param location target square
/// @param color color to move
/// @param piece_on_square piece currently standing on the target square
/// @return int swap off (static exchange) score
static int EvaluateSwapOff(SeeBoard &bb, Square location, Color color, Piece piece_on_square)
{
    const Bitboard pawn_attacks   = color == WHITE ? PAWN_ATTACKS_BLACK[location] : PAWN_ATTACKS_WHITE[location];
    const Bitboard knight_attacks = KNIGHT_ATTACKS[location];
    const Bitboard king_attacks   = KING_ATTACKS[location];
    const Bitboard square         = Bitboard(location);
    Bitboard       occupied       = bb.white_pieces | bb.black_pieces;
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
        Bitboard       attackers        = pawn_attacks & attacking_pieces & bb.pawns;
        if (attackers.IsNotEmpty())
        {
            capturing_piece = PAWN;
            goto FoundAttacker;
        }
        attackers = knight_attacks & attacking_pieces & bb.knights;
        if (attackers.IsNotEmpty())
        {
            capturing_piece = KNIGHT;
            goto FoundAttacker;
        }
        bishop_attacks = BishopAttacks(occupied, location);
        attackers      = bishop_attacks & attacking_pieces & bb.bishops;
        if (attackers.IsNotEmpty())
        {
            capturing_piece = BISHOP;
            goto FoundAttacker;
        }
        rook_attacks = RookAttacks(occupied, location);
        attackers    = rook_attacks & attacking_pieces & bb.rooks;
        if (attackers.IsNotEmpty())
        {
            capturing_piece = ROOK;
            goto FoundAttacker;
        }
        attackers = (bishop_attacks | rook_attacks) & attacking_pieces & bb.queens;
        if (attackers.IsNotEmpty())
        {
            capturing_piece = QUEEN;
            goto FoundAttacker;
        }
        attackers = king_attacks & attacking_pieces & bb.kings;
        if (attackers.IsNotEmpty())
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
        // Handle special case of pawn capture promotion
        if (piece_on_square == PAWN && (square & (RANK_1 | RANK_8)).IsNotEmpty())
        {
            piece_on_square = QUEEN;
            bb.pawns ^= square;
            bb.queens ^= square;
        }
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
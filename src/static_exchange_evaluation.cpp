#include "static_exchange_evaluation.h"
#include "debug_hashtable.h"
#include "position.h"
#include "transposition_table.h"

#include <algorithm>

/*
Use nominal 1-3-3-5-9 material values for SEE
*/
constexpr int piece_values[7] = {0, 100, 300, 300, 500, 900, 10000};

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
/*
Determine the SEE (static exchange evaluation) for a move.
Refer to: http://chessprogramming.wikispaces.com/Static+Exchange+Evaluation
*/
int EvaluateStaticExchange(const Position &src_position, Move move, int &is_checking)
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

/**
 * @brief Determine the swap off value for a capture on a square.
 *
 * @param bb set of bitboards of piece locations on the board
 * @param location target square
 * @param color color to move
 * @param piece_on_square piece currently standing on the target square
 * @return int swap off (static exchange) score
 */
static int EvaluateSwapOff(SeeBoards &bb, Square location, Color color, Piece piece_on_square)
{
    const Sets    &sets     = SETS[location];
    const Bitboard square   = BITBOARD(location);
    Bitboard       occupied = bb.white_pieces | bb.black_pieces;
    int            scores[32];
    int            ply;
    /* First pass: perform all the captures onto the square least valuable piece first */
    for (ply = 0; ply != 32; ++ply)
    {
        /* Find the least valuable piece of color which directly attacks location */
        Piece          capturing_piece;
        Bitboard       bishop_attacks;
        Bitboard       rook_attacks;
        const Bitboard attacking_pieces = bb.PiecesOfColor(color);
        Bitboard       attackers        = sets.PawnAttacks(EnemyOf(color)) & attacking_pieces & bb.pawns;
        if (attackers)
        {
            attackers &= -attackers; /* isolate Lsb in case there is more than 1 pawn attacker */
            capturing_piece = PAWN;
            goto FoundAttacker;
        }
        attackers = sets.knight_attacks & attacking_pieces & bb.knights;
        if (attackers)
        {
            attackers &= -attackers; /* isolate Lsb in case there is more than 1 knight attacker */
            capturing_piece = KNIGHT;
            goto FoundAttacker;
        }
        bishop_attacks = BishopAttacks(occupied, location);
        attackers      = bishop_attacks & attacking_pieces & bb.bishops;
        if (attackers)
        {
            capturing_piece = BISHOP;
            attackers &= -attackers;
            goto FoundAttacker;
        }
        rook_attacks = RookAttacks(occupied, location);
        attackers    = rook_attacks & attacking_pieces & bb.rooks;
        if (attackers)
        {
            capturing_piece = ROOK;
            attackers &= -attackers;
            goto FoundAttacker;
        }
        attackers = (bishop_attacks | rook_attacks) & attacking_pieces & bb.queens;
        if (attackers)
        {
            capturing_piece = QUEEN;
            attackers &= -attackers;
            goto FoundAttacker;
        }
        attackers = sets.king_attacks & attacking_pieces & bb.kings;
        if (attackers)
        {
            capturing_piece = KING;
            goto FoundAttacker;
        }

        /* No more attackers - finished first pass */
        scores[ply] = 0;
        break;

    FoundAttacker:
        bb.PiecesOfType(piece_on_square) ^= square;
        bb.PiecesOfColor(EnemyOf(color)) ^= square;
        bb.PiecesOfType(capturing_piece) ^= attackers | square;
        bb.PiecesOfColor(color) ^= attackers | square;
        occupied ^= attackers;
        scores[ply]     = piece_values[piece_on_square];
        piece_on_square = capturing_piece;
        color           = EnemyOf(color);
    }
    /* Second pass: unwind the capture stack and propagate values
       back to the top for material winning sequences */
    for (--ply; ply >= 0; --ply)
    {
        scores[ply] -= scores[ply + 1];
        if (scores[ply] < 0)
        {
            scores[ply] = 0; /* would not initiate a losing capture sequence */
        }
    }
    return scores[0];
}
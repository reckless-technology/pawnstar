#include "debug_hashtable.h"
#include "function_prototypes.h"
#include "position.h"
#include "transposition_table.h"

/*
Use nominal 1-3-3-5-9 material values for SEE
*/
static const int piece_values[7] = {0, 100, 300, 300, 500, 900, 10000};

struct Bitboards
{
    union {
        Bitboard pieces[6];
        struct
        {
            Bitboard pawns;
            Bitboard knights;
            Bitboard bishops;
            Bitboard rooks;
            Bitboard queens;
            Bitboard kings;
        };
    };
    union {
        Bitboard pieces_of_color[2];
        struct
        {
            Bitboard white_pieces;
            Bitboard black_pieces;
        };
    };
};

static int EvaluateSwapOff(Bitboards &bb, Square location, Color color, Piece piece_on_square);
/*
Determine the SEE (static exchange evaluation) for a move.
Refer to: http://chessprogramming.wikispaces.com/Static+Exchange+Evaluation
*/
int EvaluateStaticExchange(const Position &src_position, Move move, bool &is_checking)
{
    Position position{src_position.MakeMove(move)};
    if (position.HasMovedIntoCheck())
    {
        return MOVED_INTO_CHECK_SCORE;
    }
    is_checking = position.IsInCheck();
    Bitboards bb{.pawns        = position.Pawns(),
                 .knights      = position.Knights(),
                 .bishops      = position.Bishops(),
                 .rooks        = position.Rooks(),
                 .queens       = position.Queens(),
                 .kings        = position.Kings(),
                 .white_pieces = position.WhitePieces(),
                 .black_pieces = position.BlackPieces()};
    if (MovePromoted(move))
    {
        return piece_values[MoveCaptured(move)] + piece_values[MovePromoted(move)] - piece_values[PAWN] -
               EvaluateSwapOff(bb, MoveTo(move), position.ColorToMove(), MovePromoted(move));
    }
    return piece_values[MoveCaptured(move)] - EvaluateSwapOff(bb, MoveTo(move), position.ColorToMove(), MovePiece(move));
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
static int EvaluateSwapOff(Bitboards &bb, Square location, Color color, Piece piece_on_square)
{
    const Sets           &sets                = SETS[location];
    const Bitboard        square              = BITBOARD(location);
    const Bitboard *const intervening_squares = &INTERVENING_SQUARES[location][0];
    Bitboard              occupied            = bb.white_pieces | bb.black_pieces;
    int                   scores[32];
    int                   ply;
    /* First pass: perform all the captures onto the square least valuable piece first */
    for (ply = 0; ply != 32; ++ply)
    {
        /* Find the least valuable piece of color which directly attacks location */
        Piece          capturing_piece;
        Bitboard       sliders;
        const Bitboard attacking_pieces = bb.pieces_of_color[color];
        Bitboard       attacker         = sets.pawn_attacks[EnemyOf(color)] & attacking_pieces & bb.pawns;
        if (attacker)
        {
            attacker &= -attacker; /* isolate Lsb in case there is more than 1 pawn attacker */
            capturing_piece = PAWN;
            goto FoundAttacker;
        }
        attacker = sets.knight_attacks & attacking_pieces & bb.knights;
        if (attacker)
        {
            attacker &= -attacker; /* isolate Lsb in case there is more than 1 knight attacker */
            capturing_piece = KNIGHT;
            goto FoundAttacker;
        }
        sliders = sets.bishop_attacks & attacking_pieces & bb.bishops;
        while (sliders)
        {
            const Square slider_locn = FindAndClearLsb(sliders);
            if (!(intervening_squares[slider_locn] & occupied))
            {
                capturing_piece = BISHOP;
                attacker        = BITBOARD(slider_locn);
                goto FoundAttacker;
            }
        }
        sliders = sets.rook_attacks & attacking_pieces & bb.rooks;
        while (sliders)
        {
            const Square slider_locn = FindAndClearLsb(sliders);
            if (!(intervening_squares[slider_locn] & occupied))
            {
                capturing_piece = ROOK;
                attacker        = BITBOARD(slider_locn);
                goto FoundAttacker;
            }
        }
        sliders = sets.queen_attacks & attacking_pieces & bb.queens;
        while (sliders)
        {
            const Square slider_locn = FindAndClearLsb(sliders);
            if (!(intervening_squares[slider_locn] & occupied))
            {
                capturing_piece = QUEEN;
                attacker        = BITBOARD(slider_locn);
                goto FoundAttacker;
            }
        }
        attacker = sets.king_attacks & attacking_pieces & bb.kings;
        if (attacker)
        {
            capturing_piece = KING;
            goto FoundAttacker;
        }

        /* No more attackers - finished first pass */
        scores[ply] = 0;
        break;

    FoundAttacker:
        bb.pieces[piece_on_square - 1] ^= square;
        bb.pieces_of_color[EnemyOf(color)] ^= square;
        bb.pieces[capturing_piece - 1] ^= attacker | square;
        bb.pieces_of_color[color] ^= attacker | square;
        occupied ^= attacker;
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
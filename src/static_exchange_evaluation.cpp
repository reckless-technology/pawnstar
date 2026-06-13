#include "static_exchange_evaluation.h"
#include "debug_hashtable.h"
#include "position.h"
#include "transposition_table.h"

#include <algorithm>

/// @brief Use nominal 1-3-3-5-9 material values for SEE
constexpr std::array<int, 7> piece_values{0, 100, 300, 300, 500, 900, 10000};

/// @brief Bitboards of piece on the board used for static exchange evaluation.
struct SeeBoard
{
    std::array<Bitboard, 7> pieces_; ///< Per-piece-type bitboards indexed by Piece (index 0 / kNone unused).
    std::array<Bitboard, 2> colors_; ///< Per-color bitboards indexed by Color.

    /// @brief Construct the working board from a position.
    /// @param p Position to copy piece bitboards from.
    constexpr SeeBoard(const Position &p) : pieces_(p.pieces_), colors_(p.colors_)
    {
    }
};

static int EvaluateSwapOff(SeeBoard &bb, Square location, Color color, Piece piece_on_square);

/// @brief Evaluate the static exchange evaluation for a move.
/// @param src_position Position to evaluate.
/// @param move Move to evaluate.
/// @return Pair of {static exchange evaluation score for this move, whether the move gives check}.
std::pair<int, bool> EvaluateStaticExchange(const Position &src_position, Move move)
{
    Position    dst_position{src_position.MakeMove(move)};
    const bool  is_checking = dst_position.IsInCheck();
    SeeBoard    bb{dst_position};
    const Piece piece    = src_position.PieceAt(move.from());
    const Piece captured = move.type() == Move::kEpCapture ? kPawn : src_position.PieceAt(move.to());
    const Piece promoted = move.promoted();
    if (promoted != Piece::kNone)
    {
        return {piece_values[captured] + piece_values[promoted] - piece_values[kPawn] -
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
    // Squares a pawn of each color must stand on to attack location. A white pawn attacks location from the squares a
    // black pawn on location would attack, and vice versa. The relevant mask depends on whose turn it is, so it must be
    // reselected each ply as color alternates (otherwise a recapturing pawn of the other color is missed).
    const Bitboard      white_pawn_attacks = kPawnAttacksBlack[location];
    const Bitboard      black_pawn_attacks = kPawnAttacksWhite[location];
    const Bitboard      knight_attacks     = kKnightAttacks[location];
    const Bitboard      king_attacks       = kKingAttacks[location];
    const Bitboard      square             = Bitboard(location);
    Bitboard            occupied           = bb.pieces_[kNone]; // pieces_[kNone] holds the occupied-squares bitboard
    std::array<int, 32> scores;
    int                 ply;
    // First pass: perform all the captures onto the square least valuable piece first.
    for (ply = 0; ply != 32; ++ply)
    {
        // Find the least valuable piece of color which directly attacks location.
        Piece          capturing_piece;
        Bitboard       bishop_attacks;
        Bitboard       rook_attacks;
        const Bitboard pawn_attacks     = color == kWhite ? white_pawn_attacks : black_pawn_attacks;
        const Bitboard attacking_pieces = bb.colors_[color];
        Bitboard       attackers        = pawn_attacks & attacking_pieces & bb.pieces_[kPawn];
        if (attackers.IsNotEmpty())
        {
            capturing_piece = kPawn;
            goto FoundAttacker;
        }
        attackers = knight_attacks & attacking_pieces & bb.pieces_[kKnight];
        if (attackers.IsNotEmpty())
        {
            capturing_piece = kKnight;
            goto FoundAttacker;
        }
        bishop_attacks = BishopAttacks(occupied, location);
        attackers      = bishop_attacks & attacking_pieces & bb.pieces_[kBishop];
        if (attackers.IsNotEmpty())
        {
            capturing_piece = kBishop;
            goto FoundAttacker;
        }
        rook_attacks = RookAttacks(occupied, location);
        attackers    = rook_attacks & attacking_pieces & bb.pieces_[kRook];
        if (attackers.IsNotEmpty())
        {
            capturing_piece = kRook;
            goto FoundAttacker;
        }
        attackers = (bishop_attacks | rook_attacks) & attacking_pieces & bb.pieces_[kQueen];
        if (attackers.IsNotEmpty())
        {
            capturing_piece = kQueen;
            goto FoundAttacker;
        }
        attackers = king_attacks & attacking_pieces & bb.pieces_[kKing];
        if (attackers.IsNotEmpty())
        {
            capturing_piece = kKing;
            goto FoundAttacker;
        }

        // No more attackers - finished first pass.
        scores[ply] = 0;
        break;

    FoundAttacker:
        attackers.IsolateLsb();
        bb.pieces_[piece_on_square] ^= square;
        bb.colors_[EnemyOf(color)] ^= square;
        bb.pieces_[capturing_piece] ^= attackers | square;
        bb.colors_[color] ^= attackers | square;
        occupied ^= attackers;
        scores[ply]     = piece_values[piece_on_square];
        piece_on_square = capturing_piece;
        color           = EnemyOf(color);
        // Handle special case of pawn capture promotion
        if (piece_on_square == kPawn && (square & (kRank1 | kRank8)).IsNotEmpty())
        {
            piece_on_square = kQueen;
            bb.pieces_[kPawn] ^= square;
            bb.pieces_[kQueen] ^= square;
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
#pragma once
/// @file static_exchange_evaluation.h Static exchange evaluation (SEE) for capture move ordering and pruning.
#include <algorithm>
#include <array>
#include <utility>

#include "move.h"
#include "position.h"

/// @brief Use nominal 1-3-3-5-9 material values for SEE
inline constexpr std::array<int, 7> piece_values{0, 100, 300, 300, 500, 900, 10000};

/// @brief Bitboards of piece on the board used for static exchange evaluation.
struct SeeBoard
{
    std::array<Bitboard, 7> pieces_; ///< Per-piece-type bitboards indexed by Piece (index 0 / kNone unused).
    std::array<Bitboard, 2> colors_; ///< Per-color bitboards indexed by Color.
    constexpr SeeBoard(const Position &p);
};

/// @brief Construct the working board from a position.
/// @param p Position to copy piece bitboards from.
constexpr SeeBoard::SeeBoard(const Position &p) : pieces_(p.pieces_), colors_(p.colors_)
{
}

/// @brief Determine the swap off value for a capture on a square.
/// @param bb set of bitboards of piece locations on the board
/// @param location target square
/// @param color color to move
/// @param piece_on_square piece currently standing on the target square
/// @return int swap off (static exchange) score
inline int EvaluateSwapOff(SeeBoard &bb, Square location, Color color, Piece piece_on_square)
{
    // Squares a pawn of each color must stand on to attack location. A white pawn attacks location from the squares a
    // black pawn on location would attack, and vice versa. The relevant mask depends on whose turn it is, so it must be
    // reselected each ply as color alternates (otherwise a recapturing pawn of the other color is missed).
    const Bitboard      white_pawn_attacks = kPawnAttacksBlack[location];
    const Bitboard      black_pawn_attacks = kPawnAttacksWhite[location];
    const Bitboard      knight_attacks     = kKnightAttacks[location];
    const Bitboard      king_attacks       = kKingAttacks[location];
    const Bitboard      square             = Bitboard(location);
    Bitboard            occupied           = bb.pieces_[kNone]; // pieces[kNone] holds the occupied-squares bitboard
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

/// @brief SeeBoard mirror of Position::AttacksTo: the set of @p color pieces attacking @p location, using
/// the working board's bitboards and its occupancy (bb.pieces_[kNone]). Kept byte-for-byte equivalent to
/// Position::AttacksTo so the SEE check test matches Position::checkers_ exactly.
/// @param bb Working board (after the capturing move has been applied).
/// @param location Target square (the opposing king, for a check test).
/// @param color Attacking color.
/// @return Bitboard of @p color attackers of @p location.
inline Bitboard SeeAttackersTo(const SeeBoard &bb, Square location, Color color)
{
    const Bitboard occupied = bb.pieces_[kNone];
    Bitboard result = (color == kWhite ? kPawnAttacksBlack[location] : kPawnAttacksWhite[location]) & bb.pieces_[kPawn];
    result |= (kKnightAttacks[location] & bb.pieces_[kKnight]) | (kKingAttacks[location] & bb.pieces_[kKing]);
    result |= RookAttacks(occupied, location) & (bb.pieces_[kRook] | bb.pieces_[kQueen]);
    result |= BishopAttacks(occupied, location) & (bb.pieces_[kBishop] | bb.pieces_[kQueen]);
    result &= bb.colors_[color];
    return result;
}

/// @brief MakeMove-based SEE, kept only as the reference the cross-check test asserts the fast path against.
/// @param src_position Position to evaluate. @param move Move to evaluate.
/// @return Pair of {static exchange evaluation score, whether the move gives check}.
inline std::pair<int, bool> EvaluateStaticExchangeReference(const Position &src_position, Move move)
{
    Position    dst_position{src_position.MakeMove(move)};
    const bool  is_checking = dst_position.IsInCheck();
    SeeBoard    bb{dst_position};
    const Piece piece    = src_position.squares_[move.from()];
    const Piece captured = move.type() == Move::kEpCapture ? kPawn : src_position.squares_[move.to()];
    const Piece promoted = move.promoted();
    if (promoted != Piece::kNone)
    {
        return {piece_values[captured] + piece_values[promoted] - piece_values[kPawn] -
                    EvaluateSwapOff(bb, move.to(), dst_position.color_to_move_, move.promoted()),
                is_checking};
    }
    return {piece_values[captured] - EvaluateSwapOff(bb, move.to(), dst_position.color_to_move_, piece), is_checking};
}

/// @brief Compute the static exchange evaluation of a capture, resolving the full capture sequence.
///
/// SEE only needs the post-move piece/colour bitboards and occupancy, so rather than a full Position
/// MakeMove (which also recomputes the Zobrist hash, castling rights, en passant and the checkers
/// bitboard, then copies squares_[64]) this applies just the capturing move to a lightweight SeeBoard.
/// The check flag is derived directly: a move gives check iff the side that just moved now attacks the
/// opposing king, which covers direct, discovered, promotion and en-passant-discovered checks uniformly
/// — the same definition Position uses for checkers_. Bit-identical to EvaluateStaticExchangeReference
/// for every capture/promotion (asserted by test_see's cross-check); SEE is never called on castling.
/// @param src_position Position to evaluate.
/// @param move Move to evaluate.
/// @return Pair of {static exchange evaluation score for this move, whether the move gives check}.
inline std::pair<int, bool> EvaluateStaticExchange(const Position &src_position, Move move)
{
    const Color  color    = src_position.color_to_move_;
    const Color  enemy    = EnemyOf(color);
    const Square from     = move.from();
    const Square to       = move.to();
    const Piece  mover    = src_position.squares_[from];
    const Piece  promoted = move.promoted();
    const bool   is_ep    = move.type() == Move::kEpCapture;
    // En-passant captures the pawn on `to`'s file at `from`'s rank; otherwise the captured piece sits on `to`
    // (kNone for a non-capturing promotion).
    const Square captured_square = is_ep ? Square(to.File(), from.Rank()) : to;
    const Piece  captured        = is_ep ? kPawn : src_position.squares_[to];
    const Piece  piece_on_to     = promoted != Piece::kNone ? promoted : mover;

    // Apply only the capturing move to a working board (no Zobrist/castling/squares_ recompute).
    SeeBoard       bb{src_position};
    const Bitboard from_bb{from};
    const Bitboard to_bb{to};
    if (captured != Piece::kNone) // a non-capturing promotion leaves `to` empty
    {
        const Bitboard captured_bb{captured_square};
        bb.pieces_[captured] ^= captured_bb;
        bb.colors_[enemy] ^= captured_bb;
    }
    bb.pieces_[mover] ^= from_bb; // lift the mover off `from`...
    bb.colors_[color] ^= from_bb;
    bb.pieces_[piece_on_to] ^= to_bb; // ...and place it (promoted type if promoting) on `to`
    bb.colors_[color] ^= to_bb;
    bb.pieces_[kNone] = bb.colors_[kWhite] | bb.colors_[kBlack]; // refreshed occupancy for the swap-off

    // Check = the side that just moved now attacks the opposing king (matches Position::checkers_).
    const Square enemy_king  = (bb.pieces_[kKing] & bb.colors_[enemy]).Lsb();
    const bool   is_checking = SeeAttackersTo(bb, enemy_king, color).IsNotEmpty();

    if (promoted != Piece::kNone)
    {
        return {piece_values[captured] + piece_values[promoted] - piece_values[kPawn] -
                    EvaluateSwapOff(bb, to, enemy, promoted),
                is_checking};
    }
    return {piece_values[captured] - EvaluateSwapOff(bb, to, enemy, mover), is_checking};
}

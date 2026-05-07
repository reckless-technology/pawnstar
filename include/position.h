#pragma once

#include <array>
#include <format>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include "attacks.h"
#include "bitboard.h"
#include "castling_rights.h"
#include "move.h"

/// @brief Class to represent a chess position. This is the primary class that holds the state of the board and
/// represents the rules of chess.
class Position
{
  public:
    Position()                                      = default;
    Position(const Position &that)                  = default;
    Position       &operator=(const Position &that) = default;
    static Position FromString(std::string_view fen_string);
    Position        MakeMove(const Move &move) const;
    Position        MakeNullMove() const;
    bool            IsLegal() const;
    bool            IsCheckmate() const;
    bool            IsStalemate() const;
    bool            IsDrawByMaterial() const;
    std::string     ToString() const;
    // clang-format off
    // Const accessors.
    constexpr Bitboard  AttacksTo(Square s, Color c) const;
    constexpr MoveList  GenerateLegalMoves() const          {return ColorToMove() == kWhite ? GenMoves<kWhite, true>()  : GenMoves<kBlack, true>();}
    constexpr MoveList  GenerateLegalCaptures() const       {return ColorToMove() == kWhite ? GenMoves<kWhite, false>() : GenMoves<kBlack, false>();}
    constexpr bool      IsAttacked(Square s, Color c) const {return AttacksTo(s, c).IsNotEmpty();}
    constexpr bool      MayWhiteCastleKingside() const      {return castling_rights_.MayWhiteCastleKingside();}
    constexpr bool      MayWhiteCastleQueenside() const     {return castling_rights_.MayWhiteCastleQueenside();}
    constexpr bool      MayBlackCastleKingside() const      {return castling_rights_.MayBlackCastleKingside();}
    constexpr bool      MayBlackCastleQueenside() const     {return castling_rights_.MayBlackCastleQueenside();}
    constexpr bool      IsNullMove() const                  {return !!(state_flags_ & kIsNullMove);}
    constexpr Color     ColorToMove() const                 {return state_flags_ & kIsBlackToMove ? kBlack : kWhite;}
    constexpr Bitboard  Pawns() const                       {return pawns_;}
    constexpr Bitboard  Knights() const                     {return knights_;}
    constexpr Bitboard  Bishops() const                     {return bishops_;}
    constexpr Bitboard  Rooks() const                       {return rooks_;}
    constexpr Bitboard  Queens() const                      {return queens_;}
    constexpr Bitboard  Kings() const                       {return kings_;}
    constexpr Bitboard  WhitePieces() const                 {return white_pieces_;}
    constexpr Bitboard  BlackPieces() const                 {return black_pieces_;}
    constexpr Bitboard  OccupiedSquares() const             {return white_pieces_ | black_pieces_;}
    constexpr Bitboard  VacantSquares() const               {return ~(white_pieces_ | black_pieces_);}
    constexpr Piece     PieceAt(Square location) const      {return squares_[location];}
    constexpr Bitboard  PiecesOfColor(Color color) const    {return (&white_pieces_)[color];}
    constexpr Bitboard  PiecesOfType(Piece piece) const     {return (&pawns_)[piece - kPawn];}
    constexpr Square    KingLocation(Color color) const     {return king_location_[color];}
    constexpr zobrist_t Hash() const                        {return hash_;}
    constexpr bool      IsInCheck() const                   {return checkers_.IsNotEmpty();}
    constexpr uint8_t   MoveCount() const                   {return full_move_count_;}
    constexpr uint8_t   ReversibleMoveCount() const         {return reversible_move_count_;}
    constexpr Square    EnPassantIndex() const              {return en_passant_square_;}
    // Non const accessors.
    constexpr Bitboard& PiecesOfColor(Color color)          {return (&white_pieces_)[color];}
    constexpr Bitboard& PiecesOfType(Piece piece)           {return (&pawns_)[piece - kPawn];}
    // clang-format on

  private:
    constexpr void      AddPiece(Color color, Piece piece, Square to);               ///< Place a piece on the board.
    constexpr void      RemovePiece(Color color, Piece piece, Square from);          ///< Remove a piece from the board.
    constexpr void      MovePiece(Color color, Piece piece, Square from, Square to); ///< Move a piece on the board.
    constexpr zobrist_t ComputeHash() const;                    ///< Compute the Zobrist hash from scratch.
    template <Color, bool> constexpr MoveList GenMoves() const; ///< Generate legal moves.

    // State variables.
    Bitboard              pawns_;                 ///< Squares with a pawn on them.
    Bitboard              knights_;               ///< Squares with a knight on them.
    Bitboard              bishops_;               ///< Squares with a bishop on them.
    Bitboard              rooks_;                 ///< Squares with a rook on them.
    Bitboard              queens_;                ///< Squares with a queen on them.
    Bitboard              kings_;                 ///< Squares with a king on them.
    Bitboard              white_pieces_;          ///< Squares with a white piece on them.
    Bitboard              black_pieces_;          ///< Squares with a black piece on them.
    std::array<Piece, 64> squares_;               ///< Squares array for fast piece lookup.
    Bitboard              checkers_;              ///< Set of squares which attack the king.
    zobrist_t             hash_;                  ///< Zobrist hash of this position, maintained incrementally.
    std::array<Square, 2> king_location_;         ///< Square index of white and black kings.
    Square                en_passant_square_;     ///< En passant capture availability square.
    uint8_t               reversible_move_count_; ///< Number of consecutive reversible half-moves (plies).
    uint8_t               full_move_count_;       ///< Number of full moves (zero indexed).
    CastlingRights        castling_rights_;       ///< Castling rights flags.
    uint8_t               state_flags_;           ///< Position state flags.

    /// @brief Bit values for state flags.
    constexpr static uint8_t kIsBlackToMove = 0x01;
    constexpr static uint8_t kIsNullMove    = 0x02;
};

static_assert(sizeof(Position) == 152);

/// @brief Determine attacks to a square by a color.
/// @param location Square index.
/// @param color Attacking color.
/// @return Set of squares of color containing a piece attacking location.
constexpr Bitboard Position::AttacksTo(Square location, Color color) const
{
    const Bitboard occupied_squares = OccupiedSquares();
    Bitboard result = color == kWhite ? kPawnAttacksBlack[location] & pawns_ : kPawnAttacksWhite[location] & pawns_;
    result |= (kKnightAttacks[location] & knights_) | (kKingAttacks[location] & kings_);
    result |= RookAttacks(occupied_squares, location) & (rooks_ | queens_);
    result |= BishopAttacks(occupied_squares, location) & (bishops_ | queens_);
    result &= PiecesOfColor(color);
    return result;
}

#include "pins.h"

/// @brief Generate legal moves for this position.
/// @tparam color color to generate moves for
/// @tparam do_all_moves if true generate all moves, otherwise captures and promotions only
/// @return list of moves generated
template <Color color, bool do_all_moves> constexpr MoveList Position::GenMoves() const
{
    const Bitboard occupied_squares = white_pieces_ | black_pieces_;
    const Bitboard friendly_pieces  = PiecesOfColor(color);
    const Bitboard enemy_pieces     = occupied_squares ^ friendly_pieces;
    const Bitboard enemy_pawns      = pawns_ & enemy_pieces;
    const Square   king_locn        = king_location_[color];

    MoveList moves;

    // Determine the squares which our king cannot move to, i.e. any square which is attacked or X-ray attacked by any
    // enemy piece. Start with pawns.
    Bitboard forbidden_king_squares = color == kWhite ? enemy_pawns.ShiftSouthwest() | enemy_pawns.ShiftSoutheast()
                                                      : enemy_pawns.ShiftNorthwest() | enemy_pawns.ShiftNortheast();

    // Temporarily remove our king to detect X-ray attacks from enemy sliding pieces.
    const Bitboard occupied_except_king = occupied_squares ^ Bitboard(king_locn);

    for (auto &[piece, attack_fn] : piece_attackers)
    {
        const Bitboard b = PiecesOfType(piece) & enemy_pieces;
        for (Square s : b)
        {
            forbidden_king_squares |= attack_fn(occupied_except_king, s);
        }
    }

    // The king may only safely move to squares which are not forbidden.
    const Bitboard king_move_targets = kKingAttacks[king_locn] & ~forbidden_king_squares;

    // Generate King moves.
    const Bitboard king_captures = king_move_targets & enemy_pieces;
    for (Square to : king_captures)
    {
        moves.push_back(Move::Capture(king_locn, to, kKing, PieceAt(to)));
    }
    if constexpr (do_all_moves)
    {
        const Bitboard king_non_captures = king_move_targets & ~occupied_squares;
        for (Square to : king_non_captures)
        {
            moves.push_back(Move::NonCapture(king_locn, to, kKing));
        }
    }

    Bitboard allowed_captures     = enemy_pieces;      // The set of pieces we may capture.
    Bitboard allowed_non_captures = ~occupied_squares; // The set of empty squares we may move to.

    // If we are in check then the set of possible captures and empty squares that are legal is much smaller, since we
    // must immediately get out of check.
    if (IsInCheck())
    {
        if (checkers_.PopCount() > 1)
        {
            // Multiple check: in the case of multiple check, only moving the king is possible so we are done with move
            // generation.
            return moves;
        }
        // We are in check, the options are:
        // a) Capture the checking piece, OR
        // b) If checked by a sliding piece, interpose a piece between the checker and our king.
        // This includes an en passant capture if the en passant square is between the king and the sliding checker.
        const Square checker_locn = checkers_.Lsb();
        allowed_captures          = checkers_;
        allowed_non_captures      = kInterveningSquares[king_locn][checker_locn];
    }

    const Pins pins{*this}; // Determine pinned pieces and their allowed destination squares.

    // Generate knight, bishop, rook and queen moves.
    for (auto &[piece, attack_fn] : piece_attackers_except_king)
    {
        const Bitboard b = PiecesOfType(piece) & friendly_pieces;
        for (Square from : b)
        {
            const Bitboard attacks         = attack_fn(occupied_squares, from) & pins.AllowedSquares(from);
            const Bitboard capture_targets = attacks & allowed_captures;
            for (Square to : capture_targets)
            {
                moves.push_back(Move::Capture(from, to, piece, PieceAt(to)));
            }
            if constexpr (do_all_moves)
            {
                const Bitboard non_capture_targets = attacks & allowed_non_captures;
                for (Square to : non_capture_targets)
                {
                    moves.push_back(Move::NonCapture(from, to, piece));
                }
            }
        }
    }

    // Generate castling moves. The conditions for castling are:
    // 1) King must not have moved
    // 2) Rook must not have moved
    // 3) King is not in check
    // 4) Squares between King and Rook are vacant
    // 5) Square King passes through is not attacked by the enemy.
    if constexpr (do_all_moves)
    {
        if (!IsInCheck())
        {
            if constexpr (color == kWhite)
            {
                if (MayWhiteCastleKingside() && (occupied_squares & (Bitboard("F1") | Bitboard("G1"))).IsEmpty() &&
                    !IsAttacked("F1", kBlack) && !IsAttacked("G1", kBlack))
                {
                    moves.push_back(Move::Castling("E1", "G1"));
                }
                if (MayWhiteCastleQueenside() &&
                    (occupied_squares & (Bitboard("B1") | Bitboard("C1") | Bitboard("D1"))).IsEmpty() &&
                    !IsAttacked("D1", kBlack) && !IsAttacked("C1", kBlack))
                {
                    moves.push_back(Move::Castling("E1", "C1"));
                }
            }
            else
            {
                if (MayBlackCastleKingside() && (occupied_squares & (Bitboard("F8") | Bitboard("G8"))).IsEmpty() &&
                    !IsAttacked("F8", kWhite) && !IsAttacked("G8", kWhite))
                {
                    moves.push_back(Move::Castling("E8", "G8"));
                }
                if (MayBlackCastleQueenside() &&
                    (occupied_squares & (Bitboard("B8") | Bitboard("C8") | Bitboard("D8"))).IsEmpty() &&
                    !IsAttacked("D8", kWhite) && !IsAttacked("C8", kWhite))
                {
                    moves.push_back(Move::Castling("E8", "C8"));
                }
            }
        }
    }

    // Generate pawn moves.
    Bitboard pawns;              // friendly pawns
    Bitboard single_pushes;      // single push targets
    Bitboard double_pushes;      // double push targets
    Bitboard captures_west;      // capture west targets
    Bitboard captures_east;      // capture east targets
    Bitboard en_passant_sources; // pawns which can capture ep
    Bitboard promotions;         // push promotion targets
    Bitboard promotions_west;    // promotion capture west targets
    Bitboard promotions_east;    // promotion capture east targets
    int8_t   push_delta;         // square index delta for push forward
    int8_t   west_delta;         // square index delta for capture west
    int8_t   east_delta;         // square index delta for capture east

    if constexpr (color == kWhite)
    {
        pawns              = pawns_ & white_pieces_;
        single_pushes      = pawns.ShiftNorth() & ~occupied_squares;
        double_pushes      = single_pushes.ShiftNorth() & ~occupied_squares & kRank4;
        captures_west      = pawns.ShiftNorthwest() & black_pieces_;
        captures_east      = pawns.ShiftNortheast() & black_pieces_;
        en_passant_sources = en_passant_square_ ? kPawnAttacksBlack[en_passant_square_] & pawns : kNoSquares;
        promotions         = single_pushes & kRank8;
        promotions_west    = captures_west & kRank8;
        promotions_east    = captures_east & kRank8;
        push_delta         = 8;
        west_delta         = 7;
        east_delta         = 9;
    }
    else
    {
        pawns              = pawns_ & black_pieces_;
        single_pushes      = pawns.ShiftSouth() & ~occupied_squares;
        double_pushes      = single_pushes.ShiftSouth() & ~occupied_squares & kRank5;
        captures_west      = pawns.ShiftSouthwest() & white_pieces_;
        captures_east      = pawns.ShiftSoutheast() & white_pieces_;
        en_passant_sources = en_passant_square_ ? kPawnAttacksWhite[en_passant_square_] & pawns : kNoSquares;
        promotions         = single_pushes & kRank1;
        promotions_west    = captures_west & kRank1;
        promotions_east    = captures_east & kRank1;
        push_delta         = -8;
        west_delta         = -9;
        east_delta         = -7;
    }
    captures_west ^= promotions_west;
    captures_east ^= promotions_east;
    single_pushes ^= promotions;

    // clang-format off
    const std::array<std::pair<Bitboard, int8_t>, 2> capture_promotions 
    {{
        {promotions_west, west_delta},
        {promotions_east, east_delta}
    }};
    // clang-format on
    for (auto &[bb, delta] : capture_promotions)
    {
        const Bitboard b = bb & allowed_captures;
        for (Square to : b)
        {
            const Square from = (Square)(to - delta);
            if (pins.IsAllowed(from, to))
            {
                moves.push_back(Move::Promotion(from, to, kQueen, PieceAt(to)));
                moves.push_back(Move::Promotion(from, to, kRook, PieceAt(to)));
                moves.push_back(Move::Promotion(from, to, kBishop, PieceAt(to)));
                moves.push_back(Move::Promotion(from, to, kKnight, PieceAt(to)));
            }
        }
    }
    const Bitboard b = promotions & allowed_non_captures;
    for (Square to : b)
    {
        const Square from = (Square)(to - push_delta);
        if (pins.IsAllowed(from, to))
        {
            moves.push_back(Move::Promotion(from, to, kQueen));
            moves.push_back(Move::Promotion(from, to, kRook));
            moves.push_back(Move::Promotion(from, to, kBishop));
            moves.push_back(Move::Promotion(from, to, kKnight));
        }
    }
    // clang-format off
    const std::array<std::pair<Bitboard, int8_t>, 2> captures 
    {{
        {captures_west, west_delta}, 
        {captures_east, east_delta}
    }};
    // clang-format on
    for (auto &[bb, delta] : captures)
    {
        const Bitboard b = bb & allowed_captures;
        for (Square to : b)
        {
            const Square from = (Square)(to - delta);
            if (pins.IsAllowed(from, to))
            {
                moves.push_back(Move::Capture(from, to, kPawn, PieceAt(to)));
            }
        }
    }
    if constexpr (do_all_moves)
    {
        Bitboard b = single_pushes & allowed_non_captures;
        for (Square to : b)
        {
            const Square from = (Square)(to - push_delta);
            if (pins.IsAllowed(from, to))
            {
                moves.push_back(Move::NonCapture(from, to, kPawn));
            }
        }
        b = double_pushes & allowed_non_captures;
        for (Square to : b)
        {
            const Square from = (Square)(to - push_delta * 2);
            if (pins.IsAllowed(from, to))
            {
                moves.push_back(Move::DoublePush(from, to));
            }
        }
    }
    // Generate en passant captures. En passant captures are only legal if the captured pawn location (capture target)
    // is in the capture mask. We also have to test for the special case of discovered horizontal check when removing
    // both pawns from the king's rank, if the king is on the same rank as the capturing and captured pawns, and there
    // is also an enemy rook or queen on the same rank.
    for (Square from : en_passant_sources)
    {
        const Square to = en_passant_square_;
        // The captured pawn is on the source rank, destination file.
        const Square captured_pawn_locn = (Square)((from & 0x38) | (to & 0x07));
        if (pins.IsAllowed(from, to) && (allowed_captures & Bitboard(captured_pawn_locn)).IsNotEmpty())
        {
            // Test for the weird check that occurs when there is an enemy rook or queen on the same rank as our king
            // which is only discovered after removing both pawns from the rank during an en passant capture.
            if (king_locn.Rank() == from.Rank())
            {
                // Remove the capturing and ep captured pawns from the occupied squares set and test for horizontal ray
                // attacks.
                const Bitboard pseudo_occupied_squares =
                    occupied_squares ^ Bitboard(captured_pawn_locn) ^ Bitboard(from);
                const Bitboard horizontal_attacks =
                    RookAttacks(pseudo_occupied_squares, king_locn) & (kWest[king_locn] | kEast[king_locn]);
                if ((horizontal_attacks & enemy_pieces & (rooks_ | queens_)).IsEmpty())
                {
                    // We can make this move since it's not a discovered check.
                    moves.push_back(Move::EpCapture(from, to));
                }
            }
            else
            {
                moves.push_back(Move::EpCapture(from, to));
            }
        }
    }
    return moves;
}
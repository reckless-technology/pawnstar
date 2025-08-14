#pragma once

#include <array>
#include <format>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include "attacks.h"
#include "bitboard.h"
#include "move.h"

/// @brief Class to represent a chess position.
/// This is the primary class that holds the state of the board and represents the rules of chess.
class Position
{
  public:
    Position()                                      = default;
    Position(const Position &that)                  = default;
    Position       &operator=(const Position &that) = default;
    static Position FromString(std::string_view fen_string);        ///< Construct a position from a FEN string.
    Position        MakeMove(const Move &move) const;               ///< Create a position by making a regular move.
    Position        MakeNullMove() const;                           ///< Create a position by making a null move.
    bool            IsAttacked(Square location, Color color) const; ///< Determine if location is attacked by color.
    Bitboard        AttacksTo(Square location, Color color) const;  ///< Find all attackers to specified square.
    bool            IsLegal() const;                                ///< Is this a legal chess position.
    bool            IsCheckmate() const;                            ///< Is this position checkmate.
    bool            IsStalemate() const;                            ///< Is this position stalemate.
    bool            IsDrawByMaterial() const;                       ///< Is this position material draw.
    std::string     ToString() const;                               ///< Return the FEN string for this position.

    // clang-format off
    MoveList            GenerateLegalMoves() const          {return ColorToMove() == WHITE ? GenMoves<WHITE, true>() : GenMoves<BLACK, true>();}
    MoveList            GenerateLegalCaptures() const       {return ColorToMove() == WHITE ? GenMoves<WHITE, false>() : GenMoves<BLACK, false>();}
    // Const accessors.
    constexpr bool      MayWhiteCastleKingside() const      {return !!(castling_rights_ & MAY_WHITE_CASTLE_KINGSIDE);}
    constexpr bool      MayWhiteCastleQueenside() const     {return !!(castling_rights_ & MAY_WHITE_CASTLE_QUEENSIDE);}
    constexpr bool      MayBlackCastleKingside() const      {return !!(castling_rights_ & MAY_BLACK_CASTLE_KINGSIDE);}
    constexpr bool      MayBlackCastleQueenside() const     {return !!(castling_rights_ & MAY_BLACK_CASTLE_QUEENSIDE);}
    constexpr bool      IsNullMove() const                  {return !!(state_flags_ & IS_NULL_MOVE);}
    constexpr Color     ColorToMove() const                 {return state_flags_ & IS_BLACK_TO_MOVE ? BLACK : WHITE;}
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
    constexpr Bitboard  PiecesOfType(Piece piece) const     {return (&pawns_)[piece - PAWN];}
    constexpr Square    KingLocation(Color color) const     {return king_location_[color];}
    constexpr zobrist_t Hash() const                        {return hash_;}
    constexpr bool      IsInCheck() const                   {return checkers_.IsNotEmpty();}
    constexpr uint8_t   MoveCount() const                   {return full_move_count_;}
    constexpr uint8_t   ReversibleMoveCount() const         {return reversible_move_count_;}
    constexpr Square    EnPassantIndex() const              {return en_passant_square_;}
    // Non const accessors.
    constexpr Bitboard& PiecesOfColor(Color color)          {return (&white_pieces_)[color];}
    constexpr Bitboard& PiecesOfType(Piece piece)           {return (&pawns_)[piece - PAWN];}
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
    uint8_t               castling_rights_;       ///< Castling rights flags.
    uint8_t               state_flags_;           ///< Position state flags.

    /// @brief Bit values for castling rights.
    constexpr static uint8_t MAY_WHITE_CASTLE_KINGSIDE  = 0x01;
    constexpr static uint8_t MAY_WHITE_CASTLE_QUEENSIDE = 0x02;
    constexpr static uint8_t MAY_BLACK_CASTLE_KINGSIDE  = 0x04;
    constexpr static uint8_t MAY_BLACK_CASTLE_QUEENSIDE = 0x08;
    constexpr static uint8_t OKM                        = 0x0F; ///< Default castling rights.
    constexpr static uint8_t A1M                        = ~MAY_WHITE_CASTLE_QUEENSIDE;
    constexpr static uint8_t E1M                        = ~(MAY_WHITE_CASTLE_KINGSIDE | MAY_WHITE_CASTLE_QUEENSIDE);
    constexpr static uint8_t H1M                        = ~MAY_WHITE_CASTLE_KINGSIDE;
    constexpr static uint8_t A8M                        = ~MAY_BLACK_CASTLE_QUEENSIDE;
    constexpr static uint8_t E8M                        = ~(MAY_BLACK_CASTLE_KINGSIDE | MAY_BLACK_CASTLE_QUEENSIDE);
    constexpr static uint8_t H8M                        = ~MAY_BLACK_CASTLE_KINGSIDE;

    /// @brief Bit values for state flags.
    constexpr static uint8_t IS_BLACK_TO_MOVE = 0x01;
    constexpr static uint8_t IS_NULL_MOVE     = 0x02;

    // clang-format off
    /// @brief AND bit masks applied to move from and to, determining new castling rights following a move.
    constexpr static std::array<uint8_t, 64> castling_rights_masks
    {
        A1M, OKM, OKM, OKM, E1M, OKM, OKM, H1M, 
        OKM, OKM, OKM, OKM, OKM, OKM, OKM, OKM, 
        OKM, OKM, OKM, OKM, OKM, OKM, OKM, OKM, 
        OKM, OKM, OKM, OKM, OKM, OKM, OKM, OKM, 
        OKM, OKM, OKM, OKM, OKM, OKM, OKM, OKM, 
        OKM, OKM, OKM, OKM, OKM, OKM, OKM, OKM, 
        OKM, OKM, OKM, OKM, OKM, OKM, OKM, OKM, 
        A8M, OKM, OKM, OKM, E8M, OKM, OKM, H8M,
    };
    // clang-format on
};

static_assert(sizeof(Position) == 152);

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

    using AttackFn = Bitboard (*)(Bitboard occupied_squares, Square locn);

    // clang-format off
    constexpr std::array<std::pair<Piece, AttackFn>, 5> attackers 
    {{
        { KNIGHT,   KnightAttacks   }, 
        { BISHOP,   BishopAttacks   }, 
        { ROOK,     RookAttacks     }, 
        { QUEEN,    QueenAttacks    },
        { KING,     KingAttacks     }
    }};
    // clang-format on

    // Determine the squares which our king cannot move to, i.e. any square which is attacked or X-ray attacked by any
    // enemy piece.
    Bitboard forbidden_king_squares = color == WHITE ? enemy_pawns.ShiftSouthwest() | enemy_pawns.ShiftSoutheast()
                                                     : enemy_pawns.ShiftNorthwest() | enemy_pawns.ShiftNortheast();

    // Temporarily remove our king to detect X-ray attacks from enemy sliding pieces.
    const Bitboard occupied_except_king = occupied_squares ^ Bitboard(king_locn);

    for (auto &[piece, attack_fn] : attackers)
    {
        Bitboard b = PiecesOfType(piece) & enemy_pieces;
        for (Square s : b)
        {
            forbidden_king_squares |= attack_fn(occupied_except_king, s);
        }
    }

    // The king may only safely move to squares which are not forbidden.
    const Bitboard king_move_targets = KING_ATTACKS[king_locn] & ~forbidden_king_squares;

    // Generate King moves.
    const Bitboard king_captures = king_move_targets & enemy_pieces;
    for (Square to : king_captures)
    {
        moves.push_back(Move::Capture(king_locn, to, KING, PieceAt(to)));
    }
    if constexpr (do_all_moves)
    {
        const Bitboard king_non_captures = king_move_targets & ~occupied_squares;
        for (Square to : king_non_captures)
        {
            moves.push_back(Move::NonCapture(king_locn, to, KING));
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
        allowed_non_captures      = INTERVENING_SQUARES[king_locn][checker_locn];
    }

    const Pins pins{*this}; // Determine pinned pieces and their allowed destination squares.

    // Generate knight, bishop, rook and queen moves.

    // clang-format off
    constexpr std::array<std::pair<Piece, AttackFn>, 4> pieces 
    {{
        { KNIGHT,   KnightAttacks   },
        { BISHOP,   BishopAttacks   }, 
        { ROOK,     RookAttacks     }, 
        { QUEEN,    QueenAttacks    }
    }};
    // clang-format on
    for (auto &[piece, attack_fn] : pieces)
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
            if constexpr (color == WHITE)
            {
                if (MayWhiteCastleKingside() && (occupied_squares & (Bitboard("F1") | Bitboard("G1"))).IsEmpty() &&
                    !IsAttacked("F1", BLACK) && !IsAttacked("G1", BLACK))
                {
                    moves.push_back(Move::Castling("E1", "G1"));
                }
                if (MayWhiteCastleQueenside() &&
                    (occupied_squares & (Bitboard("B1") | Bitboard("C1") | Bitboard("D1"))).IsEmpty() &&
                    !IsAttacked("D1", BLACK) && !IsAttacked("C1", BLACK))
                {
                    moves.push_back(Move::Castling("E1", "C1"));
                }
            }
            else
            {
                if (MayBlackCastleKingside() && (occupied_squares & (Bitboard("F8") | Bitboard("G8"))).IsEmpty() &&
                    !IsAttacked("F8", WHITE) && !IsAttacked("G8", WHITE))
                {
                    moves.push_back(Move::Castling("E8", "G8"));
                }
                if (MayBlackCastleQueenside() &&
                    (occupied_squares & (Bitboard("B8") | Bitboard("C8") | Bitboard("D8"))).IsEmpty() &&
                    !IsAttacked("D8", WHITE) && !IsAttacked("C8", WHITE))
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

    if constexpr (color == WHITE)
    {
        pawns              = pawns_ & white_pieces_;
        single_pushes      = pawns.ShiftNorth() & ~occupied_squares;
        double_pushes      = single_pushes.ShiftNorth() & ~occupied_squares & RANK_4;
        captures_west      = pawns.ShiftNorthwest() & black_pieces_;
        captures_east      = pawns.ShiftNortheast() & black_pieces_;
        en_passant_sources = en_passant_square_ ? PAWN_ATTACKS_BLACK[en_passant_square_] & pawns : NO_SQUARES;
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
        single_pushes      = pawns.ShiftSouth() & ~occupied_squares;
        double_pushes      = single_pushes.ShiftSouth() & ~occupied_squares & RANK_5;
        captures_west      = pawns.ShiftSouthwest() & white_pieces_;
        captures_east      = pawns.ShiftSoutheast() & white_pieces_;
        en_passant_sources = en_passant_square_ ? PAWN_ATTACKS_WHITE[en_passant_square_] & pawns : NO_SQUARES;
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
            if ((pins.AllowedSquares(from) & Bitboard(to)).IsNotEmpty())
            {
                moves.push_back(Move::Promotion(from, to, QUEEN, PieceAt(to)));
                moves.push_back(Move::Promotion(from, to, ROOK, PieceAt(to)));
                moves.push_back(Move::Promotion(from, to, BISHOP, PieceAt(to)));
                moves.push_back(Move::Promotion(from, to, KNIGHT, PieceAt(to)));
            }
        }
    }
    const Bitboard b = promotions & allowed_non_captures;
    for (Square to : b)
    {
        const Square from = (Square)(to - push_delta);
        if ((pins.AllowedSquares(from) & Bitboard(to)).IsNotEmpty())
        {
            moves.push_back(Move::Promotion(from, to, QUEEN));
            moves.push_back(Move::Promotion(from, to, ROOK));
            moves.push_back(Move::Promotion(from, to, BISHOP));
            moves.push_back(Move::Promotion(from, to, KNIGHT));
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
            if ((pins.AllowedSquares(from) & Bitboard(to)).IsNotEmpty())
            {
                moves.push_back(Move::Capture(from, to, PAWN, PieceAt(to)));
            }
        }
    }
    if constexpr (do_all_moves)
    {
        Bitboard b = single_pushes & allowed_non_captures;
        for (Square to : b)
        {
            const Square from = (Square)(to - push_delta);
            if ((pins.AllowedSquares(from) & Bitboard(to)).IsNotEmpty())
            {
                moves.push_back(Move::NonCapture(from, to, PAWN));
            }
        }
        b = double_pushes & allowed_non_captures;
        for (Square to : b)
        {
            const Square from = (Square)(to - push_delta * 2);
            if ((pins.AllowedSquares(from) & Bitboard(to)).IsNotEmpty())
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
        const Square to                 = en_passant_square_;
        const Square captured_pawn_locn = (Square)((from & 0x38) | (to & 0x07));
        if ((pins.AllowedSquares(from) & Bitboard(to)).IsNotEmpty() &&
            (allowed_captures & Bitboard(captured_pawn_locn)).IsNotEmpty())
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
                    RookAttacks(pseudo_occupied_squares, king_locn) & (WEST[king_locn] | EAST[king_locn]);
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
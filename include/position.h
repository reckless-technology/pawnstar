#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "attacks.h"
#include "bitboard.h"
#include "move.h"

/// @brief Class to represent a chess position.
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
    bool            IsDrawByMaterial() const; ///< Is this position a draw by insufficient material.
    std::string     ToString() const;         ///< Return the FEN string for this position.
    // clang-format off
    // Const accessors.
    constexpr bool      MayWhiteCastleKingside() const      {return !!(flags_ & MAY_WHITE_CASTLE_KINGSIDE);}
    constexpr bool      MayWhiteCastleQueenside() const     {return !!(flags_ & MAY_WHITE_CASTLE_QUEENSIDE);}
    constexpr bool      MayBlackCastleKingside() const      {return !!(flags_ & MAY_BLACK_CASTLE_KINGSIDE);}
    constexpr bool      MayBlackCastleQueenside() const     {return !!(flags_ & MAY_BLACK_CASTLE_QUEENSIDE);}
    constexpr bool      IsNullMove() const                  {return !!(flags_ & IS_NULL_MOVE);}
    constexpr Color     ColorToMove() const                 {return flags_ & IS_BLACK_TO_MOVE ? BLACK : WHITE;}
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
    constexpr uint64_t  Hash() const                        {return hash_;}
    constexpr bool      IsInCheck() const                   {return checkers_.IsNotEmpty();}
    constexpr uint8_t   MoveCount() const                   {return full_move_count_;}
    constexpr uint8_t   ReversibleMoveCount() const         {return reversible_move_count_;}
    constexpr Square    EnPassantIndex() const              {return en_passant_square_;}
    // Non const accessors.
    constexpr Bitboard& PiecesOfColor(Color color)          {return (&white_pieces_)[color];}
    constexpr Bitboard& PiecesOfType(Piece piece)           {return (&pawns_)[piece - PAWN];}
    // clang-format on

    MoveList GenerateLegalMoves() const
    {
        return ColorToMove() == WHITE ? GenMoves<WHITE, true>() : GenMoves<BLACK, true>();
    }

    MoveList GenerateLegalCaptures() const
    {
        return ColorToMove() == WHITE ? GenMoves<WHITE, false>() : GenMoves<BLACK, false>();
    }

  private:
    void     AddPiece(Color color, Piece piece, Square to);               ///< Place a piece on the board.
    void     RemovePiece(Color color, Piece piece, Square from);          ///< Remove a piece from the board
    void     MovePiece(Color color, Piece piece, Square from, Square to); ///< Move a piece on the board
    uint64_t ComputeHash() const;                                         ///< Compute the Zobrist hash from scratch
    template <Color, bool> MoveList GenMoves() const;                     ///< Generate legal moves

    Bitboard             pawns_;                    ///< squares with a pawn on them
    Bitboard             knights_;                  ///< squares with a knight on them
    Bitboard             bishops_;                  ///< squares with a bishop on them
    Bitboard             rooks_;                    ///< squares with a rook on them
    Bitboard             queens_;                   ///< squares with a queen on them
    Bitboard             kings_;                    ///< squares with a king on them
    Bitboard             white_pieces_;             ///< squares with a white piece on them
    Bitboard             black_pieces_;             ///< squares with a black piece on them
    Piece                squares_[64];              ///< Squares array for fast piece lookup
    Bitboard             checkers_;                 ///< Set of squares which attack the king
    uint64_t             hash_;                     ///< Zobrist hash of this position, maintained incrementally
    uint8_t              flags_;                    ///< game state-machine flags
    Square               king_location_[2];         ///< square index of white and black kings
    Square               en_passant_square_;        ///< en passant capture availability square
    uint8_t              reversible_move_count_;    ///< number of consecutive reversible half-moves (plies)
    uint8_t              full_move_count_;          ///< number of full moves (zero indexed)
    static const uint8_t CASTLING_RIGHTS_MASKS[64]; ///< ANDed with move source and dest to compute new rights

    /// @brief Position state flags bit set.
    enum StateFlags : uint8_t
    {
        MAY_WHITE_CASTLE_KINGSIDE  = 1 << 0, ///< White has the right to castle king side.
        MAY_WHITE_CASTLE_QUEENSIDE = 1 << 1, ///< White has the right to castle queen side.
        MAY_BLACK_CASTLE_KINGSIDE  = 1 << 2, ///< Black has the right to castle king side.
        MAY_BLACK_CASTLE_QUEENSIDE = 1 << 3, ///< Black has the right to castle queen side.
        IS_BLACK_TO_MOVE           = 1 << 4, ///< It is black's turn to move.
        IS_NULL_MOVE               = 1 << 5, ///< Position was the result of a null move.
        CASTLING_RIGHTS_MASK = (MAY_WHITE_CASTLE_KINGSIDE | MAY_WHITE_CASTLE_QUEENSIDE | MAY_BLACK_CASTLE_KINGSIDE |
                                MAY_BLACK_CASTLE_QUEENSIDE),
    };
};

static_assert(sizeof(Position) == 152);

#include "pins.h"

/// @brief Generate legal moves for this position.
/// @tparam color color to generate moves for
/// @tparam do_all_moves if true generate all moves, otherwise captures and promotions only
/// @return list of moves generated
template <Color color, bool do_all_moves> MoveList Position::GenMoves() const
{
    constexpr Color enemy_color      = EnemyOf(color);
    const Bitboard  occupied_squares = white_pieces_ | black_pieces_;
    const Bitboard  friendly_pieces  = PiecesOfColor(color);
    const Bitboard  enemy_pieces     = occupied_squares ^ friendly_pieces;
    const Bitboard  enemy_pawns      = pawns_ & enemy_pieces;
    const Square    king_locn        = king_location_[color];
    const Square    enemy_king_locn  = king_location_[enemy_color];

    MoveList moves;

    // Determine the squares which our king cannot move to, i.e. any square which is attacked or X-ray attacked by any
    // enemy piece.

    Bitboard forbidden_king_squares = color == WHITE ? enemy_pawns.ShiftSouthwest() | enemy_pawns.ShiftSoutheast()
                                                     : enemy_pawns.ShiftNorthwest() | enemy_pawns.ShiftNortheast();

    Bitboard b = knights_ & enemy_pieces;
    for (Square s : b)
    {
        forbidden_king_squares |= SETS[s].knight_attacks;
    }
    // Temporarily remove our king to detect X-ray attacks from enemy sliding pieces.
    const Bitboard occupied_except_king = occupied_squares ^ Bitboard(king_locn);

    b = (bishops_ | queens_) & enemy_pieces;
    for (Square s : b)
    {
        forbidden_king_squares |= BishopAttacks(occupied_except_king, s);
    }
    b = (rooks_ | queens_) & enemy_pieces;
    for (Square s : b)
    {
        forbidden_king_squares |= RookAttacks(occupied_except_king, s);
    }
    // King cannot move to squares controlled by the enemy king.
    forbidden_king_squares |= SETS[enemy_king_locn].king_attacks;

    // The king may only safely move to squares which are not forbidden.
    const Bitboard king_move_targets = SETS[king_locn].king_attacks & ~forbidden_king_squares;

    // Generate King moves.
    Bitboard king_captures = king_move_targets & enemy_pieces;
    for (Square to : king_captures)
    {
        moves.push_back(Move::Regular(king_locn, to));
    }
    if constexpr (do_all_moves)
    {
        Bitboard king_non_captures = king_move_targets & ~occupied_squares;
        for (Square to : king_non_captures)
        {
            moves.push_back(Move::Regular(king_locn, to));
        }
    }

    Bitboard allowed_captures     = enemy_pieces;      // The set of pieces we may capture.
    Bitboard allowed_non_captures = ~occupied_squares; // The set of empty squares we may move to.

    // If we are in check then the set of possible captures and empty squares that are legal is much smaller.
    if (IsInCheck())
    {
        if (checkers_.PopCount() > 1)
        {
            // Multiple check: only moving the king is possible so we are done with move generation.
            return moves;
        }
        // We are in single check, the options are:
        // a) Capture the checking piece, OR
        // b) If checked by a sliding piece, interpose a piece between the checker and our king.
        // NB: This includes an en passant capture if the en passant square is between the king and the sliding checker.
        const Square checker_locn = checkers_.Lsb();
        allowed_captures          = checkers_;
        allowed_non_captures      = INTERVENING_SQUARES[king_locn][checker_locn];
    }

    const Pins pins{*this}; // Determine pinned pieces and their allowed destination squares.

    // Generate knight moves.
    b = knights_ & friendly_pieces;
    for (Square from : b)
    {
        const Bitboard attacks         = SETS[from].knight_attacks & pins.AllowedSquares(from);
        Bitboard       capture_targets = attacks & allowed_captures;
        for (Square to : capture_targets)
        {
            moves.push_back(Move::Regular(from, to));
        }
        if constexpr (do_all_moves)
        {
            Bitboard non_capture_targets = attacks & allowed_non_captures;
            for (Square to : non_capture_targets)
            {
                moves.push_back(Move::Regular(from, to));
            }
        }
    }

    // Generate bishop, rook and queen moves.
    typedef Bitboard (*AttackFn)(Bitboard occupied_squares, Square locn);
    // clang-format off
    constexpr std::pair<Piece, AttackFn> SLIDING_ATTACKERS[3] = 
    {
        { BISHOP,   BishopAttacks }, 
        { ROOK,     RookAttacks   }, 
        { QUEEN,    QueenAttacks  }
    };
    // clang-format on
    for (auto &[piece, attack_fn] : SLIDING_ATTACKERS)
    {
        b = PiecesOfType(piece) & friendly_pieces;
        for (Square from : b)
        {
            const Bitboard attacks         = attack_fn(occupied_squares, from) & pins.AllowedSquares(from);
            Bitboard       capture_targets = attacks & allowed_captures;
            for (Square to : capture_targets)
            {
                moves.push_back(Move::Regular(from, to));
            }
            if constexpr (do_all_moves)
            {
                Bitboard non_capture_targets = attacks & allowed_non_captures;
                for (Square to : non_capture_targets)
                {
                    moves.push_back(Move::Regular(from, to));
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
                if (MayWhiteCastleKingside() && (occupied_squares & (Bitboard(F1) | Bitboard(G1))).IsEmpty() &&
                    !IsAttacked(F1, BLACK) && !IsAttacked(G1, BLACK))
                {
                    moves.push_back(Move::Castling(E1, G1));
                }
                if (MayWhiteCastleQueenside() &&
                    (occupied_squares & (Bitboard(B1) | Bitboard(C1) | Bitboard(D1))).IsEmpty() &&
                    !IsAttacked(D1, BLACK) && !IsAttacked(C1, BLACK))
                {
                    moves.push_back(Move::Castling(E1, C1));
                }
            }
            else
            {
                if (MayBlackCastleKingside() && (occupied_squares & (Bitboard(F8) | Bitboard(G8))).IsEmpty() &&
                    !IsAttacked(F8, WHITE) && !IsAttacked(G8, WHITE))
                {
                    moves.push_back(Move::Castling(E8, G8));
                }
                if (MayBlackCastleQueenside() &&
                    (occupied_squares & (Bitboard(B8) | Bitboard(C8) | Bitboard(D8))).IsEmpty() &&
                    !IsAttacked(D8, WHITE) && !IsAttacked(C8, WHITE))
                {
                    moves.push_back(Move::Castling(E8, C8));
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
        en_passant_sources = en_passant_square_ ? SETS[en_passant_square_].pawn_attacks_black & pawns : NO_SQUARES;
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
        en_passant_sources = en_passant_square_ ? SETS[en_passant_square_].pawn_attacks_white & pawns : NO_SQUARES;
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
    const std::pair<Bitboard, int8_t> capture_promotions[] = 
    {
        {promotions_west, west_delta},
        {promotions_east, east_delta}
    };
    // clang-format on
    for (auto &[bb, delta] : capture_promotions)
    {
        b = bb & allowed_captures;
        for (Square to : b)
        {
            const Square from = (Square)(to - delta);
            if ((pins.AllowedSquares(from) & Bitboard(to)).IsNotEmpty())
            {
                moves.push_back(Move::Promotion(from, to, QUEEN));
                moves.push_back(Move::Promotion(from, to, ROOK));
                moves.push_back(Move::Promotion(from, to, BISHOP));
                moves.push_back(Move::Promotion(from, to, KNIGHT));
            }
        }
    }
    b = promotions & allowed_non_captures;
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
    const std::pair<Bitboard, int8_t> captures[] = 
    {
        {captures_west, west_delta}, 
        {captures_east, east_delta}
    };
    // clang-format on
    for (auto &[bb, delta] : captures)
    {
        b = bb & allowed_captures;
        for (Square to : b)
        {
            const Square from = (Square)(to - delta);
            if ((pins.AllowedSquares(from) & Bitboard(to)).IsNotEmpty())
            {
                moves.push_back(Move::Regular(from, to));
            }
        }
    }
    if constexpr (do_all_moves)
    {
        b = single_pushes & allowed_non_captures;
        for (Square to : b)
        {
            const Square from = (Square)(to - push_delta);
            if ((pins.AllowedSquares(from) & Bitboard(to)).IsNotEmpty())
            {
                moves.push_back(Move::Regular(from, to));
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
    // Generate en passant captures. En passant captures are legal if:
    //  a) The en passant square (push target) is in the non capture mask, OR
    //  b) The captured pawn location (capture target) is in the capture mask
    // We also have to test for the special case of "discovered" horizontal check when removing both pawns from the
    // king's rank, if the king is on the same rank as the capturing and captured pawns.
    b = en_passant_sources;
    for (Square from : b)
    {
        const Square to                 = en_passant_square_;
        const Square captured_pawn_locn = (Square)((from & 0x38) | (to & 0x07));
        if ((pins.AllowedSquares(from) & Bitboard(to)).IsNotEmpty())
        {
            if ((allowed_non_captures & Bitboard(to)).IsNotEmpty() ||
                (allowed_captures & Bitboard(captured_pawn_locn)).IsNotEmpty())
            {
                // Test for the "weird" check that occurs when there is an enemy rook or queen on the same rank as our
                // king which is only discovered after removing both pawns from the rank during an en passant capture.
                bool is_discovered_check = false;
                if (RankOf(king_locn) == RankOf(from)) // Only applies when our king is on the ep capturing pawn's rank.
                {
                    // Remove the capturing and ep captured pawns from the occupied squares set and test for horizontal
                    // ray attacks.
                    const Bitboard pseudo_occupied_squares =
                        occupied_squares ^ Bitboard(captured_pawn_locn) ^ Bitboard(from);
                    Bitboard bb = (rooks_ | queens_) & enemy_pieces & (SETS[king_locn].west | SETS[king_locn].east);
                    for (Square s : bb)
                    {
                        if ((INTERVENING_SQUARES[king_locn][s] & pseudo_occupied_squares).IsEmpty())
                        {
                            // We can't make this move as it would lead to a discovered check.
                            is_discovered_check = true;
                            break;
                        }
                    }
                }
                if (!is_discovered_check)
                {
                    moves.push_back(Move::EpCapture(from, to));
                }
            }
        }
    }
    return moves;
}
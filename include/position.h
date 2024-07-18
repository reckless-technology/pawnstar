#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "attacks.h"
#include "bitboard.h"
#include "move.h"

/**
 * @brief A chess position.
 * The position comprises the pieces on the board, whose turn it is to
 * move, castling rights for each side, whether en passant capture is possible,
 * and the number of consecutive reversible half-moves.
 */
class Position
{

  private:
    /**
     * @brief Bitset of position state flags.
     */
    enum StateFlags : uint16_t
    {
        MAY_WHITE_CASTLE_KINGSIDE  = 1 << 0, /**< white has the right to castle king side                                 */
        MAY_WHITE_CASTLE_QUEENSIDE = 1 << 1, /**< white has the right to castle queen side                                */
        MAY_BLACK_CASTLE_KINGSIDE  = 1 << 2, /**< black has the right to castle king side                                 */
        MAY_BLACK_CASTLE_QUEENSIDE = 1 << 3, /**< black has the right to castle queen side                                */
        IS_BLACK_TO_MOVE           = 1 << 4, /**< it is black's turn to move                                              */
        IS_NULL_MOVE               = 1 << 5, /**< position was the result of a null move                                  */
        HAS_BEEN_REDUCED           = 1 << 6, /**< has late move reduction been applied in this position or its ancestors  */
        CASTLING_RIGHTS_MASK =
            (MAY_WHITE_CASTLE_KINGSIDE | MAY_WHITE_CASTLE_QUEENSIDE | MAY_BLACK_CASTLE_KINGSIDE | MAY_BLACK_CASTLE_QUEENSIDE),
    };

  public:
    Position(){};
    Position(const Position &that)            = default;
    Position &operator=(const Position &that) = default;
    Position(std::string_view fen_string);                           /**< Construct a position from a FEN string. */
    Position    MakeMove(const Move &move) const;                    /**< Create a position from this one by making a regular move. */
    Position    MakeNullMove() const;                                /**< Create a position from this one by making a null move. */
    void        AddPiece(Color color, Piece piece, Square to);       /**< Place a piece on the board. */
    bool        IsAttacked(Square location, Color color) const;      /**< Determine if location is attacked by color. */
    Bitboard    AttacksTo(Square location, Color color) const;       /**< Find all attackers to specified square.  */
    Bitboard    AttacksFrom(Square location) const;                  /**< Squares attacked by piece on location.  */
    bool        IsLegal() const;                                     /**< Is this a legal chess position. */
    bool        IsCheckmate() const;                                 /**< Is this position checkmate.  */
    bool        IsStalemate() const;                                 /**< Is this position stalemate. */
    bool        IsDrawByMaterial() const;                            /**< Is this position a draw by insufficient material. */
    std::string ToString() const;                                    /**< Return the FEN string for this position. */
    std::string MoveToString(const Move &move) const;                /**< Generate the SAN string for a specific legal move. */
    std::string VariationToString(const Variation &variation) const; /**< Generate SAN strings for a legal sequence of moves. */

    /* clang-format off */
    constexpr Color     ColorAt(Square location) const      { const Bitboard s = BITBOARD(location); return (s & white_pieces_) ? WHITE : (s & black_pieces_) ? BLACK : NEITHER_COLOR;}
    constexpr bool      MayWhiteCastleKingside() const      {return !!(flags_ & MAY_WHITE_CASTLE_KINGSIDE);}
    constexpr bool      MayWhiteCastleQueenside() const     {return !!(flags_ & MAY_WHITE_CASTLE_QUEENSIDE);}
    constexpr bool      MayBlackCastleKingside() const      {return !!(flags_ & MAY_BLACK_CASTLE_KINGSIDE);}
    constexpr bool      MayBlackCastleQueenside() const     {return !!(flags_ & MAY_BLACK_CASTLE_QUEENSIDE);}
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
    constexpr Bitboard  PiecesOfColor(Color color) const    {return pieces_of_color_[color];}
    constexpr Bitboard  PiecesOfType(Piece piece) const     {return pieces_[piece - 1];}
    constexpr Color     ColorToMove() const                 {return flags_ & IS_BLACK_TO_MOVE ? BLACK : WHITE;}
    constexpr Square    KingLocation(Color color) const     {return king_location_[color];}
    constexpr uint64_t  Hash() const                        {return hash_;}
    constexpr bool      IsNullMove() const                  {return !!(flags_ & IS_NULL_MOVE);}
    constexpr bool      IsInCheck() const                   {return !!checkers_;}
    constexpr bool      HasBeenReduced() const              {return !!(flags_ & HAS_BEEN_REDUCED);}
    constexpr void      Reduce()                            {flags_ |= HAS_BEEN_REDUCED;}
    constexpr uint8_t   MoveCount() const                   {return full_move_count_;}
    constexpr uint8_t   ReversibleMoveCount() const         {return reversible_move_count_;}
    constexpr Square    EnPassantIndex() const              {return en_passant_square_;}
    /* clang-format on */
    constexpr Piece PieceAt(Square location) const
    {
        const Bitboard square = BITBOARD(location);
        return (square & pawns_)     ? PAWN
               : (square & knights_) ? KNIGHT
               : (square & bishops_) ? BISHOP
               : (square & rooks_)   ? ROOK
               : (square & queens_)  ? QUEEN
               : (square & kings_)   ? KING
                                     : NONE;
    }

    MoveList GenerateLegalMoves() const
    {
        return ColorToMove() == WHITE ? GenMoves<WHITE, true>() : GenMoves<BLACK, true>();
    }

    MoveList GenerateLegalCaptures() const
    {
        return ColorToMove() == WHITE ? GenMoves<WHITE, false>() : GenMoves<BLACK, false>();
    }

  private:
    void                            RemovePiece(Color color, Piece piece, Square from);          /**< Remove a piece from the board. */
    void                            MovePiece(Color color, Piece piece, Square from, Square to); /**< Move a piece on the board. */
    uint64_t                        ComputeHash() const; /**< Compute the Zobrist hash from scratch. */
    template <Color, bool> MoveList GenMoves() const;    /**< Generate legal moves.  */

    union {
        Bitboard pieces_[6]; /**< used to index pieces by [piece - 1] */
        struct
        {
            Bitboard pawns_;   /**< squares with a pawn on them        */
            Bitboard knights_; /**< squares with a knight on them      */
            Bitboard bishops_; /**< squares with a bishop on them      */
            Bitboard rooks_;   /**< squares with a rook on them        */
            Bitboard queens_;  /**< squares with a queen on them       */
            Bitboard kings_;   /**< squares with a king on them        */
        };
    };
    union {
        Bitboard pieces_of_color_[2]; /**< used to index pieces by color */
        struct
        {
            Bitboard white_pieces_; /**< squares with a white piece on them */
            Bitboard black_pieces_; /**< squares with a black piece on them */
        };
    };
    Bitboard              checkers_;                 /**< Set of squares which attack the king */
    uint64_t              hash_;                     /**< Zobrist hash of this position, maintained incrementally    */
    uint16_t              flags_;                    /**< game state-machine flags                                   */
    Square                king_location_[2];         /**< square index of white and black kings                      */
    Square                en_passant_square_;        /**< en passant capture availability square                     */
    uint8_t               reversible_move_count_;    /**< number of consecutive reversible half-moves (plies)        */
    uint8_t               full_move_count_;          /**< number of full moves (zero indexed)                        */
    static const uint16_t CASTLING_RIGHTS_MASKS[64]; /**< ANDed with move source and dest to compute new rights      */
};

static_assert(sizeof(Position) == 88);

#include "pins.h"

/**
 * @brief Generate legal chess moves for a position.
 * @param color Color to move.
 * @param do_non_captures Generate all moves if true, otherwise captures and promotions only.
 * @return list of legal moves for this position.
 */
template <Color color, bool do_non_captures> MoveList Position::GenMoves() const
{
    constexpr Color enemy_color      = EnemyOf(color);
    const Bitboard  occupied_squares = white_pieces_ | black_pieces_;
    const Bitboard  friendly_pieces  = pieces_of_color_[color];
    const Bitboard  enemy_pieces     = occupied_squares ^ friendly_pieces;
    const Bitboard  enemy_pawns      = pawns_ & enemy_pieces;
    const Square    king_locn        = king_location_[color];
    const Square    enemy_king_locn  = king_location_[enemy_color];

    MoveList moves;

    /* Determine the squares which our king cannot move to, i.e.
       any square which is attacked or X-ray attacked by the enemy. */

    Bitboard forbidden_king_squares = color == WHITE ? ShiftSouthwest(enemy_pawns) | ShiftSoutheast(enemy_pawns)
                                                     : ShiftNorthwest(enemy_pawns) | ShiftNortheast(enemy_pawns);

    Bitboard b = knights_ & enemy_pieces;
    while (b)
    {
        forbidden_king_squares |= SETS[FindAndClearLsb(b)].knight_attacks;
    }
    b = (bishops_ | queens_) & enemy_pieces;
    while (b)
    {
        /* Temporarily remove our king to detect sliding piece X-ray attacks. */
        forbidden_king_squares |= BishopAttacks(occupied_squares ^ BITBOARD(king_locn), FindAndClearLsb(b));
    }
    b = (rooks_ | queens_) & enemy_pieces;
    while (b)
    {
        /* Temporarily remove our king to detect sliding piece X-ray attacks. */
        forbidden_king_squares |= RookAttacks(occupied_squares ^ BITBOARD(king_locn), FindAndClearLsb(b));
    }
    forbidden_king_squares |= SETS[enemy_king_locn].king_attacks;

    /* The king may only move to squares which are not forbidden. */
    const Bitboard king_move_targets = SETS[king_locn].king_attacks & ~forbidden_king_squares;

    /* Generate King moves. */
    Bitboard king_captures = king_move_targets & enemy_pieces;
    while (king_captures)
    {
        const Square to = FindAndClearLsb(king_captures);
        moves.push_back(Move::CaptureMove(king_locn, to, KING, PieceAt(to)));
    }
    if constexpr (do_non_captures)
    {
        Bitboard king_non_captures = king_move_targets & ~occupied_squares;
        while (king_non_captures)
        {
            moves.push_back(Move::NonCaptureMove(king_locn, FindAndClearLsb(king_non_captures), KING));
        }
    }

    Bitboard allowed_captures     = enemy_pieces;      /* The set of pieces we may capture. */
    Bitboard allowed_non_captures = ~occupied_squares; /* The set of empty squares we may move to. */

    /* If we are in check then the set of possible captures and empty squares that are legal is much smaller. */
    if (IsInCheck())
    {
        if (PopCount(checkers_) > 1)
        {
            /* Multiple check: only moving the king is possible so we are done. */
            return moves;
        }
        /* We are in single check, the options are:
        a) Capture the checking piece, OR
        b) If checked by a sliding piece, interpose a piece between the checker and our king.
        NB: This includes an en passant capture if the en passant square is between the king and the sliding checker.
        */
        const Square checker_locn   = Lsb(checkers_);
        const Piece  checking_piece = PieceAt(checker_locn);
        switch (checking_piece)
        {
        case PAWN:
        case KNIGHT:
            allowed_captures     = checkers_;
            allowed_non_captures = NO_SQUARES; /* Can't block pawn or knight checks. */
            break;

        case BISHOP:
        case ROOK:
        case QUEEN:
            allowed_captures     = checkers_;
            allowed_non_captures = INTERVENING_SQUARES[king_locn][checker_locn];
            break;

        default:
            printf("ERROR in checking piece logic\n");
            break;
        }
    }
    const Pins pins{*this}; /* Determine pinned pieces and their allowed target squares. */

    /* Generate knight moves */
    b = knights_ & friendly_pieces;
    while (b)
    {
        const Square from            = FindAndClearLsb(b);
        Bitboard     capture_targets = SETS[from].knight_attacks & allowed_captures & pins.AllowedSquares(from);
        while (capture_targets)
        {
            const Square to = FindAndClearLsb(capture_targets);
            moves.push_back(Move::CaptureMove(from, to, KNIGHT, PieceAt(to)));
        }
        if constexpr (do_non_captures)
        {
            Bitboard non_capture_targets = SETS[from].knight_attacks & allowed_non_captures & pins.AllowedSquares(from);
            while (non_capture_targets)
            {
                moves.push_back(Move::NonCaptureMove(from, FindAndClearLsb(non_capture_targets), KNIGHT));
            }
        }
    }
    /* Generate sliding piece moves. */
    typedef Bitboard (*AttackFn)(Bitboard occupied_squares, Square locn);
    constexpr std::pair<Piece, AttackFn> SLIDING_ATTACKERS[3] = {{BISHOP, BishopAttacks}, {ROOK, RookAttacks}, {QUEEN, QueenAttacks}};
    for (const auto &[piece, attack_fn] : SLIDING_ATTACKERS)
    {
        b = PiecesOfType(piece) & friendly_pieces;
        while (b)
        {
            const Square   from            = FindAndClearLsb(b);
            const Bitboard attacks         = attack_fn(occupied_squares, from);
            Bitboard       capture_targets = attacks & allowed_captures & pins.AllowedSquares(from);
            while (capture_targets)
            {
                const Square to = FindAndClearLsb(capture_targets);
                moves.push_back(Move::CaptureMove(from, to, piece, PieceAt(to)));
            }
            if constexpr (do_non_captures)
            {
                Bitboard non_capture_targets = attacks & allowed_non_captures & pins.AllowedSquares(from);
                while (non_capture_targets)
                {
                    moves.push_back(Move::NonCaptureMove(from, FindAndClearLsb(non_capture_targets), piece));
                }
            }
        }
    }

    /* Generate castling moves. */
    if constexpr (do_non_captures)
    {
        if (!IsInCheck())
        {
            if constexpr (color == WHITE)
            {
                if (MayWhiteCastleKingside() && !(occupied_squares & (BITBOARD(F1) | BITBOARD(G1))) && !IsAttacked(F1, BLACK) &&
                    !IsAttacked(G1, BLACK))
                {
                    moves.push_back(Move::CastlingMove(E1, G1));
                }
                if (MayWhiteCastleQueenside() && !(occupied_squares & (BITBOARD(B1) | BITBOARD(C1) | BITBOARD(D1))) &&
                    !IsAttacked(D1, BLACK) && !IsAttacked(C1, BLACK))
                {
                    moves.push_back(Move::CastlingMove(E1, C1));
                }
            }
            else
            {
                if (MayBlackCastleKingside() && !(occupied_squares & (BITBOARD(F8) | BITBOARD(G8))) && !IsAttacked(F8, WHITE) &&
                    !IsAttacked(G8, WHITE))
                {
                    moves.push_back(Move::CastlingMove(E8, G8));
                }
                if (MayBlackCastleQueenside() && !(occupied_squares & (BITBOARD(B8) | BITBOARD(C8) | BITBOARD(D8))) &&
                    !IsAttacked(D8, WHITE) && !IsAttacked(C8, WHITE))
                {
                    moves.push_back(Move::CastlingMove(E8, C8));
                }
            }
        }
    }

    /* Generate pawn moves. */
    struct PawnMoveVars
    {
        Bitboard pawns;
        Bitboard single_pushes;
        Bitboard double_pushes;
        Bitboard captures_west;
        Bitboard captures_east;
        Bitboard en_passant_sources;
        Bitboard promotions;
        Bitboard promotions_west;
        Bitboard promotions_east;
        int8_t   push_delta;
        int8_t   west_delta;
        int8_t   east_delta;
    } pmv;
    if constexpr (color == WHITE)
    {
        pmv.pawns              = pawns_ & white_pieces_;
        pmv.single_pushes      = ShiftNorth(pmv.pawns) & ~occupied_squares;
        pmv.double_pushes      = ShiftNorth(pmv.single_pushes) & ~occupied_squares & RANK_4;
        pmv.captures_west      = ShiftNorthwest(pmv.pawns) & black_pieces_;
        pmv.captures_east      = ShiftNortheast(pmv.pawns) & black_pieces_;
        pmv.en_passant_sources = en_passant_square_ ? SETS[en_passant_square_].pawn_attacks_black & pmv.pawns : NO_SQUARES;
        pmv.promotions         = pmv.single_pushes & RANK_8;
        pmv.promotions_west    = pmv.captures_west & RANK_8;
        pmv.promotions_east    = pmv.captures_east & RANK_8;
        pmv.push_delta         = 8;
        pmv.west_delta         = 7;
        pmv.east_delta         = 9;
    }
    else
    {
        pmv.pawns              = pawns_ & black_pieces_;
        pmv.single_pushes      = ShiftSouth(pmv.pawns) & ~occupied_squares;
        pmv.double_pushes      = ShiftSouth(pmv.single_pushes) & ~occupied_squares & RANK_5;
        pmv.captures_west      = ShiftSouthwest(pmv.pawns) & white_pieces_;
        pmv.captures_east      = ShiftSoutheast(pmv.pawns) & white_pieces_;
        pmv.en_passant_sources = en_passant_square_ ? SETS[en_passant_square_].pawn_attacks_white & pmv.pawns : NO_SQUARES;
        pmv.promotions         = pmv.single_pushes & RANK_1;
        pmv.promotions_west    = pmv.captures_west & RANK_1;
        pmv.promotions_east    = pmv.captures_east & RANK_1;
        pmv.push_delta         = -8;
        pmv.west_delta         = -9;
        pmv.east_delta         = -7;
    }
    pmv.captures_west ^= pmv.promotions_west;
    pmv.captures_east ^= pmv.promotions_east;
    pmv.single_pushes ^= pmv.promotions;

    const std::pair<Bitboard, int8_t> capture_promotions[] = {{pmv.promotions_west, pmv.west_delta}, {pmv.promotions_east, pmv.east_delta}};
    for (auto &[bb, delta] : capture_promotions)
    {
        b = bb & allowed_captures;
        while (b)
        {
            const Square to       = FindAndClearLsb(b);
            const Piece  captured = PieceAt(to);
            const Square from     = (Square)(to - delta);
            if (pins.AllowedSquares(from) & BITBOARD(to))
            {
                moves.push_back(Move::PromotionCaptureMove(from, to, captured, QUEEN));
                moves.push_back(Move::PromotionCaptureMove(from, to, captured, ROOK));
                moves.push_back(Move::PromotionCaptureMove(from, to, captured, BISHOP));
                moves.push_back(Move::PromotionCaptureMove(from, to, captured, KNIGHT));
            }
        }
    }
    b = pmv.promotions & allowed_non_captures;
    while (b)
    {
        const Square to   = FindAndClearLsb(b);
        const Square from = (Square)(to - pmv.push_delta);
        if (pins.AllowedSquares(from) & BITBOARD(to))
        {
            moves.push_back(Move::PromotionCaptureMove(from, to, NONE, QUEEN));
            moves.push_back(Move::PromotionCaptureMove(from, to, NONE, ROOK));
            moves.push_back(Move::PromotionCaptureMove(from, to, NONE, BISHOP));
            moves.push_back(Move::PromotionCaptureMove(from, to, NONE, KNIGHT));
        }
    }
    const std::pair<Bitboard, int8_t> captures[] = {{pmv.captures_west, pmv.west_delta}, {pmv.captures_east, pmv.east_delta}};
    for (auto &[bb, delta] : captures)
    {
        b = bb & allowed_captures;
        while (b)
        {
            const Square to       = FindAndClearLsb(b);
            const Piece  captured = PieceAt(to);
            const Square from     = (Square)(to - delta);
            if (pins.AllowedSquares(from) & BITBOARD(to))
            {
                moves.push_back(Move::CaptureMove(from, to, PAWN, captured));
            }
        }
    }
    if constexpr (do_non_captures)
    {
        b = pmv.single_pushes & allowed_non_captures;
        while (b)
        {
            const Square to   = FindAndClearLsb(b);
            const Square from = (Square)(to - pmv.push_delta);
            if (pins.AllowedSquares(from) & BITBOARD(to))
            {
                moves.push_back(Move::NonCaptureMove(from, to, PAWN));
            }
        }
        b = pmv.double_pushes & allowed_non_captures;
        while (b)
        {
            const Square to   = FindAndClearLsb(b);
            const Square from = (Square)(to - pmv.push_delta * 2);
            if (pins.AllowedSquares(from) & BITBOARD(to))
            {
                moves.push_back(Move::DoublePushMove(from, to));
            }
        }
    }

    /* Generate en passant captures.
       En passant captures are legal if:
        a) The en passant square (push target) is in the non capture mask, OR
        b) The captured pawn location (capture target) is in the capture mask

       We also have to test for the special case of "discovered" horizontal check
       when removing both pawns from the king's rank, if the king is on the same
       rank as the capturing and captured pawns.
    */
    b = pmv.en_passant_sources;
    while (b)
    {
        const Square from               = FindAndClearLsb(b);
        const Square to                 = en_passant_square_;
        const Square captured_pawn_locn = (Square)((from & 0x38) | (to & 0x07));
        if (pins.AllowedSquares(from) & BITBOARD(to))
        {
            if ((allowed_non_captures & BITBOARD(to)) || (allowed_captures & BITBOARD(captured_pawn_locn)))
            {
                /* Test for the weird check that occurs when there is an enemy rook or queen on the same rank
                   as our king which is only discovered after removing both pawns from the rank. */
                bool is_discovered_check = false;
                if (RankOf(king_locn) == RankOf(from)) /* Only applies when our king is on the ep capturing pawn's rank. */
                {
                    /* Remove the capturing and ep captured pawns from the occupied squares set and test for horizontal ray attacks. */
                    const Bitboard pseudo_occupied_squares = occupied_squares ^ BITBOARD(captured_pawn_locn) ^ BITBOARD(from);
                    Bitboard       bb = (rooks_ | queens_) & enemy_pieces & (SETS[king_locn].west | SETS[king_locn].east);
                    while (bb)
                    {
                        Square s = FindAndClearLsb(bb);
                        if ((INTERVENING_SQUARES[king_locn][s] & pseudo_occupied_squares) == NO_SQUARES)
                        {
                            /* We can't make this move as it would lead to discovered check. */
                            is_discovered_check = true;
                            break;
                        }
                    }
                }
                if (!is_discovered_check)
                {
                    moves.push_back(Move::EpCaptureMove(from, to));
                }
            }
        }
    }
    return moves;
}
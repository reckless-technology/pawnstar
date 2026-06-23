#pragma once
/// @file position.h Chess position representation and legal move generation.

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
    /// @brief Default constructor; leaves the position uninitialized.
    constexpr Position() = default;

    /// @brief Copy constructor.
    /// @param that Position to copy.
    constexpr Position(const Position &that) = default;

    /// @brief Copy assignment.
    /// @param that Position to copy.
    /// @return Reference to this position.
    constexpr Position &operator=(const Position &that) = default;

    /// @brief Parse a position from a FEN string (see definition for details).
    static Position FromString(std::string_view fen_string);

    /// @brief Return a new position with the given move applied (see definition for details).
    constexpr Position MakeMove(const Move &move) const;

    /// @brief Return a new position with a null (pass) move applied.
    /// @return The resulting position.
    constexpr Position MakeNullMove() const;

    /// @brief Whether the position is a draw by insufficient material.
    /// @return true if drawn by material.
    constexpr bool IsDrawByMaterial() const;

    /// @brief Render the board as a human-readable string.
    /// @return Board string.
    std::string ToString() const;

    // Const accessors.
    /// @brief Pieces of a colour attacking a square (see definition for details).
    constexpr Bitboard AttacksTo(Square location, Color color) const;

    /// @brief Generate all legal moves. @return The legal move list.
    constexpr MoveList GenerateLegalMoves() const
    {
        return color_to_move_ == kWhite ? GenMoves<kWhite, true>() : GenMoves<kBlack, true>();
    }

    /// @brief Generate legal captures and promotions only. @return The legal capture list.
    constexpr MoveList GenerateLegalCaptures() const
    {
        return color_to_move_ == kWhite ? GenMoves<kWhite, false>() : GenMoves<kBlack, false>();
    }

    /// @brief Whether a square is attacked by a colour. @param s Target square. @param c Attacking colour. @return true
    /// if attacked.
    constexpr bool IsAttacked(Square s, Color c) const
    {
        return AttacksTo(s, c).IsNotEmpty();
    }

    /// @brief All occupied squares. @return Occupancy bitboard.
    constexpr Bitboard OccupiedSquares() const
    {
        return pieces_[kNone];
    }

    /// @brief Piece on a square. @param location Square to query. @return The piece (kNone if empty).
    constexpr Piece PieceAt(Square location) const
    {
        return squares_[location];
    }

    /// @brief King location for a colour. @param color Colour to query. @return The king's square.
    constexpr Square KingLocation(Color color) const
    {
        return king_location_[color];
    }

    /// @brief Whether the side to move is in check. @return true if in check.
    constexpr bool IsInCheck() const
    {
        return checkers_.IsNotEmpty();
    }

    std::array<Bitboard, 7> pieces_; ///< @brief Per-piece-type bitboards indexed by Piece; index 0 (kNone) holds the
                                     ///< occupied-squares bitboard
    std::array<Bitboard, 2> colors_; ///< @brief Per-color bitboards indexed by Color.

  private:
    constexpr void      AddPiece(Color color, Piece piece, Square to);               ///< Place a piece on the board.
    constexpr void      RemovePiece(Color color, Piece piece, Square from);          ///< Remove a piece from the board.
    constexpr void      MovePiece(Color color, Piece piece, Square from, Square to); ///< Move a piece on the board.
    constexpr zobrist_t ComputeHash() const;                    ///< Compute the Zobrist hash from scratch.
    template <Color, bool> constexpr MoveList GenMoves() const; ///< Generate legal moves.

    // State variables. The access labels below are interleaved (rather than reordering fields) so the
    // member declaration order is preserved exactly and the layout stays 160 bytes — see the static_assert.
    std::array<Piece, 64> squares_;  ///< Squares array for fast piece lookup.
    Bitboard              checkers_; ///< Set of squares which attack the king.

  public:
    zobrist_t hash_; ///< Zobrist hash of this position, maintained incrementally.

  private:
    std::array<Square, 2> king_location_;     ///< Square index of white and black kings.
    Square                en_passant_square_; ///< En passant capture availability square.

  public:
    uint8_t reversible_move_count_; ///< Number of consecutive reversible half-moves (plies).
    uint8_t full_move_count_;       ///< Number of full moves (zero indexed).

  private:
    CastlingRights castling_rights_; ///< Castling rights flags.

  public:
    bool  is_null_move_;  ///< @brief Whether the last move was a null (pass) move.
    Color color_to_move_; ///< @brief Side to move. Declared last so it occupies what was the trailing padding byte.
};

static_assert(sizeof(Position) == 160);

/// @brief Determine attacks to a square by a color.
/// @param location Square index.
/// @param color Attacking color.
/// @return Set of squares of color containing a piece attacking location.
constexpr Bitboard Position::AttacksTo(Square location, Color color) const
{
    const Bitboard occupied_squares = OccupiedSquares();
    Bitboard       result =
        color == kWhite ? kPawnAttacksBlack[location] & pieces_[kPawn] : kPawnAttacksWhite[location] & pieces_[kPawn];
    result |= (kKnightAttacks[location] & pieces_[kKnight]) | (kKingAttacks[location] & pieces_[kKing]);
    result |= RookAttacks(occupied_squares, location) & (pieces_[kRook] | pieces_[kQueen]);
    result |= BishopAttacks(occupied_squares, location) & (pieces_[kBishop] | pieces_[kQueen]);
    result &= colors_[color];
    return result;
}

#include "pins.h"

/// @brief Generate legal moves for this position.
/// @tparam color color to generate moves for
/// @tparam do_all_moves if true generate all moves, otherwise captures and promotions only
/// @return list of moves generated
template <Color color, bool do_all_moves> constexpr MoveList Position::GenMoves() const
{
    const Bitboard occupied_squares = OccupiedSquares();
    const Bitboard friendly_pieces  = colors_[color];
    const Bitboard enemy_pieces     = occupied_squares ^ friendly_pieces;
    const Bitboard enemy_pawns      = pieces_[kPawn] & enemy_pieces;
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
        const Bitboard b = pieces_[piece] & enemy_pieces;
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
        const Bitboard b = pieces_[piece] & friendly_pieces;
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
                if (castling_rights_.MayWhiteCastleKingside() &&
                    (occupied_squares & (Bitboard("F1") | Bitboard("G1"))).IsEmpty() && !IsAttacked("F1", kBlack) &&
                    !IsAttacked("G1", kBlack))
                {
                    moves.push_back(Move::Castling("E1", "G1"));
                }
                if (castling_rights_.MayWhiteCastleQueenside() &&
                    (occupied_squares & (Bitboard("B1") | Bitboard("C1") | Bitboard("D1"))).IsEmpty() &&
                    !IsAttacked("D1", kBlack) && !IsAttacked("C1", kBlack))
                {
                    moves.push_back(Move::Castling("E1", "C1"));
                }
            }
            else
            {
                if (castling_rights_.MayBlackCastleKingside() &&
                    (occupied_squares & (Bitboard("F8") | Bitboard("G8"))).IsEmpty() && !IsAttacked("F8", kWhite) &&
                    !IsAttacked("G8", kWhite))
                {
                    moves.push_back(Move::Castling("E8", "G8"));
                }
                if (castling_rights_.MayBlackCastleQueenside() &&
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
        pawns              = pieces_[kPawn] & colors_[kWhite];
        single_pushes      = pawns.ShiftNorth() & ~occupied_squares;
        double_pushes      = single_pushes.ShiftNorth() & ~occupied_squares & kRank4;
        captures_west      = pawns.ShiftNorthwest() & colors_[kBlack];
        captures_east      = pawns.ShiftNortheast() & colors_[kBlack];
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
        pawns              = pieces_[kPawn] & colors_[kBlack];
        single_pushes      = pawns.ShiftSouth() & ~occupied_squares;
        double_pushes      = single_pushes.ShiftSouth() & ~occupied_squares & kRank5;
        captures_west      = pawns.ShiftSouthwest() & colors_[kWhite];
        captures_east      = pawns.ShiftSoutheast() & colors_[kWhite];
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
                    RookAttacks(pseudo_occupied_squares, king_locn) & kEastWest[king_locn];
                if ((horizontal_attacks & enemy_pieces & (pieces_[kRook] | pieces_[kQueen])).IsEmpty())
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

// ---- Definitions (moved from position.cpp for header-only build) ----
#include <algorithm>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>
using std::istringstream;
using std::ostringstream;
using std::string;
using std::string_view;
using std::vector;
#include "debug_hashtable.h"
#include "transposition_table.h"

constexpr Position Position::MakeNullMove() const
{
    Position position{*this};
    position.is_null_move_  = true;
    position.color_to_move_ = EnemyOf(position.color_to_move_);
    position.hash_ ^= kEnPassantHashes[position.en_passant_square_];
    position.hash_ ^= kBlackMoveHash;
    position.en_passant_square_ = Square{(uint8_t)0};
    return position;
}

/// @brief Add a piece to the board.
/// @param color Piece color
/// @param piece Piece type
/// @param to Target square
constexpr void Position::AddPiece(Color color, Piece piece, Square to)
{
    const Bitboard to_bb = Bitboard{to};
    pieces_[piece] ^= to_bb;
    colors_[color] ^= to_bb;
    pieces_[kNone] ^= to_bb; // pieces[kNone] holds the occupied-squares bitboard
    hash_ ^= kPieceSquareHashes[color][piece - 1][to];
    squares_[to] = piece;
}

/// @brief Remove a piece from the board.
/// @param color Piece color
/// @param piece Piece type
/// @param from Target square
constexpr void Position::RemovePiece(Color color, Piece piece, Square from)
{
    const Bitboard from_bb = Bitboard{from};
    pieces_[piece] ^= from_bb;
    colors_[color] ^= from_bb;
    pieces_[kNone] ^= from_bb; // pieces[kNone] holds the occupied-squares bitboard
    hash_ ^= kPieceSquareHashes[color][piece - 1][from];
    squares_[from] = Piece::kNone;
}

/// @brief Move a piece on the board.
/// @param color Piece color
/// @param piece Piece type
/// @param from Source square
/// @param to Destination square
constexpr void Position::MovePiece(Color color, Piece piece, Square from, Square to)
{
    const Bitboard                   from_to_bb = Bitboard{from} | Bitboard{to};
    const std::array<zobrist_t, 64> &hash       = kPieceSquareHashes[color][piece - 1];
    pieces_[piece] ^= from_to_bb;
    colors_[color] ^= from_to_bb;
    pieces_[kNone] ^= from_to_bb; // pieces[kNone] holds the occupied-squares bitboard
    hash_ ^= hash[to] ^ hash[from];
    squares_[from] = Piece::kNone;
    squares_[to]   = piece;
}

/// @brief Make a move and return the new position.
/// @param move The move to be made.
/// @return new Position following the move.
constexpr Position Position::MakeMove(const Move &move) const
{
    constexpr Square G1{"G1"}; // White kingside castling king destination.
    constexpr Square C1{"C1"}; // White queenside castling king destination.
    constexpr Square G8{"G8"}; // Black kingside castling king destination.
    constexpr Square C8{"C8"}; // Black queenside castling king destination.

    const Color  color = color_to_move_;
    const Square from  = move.from();
    const Square to    = move.to();
    const Piece  piece = move.piece();

    Position position{*this};
    position.is_null_move_ = false;
    position.castling_rights_.AfterMove(move);
    position.hash_ ^= position.castling_rights_.Hash() ^ castling_rights_.Hash();
    position.hash_ ^= kEnPassantHashes[position.en_passant_square_];
    position.en_passant_square_ = Square{(uint8_t)0};

    switch (move.type())
    {
    case Move::Type::kNonCapture:
        ++position.reversible_move_count_;
        position.MovePiece(color, piece, from, to);
        break;

    case Move::Type::kCapture:
        position.reversible_move_count_ = 0;
        position.RemovePiece(EnemyOf(color), move.captured(), to);
        position.MovePiece(color, piece, from, to);
        break;

    case Move::Type::kPromotionKnight:
    case Move::Type::kPromotionBishop:
    case Move::Type::kPromotionRook:
    case Move::Type::kPromotionQueen:
        position.reversible_move_count_ = 0;
        if (move.captured() != Piece::kNone)
        {
            position.RemovePiece(EnemyOf(color), move.captured(), to);
        }
        position.RemovePiece(color, kPawn, from);
        position.AddPiece(color, move.promoted(), to);
        break;

    case Move::Type::kPawnDoublePush:
        position.reversible_move_count_ = 0;
        position.MovePiece(color, kPawn, from, to);
        position.en_passant_square_ = Square{(from + to) >> 1};
        position.hash_ ^= kEnPassantHashes[position.en_passant_square_];
        break;

    case Move::Type::kEpCapture:
        position.reversible_move_count_ = 0;
        // En passant capture: capture location is source rank, destination file.
        position.RemovePiece(EnemyOf(color), kPawn, Square{(from & 0x38) | (to & 0x07)});
        position.MovePiece(color, kPawn, from, to);
        break;

    case Move::Type::kCastling:
        position.reversible_move_count_ = 0;
        switch (to)
        {
        case G1:
            position.MovePiece(kWhite, kKing, "E1", "G1");
            position.MovePiece(kWhite, kRook, "H1", "F1");
            break;
        case C1:
            position.MovePiece(kWhite, kKing, "E1", "C1");
            position.MovePiece(kWhite, kRook, "A1", "D1");
            break;
        case G8:
            position.MovePiece(kBlack, kKing, "E8", "G8");
            position.MovePiece(kBlack, kRook, "H8", "F8");
            break;
        case C8:
            position.MovePiece(kBlack, kKing, "E8", "C8");
            position.MovePiece(kBlack, kRook, "A8", "D8");
            break;
        default:
            break;
        }
        break;
    }
    position.color_to_move_ = EnemyOf(color);
    position.hash_ ^= kBlackMoveHash;
    position.full_move_count_ += color; // Increments after black's move.
    position.king_location_[color] = (position.pieces_[kKing] & position.colors_[color]).Lsb();
    position.checkers_             = position.AttacksTo(position.king_location_[EnemyOf(color)], color);
    return position;
}

/// @brief Construct a position.
/// @param fen_string Forsyth Edwards string.
inline Position Position::FromString(std::string_view fen_string)
{
    Position position{};
    // std::memset((void *)&position, 0, sizeof(position));
    constexpr string_view white_piece_names{"PNBRQK"};
    constexpr string_view black_piece_names{"pnbrqk"};
    istringstream         ss{string{fen_string}};
    //  Pieces on the board
    string pieces;
    ss >> pieces;
    int x = 0, y = 7;
    for (const char &c : pieces)
    {
        if (c == '/')
        {
            x = 0;
            --y;
            continue;
        }
        if (c >= '1' && c <= '8')
        {
            x += c - '0';
            continue;
        }
        auto a = white_piece_names.find(c);
        if (a != string::npos)
        {
            const Piece piece = (Piece)(a + 1);
            position.AddPiece(kWhite, piece, Square{x, y});
            ++x;
            continue;
        }
        a = black_piece_names.find(c);
        if (a != string::npos)
        {
            const Piece piece = (Piece)(a + 1);
            position.AddPiece(kBlack, piece, Square{x, y});
            ++x;
            continue;
        }
    }
    // Side to move
    string color_to_move;
    ss >> color_to_move;
    if (color_to_move == "b")
    {
        position.color_to_move_ = kBlack;
    }
    position.king_location_[kWhite] = (position.pieces_[kKing] & position.colors_[kWhite]).Lsb();
    position.king_location_[kBlack] = (position.pieces_[kKing] & position.colors_[kBlack]).Lsb();
    // Castling rights
    string castling_rights;
    ss >> castling_rights;
    position.castling_rights_ = CastlingRights::FromFen(castling_rights);
    // En passant capture square
    string ep_square;
    ss >> ep_square;
    if (ep_square == "-")
    {
        position.en_passant_square_ = Square{0};
    }
    else
    {
        position.en_passant_square_ = Square{ep_square.c_str()};
    }
    if (ss)
    {
        // Half move clock - optional
        int hmc = 0;
        ss >> hmc;
        position.reversible_move_count_ = (uint8_t)hmc;
    }
    if (ss)
    {
        // Full move number - optional
        int fmn = 1;
        ss >> fmn;
        position.full_move_count_ = (uint8_t)fmn - 1;
    }
    position.hash_ = position.ComputeHash();
    // Is this position check?
    const Color color  = position.color_to_move_;
    position.checkers_ = position.AttacksTo(position.king_location_[color], EnemyOf(color));
    return position;
}

/// @brief Serialize the position to a FEN string.
/// @return The FEN representation of the position.
inline std::string Position::ToString() const
{
    ostringstream ss;
    // Pieces on the board
    const Bitboard occupied_squares = OccupiedSquares();
    for (int y = 7; y >= 0; --y)
    {
        int num_empty_squares = 0;
        for (int x = 0; x < 8; ++x)
        {
            if ((occupied_squares & Bitboard(x, y)).IsEmpty())
            {
                ++num_empty_squares;
            }
            else
            {
                if (num_empty_squares)
                {
                    ss << num_empty_squares;
                    num_empty_squares = 0;
                }
                const char piece = (colors_[kWhite] & Bitboard(x, y)).IsNotEmpty()
                                       ? " PNBRQK"[PieceAt((Square)(x + 8 * y))]
                                       : " pnbrqk"[PieceAt((Square)(x + 8 * y))];
                ss << piece;
            }
        }
        if (num_empty_squares)
        {
            ss << num_empty_squares;
            num_empty_squares = 0;
        }
        if (y)
        {
            ss << '/';
        }
    }
    // Side to move
    ss << ' ' << (color_to_move_ == kBlack ? 'b' : 'w') << ' ';
    // Castling rights
    ss << castling_rights_.ToFenString();
    ss << ' ';
    // En passant capture availability
    if (en_passant_square_ == 0)
    {
        ss << '-';
    }
    else
    {
        ss << en_passant_square_.ToString();
    }
    ss << ' ';
    // Half move clock and full move number
    ss << (int)reversible_move_count_ << ' ' << (int)(full_move_count_ + 1);
    return ss.str();
}

/// @brief Determine if the position is draw by insufficient material.
/// @return true if a material draw
constexpr bool Position::IsDrawByMaterial() const
{
    const Bitboard occupied_squares = OccupiedSquares();
    switch (occupied_squares.PopCount())
    {
    case 0:
    case 1:
        std::cout << "ERROR: too few pieces for play\n";
        return true;
    case 2:
        // king vs king
        INCREMENT("draws by material (2)");
        return true;
    case 3:
        // king and bishop vs king or king and knight vs king
        if ((pieces_[kBishop] | pieces_[kKnight]).IsNotEmpty())
        {
            INCREMENT("draws by material (3)");
            return true;
        }
        return false;
    case 4:
        // king and bishop vs king and bishop with bishops on same color square
        {
            const Bitboard white_bishops                   = pieces_[kBishop] & colors_[kWhite];
            const Bitboard black_bishops                   = pieces_[kBishop] & colors_[kBlack];
            const bool     is_white_bishop_on_white_square = (white_bishops & kWhiteSquares).IsNotEmpty();
            const bool     is_black_bishop_on_white_square = (black_bishops & kWhiteSquares).IsNotEmpty();
            if (white_bishops.IsNotEmpty() && black_bishops.IsNotEmpty() &&
                is_white_bishop_on_white_square == is_black_bishop_on_white_square)
            {
                INCREMENT("draws by material (4)");
                return true;
            }
            return false;
        }
    default:
        return false;
    }
}

/// @brief Compute the Zobrist hash for a chess position.
/// @return the 64 bit hash
constexpr zobrist_t Position::ComputeHash() const
{
    zobrist_t hash = color_to_move_ == kBlack ? kBlackMoveHash : 0ull;
    hash ^= castling_rights_.Hash();
    hash ^= kEnPassantHashes[en_passant_square_];
    constexpr std::array piece_types{kPawn, kKnight, kBishop, kRook, kQueen, kKing};
    for (auto piece : piece_types)
    {
        Bitboard b = pieces_[piece] & colors_[kWhite];
        for (Square s : b)
        {
            hash ^= kPieceSquareHashes[kWhite][piece - 1][s];
        }
        b = pieces_[piece] & colors_[kBlack];
        for (Square s : b)
        {
            hash ^= kPieceSquareHashes[kBlack][piece - 1][s];
        }
    }
    return hash;
}

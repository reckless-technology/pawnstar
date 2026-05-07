#include <algorithm>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

using std::string;
using std::string_view;
using std::stringstream;
using std::vector;

#include "debug_hashtable.h"
#include "position.h"
#include "transposition_table.h"

/// @brief Make a null move: don't actually move any pieces on the board.
/// Flip side to move.
/// Clear en passant capture availablity.
/// Set the flag indicating this position is the result of a null move.
/// @return new position.
Position Position::MakeNullMove() const
{
    Position position{*this};
    position.state_flags_ |= kIsNullMove;
    position.state_flags_ ^= kIsBlackToMove;
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
    PiecesOfType(piece) ^= to_bb;
    PiecesOfColor(color) ^= to_bb;
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
    PiecesOfType(piece) ^= from_bb;
    PiecesOfColor(color) ^= from_bb;
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
    PiecesOfType(piece) ^= from_to_bb;
    PiecesOfColor(color) ^= from_to_bb;
    hash_ ^= hash[to] ^ hash[from];
    squares_[from] = Piece::kNone;
    squares_[to]   = piece;
}

static constexpr Square G1{"G1"};
static constexpr Square C1{"C1"};
static constexpr Square G8{"G8"};
static constexpr Square C8{"C8"};

/// @brief Make a move and return the new position.
/// @param move The move to be made.
/// @return new Position following the move.
Position Position::MakeMove(const Move &move) const
{
    const Color  color = ColorToMove();
    const Square from  = move.from();
    const Square to    = move.to();
    const Piece  piece = move.piece();

    Position position{*this};
    position.state_flags_ &= ~kIsNullMove;
    position.castling_rights_ = castling_rights_.AfterMove(move);
    position.hash_ ^=
        kCastlingRightsHashes[position.castling_rights_.Value()] ^ kCastlingRightsHashes[castling_rights_.Value()];
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
    position.state_flags_ ^= kIsBlackToMove;
    position.hash_ ^= kBlackMoveHash;
    position.full_move_count_ += color; // Increments after black's move.
    position.king_location_[color] = (position.kings_ & position.PiecesOfColor(color)).Lsb();
    position.checkers_             = position.AttacksTo(position.king_location_[EnemyOf(color)], color);
    return position;
}

/// @brief Construct a position.
/// @param fen_string Forsyth Edwards string.
Position Position::FromString(std::string_view fen_string)
{
    Position position;
    std::memset((void *)&position, 0, sizeof(position));
    constexpr string_view white_piece_names{"PNBRQK"};
    constexpr string_view black_piece_names{"pnbrqk"};
    stringstream          ss;
    ss << fen_string;
    // Pieces on the board
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
            position.AddPiece(kWhite, piece, Square{(uint8_t)x, (uint8_t)y});
            ++x;
            continue;
        }
        a = black_piece_names.find(c);
        if (a != string::npos)
        {
            const Piece piece = (Piece)(a + 1);
            position.AddPiece(kBlack, piece, Square{(uint8_t)x, (uint8_t)y});
            ++x;
            continue;
        }
    }
    // Side to move
    string color_to_move;
    ss >> color_to_move;
    if (color_to_move == "b")
    {
        position.state_flags_ |= kIsBlackToMove;
    }
    position.king_location_[kWhite] = (position.kings_ & position.white_pieces_).Lsb();
    position.king_location_[kBlack] = (position.kings_ & position.black_pieces_).Lsb();
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
    const Color color  = position.ColorToMove();
    position.checkers_ = position.AttacksTo(position.king_location_[color], EnemyOf(color));
    return position;
}

std::string Position::ToString() const
{
    stringstream ss;
    // Pieces on the board
    const Bitboard occupied_squares = white_pieces_ | black_pieces_;
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
                const char piece = (white_pieces_ & Bitboard(x, y)).IsNotEmpty()
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
    ss << ' ' << (state_flags_ & kIsBlackToMove ? 'b' : 'w') << ' ';
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
bool Position::IsDrawByMaterial() const
{
    const Bitboard occupied_squares = white_pieces_ | black_pieces_;
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
        if ((bishops_ | knights_).IsNotEmpty())
        {
            INCREMENT("draws by material (3)");
            return true;
        }
        return false;
    case 4:
        // king and bishop vs king and bishop with bishops on same color square
        {
            const Bitboard white_bishops                   = bishops_ & white_pieces_;
            const Bitboard black_bishops                   = bishops_ & black_pieces_;
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

/// @brief Determine if the current position state is a legal chess position:
/// a) each side shall have strictly 1 king
/// b) kings shall not be adjacent
/// c) the side not on move shall not be in check
/// @return true if legal
bool Position::IsLegal() const
{
    const Color    color      = ColorToMove();
    const Bitboard white_king = kings_ & white_pieces_;
    const Bitboard black_king = kings_ & black_pieces_;
    return white_king.PopCount() == 1 && black_king.PopCount() == 1 && white_king != black_king &&
           (kKingAttacks[white_king.Lsb()] & black_king).IsEmpty() &&
           !IsAttacked(king_location_[EnemyOf(color)], color);
}

/// @brief Compute the Zobrist hash for a chess position.
/// @return the 64 bit hash
constexpr zobrist_t Position::ComputeHash() const
{
    zobrist_t hash = state_flags_ & kIsBlackToMove ? kBlackMoveHash : 0ull;
    hash ^= kCastlingRightsHashes[castling_rights_.Value()];
    hash ^= kEnPassantHashes[en_passant_square_];
    constexpr std::array pieces{kPawn, kKnight, kBishop, kRook, kQueen, kKing};
    for (auto piece : pieces)
    {
        Bitboard b = PiecesOfType(piece) & white_pieces_;
        for (Square s : b)
        {
            hash ^= kPieceSquareHashes[kWhite][piece - 1][s];
        }
        b = PiecesOfType(piece) & black_pieces_;
        for (Square s : b)
        {
            hash ^= kPieceSquareHashes[kBlack][piece - 1][s];
        }
    }
    return hash;
}

bool Position::IsStalemate() const
{
    return !IsInCheck() && GenerateLegalMoves().size() == 0;
}

bool Position::IsCheckmate() const
{
    return IsInCheck() && GenerateLegalMoves().size() == 0;
}

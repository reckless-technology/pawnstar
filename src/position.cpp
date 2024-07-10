#include <cstring>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

using std::string;
using std::string_view;
using std::stringstream;
using std::vector;

#include "debug_hashtable.h"
#include "function_prototypes.h"
#include "move_generation.h"
#include "position.h"
#include "transposition_table.h"

#define A1M (uint16_t)(~MAY_WHITE_CASTLE_QUEENSIDE)
#define E1M (uint16_t)(~(MAY_WHITE_CASTLE_QUEENSIDE | MAY_WHITE_CASTLE_KINGSIDE))
#define H1M (uint16_t)(~MAY_WHITE_CASTLE_KINGSIDE)
#define A8M (uint16_t)(~MAY_BLACK_CASTLE_QUEENSIDE)
#define E8M (uint16_t)(~(MAY_BLACK_CASTLE_QUEENSIDE | MAY_BLACK_CASTLE_KINGSIDE))
#define H8M (uint16_t)(~MAY_BLACK_CASTLE_KINGSIDE)
#define OK  0xFFFF

/**
 * @brief Castling rights flags based on move square from and to index.
 * Moves to and from king and rook squares invalidate castling rights.
 */

static const uint16_t CASTLING_RIGHTS_MASKS[64] = {
    /* clang-format off */
    A1M, OK, OK, OK,E1M, OK, OK,H1M, 
     OK, OK, OK, OK, OK, OK, OK, OK, 
     OK, OK, OK, OK, OK, OK, OK, OK, 
     OK, OK, OK, OK, OK, OK, OK, OK, 
     OK, OK, OK, OK, OK, OK, OK, OK, 
     OK, OK, OK, OK, OK, OK, OK, OK, 
     OK, OK, OK, OK, OK, OK, OK, OK, 
    A8M, OK, OK, OK,E8M, OK, OK,H8M,
    /* clang-format on */
};

/**
 * @brief Make a null move: don't actually move any pieces on the board.
 * # Flip side to move
 * # Clear en passant capture availablity
 * # Set the flag indicating this position is the result of a null move
 * @param position destination position
 * @param src_position source position
 */
Position Position::MakeNullMove() const
{
    Position dst_position{*this};
    dst_position.previous_ = this;
    dst_position.flags_ |= IS_NULL_MOVE;
    dst_position.flags_ ^= IS_BLACK_TO_MOVE;
    dst_position.hash_ ^= EN_PASSANT_HASHES[dst_position.en_passant_index_];
    dst_position.hash_ ^= BLACK_MOVE_HASH;
    dst_position.en_passant_index_ = NO_SQUARE;
    return dst_position;
}

void Position::AddPiece(Color color, Piece piece, Square to)
{
    const Bitboard to_bb = BITBOARD(to);
    pieces_[piece - 1] ^= to_bb;
    pieces_of_color_[color] ^= to_bb;
    hash_ ^= PIECE_SQUARE_HASHES[color][piece - 1][to];
}

void Position::RemovePiece(Color color, Piece piece, Square from)
{
    const Bitboard from_bb = BITBOARD(from);
    pieces_[piece - 1] ^= from_bb;
    pieces_of_color_[color] ^= from_bb;
    hash_ ^= PIECE_SQUARE_HASHES[color][piece - 1][from];
}

void Position::MovePiece(Color color, Piece piece, Square from, Square to)
{
    const Bitboard        from_to_bb = BITBOARD(from) | BITBOARD(to);
    const uint64_t *const hash       = &PIECE_SQUARE_HASHES[color][piece - 1][0];
    pieces_[piece - 1] ^= from_to_bb;
    pieces_of_color_[color] ^= from_to_bb;
    hash_ ^= hash[to] ^ hash[from];
}

Position Position::MakeMove(Move move) const
{
    const Color  color    = ColorToMove();
    const Square from     = MoveFrom(move);
    const Square to       = MoveTo(move);
    const Piece  piece    = ::MovePiece(move);
    const Piece  captured = MoveCaptured(move);

    Position position{*this};
    position.previous_ = this;
    position.flags_ &= ~IS_NULL_MOVE;
    position.flags_ &= CASTLING_RIGHTS_MASKS[from] & CASTLING_RIGHTS_MASKS[to];
    position.hash_ ^=
        CASTLING_RIGHTS_HASHES[position.flags_ & CASTLING_RIGHTS_MASK] ^ CASTLING_RIGHTS_HASHES[this->flags_ & CASTLING_RIGHTS_MASK];
    position.hash_ ^= EN_PASSANT_HASHES[position.en_passant_index_];
    position.en_passant_index_ = NO_SQUARE;
    ++position.reversible_move_count_;
    switch (piece)
    {
    case PAWN:
        position.reversible_move_count_ = 0;
        if (captured)
        {
            if (IsEpCaptureMove(move))
            {
                /* En passant capture: capture location is source rank, destination file. */
                const Square captured_pawn_locn = (Square)((from & 0x38) | (to & 0x07));
                position.RemovePiece(EnemyOf(color), PAWN, captured_pawn_locn);
            }
            else
            {
                position.RemovePiece(EnemyOf(color), captured, to);
            }
        }
        if (MovePromoted(move))
        {
            /* Replace the pawn with the promoted piece. */
            position.RemovePiece(color, PAWN, from);
            position.AddPiece(color, MovePromoted(move), to);
        }
        else
        {
            position.MovePiece(color, PAWN, from, to);
            if (IsPawnDoublePushMove(move))
            {
                /* Pawn double push: affects en passant. */
                position.en_passant_index_ = (Square)((from + to) >> 1);
                position.hash_ ^= EN_PASSANT_HASHES[position.en_passant_index_];
            }
        }
        break;

    case KNIGHT:
    case BISHOP:
    case ROOK:
    case QUEEN:
    default:
        if (captured)
        {
            position.RemovePiece(EnemyOf(color), captured, to);
            position.reversible_move_count_ = 0;
        }
        position.MovePiece(color, piece, from, to);
        break;

    case KING:
        if (!IsCastlingMove(move))
        {
            if (captured)
            {
                position.RemovePiece(EnemyOf(color), captured, to);
                position.reversible_move_count_ = 0;
            }
            position.MovePiece(color, KING, from, to);
            break;
        }
        /* Castling move. */
        switch (to)
        {
        case G1:
            position.MovePiece(WHITE, KING, E1, G1);
            position.MovePiece(WHITE, ROOK, H1, F1);
            break;
        case C1:
            position.MovePiece(WHITE, KING, E1, C1);
            position.MovePiece(WHITE, ROOK, A1, D1);
            break;
        case G8:
            position.MovePiece(BLACK, KING, E8, G8);
            position.MovePiece(BLACK, ROOK, H8, F8);
            break;
        case C8:
            position.MovePiece(BLACK, KING, E8, C8);
            position.MovePiece(BLACK, ROOK, A8, D8);
            break;
        default:
            break;
        }
        break;
    };
    position.flags_ ^= IS_BLACK_TO_MOVE;
    position.hash_ ^= BLACK_MOVE_HASH;
    position.full_move_count_ += color;
    position.king_location_[WHITE] = Lsb(position.kings_ & position.white_pieces_);
    position.king_location_[BLACK] = Lsb(position.kings_ & position.black_pieces_);
    if (position.IsAttacked(position.king_location_[color], EnemyOf(color)))
    {
        position.flags_ |= HAS_MOVED_INTO_CHECK;
    }
    else
    {
        if (position.IsAttacked(position.king_location_[EnemyOf(color)], color))
        {
            position.flags_ |= IS_CHECK;
        }
        else
        {
            position.flags_ &= ~IS_CHECK;
        }
    }
    return position;
}

#define BAD_FEN_STRING                                   \
    {                                                    \
        const std::string s{fen_string};                 \
        printf("ERROR: bad FEN string %s\n", s.c_str()); \
        return;                                          \
    }

Position::Position(std::string_view fen_string)
{
    static constexpr string_view white_piece_names{"PNBRQK"};
    static constexpr string_view black_piece_names{"pnbrqk"};
    static constexpr string_view ep_files{"abcdefgh"};
    static constexpr string_view ep_ranks{"36"};
    stringstream                 ss;
    ss << fen_string;
    std::memset(this, 0, sizeof(Position));
    previous_ = this;
    /* Pieces on the board */
    string pieces;
    ss >> pieces;
    if (pieces == "")
    {
        BAD_FEN_STRING;
    }
    int x = 0, y = 7;
    for (const char &c : pieces)
    {
        if (c == '/')
        {
            if (x != 8)
            {
                BAD_FEN_STRING;
            }
            x = 0;
            if (--y == -1)
            {
                BAD_FEN_STRING;
            }
            continue;
        }
        if (c >= '1' && c <= '8')
        {
            x += c - '0';
            continue;
        }
        if (x >= 8)
        {
            BAD_FEN_STRING;
        }
        auto a = white_piece_names.find(c);
        if (a != string::npos)
        {
            const Piece piece = (Piece)(a + 1);
            AddPiece(WHITE, piece, (Square)(x + 8 * y));
            ++x;
            continue;
        }
        a = black_piece_names.find(c);
        if (a != string::npos)
        {
            const Piece piece = (Piece)(a + 1);
            AddPiece(BLACK, piece, (Square)(x + 8 * y));
            ++x;
            continue;
        }
        BAD_FEN_STRING;
    }
    if (x != 8 || y != 0)
    {
        BAD_FEN_STRING;
    }
    /* Side to move */
    string color_to_move;
    ss >> color_to_move;
    if (color_to_move == "")
    {
        BAD_FEN_STRING;
    }
    if (color_to_move == "b")
    {
        flags_ |= IS_BLACK_TO_MOVE;
    }
    else if (color_to_move != "w")
    {
        BAD_FEN_STRING;
    }
    king_location_[WHITE] = Lsb(kings_ & white_pieces_);
    king_location_[BLACK] = Lsb(kings_ & black_pieces_);
    /* Castling rights */
    string castling_rights;
    ss >> castling_rights;
    if (castling_rights == "")
    {
        BAD_FEN_STRING;
    }
    if (castling_rights == "-")
    {
        flags_ &= ~CASTLING_RIGHTS_MASK;
    }
    else
    {
        for (const char &c : castling_rights)
        {
            switch (c)
            {
            case 'K':
                flags_ |= MAY_WHITE_CASTLE_KINGSIDE;
                break;
            case 'Q':
                flags_ |= MAY_WHITE_CASTLE_QUEENSIDE;
                break;
            case 'k':
                flags_ |= MAY_BLACK_CASTLE_KINGSIDE;
                break;
            case 'q':
                flags_ |= MAY_BLACK_CASTLE_QUEENSIDE;
                break;
            default:
                BAD_FEN_STRING;
            }
        }
    }
    /* En passant capture square */
    string ep_square;
    ss >> ep_square;
    if (ep_square == "")
    {
        BAD_FEN_STRING;
    }
    if (ep_square == "-")
    {
        en_passant_index_ = NO_SQUARE;
    }
    else
    {
        if (ep_square.length() != 2 || ep_files.find(ep_square[0]) == string::npos || ep_ranks.find(ep_square[1]) == string::npos)
        {
            BAD_FEN_STRING;
        }
        en_passant_index_ = (Square)((ep_square[0] - 'a') + 8 * (ep_square[1] - '1'));
    }
    if (ss)
    {
        /* Half move clock - optional */
        int hmc = 0;
        ss >> hmc;
        reversible_move_count_ = (uint8_t)hmc;
    }
    if (ss)
    {
        /* Full move number - optional */
        int fmn = 1;
        ss >> fmn;
        full_move_count_ = (uint8_t)fmn - 1;
    }
    hash_ = ComputeHash();
    /* Legality of position */
    if (!IsLegal())
    {
        BAD_FEN_STRING;
    }
    /* Is the position in check? */
    const Color color = ColorToMove();
    if (IsAttacked(king_location_[color], EnemyOf(color)))
    {
        flags_ |= IS_CHECK;
    }
}

Position::operator std::string() const
{
    stringstream ss;
    /* Pieces on the board */
    const Bitboard occupied_squares = white_pieces_ | black_pieces_;
    for (int y = 7; y >= 0; --y)
    {
        int num_empty_squares = 0;
        for (int x = 0; x < 8; ++x)
        {
            if (!(occupied_squares & BITBOARD(x, y)))
            {
                ++num_empty_squares;
            }
            else
            {
                if (num_empty_squares)
                {
                    ss << (char)('0' + num_empty_squares);
                    num_empty_squares = 0;
                }
                const char piece =
                    (white_pieces_ & BITBOARD(x, y)) ? " PNBRQK"[PieceAt((Square)(x + 8 * y))] : " pnbrqk"[PieceAt((Square)(x + 8 * y))];
                ss << piece;
            }
        }
        if (num_empty_squares)
        {
            ss << (char)('0' + num_empty_squares);
            num_empty_squares = 0;
        }
        if (y)
        {
            ss << '/';
        }
    }
    /* Side to move */
    ss << ' ' << (flags_ & IS_BLACK_TO_MOVE ? 'b' : 'w') << ' ';
    /* Castling rights */
    if (!(flags_ & CASTLING_RIGHTS_MASK))
    {
        ss << '-';
    }
    else
    {
        if (flags_ & MAY_WHITE_CASTLE_KINGSIDE)
        {
            ss << 'K';
        }
        if (flags_ & MAY_WHITE_CASTLE_QUEENSIDE)
        {
            ss << 'Q';
        }
        if (flags_ & MAY_BLACK_CASTLE_KINGSIDE)
        {
            ss << 'k';
        }
        if (flags_ & MAY_BLACK_CASTLE_QUEENSIDE)
        {
            ss << 'q';
        }
    }
    ss << ' ';
    /* En passant capture availability */
    if (en_passant_index_ == 0)
    {
        ss << '-';
    }
    else
    {
        ss << FileChar(en_passant_index_) << RankChar(en_passant_index_);
    }
    ss << ' ';
    /* Half move clock and full move number */
    ss << (int)reversible_move_count_ << ' ' << (int)(full_move_count_ + 1);
    return ss.str();
}

/**
 * @brief Determine if a square is attacked by a color
 * @param location  location of attacked square
 * @param color attacking color
 * @return true if color attacks position, otherwise false
 */
bool Position::IsAttacked(Square location, Color color) const
{
    const Sets           &sets                = SETS[location];
    const Bitboard *const intervening_squares = &INTERVENING_SQUARES[location][0];
    const Bitboard        attacking_pieces    = pieces_of_color_[color];
    const Bitboard        occupied_squares    = white_pieces_ | black_pieces_;
    /*
    Pawn, knight and king attacks can be done by direct lookup since blockers
    do not affect their attack set.
    */
    if (attacking_pieces & ((sets.pawn_attacks[EnemyOf(color)] & pawns_) | (sets.knight_attacks & knights_) | (sets.king_attacks & kings_)))
    {
        return true;
    }
    /*
    Rook and queen horizontal and vertical sliding attacks
    */
    Bitboard sliding_attackers = (rooks_ | queens_) & sets.rook_attacks & attacking_pieces;
    while (sliding_attackers)
    {
        if (!(intervening_squares[FindAndClearLsb(sliding_attackers)] & occupied_squares))
        {
            return true;
        }
    }
    /*
    Bishop and queen diagonal and antidiagonal sliding attacks
    */
    sliding_attackers = (bishops_ | queens_) & sets.bishop_attacks & attacking_pieces;
    while (sliding_attackers)
    {
        if (!(intervening_squares[FindAndClearLsb(sliding_attackers)] & occupied_squares))
        {
            return true;
        }
    }
    return false;
}

/**
 * @brief Determine attacks to a square by a color.
 * @param position Board position.
 * @param location Square index.
 * @param color Attacking color.
 * @return Set of squares of color containing a piece attacking location.
 */
Bitboard Position::AttacksTo(Square location, Color color) const
{
    const Sets           &sets                = SETS[location];
    const Bitboard *const intervening_squares = &INTERVENING_SQUARES[location][0];
    const Bitboard        attacking_pieces    = pieces_of_color_[color];
    const Bitboard        occupied_squares    = white_pieces_ | black_pieces_;
    /*
    Pawn, knight and king attacks can be done by direct lookup since blockers
    do not affect their attack set.
    */
    Bitboard result = (attacking_pieces &
                       ((sets.pawn_attacks[EnemyOf(color)] & pawns_) | (sets.knight_attacks & knights_) | (sets.king_attacks & kings_)));
    /*
    Rook and queen horizontal and vertical sliding attacks
    */
    Bitboard sliding_attackers = (rooks_ | queens_) & sets.rook_attacks & attacking_pieces;
    while (sliding_attackers)
    {
        const Square locn = FindAndClearLsb(sliding_attackers);
        if (!(intervening_squares[locn] & occupied_squares))
        {
            result |= BITBOARD(locn);
        }
    }
    /*
    Bishop and queen diagonal and antidiagonal sliding attacks
    */
    sliding_attackers = (bishops_ | queens_) & sets.bishop_attacks & attacking_pieces;
    while (sliding_attackers)
    {
        const Square locn = FindAndClearLsb(sliding_attackers);
        if (!(intervening_squares[locn] & occupied_squares))
        {
            result |= BITBOARD(locn);
        }
    }
    return result;
}

/**
 * @brief Determine if the current position state is a legal chess position:
 * a) each side shall have strictly 1 king
 * b) kings shall not be adjacent
 * c) the side not on move shall not be in check
 */
bool Position::IsLegal() const
{
    const Color    color      = ColorToMove();
    const Bitboard white_king = kings_ & white_pieces_;
    const Bitboard black_king = kings_ & black_pieces_;
    return PopCount(white_king) == 1 && PopCount(black_king) == 1 && white_king != black_king &&
           !(SETS[Lsb(white_king)].king_attacks & black_king) && !IsAttacked(king_location_[EnemyOf(color)], color);
}

/*
Determine if the current position represents a draw by repetition.
*/
bool Position::IsDrawByRepetition(bool is_search) const
{
    const Position *position    = this;
    const uint64_t  hash        = hash_;
    int             repetitions = is_search ? 1 : 2;
    for (position = position->previous_; position->reversible_move_count_ != 0; position = position->previous_)
    {
        if (position->hash_ == hash && --repetitions == 0)
        {
            INCREMENT("draws by repetition");
            return true;
        }
    }
    return false;
}
/*
Determine if the current position represents a draw by the 50 move rule
(50 consecutive reversible moves by each player is a drawn game)
*/
bool Position::IsDrawByFiftyMoves() const
{
    if (reversible_move_count_ >= 100)
    {
        INCREMENT("draws by 50 moves");
        return true;
    }
    return false;
}
/*
Determine if the current position represents a draw by insufficient material

according to FIDE rules the drawn combinations are:

a) king vs king
b) king and knight vs king
c) king and bishop vs king
d) king and bishop vs king and bishop, with bishops on the same color square
*/
bool Position::IsDrawByMaterial() const
{
    const Bitboard occupied_squares = white_pieces_ | black_pieces_;
    switch (PopCount(occupied_squares))
    {
    case 0:
    case 1:
        printf("ERROR: too few pieces for play\n");
        return true;
    case 2:
        /*
        king vs king
        */
        INCREMENT("draws by material (2)");
        return true;
    case 3:
        /*
        king and bishop vs king
        king and knight vs king
        */
        if (bishops_ | knights_)
        {
            INCREMENT("draws by material (3)");
            return true;
        }
        return false;
    case 4:
        /*
        king and bishop vs king and bishop with bishops on same color square
        */
        {
            const Bitboard white_bishops = bishops_ & white_pieces_;
            const Bitboard black_bishops = bishops_ ^ white_bishops;
            if (white_bishops && black_bishops && (!(white_bishops & WHITE_SQUARES) == !(black_bishops & WHITE_SQUARES)))
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
/*
Determine if the current position represents stalemate
*/
bool Position::IsStalemate() const
{
    return !IsInCheck() && GenerateLegalMoves(*this).size() == 0;
}
/*
Determine if the current position represents checkmate
*/
bool Position::IsCheckmate() const
{
    return IsInCheck() && GenerateLegalMoves(*this).size() == 0;
}

/**
 * @brief Compute the Zobrist hash for a chess position.
 * @return the 64 bit hash
 */
uint64_t Position::ComputeHash() const
{
    uint64_t hash = flags_ & IS_BLACK_TO_MOVE ? BLACK_MOVE_HASH : 0ull;
    hash ^= CASTLING_RIGHTS_HASHES[flags_ & CASTLING_RIGHTS_MASK];
    hash ^= EN_PASSANT_HASHES[en_passant_index_];
    for (int piece = PAWN - 1; piece <= KING - 1; ++piece)
    {
        Bitboard b = pieces_[piece] & white_pieces_;
        while (b)
        {
            hash ^= PIECE_SQUARE_HASHES[WHITE][piece][FindAndClearLsb(b)];
        }
        b = pieces_[piece] & black_pieces_;
        while (b)
        {
            hash ^= PIECE_SQUARE_HASHES[BLACK][piece][FindAndClearLsb(b)];
        }
    }
    return hash;
}

/**
 * @brief Convert a sequence of moves into a string in SAN format.
 * @param position Starting position.
 * @param moves Zero terminated list of moves.
 * @param move_string Pointer to store the formatted string.
 * @return Number of characters written to output.
 */
string Position::VariationToString(const Variation &variation) const
{
    stringstream ss;
    bool         is_first_move = true;
    Position     src_position{*this};
    for (const auto move : variation)
    {
        if (!is_first_move)
        {
            ss << ' ';
        }
        ss << src_position.MoveToString(move);
        Position dst_position{src_position.MakeMove(move)};
        src_position  = dst_position;
        is_first_move = false;
    }
    return ss.str();
}

/**
 * @brief Convert a move to standard algebraic notation string.
 * @param position Current position corresponding to the move.
 * @param the_move The move input to be formatted as a string.
 * @param move_string Where to store the move string.
 * @return Number of characters written to output.
 */
string Position::MoveToString(Move move) const
{
    stringstream result;
    string       disambiguation;
    string       from_square{FileChar(MoveFrom(move)), RankChar(MoveFrom(move))};
    string       to_square{FileChar(MoveTo(move)), RankChar(MoveTo(move))};
    bool         is_source_ambiguous = false;
    bool         is_file_unique      = true;
    bool         is_rank_unique      = true;
    const char   move_piece          = " PNBRQK"[::MovePiece(move)];
    /*
    Determine if there is more than one piece of the same type which is capable
    of moving to the target square, and will require further disambiguation
    */
    MoveList legal_moves = GenerateLegalMoves(*this);
    for (const Move &m : legal_moves)
    {
        if (::MovePiece(m) == ::MovePiece(move) && MoveTo(m) == MoveTo(move) && MoveFrom(m) != MoveFrom(move))
        {
            is_source_ambiguous = true;
            if (FileOf(MoveFrom(m)) == FileOf(MoveFrom(move)))
            {
                is_file_unique = false;
            }
            if (RankOf(MoveFrom(m)) == RankOf(MoveFrom(move)))
            {
                is_rank_unique = false;
            }
        }
    }
    /*
    Determine how to disambiguate source square based on the uniqueness of the
    source rank and file
    */
    if (is_source_ambiguous)
    {
        if (is_file_unique)
        {
            disambiguation = from_square[0];
        }
        else if (is_rank_unique)
        {
            disambiguation = from_square[1];
        }
        else
        {
            disambiguation = from_square;
        }
    }
    switch (::MovePiece(move))
    {
    RegularMove:
    default:
    case KNIGHT:
    case BISHOP:
    case ROOK:
    case QUEEN:
        if (MoveCaptured(move))
        {
            result << move_piece << disambiguation << 'x' << to_square;
        }
        else
        {
            result << move_piece << disambiguation << to_square;
        }
        break;

    case KING:
        if (IsCastlingMove(move))
        {
            switch (MoveTo(move))
            {
            case G1:
            case G8:
                result << "O-O";
                break;
            case C1:
            case C8:
                result << "O-O-O";
                break;
            default:
                break;
            }
        }
        else
        {
            goto RegularMove;
        }
        break;

    case PAWN:
        if (MoveCaptured(move))
        {
            result << from_square[0] << 'x' << to_square;
            if (IsEpCaptureMove(move))
            {
                result << "e.p.";
            }
        }
        else
        {
            result << to_square;
        }
        if (MovePromoted(move))
        {
            result << '=' << "  NBRQ"[MovePromoted(move)];
        }
        break;
    }
    /*
    Determine if this move results in check or checkmate
    */
    Position dst_position{MakeMove(move)};
    if (dst_position.IsCheckmate())
    {
        result << '#';
    }
    else if (dst_position.IsInCheck())
    {
        result << "+";
    }
    return result.str();
}
#include <sstream>
#include <string>
#include <memory.h>

using std::stringstream;
using std::string;

#include "position.h"
#include "pawnstar.h"

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
static const uint16_t CASTLING_RIGHTS_MASKS[64] =
{
   A1M,  OK,  OK,  OK, E1M,  OK,  OK, H1M,
    OK,  OK,  OK,  OK,  OK,  OK,  OK,  OK,
    OK,  OK,  OK,  OK,  OK,  OK,  OK,  OK,
    OK,  OK,  OK,  OK,  OK,  OK,  OK,  OK,
    OK,  OK,  OK,  OK,  OK,  OK,  OK,  OK,
    OK,  OK,  OK,  OK,  OK,  OK,  OK,  OK,
    OK,  OK,  OK,  OK,  OK,  OK,  OK,  OK,
   A8M,  OK,  OK,  OK, E8M,  OK,  OK, H8M,
};

/**
 * @brief Make a null move: don't actually move any pieces on the board.
 * # Flip side to move
 * # Clear en passant capture availablity
 * # Set the flag indicating this position is the result of a null move
 * @param position destination position
 * @param src_position source position
*/
void 
MakeNullMove(Position&       position, 
             const Position& src_position)
{
    position = src_position;
    position.previous = &src_position;
    position.flags |= IS_NULL_MOVE;
    position.flags ^= IS_BLACK_TO_MOVE;
    position.hash ^= EN_PASSANT_HASHES[position.en_passant_index];
    position.hash ^= BLACK_MOVE_HASH;
    position.en_passant_index = 0;
}

void
AddPiece(Position& position, 
         int       color, 
         int       piece, 
         int       to)
{
    const Bitboard to_bb = BITBOARD(to);
    position.piece[piece - 1] ^= to_bb;
    position.pieces_of_color[color] ^= to_bb;
    position.hash ^= PIECE_SQUARE_HASHES[color][piece - 1][to];
}

static void
RemovePiece(Position& position, 
            int       color, 
            int       piece, 
            int       from)
{
    const Bitboard from_bb = BITBOARD(from);
    position.piece[piece - 1] ^= from_bb;
    position.pieces_of_color[color] ^= from_bb;
    position.hash ^= PIECE_SQUARE_HASHES[color][piece - 1][from];
}

static void
MovePiece(Position& position, 
          int       color, 
          int       piece, 
          int       from, 
          int       to)
{
    const Bitboard from_to_bb = BITBOARD(from) | BITBOARD(to);
    const uint64_t* const hash = &PIECE_SQUARE_HASHES[color][piece -1][0];
    position.piece[piece - 1] ^= from_to_bb;
    position.pieces_of_color[color] ^= from_to_bb;
    position.hash ^= hash[to] ^ hash[from];
}

#define BAD_FEN_STRING printf("ERROR: bad FEN string %s\n", fen_string); return

Position::Position(const Position& that, Move move) noexcept
{                          
    const int color    = ColorToMove(that);
    const int from     = MoveFrom(move);
    const int to       = MoveTo(move);
    const int piece    = MovePiece(move);
    const int captured = MoveCaptured(move);
     
    *this = that;
    previous = &that;
    flags &= ~IS_NULL_MOVE;
    flags &= CASTLING_RIGHTS_MASKS[from] & CASTLING_RIGHTS_MASKS[to];
    hash ^= CASTLING_RIGHTS_HASHES[flags & CASTLING_RIGHTS_MASK] 
          ^ CASTLING_RIGHTS_HASHES[that.flags & CASTLING_RIGHTS_MASK];
    hash ^= EN_PASSANT_HASHES[en_passant_index];
    en_passant_index = 0;
    ++reversible_move_count;
    switch (piece)
    {
    case PAWN:
        reversible_move_count = 0;
        if (captured)
        {
            if (IsEpCaptureMove(move))
            {
                /* En passant capture: capture location is source rank, destination file. */
                const int captured_pawn_locn = (from & 0x38) | (to & 0x07);
                RemovePiece(*this, EnemyOf(color), PAWN, captured_pawn_locn);
            }
            else
            {
                RemovePiece(*this, EnemyOf(color), captured, to);
            }
        }                
        if (MovePromoted(move))
        {
            /* Replace the pawn with the promoted piece. */
            RemovePiece(*this, color, PAWN, from);
            AddPiece(*this, color, MovePromoted(move), to);
        }
        else
        {
            MovePiece(*this, color, PAWN, from, to);
            if (IsPawnDoublePushMove(move))
            {
                /* Pawn double push: affects en passant. */
                en_passant_index = (uint8_t)((from + to) >> 1);
                hash ^= EN_PASSANT_HASHES[en_passant_index];
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
            RemovePiece(*this, EnemyOf(color), captured, to);
            reversible_move_count = 0;
        }
        MovePiece(*this, color, piece, from, to);
        break;

    case KING:
        if (!IsCastlingMove(move))
        {
            if (captured)
            {
                RemovePiece(*this, EnemyOf(color), captured, to);
                reversible_move_count = 0;
            }
            MovePiece(*this, color, KING, from, to);
            break;
        }
        /* Castling move. */
        switch (to)
        {
        case G1:
            MovePiece(*this, WHITE, KING, E1, G1);
            MovePiece(*this, WHITE, ROOK, H1, F1);
            break;
        case C1:
            MovePiece(*this, WHITE, KING, E1, C1);
            MovePiece(*this, WHITE, ROOK, A1, D1);
            break;
        case G8:
            MovePiece(*this, BLACK, KING, E8, G8);
            MovePiece(*this, BLACK, ROOK, H8, F8);
            break;
        case C8:
            MovePiece(*this, BLACK, KING, E8, C8);
            MovePiece(*this, BLACK, ROOK, A8, D8);
            break;
        default:
            break;
        }
        break;
    };  
    flags ^= IS_BLACK_TO_MOVE;
    hash ^= BLACK_MOVE_HASH;
    full_move_count += color;
    king_location[WHITE] = Lsb(kings & white_pieces);
    king_location[BLACK] = Lsb(kings & black_pieces);
    if (IsAttacked(*this, king_location[color], EnemyOf(color)))
    {
        flags |= IS_MOVED_INTO_CHECK;
    }
    else
    {
        if (IsAttacked(*this, king_location[EnemyOf(color)], color))
        {
            flags |= IS_CHECK;
        }
        else
        {
            flags &= ~IS_CHECK;
        }
    }
}

Position::Position(const char* fen_string) noexcept
{
    const string white_piece_names { "PNBRQK"   };
    const string black_piece_names { "pnbrqk"   };
    const string ep_files { "abcdefgh" };
    const string ep_ranks { "36"       };
    stringstream ss { fen_string };
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wclass-memaccess"
    memset(this, 0, sizeof(Position));
#pragma GCC diagnostic pop
    previous = this;
    /* Pieces on the board */
    string pieces;
    ss >> pieces;
    if (pieces == "")
    {
        BAD_FEN_STRING;
    }
    int x = 0, y = 7;
    for (const auto& c : pieces)
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
            const int piece = a + 1;
            AddPiece(*this, WHITE, piece, x + 8 * y);
            ++x;
            continue;
        }
        a = black_piece_names.find(c);
        if (a != string::npos)
        {
            const int piece = a + 1;
            AddPiece(*this, BLACK, piece, x + 8 * y);
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
        flags |= IS_BLACK_TO_MOVE;
    }
    else if (color_to_move != "w")
    {
        BAD_FEN_STRING;
    }
    king_location[WHITE] = Lsb(kings & white_pieces);
    king_location[BLACK] = Lsb(kings & black_pieces);
    /* Castling rights */
    string castling_rights;
    ss >> castling_rights;
    if (castling_rights == "")
    {
        BAD_FEN_STRING;
    }
    if (castling_rights == "-")
    {
        flags &= ~CASTLING_RIGHTS_MASK;
    }
    else
    {
        for (const char& c : castling_rights)
        {
            switch (c)
            {
            case 'K':
                flags |= MAY_WHITE_CASTLE_KINGSIDE;
                break;
            case 'Q':
                flags |= MAY_WHITE_CASTLE_QUEENSIDE;
                break;
            case 'k':
                flags |= MAY_BLACK_CASTLE_KINGSIDE;
                break;
            case 'q':
                flags |= MAY_BLACK_CASTLE_QUEENSIDE;
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
        en_passant_index = 0;
    }
    else
    {
        if (ep_square.length() != 2 || 
            ep_files.find(ep_square[0]) == string::npos ||
            ep_ranks.find(ep_square[1]) == string::npos)
        {
            BAD_FEN_STRING;
        }
        en_passant_index = (ep_square[0] - 'a') + 8 * (ep_square[1] - '1');
    }
    if (ss)
    {
        /* Half move clock - optional */
        int hmc = 0;
        ss >> hmc;
        reversible_move_count = (uint8_t)hmc;
    }
    if (ss)
    {
        /* Full move number - optional */
        int fmn = 1;
        ss >> fmn;
        full_move_count = (uint8_t)fmn - 1;
    }
    hash = ComputeHash(*this);
    /* Legality of position */
    if (!IsPositionLegal(*this))
    {
        BAD_FEN_STRING;
    }
    /* Is the position in check? */
    const int color = ColorToMove(*this);
    if (IsAttacked(*this, king_location[color], EnemyOf(color)))
    {
        flags |= IS_CHECK;
    }
    /*
    Check to see if this position represents checkmate, stalemate or 
    draw by insufficient material and set flags accordingly.
    */
    if (IsCheckmate(*this))
    {
        printf("NOTE: FEN string specifies a checkmate position\n");
        flags |= IS_CHECKMATE;
    }
    else if (IsStalemate(*this))
    {
        printf("NOTE: FEN string specifies a stalemate position\n");
        flags |= IS_STALEMATE;
    }
    else if (IsDrawByMaterial(*this))
    {
        printf("NOTE: FEN string specifies a draw by insufficient material position\n");
        flags |= IS_DRAW_MATERIAL;
    }
}

Position::operator std::string() const
{
    stringstream ss;
    /* Pieces on the board */
    const Bitboard occupied_squares = white_pieces | black_pieces;
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
                const char piece = (white_pieces & BITBOARD(x, y)) ?
                    " PNBRQK"[PieceAt(*this, x + 8 * y)] : 
                    " pnbrqk"[PieceAt(*this, x + 8 * y)];
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
    ss << ' ' << (flags & IS_BLACK_TO_MOVE ? 'b' : 'w') << ' ';
    /* Castling rights */
    if (!(flags & CASTLING_RIGHTS_MASK))
    {
        ss << '-';
    }
    else
    {
        if (flags & MAY_WHITE_CASTLE_KINGSIDE)
        {
            ss << 'K';
        }
        if (flags & MAY_WHITE_CASTLE_QUEENSIDE)
        {
            ss << 'Q';
        }
        if (flags & MAY_BLACK_CASTLE_KINGSIDE)
        {
            ss << 'k';
        }
        if (flags & MAY_BLACK_CASTLE_QUEENSIDE)
        {
            ss << 'q';
        }
    }
    ss << ' ';
    /* En passant capture availability */
    if (en_passant_index == 0)
    {
        ss << '-';
    }
    else
    {
        ss << FileChar(en_passant_index) << RankChar(en_passant_index);
    }
    ss <<  ' ';
    /* Half move clock and full move number */
    ss << (int)reversible_move_count << ' ' << (int)(full_move_count + 1);
    return ss.str();

}
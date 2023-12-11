#include <sstream>
#include <string>
#include <memory.h>

using std::stringstream;
using std::string;

#include "position.h"
#include "pawnstar.h"

#define BAD_FEN_STRING printf("ERROR: bad FEN string %s\n", fen_string); return

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
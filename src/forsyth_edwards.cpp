#include <ctype.h>
#include <string>
#include <sstream>

#include "pawnstar.h"

using std::string;
using std::stringstream;

#define BAD_FEN_STRING printf("ERROR: bad FEN string %s\n", fen_string); return false

/**
 * @brief Construct a position from a Forsyth-Edwards (FEN) string.
 * @param fen_string string format of position
 * @param position position object to initialize
 * @return true on success, false if an illegal position string was provided
*/
bool 
PositionFromString(const char* fen_string, 
                   Position&   position)
{
    const string white_pieces   { "PNBRQK"   };
    const string black_pieces   { "pnbrqk"   };
    const string ep_files       { "abcdefgh" };
    const string ep_ranks       { "36"       };
    stringstream ss { fen_string };   
    memset(&position, 0, sizeof(Position));
    position.previous = &position;
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
        auto a = white_pieces.find(c);
        if (a != string::npos)
        {
            const int piece = a + 1;
            AddPiece(position, WHITE, piece, x + 8 * y);
            ++x;
            continue;
        }
        a = black_pieces.find(c);
        if (a != string::npos)
        {
            const int piece = a + 1;
            AddPiece(position, BLACK, piece, x + 8 * y);
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
        position.flags |= IS_BLACK_TO_MOVE;
    }
    else if (color_to_move != "w")
    {
        BAD_FEN_STRING;
    }
    position.king_location[WHITE] = Lsb(position.kings & position.white_pieces);
    position.king_location[BLACK] = Lsb(position.kings & position.black_pieces);
    /* Castling rights */
    string castling_rights;
    ss >> castling_rights;
    if (castling_rights == "")
    {
        BAD_FEN_STRING;
    }
    if (castling_rights == "-")
    {
        position.flags &= ~CASTLING_RIGHTS_MASK;
    }
    else
    {
        for (const char& c : castling_rights)
        {
            switch (c)
            {
            case 'K':
                position.flags |= MAY_WHITE_CASTLE_KINGSIDE;
                break;
            case 'Q':
                position.flags |= MAY_WHITE_CASTLE_QUEENSIDE;
                break;
            case 'k':
                position.flags |= MAY_BLACK_CASTLE_KINGSIDE;
                break;
            case 'q':
                position.flags |= MAY_BLACK_CASTLE_QUEENSIDE;
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
        position.en_passant_index = 0;
    }
    else
    {
        if (ep_square.length() != 2 || 
            ep_files.find(ep_square[0]) == string::npos ||
            ep_ranks.find(ep_square[1]) == string::npos)
        {
            BAD_FEN_STRING;
        }
        position.en_passant_index = (ep_square[0] - 'a') + 8 * (ep_square[1] - '1');
    }
    if (ss)
    {
        /* Half move clock - optional */
        int hmc = 0;
        ss >> hmc;
        position.reversible_move_count = (uint8_t)hmc;
    }
    if (ss)
    {
        /* Full move number - optional */
        int fmn = 1;
        ss >> fmn;
        position.full_move_count = (uint8_t)fmn - 1;
    }
    position.hash = ComputeHash(position);
    /* Legality of position */
    if (!IsPositionLegal(position))
    {
        BAD_FEN_STRING;
    }
    /* Is the position in check? */
    const int color = ColorToMove(position);
    if (IsAttacked(position, position.king_location[color], EnemyOf(color)))
    {
        position.flags |= IS_CHECK;
    }
    /*
    Check to see if this position represents checkmate, stalemate or 
    draw by insufficient material and set flags accordingly.
    */
    if (IsCheckmate(position))
    {
        printf("NOTE: FEN string specifies a checkmate position\n");
        position.flags |= IS_CHECKMATE;
    }
    else if (IsStalemate(position))
    {
        printf("NOTE: FEN string specifies a stalemate position\n");
        position.flags |= IS_STALEMATE;
    }
    else if (IsDrawByMaterial(position))
    {
        printf("NOTE: FEN string specifies a draw by insufficient material position\n");
        position.flags |= IS_DRAW_MATERIAL;
    }
    return true;
}


/**
 * @brief Construct a Forsyth-Edwards (FEN) string from the specified position.
 * @param position the position
*/
string PositionToString(const Position& position)
{
    stringstream ss;
    /* Pieces on the board */
    const Bitboard occupied_squares = position.white_pieces | position.black_pieces;
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
                const char piece = (position.white_pieces & BITBOARD(x, y)) ?
                    " PNBRQK"[PieceAt(position, x + 8 * y)] : 
                    " pnbrqk"[PieceAt(position, x + 8 * y)];
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
    ss << ' ' << (position.flags & IS_BLACK_TO_MOVE ? 'b' : 'w') << ' ';
    /* Castling rights */
    if (!(position.flags & CASTLING_RIGHTS_MASK))
    {
        ss << '-';
    }
    else
    {
        if (position.flags & MAY_WHITE_CASTLE_KINGSIDE)
        {
            ss << 'K';
        }
        if (position.flags & MAY_WHITE_CASTLE_QUEENSIDE)
        {
            ss << 'Q';
        }
        if (position.flags & MAY_BLACK_CASTLE_KINGSIDE)
        {
            ss << 'k';
        }
        if (position.flags & MAY_BLACK_CASTLE_QUEENSIDE)
        {
            ss << 'q';
        }
    }
    ss << ' ';
    /* En passant capture availability */
    if (position.en_passant_index == 0)
    {
        ss << '-';
    }
    else
    {
        ss << FileChar(position.en_passant_index) << RankChar(position.en_passant_index);
    }
    ss <<  ' ';
    /* Half move clock and full move number */
    ss << (int)position.reversible_move_count << ' ' << (int)(position.full_move_count + 1);
    return ss.str();
}

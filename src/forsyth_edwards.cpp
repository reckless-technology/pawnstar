#include "pawnstar.h"
#include <ctype.h>

#define BAD_FEN_STRING printf("ERROR: bad FEN string %s\n", fen_string); return false

/**
 * @brief Construct a position from a Forsyth-Edwards (FEN) string.
 * @param fen_string string format of position
 * @param position position object to initialize
 * @return true on success, false if an illegal position string was provided
*/
bool 
PositionFromString(const char* fen_string, 
                   Position*   position)
{
    static const char* const white_pieces = "PNBRQK";
    static const char* const black_pieces = "pnbrqk";
    static const char* const ep_files     = "abcdefgh";
    static const char* const ep_ranks     = "36";
    char buffer[256];
    const size_t len = strlen(fen_string);
    if (len == 0 || len >= sizeof(buffer))
    {
        BAD_FEN_STRING;
    }    
    memset(position, 0, sizeof(Position));
    position->previous = position;
    strcpy(buffer, fen_string);
    if (buffer[len - 1] == '\n')
    {
        buffer[len - 1] = '\0';
    }
    char* save_ptr      = NULL;
    const char* pieces  = strtok_r(buffer, " ", &save_ptr);
    if (!pieces)
    {
        BAD_FEN_STRING;
    }
    /* Pieces on the board */
    int x = 0, y = 7;
    for ( ; *pieces; ++pieces)
    {
        const char c = *pieces;
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
        const char* index = strchr(white_pieces, c);
        if (index)
        {
            const int piece = (int)(index - white_pieces) + 1;
            AddPiece(position, WHITE, piece, x + 8 * y);
            ++x;
            continue;
        }
        index = strchr(black_pieces, c);
        if (index)
        {
            const int piece = (int)(index - black_pieces) + 1;
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
    const char* color_to_move = strtok_r(NULL, " ", &save_ptr);
    if (!color_to_move)
    {
        BAD_FEN_STRING;
    }
    if (!strcmp(color_to_move, "b"))
    {
        position->state_flags |= IS_BLACK_TO_MOVE;
    }
    else if (!!strcmp(color_to_move, "w"))
    {
        BAD_FEN_STRING;
    }
    position->king_location[WHITE] = LSB(position->kings & position->white_pieces);
    position->king_location[BLACK] = LSB(position->kings & position->black_pieces);
    /* Castling rights */
    const char* castling_rights = strtok_r(NULL, " ", &save_ptr);
    if (!castling_rights)
    {
        BAD_FEN_STRING;
    }
    if (!strcmp("-", castling_rights))
    {
        position->state_flags &= ~CASTLING_RIGHTS_MASK;
    }
    else
    {
        for (const char* c = castling_rights; *c; ++c)
        {
            switch (*c)
            {
            case 'K':
                position->state_flags |= MAY_WHITE_CASTLE_KINGSIDE;
                break;
            case 'Q':
                position->state_flags |= MAY_WHITE_CASTLE_QUEENSIDE;
                break;
            case 'k':
                position->state_flags |= MAY_BLACK_CASTLE_KINGSIDE;
                break;
            case 'q':
                position->state_flags |= MAY_BLACK_CASTLE_QUEENSIDE;
                break;
            default:
                BAD_FEN_STRING;
            }
        }
    }
    /* En passant capture square */
    const char* ep_square = strtok_r(NULL, " ", &save_ptr);
    if (!ep_square)
    {
        BAD_FEN_STRING;
    }
    if (!strcmp(ep_square, "-"))
    {
        position->en_passant_index = 0;
    }
    else
    {
        if (strlen(ep_square) != 2 || !strchr(ep_files, ep_square[0]) || !strchr(ep_ranks, ep_square[1]))
        {
            BAD_FEN_STRING;
        }
        position->en_passant_index = (ep_square[0] - 'a') + 8 * (ep_square[1] - '1');
    }
    /* Half move clock - optional */
    const char* hmc = strtok_r(NULL, " ", &save_ptr);
    if (hmc)
    {
        int half_move_clock = 0;
        if (sscanf(hmc, "%d", &half_move_clock) != 1)
        {
            BAD_FEN_STRING;
        }
        position->reversible_move_count = (uint8_t)half_move_clock;
    }
    /* Full move number - optional */
    const char* full_move_num = strtok_r(NULL, " ", &save_ptr);
    if (full_move_num)
    {
        int fmc = 0;
        if (sscanf(full_move_num, "%d", &fmc) != 1)
        {
            BAD_FEN_STRING;
        }
        position->full_move_count = (uint8_t)fmc - 1;
    }
    position->hash = ComputeHash(position);
    /* Legality of position */
    if (!IsPositionLegal(position))
    {
        BAD_FEN_STRING;
    }
    /* Is the position in check? */
    const int color = COLOR_TO_MOVE(position);
    if (IsAttacked(position, position->king_location[color], ENEMY(color)))
    {
        position->state_flags |= IS_CHECK;
    }
    /*
    Check to see if this position represents checkmate, stalemate or 
    draw by insufficient material and set flags accordingly.
    */
    if (IsCheckmate(position))
    {
        printf("NOTE: FEN string specifies a checkmate position\n");
        position->state_flags |= IS_CHECKMATE;
    }
    else if (IsStalemate(position))
    {
        printf("NOTE: FEN string specifies a stalemate position\n");
        position->state_flags |= IS_STALEMATE;
    }
    else if (IsDrawByMaterial(position))
    {
        printf("NOTE: FEN string specifies a draw by insufficient material position\n");
        position->state_flags |= IS_DRAW_MATERIAL;
    }
    return true;
}


/**
 * @brief Construct a Forsyth-Edwards (FEN) string from the specified position.
 * @param position the position
 * @param fen_string pointer to the string to be created
*/
void PositionToString(const Position* position, char* fen_string)
{
    /* Pieces on the board */
    const Bitboard occupied_squares = position->white_pieces | position->black_pieces;
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
                    *fen_string++ = (char)('0' + num_empty_squares);
                    num_empty_squares = 0;
                }
                const char piece = (position->white_pieces & BITBOARD(x, y)) ?
                                   " PNBRQK"[PieceAt(position, x + 8 * y)] : " pnbrqk"[PieceAt(position, x + 8 * y)];
                *fen_string++ = piece;
            }
        }
        if (num_empty_squares)
        {
            *fen_string++ = (char)('0' + num_empty_squares);
            num_empty_squares = 0;
        }
        if (y)
        {
            *fen_string++ = '/';
        }
    }
    /* Side to move */
    *fen_string++ = ' ';
    *fen_string++ = position->state_flags & IS_BLACK_TO_MOVE ? 'b' : 'w';
    *fen_string++ = ' ';;
    /* Castling rights */
    if (!(position->state_flags & CASTLING_RIGHTS_MASK))
    {
        *fen_string++ = '-';
    }
    else
    {
        if (position->state_flags & MAY_WHITE_CASTLE_KINGSIDE)
        {
            *fen_string++ = 'K';
        }
        if (position->state_flags & MAY_WHITE_CASTLE_QUEENSIDE)
        {
            *fen_string++ = 'Q';
        }
        if (position->state_flags & MAY_BLACK_CASTLE_KINGSIDE)
        {
            *fen_string++ = 'k';
        }
        if (position->state_flags & MAY_BLACK_CASTLE_QUEENSIDE)
        {
            *fen_string++ = 'q';
        }
    }
    *fen_string++ = ' ';
    /* En passant capture availability */
    if (!position->en_passant_index)
    {
        *fen_string++ = '-';
    }
    else
    {
        *fen_string++ = FILE_CHAR(position->en_passant_index);
        *fen_string++ = RANK_CHAR(position->en_passant_index);
    }
    *fen_string++ = ' ';
    /* Half move clock and full move number */
    fen_string += sprintf(fen_string, "%hu %hu", position->reversible_move_count, position->full_move_count + 1);
}

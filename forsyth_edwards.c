#include "pawnstar.h"
#include <ctype.h>

#define IS_IN_CHECK(position, color) (IsAttacked(position, position->king_location[color], ENEMY(color)))
/*
Compute the Zobrist hash of a position
*/
uint64_t 
ComputeHash(const Position* position)
{
    uint64_t hash = position->state_flags & IS_BLACK_TO_MOVE ? BLACK_MOVE_HASH : 0ull;
    hash += CASTLING_RIGHTS_HASHES[position->castle_flags];
    if (position->en_passant_index)
    {
        hash += EN_PASSANT_HASHES[FILE_OF(position->en_passant_index)];
    }
    for (int piece = PAWN; piece <= KING; ++piece)
    {
        bitboard b = position->pieces[piece] & position->white_pieces;
        while (b)
        {
            hash += PIECE_SQUARE_HASHES[WHITE][piece - 1][FindAndClearLsb(&b)];
        }
        b = position->pieces[piece] & position->black_pieces;
        while (b)
        {
            hash += PIECE_SQUARE_HASHES[BLACK][piece - 1][FindAndClearLsb(&b)];
        }
    }
    return hash;
}
/*
Set the state of the board to that specified in a FEN string
refer to http://en.wikipedia.org/wiki/Forsyth%E2%80%93Edwards_Notation
returns true on success
returns false if an illegal FEN string was provided
*/
bool 
PositionFromString(const char* fen_string, 
                   Position*   position)
{
    static const char* const white_pieces = "PNBRQK";
    static const char* const black_pieces = "pnbrqk";
    static const char* const ep_files     = "abcdefgh";
    static const char* const ep_ranks     = "36";
    const size_t len = strlen(fen_string);
    if (len == 0 || len >= STRING_BUF_LEN)
    {
        goto Error;
    }
    char buffer[STRING_BUF_LEN];
    strcpy(buffer, fen_string);
    if (buffer[len - 1] == '\n')
    {
        buffer[len - 1] = '\0';
    }
    char* save_ptr      = NULL;
    const char* pieces  = strtok_r(buffer, " ", &save_ptr);
    if (!pieces)
    {
        goto Error;
    }
    memset(position, 0, sizeof(Position));
    position->previous = position;
    /*
    Pieces on the board
    */
    int x = 0, y = 7;
    for ( ; *pieces; ++pieces)
    {
        const char c = *pieces;
        if (c == '/')
        {
            if (x != 8)
            {
                goto Error;
            }
            x = 0;
            if (--y == -1)
            {
                goto Error;
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
            goto Error;
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
        goto Error;
    }
    if (x != 8 || y != 0)
    {
        goto Error;
    }
    position->occupied_squares     = position->white_pieces | position->black_pieces;
    position->king_location[WHITE] = (uint8_t)Lsb(position->kings & position->white_pieces);
    position->king_location[BLACK] = (uint8_t)Lsb(position->kings & position->black_pieces);
    /*
    Side to move
    */
    const char* color_to_move = strtok_r(NULL, " ", &save_ptr);
    if (!color_to_move)
    {
        goto Error;
    }
    if (!strcmp(color_to_move, "b"))
    {
        position->state_flags |= IS_BLACK_TO_MOVE;
    }
    else if (!!strcmp(color_to_move, "w"))
    {
        goto Error;
    }
    /*
    Castling rights
    */
    const char* castling_rights = strtok_r(NULL, " ", &save_ptr);
    if (!castling_rights)
    {
        goto Error;
    }
    if (!strcmp("-", castling_rights))
    {
        position->castle_flags = 0;
    }
    else
    {
        for (const char* c = castling_rights; *c; ++c)
        {
            switch (*c)
            {
            case 'K':
                position->castle_flags |= MAY_WHITE_K;
                break;
            case 'Q':
                position->castle_flags |= MAY_WHITE_Q;
                break;
            case 'k':
                position->castle_flags |= MAY_BLACK_K;
                break;
            case 'q':
                position->castle_flags |= MAY_BLACK_Q;
                break;
            default:
                goto Error;
            }
        }
    }
    /*
    En passant capture square
    */
    const char* ep_square = strtok_r(NULL, " ", &save_ptr);
    if (!ep_square)
    {
        goto Error;
    }
    if (!strcmp(ep_square, "-"))
    {
        position->en_passant_index = 0;
    }
    else
    {
        if (strlen(ep_square) != 2 || !strchr(ep_files, ep_square[0]) || !strchr(ep_ranks, ep_square[1]))
        {
            goto Error;
        }
        position->en_passant_index = ep_square[0] - 'a' + 8 * (ep_square[1] - '1');
    }
    /*
    Half move clock - optional
    */
    const char* hmc = strtok_r(NULL, " ", &save_ptr);
    if (hmc)
    {
        int half_move_clock = 0;
        if (sscanf(hmc, "%d", &half_move_clock) != 1)
        {
            goto Error;
        }
        position->reversible_move_count = (uint8_t)half_move_clock;
    }
    /*
    Full move number - optional
    */
    const char* full_move_num = strtok_r(NULL, " ", &save_ptr);
    if (full_move_num)
    {
        int fmc = 0;
        if (sscanf(full_move_num, "%d", &fmc) != 1)
        {
            goto Error;
        }
        position->full_move_count = (uint8_t)fmc - 1;
    }
    position->hash = ComputeHash(position);
    /*
    Legality of position
    */
    if (!IsPositionLegal(position))
    {
        goto Error;
    }
    /*
    Is the position in check?
    */
    if (IS_IN_CHECK(position, COLOR_TO_MOVE(position)))
    {
        position->state_flags |= IS_CHECK;
    }
    /*
    Check to see if this position represents checkmate, stalemate or draw
    by insufficient material and set flags accordingly
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

Error:
    printf("ERROR: bad FEN string %s\n", fen_string);
    return false;
}
/*
Generate the FEN string for the current position
*/
void PositionToString(const Position* position, char fen_string[])
{
    const char* const initial_ptr = fen_string;
    int x, y;
    /*
    Pieces on the board
    */
    for (y = 7; y >= 0; --y)
    {
        for (x = 0; x < 8; ++x)
        {
            if (!(position->occupied_squares & BITBOARD_XY(x, y)))
            {
                if (initial_ptr == fen_string || !(fen_string[-1] >= '1' && fen_string[-1] <= '8'))
                {
                    *fen_string++ = '1';
                }
                else
                {
                    ++fen_string[-1];
                }
            }
            else
            {
                const char piece = (position->white_pieces & BITBOARD_XY(x, y)) ?
                                   " PNBRQK"[PieceAt(position, x + 8 * y)] : " pnbrqk"[PieceAt(position, x + 8 * y)];
                *fen_string++ = piece;
            }
        }
        if (y)
        {
            *fen_string++ = '/';
        }
    }
    /*
    Side to move
    */
    fen_string += sprintf(fen_string, (position->state_flags & IS_BLACK_TO_MOVE) ? " b " : " w ");
    /*
    Castling rights
    */
    if (!position->castle_flags)
    {
        *fen_string++ = '-';
    }
    else
    {
        if (position->castle_flags & MAY_WHITE_K)
        {
            *fen_string++ = 'K';
        }
        if (position->castle_flags & MAY_WHITE_Q)
        {
            *fen_string++ = 'Q';
        }
        if (position->castle_flags & MAY_BLACK_K)
        {
            *fen_string++ = 'k';
        }
        if (position->castle_flags & MAY_BLACK_Q)
        {
            *fen_string++ = 'q';
        }
    }
    *fen_string++ = ' ';
    /*
    En passant capture availability
    */
    if (!position->en_passant_index)
    {
        fen_string += sprintf(fen_string, "- ");
    }
    else
    {
        fen_string += sprintf(fen_string,
                             "%c%c ",
                             FILE_CHAR(position->en_passant_index),
                             RANK_CHAR(position->en_passant_index));
    }
    /*
    Half move clock
    */
    fen_string += sprintf(fen_string, "%u ", position->reversible_move_count);
    /*
    Full move number
    */
    fen_string += sprintf(fen_string, "%u", position->full_move_count + 1);
}
/*
Set up the game for the start of a new game
*/
void InitializeGame(Game* game)
{
    game->position = game->stack;
    PositionFromString("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", game->position);
}

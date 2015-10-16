#include "pawnstar.h"
#include <ctype.h>

#define IS_IN_CHECK(position, color) (IsAttacked(position, position->king_location[color], ENEMY(color)))
/******************************************************************************
Compute the Zobrist hash of a position
*******************************************************************************/
uint64 ComputeHash(const Position* position)
{
    int piece;
    bitboard b;
    uint64 hash = position->state_flags & IS_BLACK_TO_MOVE ? BLACK_MOVE_HASH : 0ull;
    hash += CASTLING_RIGHTS_HASHES[position->castle_flags];
    if (position->en_passant_index)
    {
        hash += EN_PASSANT_HASHES[FILE_OF(position->en_passant_index)];
    }
    for (piece = PAWN; piece <= KING; ++piece)
    {
        b = position->pieces[piece] & position->white_pieces;
        while (b)
        {
            hash += PIECE_SQUARE_HASHES[WHITE][piece][FindAndClearLsb(&b)];
        }
        b = position->pieces[piece] & position->black_pieces;
        while (b)
        {
            hash += PIECE_SQUARE_HASHES[BLACK][piece][FindAndClearLsb(&b)];
        }
    }
    return hash;
}
/******************************************************************************
Set the state of the board to that specified in a FEN string
refer to http://en.wikipedia.org/wiki/Forsyth%E2%80%93Edwards_Notation
returns true on success
returns false if an illegal FEN string was provided
*******************************************************************************/
bool PositionFromString(const char fen_string[], Position* position)
{
    char buffer[STRING_BUF_LEN];
    size_t i;
    int x = 0, y = 7;
    const char* const white_pieces = "PNBRQK";
    const char* const black_pieces = "pnbrqk";
    const char* const ep_files     = "abcdefgh";
    const char* const ep_ranks     = "36";
    char* token;
    const char* index;
    if (strlen(fen_string) >= STRING_BUF_LEN)
    {
        printf("ERROR: FEN string too long\n");
        return false;
    }
    strcpy(buffer, fen_string);
    i = strlen(buffer);
    if (i && buffer[i - 1] == '\n')
    {
        buffer[i - 1] = '\0';
    }
    token = strtok(buffer, " ");
    if (!token)
    {
        printf("ERROR: FEN string does not contain board definition\n");
        return false;
    }
    memset(position, 0, sizeof(Position));
    position->previous = position;
    /**************************************************************************
    Pieces on the board
    ***************************************************************************/
    for ( ; *token; ++token)
    {
        if (*token == '/')
        {
            if (x != 8)
            {
                printf("ERROR: FEN string contains too few files\n");
                return false;
            }
            x = 0;
            if (--y == -1)
            {
                printf("ERROR: FEN string contains too many ranks\n");
                return false;
            }
            continue;
        }
        if (*token >= '1' && *token <= '8')
        {
            x += *token - '0';
            continue;
        }
        if (x >= 8)
        {
            printf("ERROR: FEN string contains too many files\n");
            return false;
        }
        index = strchr(white_pieces, *token);
        if (index)
        {
            const int piece = (int)(index - white_pieces) + 1;
            position->pieces[piece] |= BITBOARD_XY(x, y);
            position->white_pieces  |= BITBOARD_XY(x, y);
            ++x;
            continue;
        }
        index = strchr(black_pieces, *token);
        if (index)
        {
            const int piece = (int)(index - black_pieces) + 1;
            position->pieces[piece] |= BITBOARD_XY(x, y);
            position->black_pieces  |= BITBOARD_XY(x, y);
            ++x;
        }
        else
        {
            printf("ERROR: unrecognized character '%c' in FEN string\n", *token);
            return false;
        }
    }
    if (y)
    {
        printf("ERROR: FEN string contains too few ranks\n");
        return false;
    }
    position->occupied_squares = position->white_pieces | position->black_pieces;
    position->king_location[WHITE] = (uint8)Lsb(position->kings & position->white_pieces);
    position->king_location[BLACK] = (uint8)Lsb(position->kings & position->black_pieces);
    /**************************************************************************
    Side to move
    ***************************************************************************/
    token = strtok(NULL, " ");
    if (!token)
    {
        printf("ERROR: FEN string does not specify side to move\n");
        return false;
    }
    if (!strcmp(token, "b"))
    {
        position->state_flags |= IS_BLACK_TO_MOVE;
    }
    else if (!!strcmp(token, "w"))
    {
        printf("ERROR: FEN string contains illegal side to move '%s'\n", token);
        return false;
    }
    /**************************************************************************
    Castling rights
    ***************************************************************************/
    token = strtok(NULL, " ");
    if (!token)
    {
        printf("ERROR: FEN string does not specify castling rights\n");
        return false;
    }
    if (!strcmp("-", token))
    {
        position->castle_flags = 0;
    }
    else
    {
        for (const char* c = token; *c; ++c)
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
                printf("ERROR: FEN string specifies illegal castling right '%c'\n", *c);
                return false;
            }
        }
    }
    /**************************************************************************
    En passant capture square
    ***************************************************************************/
    token = strtok(NULL, " ");
    if (!token)
    {
        printf("ERROR: FEN string does not specify en passant capture availability\n");
        return false;
    }
    if (!strcmp(token, "-"))
    {
        position->en_passant_index = 0;
    }
    else
    {
        if (strlen(token) != 2 || !strchr(ep_files, token[0]) || !strchr(ep_ranks, token[1]))
        {
            printf("ERROR: FEN string specifies illegal en passant square '%s'\n", token);
            return false;
        }
        position->en_passant_index = token[0] - 'a' + 8 * (token[1] - '1');
    }
    /**************************************************************************
    Half move clock - optional
    ***************************************************************************/
    token = strtok(NULL, " ");
    if (token)
    {
        int hmc = 0;
        if (sscanf(token, "%d", &hmc) != 1)
        {
            printf("ERROR: FEN string specifies illegal half move clock '%s'\n", token);
            return false;
        }
        position->reversible_move_count = (uint8)hmc;
    }
    /**************************************************************************
    Full move number - optional
    ***************************************************************************/
    token = strtok(NULL, " ");
    if (token)
    {
        int fmc = 0;
        if (sscanf(token, "%d", &fmc) != 1)
        {
            printf("ERROR: FEN string specifies illegal full move number '%s'\n", token);
            return false;
        }
        position->full_move_count = (uint8)fmc - 1;
    }
    position->hash = ComputeHash(position);
    /**************************************************************************
    Legality of position
    ***************************************************************************/
    if (!IsPositionLegal(position))
    {
        printf("ERROR: FEN string specifies an illegal chess position\n");
        return false;
    }
    /**************************************************************************
    Is the position in check?
    ***************************************************************************/
    if (IS_IN_CHECK(position, COLOR_TO_MOVE(position)))
    {
        position->state_flags |= IS_CHECK;
    }
    /**************************************************************************
    Check to see if this position represents checkmate, stalemate or draw
    by insufficient material and set flags accordingly
    ***************************************************************************/
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
/******************************************************************************
Generate the FEN string for the current position
*******************************************************************************/
void PositionToString(const Position* position, char fen_string[])
{
    const char* const initial_ptr = fen_string;
    int x, y;
    /**************************************************************************
    Pieces on the board
    ***************************************************************************/
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
    /**************************************************************************
    Side to move
    ***************************************************************************/
    fen_string += sprintf(fen_string, (position->state_flags & IS_BLACK_TO_MOVE) ? " b " : " w ");
    /**************************************************************************
    Castling rights
    ***************************************************************************/
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
    /**************************************************************************
    En passant capture availability
    ***************************************************************************/
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
    /**************************************************************************
    Half move clock
    ***************************************************************************/
    fen_string += sprintf(fen_string, "%u ", position->reversible_move_count);
    /**************************************************************************
    Full move number
    ***************************************************************************/
    fen_string += sprintf(fen_string, "%u", position->full_move_count + 1);
}
/******************************************************************************
Set up the position for the start of a game
*******************************************************************************/
void NewGame(Position* position)
{
    PositionFromString("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", position);
}
/******************************************************************************
Set up the game for the start of a new game
*******************************************************************************/
void InitializeGame(Game* game)
{
    game->position = game->stack;
    NewGame(game->position);
}

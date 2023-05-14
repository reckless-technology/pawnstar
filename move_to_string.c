#include "pawnstar.h"

/*
Convert a move into standard algebraic notation
Refer to:
http://en.wikipedia.org/wiki/Portable_Game_Notation#Movetext
*/
char* MoveToString(const Position* position, int the_move, char move_string[])
{
    int legal_moves[MAX_MOVES_PER_POSITION];
    Position dst_position[1];
    char disambiguation[3]   = { 0 };
    char from_square[3]      = { FILE_CHAR(MOVE_FROM(the_move)), RANK_CHAR(MOVE_FROM(the_move)), 0 };
    char to_square[3]        = { FILE_CHAR(MOVE_TO  (the_move)), RANK_CHAR(MOVE_TO  (the_move)), 0 };
    bool is_source_ambiguous = false;
    bool is_file_unique      = true;
    bool is_rank_unique      = true;   
    const char move_piece    = " PNBRQK"[MOVE_PIECE(the_move)];
    /*
    Determine if there is more than one piece of the same type which is capable 
    of moving to the target square, and will require further disambiguation
    */
    GenerateLegalMoves(position, legal_moves);
    for (const int* move = legal_moves; *move; ++move)
    {
        if (MOVE_PIECE(*move) == MOVE_PIECE(the_move) && 
            MOVE_TO   (*move) == MOVE_TO   (the_move) &&
            MOVE_FROM (*move) != MOVE_FROM (the_move))
        {
            is_source_ambiguous = true;
            if (FILE_OF(MOVE_FROM(*move)) == FILE_OF(MOVE_FROM(the_move)))
            {
                is_file_unique = false;
            }
            if (RANK_OF(MOVE_FROM(*move)) == RANK_OF(MOVE_FROM(the_move)))
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
            disambiguation[0] = from_square[0]; 
        }
        else if (is_rank_unique)
        {
            disambiguation[0] = from_square[1];
        }
        else
        {
            disambiguation[0] = from_square[0];
            disambiguation[1] = from_square[1];
        }
    }
    switch (MOVE_PIECE(the_move))
    {
    RegularMove:
    default:
    case KNIGHT:
    case BISHOP:
    case ROOK:
    case QUEEN:
        if (MOVE_CAPTURED(the_move))
        {
            move_string += sprintf(move_string, "%c%sx%s", move_piece, disambiguation, to_square);
        }
        else
        {
            move_string += sprintf(move_string, "%c%s%s", move_piece, disambiguation, to_square);
        }
        break;
    
    case KING:
        if (MOVE_IS_SPECIAL(the_move))
        {
            switch (MOVE_TO(the_move))
            {
            case G1:
            case G8:
                move_string += sprintf(move_string, "O-O");
                break;
            case C1:
            case C8:
                move_string += sprintf(move_string, "O-O-O");
                break;
            }
        }
        else
        {
            goto RegularMove;
        }
        break;

    case PAWN:
        if (MOVE_CAPTURED(the_move))
        {
            move_string += sprintf(move_string, "%cx%s", from_square[0], to_square);
            if (MOVE_IS_SPECIAL(the_move))
            {
                /* ep capture */
                move_string += sprintf(move_string, "e.p.");
            }
        }
        else
        {
            move_string += sprintf(move_string, "%s", to_square); 
        }
        if (MOVE_PROMOTED(the_move))
        {
            move_string += sprintf(move_string, "=%c", "  NBRQ"[MOVE_PROMOTED(the_move)]);
        }
        break;
    }    
    /*
    Determine if this move results in check or checkmate
    */
    MakeMove(dst_position, position, the_move);
    if (IsCheckmate(dst_position))
    {
        move_string += sprintf(move_string, "#");
    }
    else if (dst_position->state_flags & IS_CHECK)
    {
        move_string += sprintf(move_string, "+");
    }
    return move_string;
}
/*
Convert a sequence of moves into SAN representation with each move separated by
a space
*/
void MoveSequenceToSanString(const Position* position, const int moves[], char move_string[])
{
    bool is_first_move = true;
    Position src_position[1];
    Position dst_position[1];
    const int* move;
    *src_position = *position;
    for (move = moves; *move; ++move)
    {
        if (!is_first_move)
        {
            *move_string++ = ' ';
        }
        move_string = MoveToString(src_position, *move, move_string);
        MakeMove(dst_position, src_position, *move);
        *src_position = *dst_position;
        is_first_move = false;
    }
}
static const char* const strings_to_remove[] = {
    "+",
    "#",
    "ep",
    "e.p.",
    "!",
    "?",
    NULL,
};
/*
Compare two move strings for equality. Sometimes GUIs don't bother to put the 
"e.p.", "+" or "#" at the end of the move string; we still want these to 
compare as equal.
*/
bool AreMoveStringsEquivalent(char* str1, char* str2)
{
    if (!str1 || !str2 || !strlen(str1) || !strlen(str2))
    {
        return false;
    }
    for (const char* const * x = strings_to_remove; *x; ++x)
    {
        char* y;
        if ((y = strstr(str1, *x)) != NULL)
        {
            *y = '\0';
        }
        if ((y = strstr(str2, *x)) != NULL)
        {
            *y = '\0';
        }
    }
    return !strcmp(str1, str2);
}

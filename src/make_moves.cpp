#include "pawnstar.h"

/*
Make a move and update the game end status flags
*/
void PlayMove(Game& game, Move move)
{
    if (game.position->flags_ & IS_GAME_OVER)
    {
        printf("ERROR: attempt to play move after game is over\n");
        return;
    }
    game.position[1] = Position(game.position[0], move);
    ++game.position;
    if (game.position->IsCheckmate())
    {
        game.position->flags_ |= IS_CHECKMATE;
    }
    else if (game.position->IsStalemate())
    {
        game.position->flags_ |= IS_STALEMATE;
    }
    else if (game.position->IsDrawByRepetition(false))
    {
        game.position->flags_ |= IS_DRAW_REPETITION;
    }
    else if (game.position->IsDrawByMaterial())
    {
        game.position->flags_ |= IS_DRAW_MATERIAL;
    }
    else if (game.position->IsDrawByFiftyMoves())
    {
        game.position->flags_ |= IS_DRAW_50_MOVES;
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

/**
 * @brief Compare two move strings for equality, disregarding extraneous characters.
 * @param str1 first move string
 * @param str2 second move string
 * @return true if moves are equivalent
 */
static bool AreMoveStringsEquivalent(char* str1, char* str2)
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

/*
Play a move from the given move string in either standard algebraic or XBoard 
format, and update game state_flags accordingly

returns the move if a legal move was played
returns zero if the move was illegal or the game is over
*/
Move PlayMoveString(Game& game, char* move_str)
{
    if (game.position->flags_ & IS_GAME_OVER)
    {
        return 0;
    }  
    Move moves[MAX_MOVES_PER_POSITION];
    GenerateLegalMoves(*game.position, moves);
    for (const Move* move = moves; *move; ++move)
    {
        char buffer[16];
        MoveToString(*game.position, *move,  buffer);
        if (AreMoveStringsEquivalent(buffer, move_str))
        {
            PlayMove(game, *move);
            return *move;
        }
    }
    return 0;
}
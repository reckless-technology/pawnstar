#include "pawnstar.h"
#include <Windows.h>
/******************************************************************************
Thread entry function for computer thinking
*******************************************************************************/
static DWORD WINAPI SearchThreadEntry(Game* game)
{

    int move = SearchRootNode(game->position);
    if (move)
    {
        char move_string[16];
        MoveToSanString(game->position, move, move_string);
        PlayMove(game, move);
        printf("move %s\n", move_string);
        DisplayResultIfGameOver(game->position);
    }  
    return 0;
}
/******************************************************************************
Start thinking on a background worker thread
*******************************************************************************/
void StartThinking(Game* game)
{
    QueueUserWorkItem(SearchThreadEntry, game, WT_EXECUTEDEFAULT);
}
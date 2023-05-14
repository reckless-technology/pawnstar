#include <thread>

#include "pawnstar.h"

static std::thread worker_thread;
/*
Thread entry function for computer thinking
*/
static void SearchThreadEntry(Game* game)
{

    int move = SearchRootNode(game->position);
    if (move)
    {
        char move_string[16];
        MoveToString(game->position, move, move_string);
        PlayMove(game, move);
        printf("move %s\n", move_string);
        DisplayResultIfGameOver(game->position);
    }  
}

extern "C" void StopWorker(void)
{
	if (worker_thread.joinable())
	{
		worker_thread.join();
	}
}
/*
Start thinking on a background worker thread
*/
extern "C" void StartThinking(Game* game)
{
	StopWorker();
    worker_thread = std::thread(SearchThreadEntry, game);
}


#include <thread>

#include "pawnstar.h"

static std::thread worker_thread;

/**
 * @brief Entry point for the search thread.
 * @param game Game to search on.
 */
static void SearchThreadEntry(Game& game)
{
    int move = SearchRootNode(*game.position);
    if (move)
    {
        char move_string[16];
        MoveToString(*game.position, move, move_string);
        PlayMove(game, move);
        printf("move %s\n", move_string);
        DisplayResultIfGameOver(*game.position);
    }  
}

/**
 * @brief Cancel the worker thread.
 */
void StopWorker(void)
{
    StopThinking();
    if (worker_thread.joinable())
    {
        worker_thread.join();
    }
}

/**
 * @brief Start thinking on a worker thread.
 * @param game Game to search on.
 */
void StartThinking(Game& game)
{
    StopWorker();
    worker_thread = std::thread(SearchThreadEntry, std::ref(game));
}
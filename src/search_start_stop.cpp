#include <thread>

#include "pawnstar.h"

static std::thread worker_thread;

/*
If the game is over, display the xboard style result string
*/
void DisplayResultIfGameOver(const Position& position)
{
    switch (position.flags_ & IS_GAME_OVER)
    {
    default:
        break;
    case IS_CHECKMATE:
        printf((position.flags_ & IS_BLACK_TO_MOVE) ? "1-0 {white mates}\n" : "0-1 {black mates}\n");
        break;
    case IS_STALEMATE:
        printf("1/2-1/2 {stalemate}\n");
        break;
    case IS_DRAW_REPETITION:
        printf("1/2-1/2 {draw by repetition}\n");
        break;
    case IS_DRAW_MATERIAL:
        printf("1/2-1/2 {draw by insufficient material}\n");
        break;
    case IS_DRAW_50_MOVES:
        printf("1/2-1/2 {draw by 50 reversible moves}\n");
        break;
    }
}

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
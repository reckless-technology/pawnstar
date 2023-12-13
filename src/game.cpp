#include <string.h>
#include <thread>

#include "position.h"
#include "debug_hashtable.h"
#include "transposition_table.h"
#include "types.h"
#include "function_prototypes.h"
#include "move_generation.h"
#include "game.h"

Game the_game;
static std::thread worker_thread;

static bool AreMoveStringsEquivalent(char* str1, char* str2);

void InitializeGame(Game& game)
{
    game.time_control.hard_stop_search_ms              = 0;
    game.time_control.clock_type                       = CLOCK_STANDARD;
    game.time_control.standard.milliseconds_per_period = 5 * 60 * 1000; // 5 minutes
    game.time_control.standard.moves_per_period        = 40;            // 40 moves in 5 mins
    game.time_control.standard.milliseconds_remaining  = 5 * 60 * 1000;
    game.node_count                                    = 0;
    game.engine_color                                  = NEITHER_COLOR;
    game.do_show_thinking                              = true;
    game.position                                      = &game.stack[0];
    *game.position                                     = Position {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"};
}

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
If the game is over, display the xboard style result string
*/
void DisplayResultIfGameOver(const Game& game)
{
    const Position& position = *game.position;
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
        DisplayResultIfGameOver(game);
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
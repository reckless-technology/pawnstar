#pragma once

#include "options.h"
#include "chess_clock.h"
#include "position.h"

/**
 * @brief A chess game.
*/
struct Game
{
    Position*   position;               /**< stack pointer - current position               */
    Position    stack[MAX_GAME_LENGTH]; /**< position stack                                 */
    TimeControl time_control;           /**< Clock controls for the current game            */
    int         node_count;             /**< Number of nodes (positions) during search      */
    int         engine_color;           /**< The color which pawnstar is playing            */
    bool        do_show_thinking;       /**< Whether to show scores and PV during search    */
};

extern Game the_game;

void    InitializeGame(Game& game);
Move    PlayMoveString(Game& game, char* move_string);
void    StartThinking(Game& game);
void    StopThinking(void);
void    StopWorker(void);
void    PlayMove(Game& game, Move move);
void    DisplayResultIfGameOver(const Game& game);
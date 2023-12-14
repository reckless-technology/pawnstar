#pragma once

#include <thread>

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
    bool        is_cancel_pending;

    Game();
    Game(const char* fen_string);
    Move    PlayMove(char* move_string);
    void    PlayMove(Move move);
    void    MakeNullMove();
    void    UndoMove();
    void    StartThinking();
    void    StopThinking();
    bool    IsGameOver() const;

private:
    void    SearchThreadEntry();
    std::thread worker_thread;
};
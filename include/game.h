#pragma once

#include <thread>
#include <string_view>

#include "options.h"
#include "chess_clock.h"
#include "position.h"

/**
 * @brief A chess game.
*/
struct Game
{
    Position*   position_;               /**< stack pointer - current position               */
    Position    stack_[MAX_GAME_LENGTH]; /**< position stack                                 */
    TimeControl time_control_;           /**< Clock controls for the current game            */
    int         node_count_;             /**< Number of nodes (positions) during search      */
    Color       engine_color_;           /**< The color which pawnstar is playing            */
    bool        do_show_thinking_;       /**< Whether to show scores and PV during search    */
    bool        is_cancel_pending_;

    Game();
    Game(std::string_view fen_string);
    Move    PlayMove(std::string_view move_string);
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
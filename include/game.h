#pragma once

#include <string_view>
#include <thread>
#include <vector>

#include "chess_clock.h"
#include "options.h"
#include "position.h"

/**
 * @brief A chess game.
 */
struct Game
{
    TimeControl time_control_;     /**< Clock controls for the current game            */
    int         node_count_;       /**< Number of nodes (positions) during search      */
    Color       engine_color_;     /**< The color which pawnstar is playing            */
    bool        do_show_thinking_; /**< Whether to show scores and PV during search    */
    bool        is_cancel_pending_;

    Game();
    Game(std::string_view fen_string);
    Move      PlayMove(std::string_view move_string);
    void      PlayMove(Move move);
    void      MakeNullMove();
    void      UndoMove();
    void      StartThinking();
    void      StopThinking();
    bool      IsGameOver() const;
    Position &CurrentPosition()
    {
        return positions_.back();
    }
    const Position &CurrentPosition() const
    {
        return positions_.back();
    }

  private:
    void                  SearchThreadEntry();
    std::thread           worker_thread;
    std::vector<Position> positions_;
};
#pragma once

#include <string_view>
#include <thread>
#include <vector>

#include "chess_clock.h"
#include "constants.h"
#include "position.h"

/// @brief Class to represent a game of chess.
class Game
{
  public:
    TimeControl time_control_;      ///< Clock controls for the current game
    int         node_count_;        ///< Number of nodes (positions) during search
    Color       engine_color_;      ///< The color which pawnstar is playing
    bool        do_show_thinking_;  ///< Whether to show scores and PV during search
    bool        is_cancel_pending_; ///< Set to true when time for this search is expired

    Game();
    Game(std::string_view fen_string);
    Move      PlayMove(std::string_view move_string);
    void      PlayMove(Move move);
    void      MakeNullMove();
    void      UndoMove();
    void      StartThinking();
    void      StopThinking();
    bool      IsGameOver() const;
    bool      IsDrawByRepetition(bool is_search) const;
    bool      IsDrawByFiftyMoves() const;
    Position &CurrentPosition()
    {
        return *position_;
    }
    const Position &CurrentPosition() const
    {
        return *position_;
    }

  private:
    void        SearchThreadEntry();
    std::thread worker_thread_;                               ///< Worker thread for searching moves.
    Position   *position_;                                    ///< Current position.
    Position    positions_[256] __attribute__((aligned(16))); ///< Position stack.
};
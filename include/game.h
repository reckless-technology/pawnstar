#pragma once

#include <array>
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
    bool        is_cancel_pending_; ///< Set to true when time for this search is expired

    Game()
    {
        NewGame();
    }
    Position &CurrentPosition()
    {
        return positions_[index_];
    }
    const Position &CurrentPosition() const
    {
        return positions_[index_];
    }

    void NewGame(std::string_view fen_string);
    void NewGame();
    Move PlayMove(std::string_view move_string);
    void PlayMove(Move move);
    void MakeNullMove();
    void UndoMove();
    void StartThinking();
    void StopThinking();
    bool IsGameOver() const;
    bool IsDrawByRepetition() const;
    bool IsDrawByFiftyMoves() const;

  private:
    void                      SearchThreadEntry();
    std::thread               worker_thread_; ///< Worker thread for searching moves.
    int                       index_;         ///< Current position index.
    std::array<Position, 256> positions_;     ///< Position stack.
};
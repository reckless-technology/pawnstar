#include <cstring>
#include <format>
#include <iostream>
#include <string>
#include <string_view>

using std::string;
using std::string_view;

#include "debug_hashtable.h"
#include "game.h"
#include "position.h"
#include "search.h"
#include "transposition_table.h"

/// @brief Construct a game from a FEN (Forsyth Edwards) position string.
/// @param fen_string Initial position.
void Game::NewGame(std::string_view fen_string)
{
    time_control = ChessClock{};
    is_cancel_pending.store(false, std::memory_order_relaxed);
    eval_cache.Clear(); // evals are net-specific; start each game with a clean cache
    positions_.clear();
    positions_.reserve(1024); // typical games stay well under this; avoids reallocations during normal play
    positions_.push_back(Position::FromString(fen_string));
}

/// @brief Start a new game from the standard initial position.
void Game::NewGame()
{
    NewGame("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
}

/// @brief Play a move and update state. Move must be legal for the current position.
/// @param move The move to be played.
void Game::PlayMove(Move move)
{
    positions_.push_back(CurrentPosition().MakeMove(move));
}

/// @brief Make a null move. Only used during null move heuristic.
void Game::MakeNullMove()
{
    positions_.push_back(CurrentPosition().MakeNullMove());
}

/// @brief Undo the last move played.
void Game::UndoMove()
{
    positions_.pop_back();
}

/// @brief Determine if the game is a draw by the 50 move rule.
/// @return true if 50 move draw.
bool Game::IsDrawByFiftyMoves() const
{
    if (CurrentPosition().ReversibleMoveCount() >= 100)
    {
        INCREMENT("draws by 50 moves");
        return true;
    }
    return false;
}

/// @brief Determine if the game is a draw by repetition.
/// @return true if draw by repetition.
bool Game::IsDrawByRepetition() const
{
    int             repetitions = 2;
    const zobrist_t hash        = CurrentPosition().Hash();
    // Start looking 4 half moves back, i.e. 2 moves ago, for repeated positions. Index-based so an early
    // game (fewer than 5 positions) simply skips the loop rather than forming an out-of-range iterator.
    for (int i = (int)positions_.size() - 5; i >= 0; i -= 2)
    {
        const Position &pos = positions_[i];
        if (pos.Hash() == hash && --repetitions == 0)
        {
            INCREMENT("draws by repetition Game");
            return true;
        }
        if (pos.ReversibleMoveCount() == 0)
        {
            return false;
        }
    }
    return false;
}

/// @brief Play a move from the given string and update game state accordingly.
/// @param move_str The string containing the move to be made, e.g. "e2e4", "e7e8q" (promotion), "e1g1" (castling).
/// @return The move which was made, or Move::None in the case the string did not contain a legal move for this
/// position, and no move was therefore played.
Move Game::PlayMove(std::string_view move_str)
{
    MoveList move_list = CurrentPosition().GenerateLegalMoves();
    for (const Move &move : move_list)
    {
        if (move.ToString() == move_str)
        {
            PlayMove(move);
            return move;
        }
    }
    return Move::None();
}

/// @brief Check to see for the end of the game.
/// @return true if the game is over.
bool Game::IsGameOver() const
{
    if (CurrentPosition().IsCheckmate())
    {
        std::cout << (CurrentPosition().ColorToMove() == kBlack ? "1-0 {white mates}\n" : "0-1 {black mates}\n");
        return true;
    }
    if (CurrentPosition().IsStalemate())
    {
        std::cout << "1/2-1/2 {stalemate}\n";
        return true;
    }
    if (IsDrawByRepetition())
    {
        std::cout << "1/2-1/2 {draw by repetition}\n";
        return true;
    }
    if (CurrentPosition().IsDrawByMaterial())
    {
        std::cout << "1/2-1/2 {draw by insufficient material}\n";
        return true;
    }
    if (IsDrawByFiftyMoves())
    {
        std::cout << "1/2-1/2 {draw by 50 reversible moves}\n";
        return true;
    }
    return false;
}

/// @brief Entry point for the search worker thread.
void Game::SearchThreadEntry()
{
    Move move = SearchRootNode(*this);
    if (move != Move::None())
    {
        PlayMove(move);
        std::cout << std::format("bestmove {}\n", move.ToString());
        IsGameOver();
    }
}

/// @brief If currently thinking, stop immediately.
void Game::StopThinking()
{
    is_cancel_pending.store(true, std::memory_order_relaxed);
    if (worker_thread_.joinable())
    {
        worker_thread_.join();
    }
}

/// @brief Start thinking on the worker thread.
void Game::StartThinking()
{
    StopThinking();
    worker_thread_ = std::thread([this] { this->SearchThreadEntry(); });
}

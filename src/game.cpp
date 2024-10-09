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
    time_control_.hard_stop_ms        = 0;
    time_control_.clock_type          = CHESS_CLOCK_STANDARD;
    time_control_.ms_remaining        = 5 * 60 * 1000; // 5 minutes
    time_control_.num_moves_remaining = 0;
    node_count_                       = 0;
    index_                            = 0;
    is_cancel_pending_                = false;
    positions_[index_]                = Position::FromString(fen_string);
}

void Game::NewGame()
{
    NewGame("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
}

/// @brief Play a move and update state. Move must be legal for the current position.
/// @param move The move to be played.
void Game::PlayMove(Move move)
{

    positions_[index_ + 1] = CurrentPosition().MakeMove(move);
    ++index_;
}

/// @brief Make a null move. Only used during null move heuristic.
void Game::MakeNullMove()
{
    positions_[index_ + 1] = CurrentPosition().MakeNullMove();
    ++index_;
}

/// @brief Undo the last move played.
void Game::UndoMove()
{
    if (index_ > 0)
    {
        --index_;
    }
    else
    {
        std::cout << "ERROR: undo a non existent move!\n";
    }
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
/// @param is_search true in the main search (single repetition), false for the FIDE rule (double repetition)
/// @return true if draw by repetition.
bool Game::IsDrawByRepetition(bool is_search) const
{
    int            repetitions = is_search ? 1 : 2;
    const uint64_t hash        = CurrentPosition().Hash();
    for (int i = index_ - 4; i >= 0; i -= 2)
    {
        if (positions_[i].Hash() == hash && --repetitions == 0)
        {
            INCREMENT("draws by repetition");
            return true;
        }
        if (positions_[i].ReversibleMoveCount() == 0)
        {
            return false;
        }
    }
    return false;
}

/// @brief Play a move from the given move string and update game state_flags accordingly.
/// @param move_str The string containing the move to be made.
/// @return The move which was made, or Move::None in the case the string did not contain a legal move.
Move Game::PlayMove(std::string_view move_str)
{
    string   candidate{move_str};
    MoveList move_list = CurrentPosition().GenerateLegalMoves();
    for (const Move &move : move_list)
    {
        const string algebraic_move_str{move.ToString()};
        if (algebraic_move_str == candidate)
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
        std::cout << (CurrentPosition().ColorToMove() == BLACK ? "1-0 {white mates}\n" : "0-1 {black mates}\n");
        return true;
    }
    if (CurrentPosition().IsStalemate())
    {
        std::cout << "1/2-1/2 {stalemate}\n";
        return true;
    }
    if (IsDrawByRepetition(false))
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
    if (move)
    {
        PlayMove(move);
        std::cout << std::format("bestmmove {}\n", move.ToString());
        IsGameOver();
    }
}

/// @brief If currently thinking, stop immediately.
void Game::StopThinking()
{
    is_cancel_pending_ = true;
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
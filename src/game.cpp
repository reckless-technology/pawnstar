#include <cstring>
#include <string>
#include <string_view>

using std::string;
using std::string_view;

#include "debug_hashtable.h"
#include "game.h"
#include "position.h"
#include "search.h"
#include "transposition_table.h"

constexpr string_view move_suffixes[] = {"+", "#", "ep", "e.p.", "!", "?"};

/// @brief Remove ambiguous or extraneous suffices from the SAN move.
/// @param move_str Move string to process.
constexpr void RemoveMoveSuffixes(string &move_str)
{
    for (const string_view sv : move_suffixes)
    {
        auto locn = move_str.find(sv);
        if (locn != string::npos)
        {
            move_str.erase(locn);
        }
    }
}

/// @brief Construct a game from a FEN (Forsyth Edwards) position string.
/// @param fen_string Initial position.
Game::Game(std::string_view fen_string)
{
    time_control_.hard_stop_search_ms              = 0;
    time_control_.clock_type                       = CLOCK_STANDARD;
    time_control_.standard.milliseconds_per_period = 5 * 60 * 1000; // 5 minutes
    time_control_.standard.moves_per_period        = 40;            // 40 moves in 5 mins
    time_control_.standard.milliseconds_remaining  = 5 * 60 * 1000;
    node_count_                                    = 0;
    engine_color_                                  = NEITHER_COLOR;
    do_show_thinking_                              = true;
    index_                                         = 0;
    positions_[index_]                             = Position{fen_string};
}

/// @brief Construct a game from the default starting position.
Game::Game() : Game("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1")
{
}

Game &Game::operator=(const Game &that)
{
    std::memcpy(positions_, that.positions_, sizeof(positions_));
    index_ = that.index_;
    return *this;
}

/// @brief Play a move and update state. Move must be legal for the current position.
/// @param move The move to be played.
void Game::PlayMove(Move move)
{
    positions_[index_ + 1] = positions_[index_].MakeMove(move);
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
        printf("ERROR: undo a non existent move!\n");
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
    for (int i = index_ - 2; i >= 0; --i)
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

/// @brief Make a null move. Only used during null move heuristic.
void Game::MakeNullMove()
{
    positions_[index_ + 1] = positions_[index_].MakeNullMove();
    ++index_;
}

/// @brief Play a move from the given move string in either standard algebraic or XBoard format, and update game
/// state_flags accordingly.
/// @param move_str The string containing the move to be made.
/// @return The move which was made, or Move::None in the case the string did not contain a legal move.
Move Game::PlayMove(std::string_view move_str)
{
    string candidate{move_str};
    RemoveMoveSuffixes(candidate);
    MoveList move_list = positions_[index_].GenerateLegalMoves();
    for (const Move &move : move_list)
    {
        const string algebraic_move_str{move.ToString()};
        string       san_move_str{CurrentPosition().MoveToString(move, &move_list)};
        RemoveMoveSuffixes(san_move_str);
        if (san_move_str == candidate || algebraic_move_str == candidate)
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
        printf(CurrentPosition().ColorToMove() == BLACK ? "1-0 {white mates}\n" : "0-1 {black mates}\n");
        return true;
    }
    if (CurrentPosition().IsStalemate())
    {
        printf("1/2-1/2 {stalemate}\n");
        return true;
    }
    if (IsDrawByRepetition(false))
    {
        printf("1/2-1/2 {draw by repetition}\n");
        return true;
    }
    if (CurrentPosition().IsDrawByMaterial())
    {
        printf("1/2-1/2 {draw by insufficient material}\n");
        return true;
    }
    if (IsDrawByFiftyMoves())
    {
        printf("1/2-1/2 {draw by 50 reversible moves}\n");
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
        std::string move_string{CurrentPosition().MoveToString(move, nullptr)};
        PlayMove(move);
        printf("move %s\n", move_string.c_str());
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
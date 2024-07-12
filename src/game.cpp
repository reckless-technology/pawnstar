#include <string>
#include <string_view>

using std::string;
using std::string_view;

#include "debug_hashtable.h"
#include "function_prototypes.h"
#include "game.h"
#include "move_generation.h"
#include "position.h"
#include "search.h"
#include "transposition_table.h"

static constexpr string_view move_suffixes[6] = {
    "+", "#", "ep", "e.p.", "!", "?",
};

static void RemoveMoveSuffixes(string &move_str)
{
    for (const string_view &sv : move_suffixes)
    {
        auto locn = move_str.find(sv);
        if (locn != string::npos)
        {
            move_str.erase(locn);
        }
    }
}

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
    positions_.clear();
    positions_.push_back(Position{fen_string});
}

Game::Game() : Game("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1")
{
}

/*
Make a move and update the game end status flags
*/
void Game::PlayMove(Move move)
{
    positions_.push_back(CurrentPosition().MakeMove(move));
}

void Game::UndoMove()
{
    if (positions_.size() > 1)
    {
        positions_.pop_back();
    }
    else
    {
        printf("ERROR: undo a non existent move!\n");
    }
}

/*
Determine if the current position represents a draw by the 50 move rule
(50 consecutive reversible moves by each player is a drawn game)
*/
bool Game::IsDrawByFiftyMoves() const
{
    if (CurrentPosition().ReversibleMoveCount() >= 100)
    {
        INCREMENT("draws by 50 moves");
        return true;
    }
    return false;
}

/*
Determine if the current position represents a draw by repetition.
*/
bool Game::IsDrawByRepetition(bool is_search) const
{
    int            repetitions = is_search ? 1 : 2;
    const uint64_t hash        = CurrentPosition().Hash();
    for (int i = positions_.size() - 2; i >= 0; --i)
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

void Game::MakeNullMove()
{
    positions_.push_back(CurrentPosition().MakeNullMove());
}

/*
Play a move from the given move string in either standard algebraic or XBoard
format, and update game state_flags accordingly

returns the move if a legal move was played
returns zero if the move was illegal or the game is over
*/
Move Game::PlayMove(std::string_view move_str)
{
    string candidate{move_str};
    RemoveMoveSuffixes(candidate);
    MoveList move_list = GenerateLegalMoves(positions_.back());
    for (const Move &move : move_list)
    {
        string san_move_str{CurrentPosition().MoveToString(move)};
        string algebraic_move_str{MoveString(move)};
        RemoveMoveSuffixes(san_move_str);
        if (san_move_str == candidate || algebraic_move_str == candidate)
        {
            PlayMove(move);
            return move;
        }
    }
    return 0;
}

/**
 * @brief Check to see if the game is over. Display the result and return true if so.
 * @param game The game
 * @return true in the case of checkmate, stalemate or a draw for insufficient material, 50 moves or repetition.
 */
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

/**
 * @brief Entry point for the search thread.
 * @param game Game to search on.
 */
void Game::SearchThreadEntry()
{
    Move move = SearchRootNode(*this);
    if (move)
    {
        std::string move_string{CurrentPosition().MoveToString(move)};
        PlayMove(move);
        printf("move %s\n", move_string.c_str());
        IsGameOver();
    }
}

void Game::StopThinking()
{
    is_cancel_pending_ = true;
    if (worker_thread.joinable())
    {
        worker_thread.join();
    }
}

/**
 * @brief Start thinking on a worker thread.
 * @param game Game to search on.
 */
void Game::StartThinking()
{
    StopThinking();
    worker_thread = std::thread([this] { this->SearchThreadEntry(); });
}
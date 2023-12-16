#include <string>
#include <string_view>

using std::string;
using std::string_view;

#include "position.h"
#include "debug_hashtable.h"
#include "transposition_table.h"
#include "function_prototypes.h"
#include "game.h"
#include "search.h"

static constexpr string_view move_suffixes[6] = 
{
    "+",
    "#",
    "ep",
    "e.p.",
    "!",
    "?",
};

static void RemoveMoveSuffixes(string& move_str)
{
    for (const string_view& sv : move_suffixes)
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
   position_                                      = &stack_[0];
   *position_                                     = Position { fen_string };
}

Game::Game() : Game("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1") {}

/*
Make a move and update the game end status flags
*/
void Game::PlayMove(Move move)
{
    position_[1] = Position(position_[0], move);
    ++position_;
}

void Game::UndoMove()
{
    --position_;
}

void Game::MakeNullMove()
{
    position_->MakeNullMove(position_[1]);
    ++position_;
}

/*
Play a move from the given move string in either standard algebraic or XBoard 
format, and update game state_flags accordingly

returns the move if a legal move was played
returns zero if the move was illegal or the game is over
*/
Move Game::PlayMove(std::string_view move_str)
{
    string candidate { move_str };
    RemoveMoveSuffixes(candidate);
    MoveList move_list = position_->GenerateLegalMoves();
    for (const auto& move : move_list)
    {
        string move_string { position_->MoveToString(move) };
        string alebraic_move_string = MoveString(move);
        RemoveMoveSuffixes(move_string);
        if (move_string == candidate || alebraic_move_string == candidate)
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
    if (position_->IsCheckmate())
    {
        printf(position_->ColorToMove() == BLACK ? "1-0 {white mates}\n" : "0-1 {black mates}\n");
        return true;
    }
    if (position_->IsStalemate())
    {
        printf("1/2-1/2 {stalemate}\n");
        return true;
    }
    if (position_->IsDrawByRepetition(false))
    {
        printf("1/2-1/2 {draw by repetition}\n");
        return true;
    }
    if (position_->IsDrawByMaterial())
    {
        printf("1/2-1/2 {draw by insufficient material}\n");
        return true;
    }
    if (position_->IsDrawByFiftyMoves())
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
        std::string move_string { position_->MoveToString(move) };
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
    worker_thread = std::thread(Game::SearchThreadEntry, this);
}
#include <string.h>
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

Game::Game(const char* fen_string)
{
   time_control.hard_stop_search_ms              = 0;
   time_control.clock_type                       = CLOCK_STANDARD;
   time_control.standard.milliseconds_per_period = 5 * 60 * 1000; // 5 minutes
   time_control.standard.moves_per_period        = 40;            // 40 moves in 5 mins
   time_control.standard.milliseconds_remaining  = 5 * 60 * 1000;
   node_count                                    = 0;
   engine_color                                  = NEITHER_COLOR;
   do_show_thinking                              = true;
   position                                      = &stack[0];
   *position                                     = Position { fen_string };
}

Game::Game() : Game("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1") {}

/*
Make a move and update the game end status flags
*/
void Game::PlayMove(Move move)
{
    position[1] = Position(position[0], move);
    ++position;
}

void Game::UndoMove()
{
    --position;
}

void Game::MakeNullMove()
{
    position->MakeNullMove(position[1]);
    ++position;
}

/*
Play a move from the given move string in either standard algebraic or XBoard 
format, and update game state_flags accordingly

returns the move if a legal move was played
returns zero if the move was illegal or the game is over
*/
Move Game::PlayMove(char* move_str)
{
    string candidate { move_str };
    RemoveMoveSuffixes(candidate);
    MoveList move_list;
    position->GenerateLegalMoves(move_list);
    for (auto move : move_list)
    {
        string move_string { position->MoveToString(move) };
        RemoveMoveSuffixes(move_string);
        if (move_string == candidate)
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
    if (position->IsCheckmate())
    {
        printf(position->ColorToMove() == BLACK ? "1-0 {white mates}\n" : "0-1 {black mates}\n");
        return true;
    }
    if (position->IsStalemate())
    {
        printf("1/2-1/2 {stalemate}\n");
        return true;
    }
    if (position->IsDrawByRepetition(false))
    {
        printf("1/2-1/2 {draw by repetition}\n");
        return true;
    }
    if (position->IsDrawByMaterial())
    {
        printf("1/2-1/2 {draw by insufficient material}\n");
        return true;
    }
    if (position->IsDrawByFiftyMoves())
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
    int move = SearchRootNode(*this);
    if (move)
    {
        std::string move_string { position->MoveToString(move) };
        PlayMove(move);
        printf("move %s\n", move_string.c_str());
        IsGameOver();
    }  
}

void Game::StopThinking()
{
    is_cancel_pending = true;
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
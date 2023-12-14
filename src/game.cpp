#include <string.h>
#include <thread>
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

Game the_game;
static std::thread worker_thread;

static bool AreMoveStringsEquivalent(string& str1, string& str2);

void InitializeGame(Game& game)
{
    game.time_control.hard_stop_search_ms              = 0;
    game.time_control.clock_type                       = CLOCK_STANDARD;
    game.time_control.standard.milliseconds_per_period = 5 * 60 * 1000; // 5 minutes
    game.time_control.standard.moves_per_period        = 40;            // 40 moves in 5 mins
    game.time_control.standard.milliseconds_remaining  = 5 * 60 * 1000;
    game.node_count                                    = 0;
    game.engine_color                                  = NEITHER_COLOR;
    game.do_show_thinking                              = true;
    game.position                                      = &game.stack[0];
    *game.position                                     = Position {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"};
}

/*
Make a move and update the game end status flags
*/
void PlayMove(Game& game, Move move)
{
    game.position[1] = Position(game.position[0], move);
    ++game.position;
}

/*
Play a move from the given move string in either standard algebraic or XBoard 
format, and update game state_flags accordingly

returns the move if a legal move was played
returns zero if the move was illegal or the game is over
*/
Move PlayMove(Game& game, char* move_str)
{
    MoveList move_list;
    game.position->GenerateLegalMoves(move_list);
    for (auto move : move_list)
    {
        string move_string { game.position->MoveToString(move) };
        string candidate { move_str };
        if (AreMoveStringsEquivalent(move_string, candidate))
        {
            PlayMove(game, move);
            return move;
        }
    }
    return 0;
}

constexpr string_view strings_to_remove[6] = 
{
    "+",
    "#",
    "ep",
    "e.p.",
    "!",
    "?",
};

/**
 * @brief Compare two move strings for equality, disregarding extraneous characters.
 * @param str1 first move string
 * @param str2 second move string
 * @return true if moves are equivalent
 */
static bool AreMoveStringsEquivalent(string& str1, string& str2)
{
    for (const string_view& sv : strings_to_remove)
    {
        auto locn = str1.find(sv);
        if (locn != string::npos)
        {
            str1.erase(locn);
        }
        locn = str2.find(sv);
        if (locn != string::npos)
        {
            str2.erase(locn);
        }
    }
    return str1 == str2;
}

/**
 * @brief Check to see if the game is over. Display the result and return true if so.
 * @param game The game
 * @return true in the case of checkmate, stalemate or a draw for insufficient material, 50 moves or repetition.
 */
bool IsGameOver(const Game& game)
{
    const Position& position = *game.position;
    if (position.IsCheckmate())
    {
        printf(position.ColorToMove() == BLACK ? "1-0 {white mates}\n" : "0-1 {black mates}\n");
        return true;
    }
    if (position.IsStalemate())
    {
        printf("1/2-1/2 {stalemate}\n");
        return true;
    }
    if (position.IsDrawByRepetition(false))
    {
        printf("1/2-1/2 {draw by repetition}\n");
        return true;
    }
    if (position.IsDrawByMaterial())
    {
        printf("1/2-1/2 {draw by insufficient material}\n");
        return true;
    }
    if (position.IsDrawByFiftyMoves())
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
static void SearchThreadEntry(Game& game)
{
    int move = SearchRootNode(*game.position);
    if (move)
    {
        std::string move_string { game.position->MoveToString(move) };
        PlayMove(game, move);
        printf("move %s\n", move_string.c_str());
        IsGameOver(game);
    }  
}

/**
 * @brief Cancel the worker thread.
 */
void StopWorker(void)
{
    StopThinking();
    if (worker_thread.joinable())
    {
        worker_thread.join();
    }
}

/**
 * @brief Start thinking on a worker thread.
 * @param game Game to search on.
 */
void StartThinking(Game& game)
{
    StopWorker();
    worker_thread = std::thread(SearchThreadEntry, std::ref(game));
}
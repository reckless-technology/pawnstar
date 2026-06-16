#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <format>
#include <iostream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

using std::string;
using std::string_view;

#include "chess_clock.h"
#include "debug_hashtable.h"
#include "game.h"
#include "opening_book.h"
#include "position.h"
#include "search_state.h"
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

/// @brief Entry point for the search worker thread.
void Game::SearchThreadEntry()
{
    Move move = SearchRootNode();
    if (move != Move::None())
    {
        PlayMove(move);
        std::cout << std::format("bestmove {}\n", move.ToString());
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

/// @brief Search the current position and return the best move.
/// Returns a book move immediately on a book hit; otherwise runs the Lazy SMP iterative-deepening search:
/// one authoritative main thread plus hardware_concurrency()-1 helpers, all sharing the transposition table.
/// @return The best move, or Move::None if there are no legal moves.
Move Game::SearchRootNode()
{
    Game &game = *this; // local alias so the search body reads uniformly through `game`
    // If there is a book move for this position, do not bother with search.
    Move book_move = game.book.GetMove(game.CurrentPosition().Hash());
    if (book_move != Move::None())
    {
        return book_move;
    }
    MoveList move_list = game.CurrentPosition().GenerateLegalMoves();
    // If there is only 1 legal move available, there is no point wasting time searching it, just play it.
    if (move_list.size() == 0)
    {
        return Move::None();
    }
    if (move_list.size() == 1)
    {
        return move_list[0];
    }
    // Plan time usage for this search.
    int ms_timeout   = 0; // Hard stop: cancel search when this time expires.
    int ms_allocated = 0; // Soft allocated time for the move.
    switch (game.time_control.clock_type)
    {
    case ChessClock::kStandard:
    default:
        if (game.time_control.num_moves_remaining != 0)
        {
            ms_allocated = game.time_control.ms_remaining / game.time_control.num_moves_remaining;
        }
        else
        {
            const int num_moves_to_go_estimate = std::max(40 - game.CurrentPosition().MoveCount(), 5);
            ms_allocated                       = game.time_control.ms_remaining / num_moves_to_go_estimate;
        }
        ms_timeout = std::max(100, std::min(ms_allocated * 2, game.time_control.ms_remaining - 1000));
        game.time_control.hard_stop_ms = game.time_control.ElapsedMilliseconds() + ms_timeout;
        break;

    case ChessClock::kFixedDepth:
        ms_timeout                     = 0;
        ms_allocated                   = 0;
        game.time_control.hard_stop_ms = 0;
        break;

    case ChessClock::kFixedTime:
        ms_timeout                     = game.time_control.ms_remaining;
        game.time_control.hard_stop_ms = game.time_control.ElapsedMilliseconds() + ms_timeout;
        ms_allocated                   = 0;
        break;

    case ChessClock::kInfinite:
        game.time_control.hard_stop_ms = 0;
        break;
    }

    const int64_t start_ms = game.time_control.ElapsedMilliseconds();
    game.is_cancel_pending.store(false, std::memory_order_relaxed);
    DebugXClear();
    game.transposition_table.Age();

    // Shallow pass for initial move ordering (on a temporary state shared as the launch ordering).
    SearchState state{game};
    Variation   shallow_pv{};
    for (auto &move : move_list)
    {
        const int move_index = (int)(&move - move_list.begin());
        const int score = state.SearchSingleMove(kStartDepth, 0, kAlpha, kBeta, move, shallow_pv, move_index).score;
        move.AssignScore(score);
    }
    SortMoves<true>(move_list);
    Move best_move = move_list[0];

    // Lazy SMP: each thread runs an independent iterative-deepening search of the same root, sharing the
    // (lockless) transposition table and (atomic) history table. The main thread is authoritative; helper
    // threads deepen the shared TT to accelerate it. Each thread has its own SearchState and move-list copy.
    unsigned hw        = std::thread::hardware_concurrency();
    int      n_threads = std::clamp<int>(hw == 0 ? 1 : (int)hw, 1, kMaxSearchThreads);
    if (const char *env = std::getenv("PAWNSTAR_THREADS"))
    {
        const int requested = std::atoi(env);
        if (requested > 0)
        {
            n_threads = std::clamp(requested, 1, kMaxSearchThreads);
        }
    }
    std::vector<std::thread> helpers;
    helpers.reserve(n_threads - 1);
    for (int i = 1; i < n_threads; ++i)
    {
        // Each helper gets a distinct thread id, which selects its iteration-skip schedule (see IterativeDeepen).
        helpers.emplace_back([&game, &move_list, best_move, i, start_ms, ms_allocated] {
            SearchState helper_state{game};
            helper_state.IterativeDeepen(move_list, best_move, /*thread_id=*/i, start_ms, ms_allocated);
        });
    }

    best_move = state.IterativeDeepen(move_list, best_move, /*thread_id=*/0, start_ms, ms_allocated);

    game.is_cancel_pending.store(true, std::memory_order_relaxed); // signal helpers to stop
    for (auto &helper : helpers)
    {
        helper.join();
    }
    return best_move;
}

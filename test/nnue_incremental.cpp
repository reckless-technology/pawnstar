/// @file nnue_incremental.cpp Correctness gate for the incremental NNUE accumulator.
///
/// Verifies that the accumulator maintained incrementally by SearchState across PlayMove/UndoMove yields
/// exactly the same evaluation as a full from-scratch refresh, at every node of a random search tree
/// (covering captures, castling, en passant and promotions as the random games progress, plus make/undo
/// round-trips). With no net argument it is a no-op so `make check` stays green when no net is present.
///
/// Usage:  test_nnue_incremental [net.bin]

#include "game.h"
#include "nnue.h"
#include "position.h"
#include "search_state.h"

#include <cstdint>
#include <iostream>
#include <random>

namespace
{
long g_checks = 0;
long g_fails  = 0;

/// @brief Recursively play random legal moves, comparing the incremental accumulator against a full
/// refresh at entry and again after undoing all children (to catch UndoMove reverse bugs).
void Walk(SearchState &state, std::mt19937_64 &rng, int depth, int branch)
{
    const Position      &p   = state.CurrentPosition();
    const nnue::Network &net = state.game.NnueNetwork();
    const int            inc = net.Evaluate(state.CurrentAccumulator(), p.ColorToMove());
    const int            ref = net.Evaluate(p); // full refresh
    ++g_checks;
    if (inc != ref)
    {
        ++g_fails;
        std::cout << "MISMATCH (incremental " << inc << " != refresh " << ref << ") fen=" << p.ToString() << "\n";
    }
    if (depth <= 0)
    {
        return;
    }
    MoveList moves = p.GenerateLegalMoves();
    if (moves.size() == 0)
    {
        return;
    }
    for (int k = 0; k < branch; ++k)
    {
        const Move m = moves[rng() % moves.size()];
        state.PlayMove(m);
        Walk(state, rng, depth - 1, branch);
        state.UndoMove();
    }
    // After undoing every child the accumulator must again match this position's full refresh.
    const int inc_after = net.Evaluate(state.CurrentAccumulator(), p.ColorToMove());
    ++g_checks;
    if (inc_after != ref)
    {
        ++g_fails;
        std::cout << "UNDO MISMATCH (incremental " << inc_after << " != refresh " << ref << ") fen=" << p.ToString()
                  << "\n";
    }
}
} // namespace

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        std::cout << "test_nnue_incremental: no net provided, skipping (pass).\n";
        return 0;
    }
    // Load the net into a Game and select NNUE, so SearchState maintains the accumulator via game.NnueActive().
    Game game;
    if (!game.NnueNetwork().Load(argv[1]))
    {
        std::cout << "test_nnue_incremental: failed to load net '" << argv[1] << "'\n";
        return 1;
    }
    game.SetUseNnue(true);
    if (!game.NnueActive())
    {
        std::cout << "test_nnue_incremental: NNUE not active after load\n";
        return 1;
    }

    // Several independent random search trees from the start position (the net persists across NewGame()).
    for (uint64_t seed = 1; seed <= 12; ++seed)
    {
        game.NewGame();
        SearchState     state{game};
        std::mt19937_64 rng(seed);
        Walk(state, rng, /*depth=*/10, /*branch=*/2);
    }

    std::cout << g_checks << " checks; " << g_fails << " mismatches\n";
    if (g_fails != 0)
    {
        std::cout << "INCREMENTAL NNUE TEST FAILED\n";
        return 1;
    }
    std::cout << "INCREMENTAL NNUE TEST PASS\n";
    return 0;
}

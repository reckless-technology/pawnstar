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
#include "test_report.h"
#include <memory>

#include <cstdint>
#include <format>
#include <iostream>
#include <random>

namespace
{
long g_checks = 0;

/// @brief Recursively play random legal moves, comparing the incremental accumulator against a full
/// refresh at entry and again after undoing all children (to catch UndoMove reverse bugs).
void Walk(SearchState &state, std::mt19937_64 &rng, int depth, int branch)
{
    const Position      &p   = state.CurrentPosition();
    const nnue::Network &net = state.game_.nnue_network_;
    const int            inc = net.Evaluate(state.CurrentAccumulator(), p.color_to_move_);
    const int            ref = net.Evaluate(p); // full refresh
    ++g_checks;
    if (inc != ref)
    {
        test_report::Check(false,
                           std::format("incremental accumulator {} != full refresh {} fen={}", inc, ref, p.ToString()));
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
    const int inc_after = net.Evaluate(state.CurrentAccumulator(), p.color_to_move_);
    ++g_checks;
    if (inc_after != ref)
    {
        test_report::Check(false, std::format("incremental accumulator after undo {} != full refresh {} fen={}",
                                              inc_after, ref, p.ToString()));
    }
}
} // namespace

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        return test_report::Summary("incremental NNUE: skipped (no net supplied)");
    }
    // Load the net into a Game; SearchState then maintains the accumulator incrementally.
    auto  game_ptr = std::make_unique<Game>();
    Game &game     = *game_ptr;
    if (!game.nnue_network_.Load(argv[1]) || !game.nnue_network_.loaded_)
    {
        test_report::Check(false, std::format("incremental NNUE: failed to load net '{}'", argv[1]));
        return test_report::ExitCode();
    }

    // Several independent random search trees from the start position (the net persists across SetPosition()).
    for (uint64_t seed = 1; seed <= 12; ++seed)
    {
        game.SetPosition();
        SearchState     state{game};
        std::mt19937_64 rng(seed);
        Walk(state, rng, /*depth=*/10, /*branch=*/2);
    }

    return test_report::Summary(std::format("INCREMENTAL NNUE: {} checks bit-identical to full refresh", g_checks));
}

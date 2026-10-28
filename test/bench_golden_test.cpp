/// @file bench_golden_test.cpp The bench-golden test: the cross-engine bit-parity signature as a gate.
///
/// `bench` searches the 16 kBenchFens positions to kBenchDepth (13), single-threaded with the book off and the hash
/// cleared. With the shipped net (pawnstar-v12.bin) this engine and its two bit-exact ports, pawnstar-go and
/// pawnstar-rs, report the identical signature: 6,601,454 nodes, and the same best move and node count at every
/// position. This test runs that recipe through RunBench (input_handling.h) - the very code the `bench` command runs
/// - and checks every position's best move and node count and the total against the table below, so a change that
/// moves the search, the move ordering, the pruning, the TT or the eval fails here at once rather than being caught
/// only by a transcript diff against the ports.
///
/// The table is pinned in the ports too: pawnstar-rs's tests/bench_golden_test.rs (`GOLDEN`, `GOLDEN_TOTAL`, the
/// same 16 pairs) and pawnstar-go's engine/uci_test.go (`benchGoldenNodes`, the total). Change it ONLY for a
/// deliberate, SPRT-gated engine change or a new shipped net, and re-port that change to both before they will agree
/// again; never to quieten the test.
///
/// The net is taken from argv[1]; with no argument the suite is skipped (so `make check` stays green when the net is
/// absent - the numbers are net-specific).
///
/// Usage:  test_bench_golden [net.bin]

#include "game.h"
#include "input_handling.h"
#include "nnue.h"
#include "test_report.h"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <format>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace bench_golden
{

/// @brief One position's expected outcome at kBenchDepth.
struct GoldenEntry
{
    std::string_view best_move_; ///< Expected best move, in UCI notation.
    uint64_t         nodes_;     ///< Expected main-thread node count.
};

/// @brief The bench signature with pawnstar-v12.bin at depth 13, single-threaded, in kBenchFens order: the same
/// table as pawnstar-rs's `GOLDEN` (tests/bench_golden_test.rs).
inline constexpr std::array<GoldenEntry, 16> kGolden{{
    {"d6d1", 13710},
    {"d4d5", 147492},
    {"f8g8", 1230595},
    {"e5e6", 582804},
    {"c3d5", 410154},
    {"g5g6", 180915},
    {"d1c1", 911931},
    {"f4f5", 170315},
    {"f1b5", 686447},
    {"f6d7", 552912},
    {"f2f4", 514815},
    {"d7f5", 261314},
    {"b2b4", 324298},
    {"d1d2", 271979},
    {"g4g7", 119154},
    {"d2e4", 222619},
}};

/// @brief The bench total: the cross-engine signature, pinned in pawnstar-go (`benchGoldenNodes`) and pawnstar-rs
/// (`GOLDEN_TOTAL`) as well. It is the sum of kGolden's node counts (checked at compile time below).
inline constexpr uint64_t kGoldenTotal = 6'601'454;

/// @brief The sum of kGolden's node counts.
consteval uint64_t GoldenSum()
{
    uint64_t sum = 0;
    for (const GoldenEntry &entry : kGolden)
    {
        sum += entry.nodes_;
    }
    return sum;
}

static_assert(GoldenSum() == kGoldenTotal, "the per-position golden node counts must sum to the golden total");

} // namespace bench_golden

/// @brief Run the bench recipe and compare it with the golden table.
/// @param argc Argument count. @param argv [net.bin].
/// @return Non-zero on a net load error, a position count other than the table's, or any best move, node count or
///         total that differs from it.
int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        return test_report::Summary("bench golden: skipped (no net supplied)");
    }
    const std::string net_path = argv[1];
    // The 6,601,454 signature belongs to this net at this shape. A build at another shape refuses the net
    // outright, so there is no bench to compare: that shape's gate is the six-position node-count signature
    // over the synthetic net, tools/new-shape-signature.sh.
    if (!nnue::IsNetFileThisBuildsShape(net_path))
    {
        return test_report::Summary(
            std::format("bench golden: skipped (net '{}' is not this build's NNUE shape)", net_path));
    }

    auto  game_ptr = std::make_unique<Game>();
    Game &game     = *game_ptr;
    if (!game.nnue_network_.Load(net_path) || !game.nnue_network_.loaded_)
    {
        test_report::Check(false, std::format("bench golden: failed to load net '{}'", net_path));
        return test_report::ExitCode();
    }

    const std::vector<BenchResult> results = RunBench(game, kBenchDepth, false);
    if (!test_report::Check(results.size() == bench_golden::kGolden.size(),
                            std::format("bench golden: {} positions searched (golden table has {})", results.size(),
                                        bench_golden::kGolden.size())))
    {
        return test_report::ExitCode();
    }

    uint64_t total_nodes = 0;
    for (size_t i = 0; i < results.size(); ++i)
    {
        const BenchResult               &got  = results[i];
        const bench_golden::GoldenEntry &want = bench_golden::kGolden[i];
        total_nodes += got.nodes_;
        test_report::Check(got.best_move_ == want.best_move_ && got.nodes_ == want.nodes_,
                           std::format("bench pos {:02}: bm={} nodes={} (golden bm={} nodes={})", i + 1, got.best_move_,
                                       got.nodes_, want.best_move_, want.nodes_));
    }
    test_report::Check(total_nodes == bench_golden::kGoldenTotal,
                       std::format("bench total {} == golden {}", total_nodes, bench_golden::kGoldenTotal));
    return test_report::Summary(
        std::format("bench golden: {} nodes at depth {}, the cross-engine signature", total_nodes, kBenchDepth));
}

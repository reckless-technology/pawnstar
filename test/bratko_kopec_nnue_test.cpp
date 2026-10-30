/// @file bratko_kopec_nnue.cpp Bratko-Kopec search-quality suite using the NNUE evaluation.
///
/// It searches the 24 positions in the kCases array (defined below) with the NNUE evaluator and checks the
/// *move* (the classic Bratko-Kopec metric — did the engine find a good move). A position is "solved" when
/// the move found is among that position's accepted moves (the set of best moves the engine produces over
/// depths 8–11; see kCases below). The search is single-threaded by default (`PAWNSTAR_THREADS=1`, but
/// overridable) so it is deterministic, and the suite is a hard gate: it must solve all 24.
///
/// The net is taken from argv[1]; with no argument the suite is skipped (so `make check` stays green when
/// the net is absent). Optional argv[2] sets the search depth (default 8).
///
/// Usage:  test_bratko_kopec_nnue [net.bin] [depth]

#include "chess_clock.h"
#include "debug_hashtable.h"
#include "game.h"
#include "move.h"
#include "nnue.h"
#include "position.h"
#include "test_report.h"
#include <memory>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <format>
#include <iostream>
#include <string>
#include <string_view>

/// The Bratko-Kopec positions and their accepted best moves. Each case is a position plus up to 5 accepted
/// moves: the best moves the engine produces over depths 8–11, single-threaded and so deterministic. A
/// position is "solved" when the move found is one of these. `make check` runs single-threaded, so the
/// suite is a deterministic 24/24 gate there; under multi-threaded Lazy SMP the authoritative move varies
/// run-to-run and no finite sampled set covers that distribution, so a multi-threaded run may dip below
/// 24. The sets are **net-specific**: regenerate them whenever the net or the search changes (see the
/// regeneration step in nnue/README.md §7, *Recreating*).
///
/// These sets were regenerated for the shipped v20 net (release 2.0.0), which changed both the training
/// data and the network shape. They are the single-threaded union over depths 8–11 and nothing else, so
/// the extra moves each set carries are the depths that genuinely disagreed, not history: only pos02
/// (f2f3 at depth 8, d4d5 deeper), pos23 (c8f5/f7f6) and pos24 (g2g3/e4f5) have more than one.
///
/// **Two positions moved off the classical Bratko-Kopec answer with v20, and they are named here so that a
/// future re-blessing cannot silently absorb them:**
///
///   * **pos02** plays `f2f3` at depth 8 and reaches the classical `d4d5` only from depth 9 (measured at
///     depths 9, 10 and 11).
///   * **pos24** plays `g2g3` at depths 8–10 and `e4f5` at depth 11, and does **not** find the classical
///     `f2f4` at any of depths 8–11.
///
/// v12 found both at depth 8. Bratko-Kopec is 24 positions and is not a strength measurement — v20 measured
/// +9.99 Elo at 8+0.08 (8,000 games) and +1.13 at 12+0.12 (12,000 games) against v12 — so this is a
/// recorded difference, not a regression to chase.
///
/// Verified across the three ports on 2026-10-30 (the Legion, Windows): each port's own binary, Hash pinned
/// to 64 MB, single-threaded, book off, over all 24 positions at depths 7, 8, 9, 10 and 11 — identical best
/// move *and* identical node count in all 120 position/depth pairs in C++, Rust and Go. So the two moved
/// positions moved in all three, as bit-exactness requires, and the three repositories' sets are three
/// recordings of one measurement. pawnstar-go's set is the union over depths **7 and 8** rather than 8–11,
/// because its suite drops to depth 7 under `-short`; that adds pos03 `f7g5` and pos20 `e2h5`, which are
/// the depth-7 choices and were measured here too.
///
/// Move ordering is **platform-independent**: SortMoves (move.h) is a stable insertion sort, so equal-
/// ordering-score moves keep their generation order on every STL, and the single-threaded best move at a
/// given depth is identical across libstdc++, libc++ and MSVC.
namespace bk
{

/// @brief Depth used when none is given on the command line. The accepted-move sets were recorded over
/// depths 8–11 inclusive (their union); see the file header.
constexpr int kDefaultDepth = 8;

/// @brief A Bratko-Kopec position and the moves accepted as correct for it.
struct BkCase
{
    std::string_view                fen_;   ///< Position FEN.
    std::array<std::string_view, 5> moves_; ///< Up to 5 accepted best moves; empty entries ignored.
};

// clang-format off
/// @brief The 24 Bratko-Kopec positions and their accepted moves (engine's best moves over depths 8–11).
inline constexpr std::array<BkCase, 24> kCases{{
    {"1k1r4/pp1b1R2/3q2pp/4p3/2B5/4Q3/PPP2B2/2K5 b - -", {"d6d1"}},
    {"3r1k2/4npp1/1ppr3p/p6P/P2PPPP1/1NR5/5K2/2R5 w - -", {"f2f3", "d4d5"}},
    {"2q1rr1k/3bbnnp/p2p1pp1/2pPp3/PpP1P1P1/1P2BNNP/2BQ1PRK/7R b - -", {"f8g8"}},
    {"rnbqkb1r/p3pppp/1p6/2ppP3/3N4/2P5/PPP1QPPP/R1B1KB1R w KQkq -", {"e5e6"}},
    {"r1b2rk1/2q1b1pp/p2ppn2/1p6/3QP3/1BN1B3/PPP3PP/R4RK1 w - -", {"c3d5"}},
    {"2r3k1/pppR1pp1/4p3/4P1P1/5P2/1P4K1/P1P5/8 w - -", {"g5g6"}},
    {"1nk1r1r1/pp2n1pp/4p3/q2pPp1N/b1pP1P2/B1P2R2/2P1B1PP/R2Q2K1 w - -", {"f3g3"}},
    {"4b3/p3kp2/6p1/3pP2p/2pP1P2/4K1P1/P3N2P/8 w - -", {"f4f5"}},
    {"2kr1bnr/pbpq4/2n1pp2/3p3p/3P1P1B/2N2N1Q/PPP3PP/2KR1B1R w - -", {"f1b5"}},
    {"3rr1k1/pp3pp1/1qn2np1/8/3p4/PP1R1P2/2P1NQPP/R1B3K1 b - -", {"b6c5"}},
    {"2r1nrk1/p2q1ppp/bp1p4/n1pPp3/P1P1P3/2PBB1N1/4QPPP/R4RK1 w - -", {"f2f4"}},
    {"r3r1k1/ppqb1ppp/8/4p1NQ/8/2P5/PP3PPP/R3R1K1 b - -", {"d7f5"}},
    {"r2q1rk1/4bppp/p2p4/2pP4/3pP3/3Q4/PP1B1PPP/R3R1K1 w - -", {"b2b4"}},
    {"rnb2r1k/pp2p2p/2pp2p1/q2P1p2/8/1Pb2NP1/PB2PPBP/R2Q1RK1 w - -", {"d1e1"}},
    {"2r3k1/1p2q1pp/2b1pr2/p1pp4/6Q1/1P1PP1R1/P1PN2PP/5RK1 w - -", {"g4g7"}},
    {"r1bqkb1r/4npp1/p1p4p/1p1pP1B1/8/1B6/PPPN1PPP/R2Q1RK1 w kq -", {"d2e4"}},
    {"r2q1rk1/1ppnbppp/p2p1nb1/3Pp3/2P1P1P1/2N2N1P/PPB1QP2/R1B2RK1 b - -", {"h7h5"}},
    {"r1bq1rk1/pp2ppbp/2np2p1/2n5/P3PP2/N1P2N2/1PB3PP/R1B1QRK1 b - -", {"f7f5"}},
    {"3rr3/2pq2pk/p2p1pnp/8/2QBPP2/1P6/P5PP/4RRK1 b - -", {"d6d5"}},
    {"r4k2/pb2bp1r/1p1qp2p/3pNp2/3P1P2/2N3P1/PPP1Q2P/2KRR3 w - -", {"g3g4"}},
    {"3rn2k/ppb2rpp/2ppqp2/5N2/2P1P3/1P5Q/PB3PPP/3RR1K1 w - -", {"f5h6"}},
    {"2r2rk1/1bqnbpp1/1p1ppn1p/pP6/N1P1P3/P2B1N1P/1B2QPP1/R2R2K1 b - -", {"f6h5"}},
    {"r1bqk2r/pp2bppp/2p5/3pP3/P2Q1P2/2N1B3/1PP3PP/R4RK1 b kq -", {"c8f5", "f7f6"}},
    {"r2qnrnk/p2b2b1/1p1p2pp/2pPpp2/1PP1P3/PRNBB3/3QNPPP/5RK1 w - -", {"g2g3", "e4f5"}},
}};
// clang-format on

/// @brief Format a case's non-empty accepted moves as a space-separated string.
inline std::string AcceptedMovesString(const BkCase &tc)
{
    std::string out;
    for (std::string_view m : tc.moves_)
    {
        if (m.empty())
        {
            continue;
        }
        if (!out.empty())
        {
            out += ' ';
        }
        out += m;
    }
    return out;
}

} // namespace bk

/// @brief Run the Bratko-Kopec suite with NNUE evaluation.
/// @param argc Argument count. @param argv [net.bin] [depth].
/// @return Non-zero on any failure: net load error, a search returning no move, or any position whose best
///         move is not among its accepted moves (the suite must solve all 24).
int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        return test_report::Summary("Bratko-Kopec NNUE: skipped (no net supplied)");
    }
    const std::string net_path = argv[1];
    // The accepted-move sets belong to this net at this shape, so a build at another shape has nothing to
    // compare against — it rightly refuses the net. That shape's own gates are test_nnue_incremental's
    // synthetic net and tools/new-shape-signature.sh.
    if (!nnue::IsNetFileThisBuildsShape(net_path))
    {
        return test_report::Summary(
            std::format("Bratko-Kopec NNUE: skipped (net '{}' is not this build's NNUE shape)", net_path));
    }
    const int depth = argc > 2 ? std::atoi(argv[2]) : bk::kDefaultDepth;

    // Default to a deterministic single-threaded search so `make check` is reproducible (Lazy SMP makes
    // even a fixed-depth search non-deterministic — helper threads race through the shared TT). overwrite=0
    // so a caller can export PAWNSTAR_THREADS to run multi-threaded and sample that variation when
    // regenerating the accepted-move sets.
#if defined(_WIN32)
    if (std::getenv("PAWNSTAR_THREADS") == nullptr) // overwrite=0: respect a caller-set value
    {
        _putenv_s("PAWNSTAR_THREADS", "1");
    }
#else
    setenv("PAWNSTAR_THREADS", "1", 0);
#endif

    auto game_ptr = std::make_unique<Game>();

    Game &game = *game_ptr;
    if (!game.nnue_network_.Load(net_path) || !game.nnue_network_.loaded_)
    {
        test_report::Check(false, std::format("Bratko-Kopec NNUE: failed to load net '{}'", net_path));
        return test_report::ExitCode();
    }

    using Clock    = std::chrono::steady_clock;
    auto t_overall = Clock::now();

    for (int i = 0; i < (int)bk::kCases.size(); ++i)
    {
        const bk::BkCase &tc = bk::kCases[i];

        game.SetPosition(tc.fen_);
        game.book_.Free();
        game.time_control_.clock_type_ = ChessClock::kFixedDepth;
        game.time_control_.depth_      = depth;
        DebugXClear();
        auto t_start    = Clock::now();
        Move m          = game.SearchRootNode();
        auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - t_start).count();
        DebugXWrite();

        const std::string got_move = m.ToString();
        const bool        found    = m != Move::None();
        const bool        match = found && std::find(tc.moves_.begin(), tc.moves_.end(), got_move) != tc.moves_.end();

        // A position is solved when a legal move comes back and it is among that position's accepted moves.
        test_report::Check(match, std::format("BK pos{:02d} got={}/{:<6} accepted=[{}] {}ms {}", i + 1, got_move,
                                              m.score(), bk::AcceptedMovesString(tc), elapsed_ms, tc.fen_));
    }

    auto      total_ms = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - t_overall).count();
    const int total    = (int)bk::kCases.size();
    return test_report::Summary(std::format("BRATKO-KOPEC NNUE: solved {}/{} at depth {} ({}ms, net {})",
                                            total - test_report::failures, total, depth, total_ms, net_path));
}

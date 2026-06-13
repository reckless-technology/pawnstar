/// @file bratko_kopec.cpp Bratko-Kopec search quality test suite.
///
/// Each position carries baked single-threaded reference data for search depths 8..12: the score (in
/// centipawns) and the best move(s) found by a deterministic single-threaded search. Under Lazy SMP the
/// parallel search is non-deterministic and may legitimately pick a different best move, so the pass
/// criterion is on *score*, not move identity.
///
/// Because Lazy SMP helper threads deepen the shared transposition table ahead of the main thread, a search
/// nominally at depth D behaves like a single-threaded search somewhere along the depth D..max trajectory.
/// A position therefore passes when its returned score lies within the *span* of single-threaded reference
/// scores for depths >= the requested one, widened by @ref kScoreTolerance centipawns at each end. (Matching
/// only the discrete per-depth points leaves gaps for positions whose score swings by more than 2*tolerance
/// between consecutive depths, where the parallel score legitimately lands mid-swing.) Mate scores match any
/// mate of the same sign. The reference move(s) are reported for information only.
///
/// Regenerate the reference data after evaluation/search changes with a single-threaded run:
///   PAWNSTAR_THREADS=1 ./build/test_bratko_kopec 12
/// and parse the per-depth `info depth D score cp S ... pv M` lines (last line per depth wins).

#include "chess_clock.h"
#include "constants.h"
#include "debug_hashtable.h"
#include "game.h"
#include "position.h"
#include "search.h"

#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <format>
#include <iostream>
#include <string_view>

namespace
{

/// @brief Lowest and highest reference depths held per position (inclusive).
constexpr int kMinDepth = 8;
constexpr int kMaxDepth = 12;
/// @brief Number of reference depths per position (8, 9, 10, 11, 12).
constexpr int kDepthCount = kMaxDepth - kMinDepth + 1;
/// @brief Allowed deviation (centipawns) of a run score from the single-threaded reference score.
constexpr int kScoreTolerance = 50;

/// @brief Single-threaded reference result at one search depth.
struct BkDepth
{
    int                             score; ///< Reference score in centipawns.
    std::array<std::string_view, 4> moves; ///< Reference best move(s); empty entries ignored.
};

/// @brief A Bratko-Kopec position and its per-depth single-threaded reference data.
struct BkCase
{
    std::string_view                 fen;    ///< Position FEN.
    std::array<BkDepth, kDepthCount> depths; ///< Reference data for depths 8..12.
};

// clang-format off
/// @brief The 24 Bratko-Kopec test positions with baked single-threaded reference scores/moves.
static const std::array<BkCase, 24> bk_cases{{
    {"1k1r4/pp1b1R2/3q2pp/4p3/2B5/4Q3/PPP2B2/2K5 b - -", // forced mate found at depth 4 (search stops early)
     {{{  9995, {"d6d1"}}, {  9995, {"d6d1"}}, {  9995, {"d6d1"}}, {  9995, {"d6d1"}}, {  9995, {"d6d1"}}}}},
    {"3r1k2/4npp1/1ppr3p/p6P/P2PPPP1/1NR5/5K2/2R5 w - -",
     {{{    45, {"e4e5"}}, {    65, {"e4e5"}}, {    50, {"e4e5"}}, {    40, {"e4e5"}}, {    40, {"e4e5"}}}}},
    {"2q1rr1k/3bbnnp/p2p1pp1/2pPp3/PpP1P1P1/1P2BNNP/2BQ1PRK/7R b - -",
     {{{     0, {"e8d8"}}, {    -5, {"e8d8"}}, {    -5, {"e8d8"}}, {    -5, {"e8d8"}}, {    -5, {"e8d8"}}}}},
    {"rnbqkb1r/p3pppp/1p6/2ppP3/3N4/2P5/PPP1QPPP/R1B1KB1R w KQkq -",
     {{{    30, {"e5e6"}}, {    35, {"e5e6"}}, {    60, {"e5e6"}}, {    35, {"e5e6"}}, {    65, {"e5e6"}}}}},
    {"r1b2rk1/2q1b1pp/p2ppn2/1p6/3QP3/1BN1B3/PPP3PP/R4RK1 w - -",
     {{{    75, {"a2a4"}}, {   130, {"c3d5"}}, {   150, {"c3d5"}}, {    95, {"c3d5"}}, {   115, {"c3d5"}}}}},
    {"2r3k1/pppR1pp1/4p3/4P1P1/5P2/1P4K1/P1P5/8 w - -",
     {{{    95, {"g5g6"}}, {   110, {"g5g6"}}, {   120, {"g5g6"}}, {   120, {"g5g6"}}, {   130, {"g5g6"}}}}},
    {"1nk1r1r1/pp2n1pp/4p3/q2pPp1N/b1pP1P2/B1P2R2/2P1B1PP/R2Q2K1 w - -",
     {{{     5, {"h5f6"}}, {    35, {"h5f6"}}, {    45, {"h5f6"}}, {    35, {"h5f6"}}, {    50, {"h5f6"}}}}},
    {"4b3/p3kp2/6p1/3pP2p/2pP1P2/4K1P1/P3N2P/8 w - -",
     {{{    40, {"f4f5"}}, {    50, {"f4f5"}}, {    40, {"f4f5"}}, {    50, {"f4f5"}}, {    65, {"f4f5"}}}}},
    {"2kr1bnr/pbpq4/2n1pp2/3p3p/3P1P1B/2N2N1Q/PPP3PP/2KR1B1R w - -",
     {{{   220, {"d1e1"}}, {   250, {"f1d3"}}, {   245, {"f1d3"}}, {   245, {"f1d3"}}, {   260, {"f1d3"}}}}},
    {"3rr1k1/pp3pp1/1qn2np1/8/3p4/PP1R1P2/2P1NQPP/R1B3K1 b - -",
     {{{    80, {"c6e5"}}, {   285, {"c6e5"}}, {   260, {"c6e5"}}, {   270, {"c6e5"}}, {   265, {"c6e5"}}}}},
    {"2r1nrk1/p2q1ppp/bp1p4/n1pPp3/P1P1P3/2PBB1N1/4QPPP/R4RK1 w - -",
     {{{    65, {"g3f5"}}, {    70, {"g3f5"}}, {    75, {"g3f5"}}, {    80, {"g3f5"}}, {    75, {"g3f5"}}}}},
    {"r3r1k1/ppqb1ppp/8/4p1NQ/8/2P5/PP3PPP/R3R1K1 b - -",
     {{{     5, {"d7f5"}}, {     5, {"d7f5"}}, {    10, {"d7f5"}}, {    10, {"d7f5"}}, {    15, {"d7f5"}}}}},
    {"r2q1rk1/4bppp/p2p4/2pP4/3pP3/3Q4/PP1B1PPP/R3R1K1 w - -",
     {{{    50, {"a1c1"}}, {    50, {"a1c1"}}, {    50, {"a1c1"}}, {    50, {"a1c1"}}, {    45, {"a1c1"}}}}},
    {"rnb2r1k/pp2p2p/2pp2p1/q2P1p2/8/1Pb2NP1/PB2PPBP/R2Q1RK1 w - -",
     {{{   355, {"d1e1"}}, {   360, {"d1e1"}}, {   465, {"d1d2"}}, {   480, {"d1d2"}}, {   495, {"d1d2"}}}}},
    {"2r3k1/1p2q1pp/2b1pr2/p1pp4/6Q1/1P1PP1R1/P1PN2PP/5RK1 w - -",
     {{{    60, {"g4g7"}}, {    50, {"g4g7"}}, {    50, {"g4g7"}}, {    50, {"g4g7"}}, {    55, {"g4g7"}}}}},
    {"r1bqkb1r/4npp1/p1p4p/1p1pP1B1/8/1B6/PPPN1PPP/R2Q1RK1 w kq -",
     {{{   135, {"d2e4"}}, {   125, {"d2e4"}}, {   100, {"d2e4"}}, {    95, {"d2e4"}}, {   125, {"d2e4"}}}}},
    {"r2q1rk1/1ppnbppp/p2p1nb1/3Pp3/2P1P1P1/2N2N1P/PPB1QP2/R1B2RK1 b - -",
     {{{   -30, {"c7c5"}}, {   -35, {"d7c5"}}, {   -45, {"d7c5"}}, {   -35, {"d7c5"}}, {   -40, {"d7c5"}}}}},
    {"r1bq1rk1/pp2ppbp/2np2p1/2n5/P3PP2/N1P2N2/1PB3PP/R1B1QRK1 b - -",
     {{{    25, {"c5b3"}}, {     5, {"c5b3"}}, {     0, {"c5b3"}}, {    10, {"f7f5"}}, {     0, {"f7f5"}}}}},
    {"3rr3/2pq2pk/p2p1pnp/8/2QBPP2/1P6/P5PP/4RRK1 b - -",
     {{{   -65, {"e8e4"}}, {   -65, {"e8e4"}}, {   -55, {"e8e4"}}, {   -50, {"e8e4"}}, {   -50, {"e8e4"}}}}},
    {"r4k2/pb2bp1r/1p1qp2p/3pNp2/3P1P2/2N3P1/PPP1Q2P/2KRR3 w - -",
     {{{    50, {"c3b5"}}, {    50, {"e1g1"}}, {    45, {"e1g1"}}, {    45, {"e1g1"}}, {    40, {"e2h5"}}}}},
    {"3rn2k/ppb2rpp/2ppqp2/5N2/2P1P3/1P5Q/PB3PPP/3RR1K1 w - -",
     {{{   235, {"f5h6"}}, {   160, {"f5h6"}}, {   175, {"f5h6"}}, {   165, {"f5h6"}}, {   165, {"f5h6"}}}}},
    {"2r2rk1/1bqnbpp1/1p1ppn1p/pP6/N1P1P3/P2B1N1P/1B2QPP1/R2R2K1 b - -",
     {{{    70, {"b7e4"}}, {    70, {"b7e4"}}, {    45, {"b7e4"}}, {    70, {"b7e4"}}, {    70, {"b7e4"}}}}},
    {"r1bqk2r/pp2bppp/2p5/3pP3/P2Q1P2/2N1B3/1PP3PP/R4RK1 b kq -",
     {{{    35, {"c8f5"}}, {    40, {"c8f5"}}, {    35, {"c8f5"}}, {    40, {"c8f5"}}, {    30, {"c8f5"}}}}},
    {"r2qnrnk/p2b2b1/1p1p2pp/2pPpp2/1PP1P3/PRNBB3/3QNPPP/5RK1 w - -",
     {{{   120, {"e4f5"}}, {   130, {"e4f5"}}, {   125, {"e4f5"}}, {   130, {"e4f5"}}, {   125, {"e4f5"}}}}},
}};
// clang-format on

/// @brief Format the non-empty reference moves of @p bd as a space-separated string.
std::string ReferenceMoves(const BkDepth &bd)
{
    std::string out;
    for (std::string_view m : bd.moves)
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

} // namespace

/// @brief Run the Bratko-Kopec search-quality suite.
/// @param argc Argument count. @param argv Arguments (optional search depth 8..12, default 8).
/// @return Non-zero if any position's score deviated from its single-threaded reference by more than the
///         tolerance.
int main(int argc, char *argv[])
{
    int depth = kMinDepth;
    if (argc > 1)
    {
        depth = std::atoi(argv[1]);
    }
    if (depth < kMinDepth || depth > kMaxDepth)
    {
        std::cerr << std::format("depth must be in [{}, {}] (reference data range)\n", kMinDepth, kMaxDepth);
        return 2;
    }
    const int depth_index = depth - kMinDepth;

    using Clock    = std::chrono::steady_clock;
    int  failures  = 0;
    auto t_overall = Clock::now();

    for (int i = 0; i < (int)bk_cases.size(); ++i)
    {
        const auto    &tc        = bk_cases[i];
        const BkDepth &reference = tc.depths[depth_index]; // primary reference (same depth) — reported.

        Game game;
        game.NewGame(tc.fen);
        game.book.Free();
        game.time_control.clock_type = ChessClock::kFixedDepth;
        game.time_control.depth      = depth;
        DebugXClear();
        auto t_start    = Clock::now();
        Move m          = SearchRootNode(game);
        auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - t_start).count();
        DebugXWrite();

        const int   got_score = m.score();
        std::string got_move  = m.ToString();

        // Accept the score if it lies within the span of single-threaded references for depths >= the
        // requested one (Lazy SMP helpers run ahead, so a nominal depth-D search lands mid-trajectory),
        // widened by the tolerance at each end. Mate scores are compared by class (both winning / both losing
        // mates) since a parallel search may report a mate at a different ply.
        const bool winning_mate      = got_score > kWinThreshold;
        const bool losing_mate       = got_score < kLoseThreshold;
        int        ref_lo            = tc.depths[depth_index].score;
        int        ref_hi            = tc.depths[depth_index].score;
        bool       ref_has_win_mate  = false;
        bool       ref_has_lose_mate = false;
        for (int d = depth_index; d < kDepthCount; ++d)
        {
            const int ref     = tc.depths[d].score;
            ref_lo            = std::min(ref_lo, ref);
            ref_hi            = std::max(ref_hi, ref);
            ref_has_win_mate  = ref_has_win_mate || ref > kWinThreshold;
            ref_has_lose_mate = ref_has_lose_mate || ref < kLoseThreshold;
        }
        const bool pass = (winning_mate && ref_has_win_mate) || (losing_mate && ref_has_lose_mate) ||
                          (got_score >= ref_lo - kScoreTolerance && got_score <= ref_hi + kScoreTolerance);

        if (!pass)
        {
            ++failures;
        }

        std::cout << std::format("[{}] pos{:02d} got={}/{:<6} ref={:<6} (±{}cp, depths>={}) refmoves=[{}] {}ms {}\n",
                                 pass ? "PASS" : "FAIL", i + 1, got_move, got_score, reference.score, kScoreTolerance,
                                 depth, ReferenceMoves(reference), elapsed_ms, tc.fen);
    }

    auto total_ms = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - t_overall).count();
    std::cout << std::format(
        "\n{}/24 passed (score within single-threaded depth>={} reference span ±{}cp)  total {}ms\n", 24 - failures,
        depth, kScoreTolerance, total_ms);
    return failures > 0 ? 1 : 0;
}

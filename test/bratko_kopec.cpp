/// @file bratko_kopec.cpp Bratko-Kopec search quality test suite matching Go search_test.go.

#include "chess_clock.h"
#include "debug_hashtable.h"
#include "game.h"
#include "position.h"
#include "search.h"

#include <array>
#include <chrono>
#include <cstdlib>
#include <format>
#include <iostream>
#include <string_view>

/// @brief A single Bratko-Kopec position and its set of accepted best moves.
struct BkCase
{
    std::string_view                fen;      ///< Position FEN.
    std::array<std::string_view, 6> accepted; ///< Up to 6 accepted best moves; empty strings ignored.
};

// clang-format off
/// @brief The 24 Bratko-Kopec test positions with their accepted moves.
static const std::array<BkCase, 24> bk_cases{{
    {"1k1r4/pp1b1R2/3q2pp/4p3/2B5/4Q3/PPP2B2/2K5 b - -",                    {"d6d1", "", ""}},
    {"3r1k2/4npp1/1ppr3p/p6P/P2PPPP1/1NR5/5K2/2R5 w - -",                   {"e4e5", "", ""}},
    {"2q1rr1k/3bbnnp/p2p1pp1/2pPp3/PpP1P1P1/1P2BNNP/2BQ1PRK/7R b - -",      {"h8g8", "e8d8", ""}},
    {"rnbqkb1r/p3pppp/1p6/2ppP3/3N4/2P5/PPP1QPPP/R1B1KB1R w KQkq -",        {"d4f3", "e5e6", ""}},
    {"r1b2rk1/2q1b1pp/p2ppn2/1p6/3QP3/1BN1B3/PPP3PP/R4RK1 w - -",           {"a2a4", "c3d5", ""}},
    {"2r3k1/pppR1pp1/4p3/4P1P1/5P2/1P4K1/P1P5/8 w - -",                     {"g5g6", "", ""}},
    {"1nk1r1r1/pp2n1pp/4p3/q2pPp1N/b1pP1P2/B1P2R2/2P1B1PP/R2Q2K1 w - -",    {"h5f6", "", ""}},
    {"4b3/p3kp2/6p1/3pP2p/2pP1P2/4K1P1/P3N2P/8 w - -",                      {"f4f5", "", ""}},
    {"2kr1bnr/pbpq4/2n1pp2/3p3p/3P1P1B/2N2N1Q/PPP3PP/2KR1B1R w - -",        {"d1e1", "f1d3", ""}},
    {"3rr1k1/pp3pp1/1qn2np1/8/3p4/PP1R1P2/2P1NQPP/R1B3K1 b - -",            {"c6e5", "", ""}},
    {"2r1nrk1/p2q1ppp/bp1p4/n1pPp3/P1P1P3/2PBB1N1/4QPPP/R4RK1 w - -",       {"g3f5", "a1a2", "f2f3"}},
    {"r3r1k1/ppqb1ppp/8/4p1NQ/8/2P5/PP3PPP/R3R1K1 b - -",                   {"d7f5", "", ""}},
    {"r2q1rk1/4bppp/p2p4/2pP4/3pP3/3Q4/PP1B1PPP/R3R1K1 w - -",              {"a1c1", "b2b4", ""}},
    {"rnb2r1k/pp2p2p/2pp2p1/q2P1p2/8/1Pb2NP1/PB2PPBP/R2Q1RK1 w - -",        {"d1e1", "d1d2", ""}},
    {"2r3k1/1p2q1pp/2b1pr2/p1pp4/6Q1/1P1PP1R1/P1PN2PP/5RK1 w - -",          {"g4g7", "", ""}},
    {"r1bqkb1r/4npp1/p1p4p/1p1pP1B1/8/1B6/PPPN1PPP/R2Q1RK1 w kq -",         {"d2e4", "", ""}},
    {"r2q1rk1/1ppnbppp/p2p1nb1/3Pp3/2P1P1P1/2N2N1P/PPB1QP2/R1B2RK1 b - -",  {"c7c5", "d7c5", "h7h6", "a8c8", "a8b8", "c7c6"}},
    {"r1bq1rk1/pp2ppbp/2np2p1/2n5/P3PP2/N1P2N2/1PB3PP/R1B1QRK1 b - -",      {"c5b3", "", ""}},
    {"3rr3/2pq2pk/p2p1pnp/8/2QBPP2/1P6/P5PP/4RRK1 b - -",                   {"e8e4", "", ""}},
    {"r4k2/pb2bp1r/1p1qp2p/3pNp2/3P1P2/2N3P1/PPP1Q2P/2KRR3 w - -",          {"c3b5", "e1g1", "e2d3"}},
    {"3rn2k/ppb2rpp/2ppqp2/5N2/2P1P3/1P5Q/PB3PPP/3RR1K1 w - -",             {"f5h6", "", ""}},
    {"2r2rk1/1bqnbpp1/1p1ppn1p/pP6/N1P1P3/P2B1N1P/1B2QPP1/R2R2K1 b - -",    {"b7e4", "", ""}},
    {"r1bqk2r/pp2bppp/2p5/3pP3/P2Q1P2/2N1B3/1PP3PP/R4RK1 b kq -",           {"c8f5", "", ""}},
    {"r2qnrnk/p2b2b1/1p1p2pp/2pPpp2/1PP1P3/PRNBB3/3QNPPP/5RK1 w - -",       {"e4f5", "", ""}},
}};
// clang-format on

/// @brief Run the Bratko-Kopec search-quality suite.
/// @param argc Argument count. @param argv Arguments (optional search depth, default 8).
/// @return Non-zero if any position returned a non-accepted move.
int main(int argc, char *argv[])
{
    int depth = 8;
    if (argc > 1)
        depth = std::atoi(argv[1]);

    using Clock    = std::chrono::steady_clock;
    int  failures  = 0;
    auto t_overall = Clock::now();

    for (int i = 0; i < (int)bk_cases.size(); ++i)
    {
        const auto &tc = bk_cases[i];
        Game        game;
        game.NewGame(tc.fen);
        game.book.Free();
        game.time_control.clock_type = ChessClock::kFixedDepth;
        game.time_control.depth      = depth;
        DebugXClear();
        auto t_start    = Clock::now();
        Move m          = SearchRootNode(game);
        auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - t_start).count();
        DebugXWrite();
        std::string got = m.ToString();

        bool pass = false;
        for (std::string_view a : tc.accepted)
        {
            if (!a.empty() && got == a)
            {
                pass = true;
                break;
            }
        }

        if (!pass)
            ++failures;

        std::string valid;
        for (std::string_view a : tc.accepted)
        {
            if (!a.empty())
            {
                if (!valid.empty())
                    valid += ' ';
                valid += a;
            }
        }
        std::cout << std::format("[{}] pos{:02d} got={} accepted=[{}] {}ms {}\n", pass ? "PASS" : "FAIL", i + 1, got,
                                 valid, elapsed_ms, tc.fen);
    }

    auto total_ms = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - t_overall).count();
    std::cout << std::format("\n{}/24 passed  total {}ms\n", 24 - failures, total_ms);
    return failures > 0 ? 1 : 0;
}

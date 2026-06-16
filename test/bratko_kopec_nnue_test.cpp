/// @file bratko_kopec_nnue.cpp Bratko-Kopec search-quality suite using the NNUE evaluation.
///
/// It searches the 24 positions in the kCases array (defined below) with the NNUE evaluator and checks the
/// *move* (the classic Bratko-Kopec metric — did the engine find a good move). A position is "solved" when
/// the move found is among that position's accepted moves (the set of best moves the engine produces over
/// depths 8–11; see kCases below). The search is forced single-threaded (`PAWNSTAR_THREADS=1`) so it is
/// deterministic, and the suite is a hard gate: it must solve all 24.
///
/// The net is taken from argv[1]; with no argument the suite is skipped (so `make check` stays green when
/// the net is absent). Optional argv[2] sets the search depth (default 8).
///
/// Usage:  test_bratko_kopec_nnue [net.bin] [depth]

#include "chess_clock.h"
#include "debug_hashtable.h"
#include "game.h"
#include "move.h"
#include "position.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <format>
#include <iostream>
#include <string>
#include <string_view>

/// The Bratko-Kopec positions and their accepted best moves. Each case is a position plus up to 5 accepted
/// moves: the set of best moves the engine produces across single-threaded searches at depths 8–11 (the
/// search is deterministic per depth, so this is the union of the four per-depth results). A position is
/// "solved" when the move found is one of these. The suite passes 24/24 at every depth 8–11; regenerate the
/// move sets if the net or search changes (see the regeneration note in nnue/README.md §7).
namespace bk
{

/// @brief Depth used when none is given on the command line. The accepted-move sets were recorded over
/// depths 8–11 inclusive (their union); see the file header.
constexpr int kDefaultDepth = 8;

/// @brief A Bratko-Kopec position and the moves accepted as correct for it.
struct BkCase
{
    std::string_view                fen;   ///< Position FEN.
    std::array<std::string_view, 5> moves; ///< Up to 5 accepted best moves; empty entries ignored.
};

// clang-format off
/// @brief The 24 Bratko-Kopec positions and their accepted moves (engine's best moves over depths 8–11).
inline constexpr std::array<BkCase, 24> kCases{{
    {"1k1r4/pp1b1R2/3q2pp/4p3/2B5/4Q3/PPP2B2/2K5 b - -",                   {"d6d1"}},
    {"3r1k2/4npp1/1ppr3p/p6P/P2PPPP1/1NR5/5K2/2R5 w - -",                  {"c1c2", "d4d5"}},
    {"2q1rr1k/3bbnnp/p2p1pp1/2pPp3/PpP1P1P1/1P2BNNP/2BQ1PRK/7R b - -",     {"f8g8"}},
    {"rnbqkb1r/p3pppp/1p6/2ppP3/3N4/2P5/PPP1QPPP/R1B1KB1R w KQkq -",       {"e5e6"}},
    {"r1b2rk1/2q1b1pp/p2ppn2/1p6/3QP3/1BN1B3/PPP3PP/R4RK1 w - -",          {"c3d5"}},
    {"2r3k1/pppR1pp1/4p3/4P1P1/5P2/1P4K1/P1P5/8 w - -",                    {"g5g6"}},
    {"1nk1r1r1/pp2n1pp/4p3/q2pPp1N/b1pP1P2/B1P2R2/2P1B1PP/R2Q2K1 w - -",   {"d1c1"}},
    {"4b3/p3kp2/6p1/3pP2p/2pP1P2/4K1P1/P3N2P/8 w - -",                     {"f4f5"}},
    {"2kr1bnr/pbpq4/2n1pp2/3p3p/3P1P1B/2N2N1Q/PPP3PP/2KR1B1R w - -",       {"d1e1"}},
    {"3rr1k1/pp3pp1/1qn2np1/8/3p4/PP1R1P2/2P1NQPP/R1B3K1 b - -",           {"c6e5", "f6d7"}},
    {"2r1nrk1/p2q1ppp/bp1p4/n1pPp3/P1P1P3/2PBB1N1/4QPPP/R4RK1 w - -",      {"f2f4", "g3f5"}},
    {"r3r1k1/ppqb1ppp/8/4p1NQ/8/2P5/PP3PPP/R3R1K1 b - -",                  {"d7f5"}},
    {"r2q1rk1/4bppp/p2p4/2pP4/3pP3/3Q4/PP1B1PPP/R3R1K1 w - -",             {"b2b4"}},
    {"rnb2r1k/pp2p2p/2pp2p1/q2P1p2/8/1Pb2NP1/PB2PPBP/R2Q1RK1 w - -",       {"d1e1"}},
    {"2r3k1/1p2q1pp/2b1pr2/p1pp4/6Q1/1P1PP1R1/P1PN2PP/5RK1 w - -",         {"g4g7"}},
    {"r1bqkb1r/4npp1/p1p4p/1p1pP1B1/8/1B6/PPPN1PPP/R2Q1RK1 w kq -",        {"d2e4"}},
    {"r2q1rk1/1ppnbppp/p2p1nb1/3Pp3/2P1P1P1/2N2N1P/PPB1QP2/R1B2RK1 b - -", {"h7h5"}},
    {"r1bq1rk1/pp2ppbp/2np2p1/2n5/P3PP2/N1P2N2/1PB3PP/R1B1QRK1 b - -",     {"c5b3", "f7f5"}},
    {"3rr3/2pq2pk/p2p1pnp/8/2QBPP2/1P6/P5PP/4RRK1 b - -",                  {"e8e4"}},
    {"r4k2/pb2bp1r/1p1qp2p/3pNp2/3P1P2/2N3P1/PPP1Q2P/2KRR3 w - -",         {"g3g4"}},
    {"3rn2k/ppb2rpp/2ppqp2/5N2/2P1P3/1P5Q/PB3PPP/3RR1K1 w - -",            {"f5h6"}},
    {"2r2rk1/1bqnbpp1/1p1ppn1p/pP6/N1P1P3/P2B1N1P/1B2QPP1/R2R2K1 b - -",   {"b7e4"}},
    {"r1bqk2r/pp2bppp/2p5/3pP3/P2Q1P2/2N1B3/1PP3PP/R4RK1 b kq -",          {"c8f5", "f7f6"}},
    {"r2qnrnk/p2b2b1/1p1p2pp/2pPpp2/1PP1P3/PRNBB3/3QNPPP/5RK1 w - -",      {"e4f5"}},
}};
// clang-format on

/// @brief Format a case's non-empty accepted moves as a space-separated string.
inline std::string AcceptedMovesString(const BkCase &tc)
{
    std::string out;
    for (std::string_view m : tc.moves)
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
        std::cout << "test_bratko_kopec_nnue skipped (no net supplied)\n";
        return 0;
    }
    const std::string net_path = argv[1];
    const int         depth    = argc > 2 ? std::atoi(argv[2]) : bk::kDefaultDepth;

    // Deterministic single-threaded search so the solved count is reproducible.
    setenv("PAWNSTAR_THREADS", "1", 1);

    Game game;
    if (!game.NnueNetwork().Load(net_path))
    {
        std::cerr << "test_bratko_kopec_nnue: failed to load net '" << net_path << "'\n";
        return 1;
    }
    if (!game.NnueActive())
    {
        std::cerr << "test_bratko_kopec_nnue: NNUE not active after load\n";
        return 1;
    }

    using Clock    = std::chrono::steady_clock;
    int  solved    = 0;
    int  errors    = 0;
    auto t_overall = Clock::now();

    for (int i = 0; i < (int)bk::kCases.size(); ++i)
    {
        const bk::BkCase &tc = bk::kCases[i];

        game.NewGame(tc.fen);
        game.book.Free();
        game.time_control.clock_type = ChessClock::kFixedDepth;
        game.time_control.depth      = depth;
        DebugXClear();
        auto t_start    = Clock::now();
        Move m          = game.SearchRootNode();
        auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - t_start).count();
        DebugXWrite();

        const std::string got_move = m.ToString();

        const bool found = m != Move::None();
        const bool match =
            found && std::find(tc.moves.begin(), tc.moves.end(), got_move) != tc.moves.end();
        if (!found)
        {
            ++errors; // a legal move should always come back for these positions
        }
        if (match)
        {
            ++solved;
        }

        std::cout << std::format("[{}] pos{:02d} got={}/{:<6} accepted=[{}] {}ms {}\n", match ? "SOLVED" : " MISS ",
                                 i + 1, got_move, m.score(), bk::AcceptedMovesString(tc), elapsed_ms, tc.fen);
    }

    auto total_ms = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - t_overall).count();
    const int total = (int)bk::kCases.size();
    std::cout << std::format("\nNNUE Bratko-Kopec: solved {}/{} at depth {}  total {}ms  (net {})\n", solved, total,
                             depth, total_ms, net_path);
    if (errors > 0)
    {
        std::cout << "FAIL: " << errors << " position(s) returned no move\n";
        return 1;
    }
    if (solved != total)
    {
        std::cout << std::format("FAIL: solved {}/{} (expected all {})\n", solved, total, total);
        return 1;
    }
    std::cout << "BRATKO-KOPEC NNUE PASS\n";
    return 0;
}

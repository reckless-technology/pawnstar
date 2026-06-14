#pragma once
/// @file bratko_kopec_cases.h Shared Bratko-Kopec test positions and per-depth single-threaded reference data.
///
/// Used by both the handcrafted-eval suite (test/bratko_kopec.cpp, which checks the parallel *score*
/// against these references) and the NNUE suite (test/bratko_kopec_nnue.cpp, which searches with the net
/// and checks the *move*, since NNUE scores differ from these handcrafted references).

#include <array>
#include <string>
#include <string_view>

namespace bk
{

/// @brief Lowest and highest reference depths held per position (inclusive).
constexpr int kMinDepth = 8;
constexpr int kMaxDepth = 12;
/// @brief Number of reference depths per position (8, 9, 10, 11, 12).
constexpr int kDepthCount = kMaxDepth - kMinDepth + 1;

/// @brief Single-threaded reference result at one search depth.
struct BkDepth
{
    int                             score; ///< Reference score in centipawns (handcrafted eval).
    std::array<std::string_view, 4> moves; ///< Reference best move(s); empty entries ignored.
};

/// @brief A Bratko-Kopec position and its per-depth single-threaded reference data.
struct BkCase
{
    std::string_view                 fen;    ///< Position FEN.
    std::array<BkDepth, kDepthCount> depths; ///< Reference data for depths 8..12.
};

// clang-format off
/// @brief The 24 Bratko-Kopec test positions with baked single-threaded (handcrafted-eval) reference scores/moves.
inline constexpr std::array<BkCase, 24> kCases{{
    {"1k1r4/pp1b1R2/3q2pp/4p3/2B5/4Q3/PPP2B2/2K5 b - -",                   {{{  9995, {"d6d1"}}, {  9995, {"d6d1"}}, {  9995, {"d6d1"}}, {  9995, {"d6d1"}}, {  9995, {"d6d1"}}}}}, // forced mate found at depth 4 (search stops early)
    {"3r1k2/4npp1/1ppr3p/p6P/P2PPPP1/1NR5/5K2/2R5 w - -",                  {{{    25, {"e4e5", "f2g1"}}, {    65, {"e4e5"}}, {    45, {"e4e5"}}, {    40, {"e4e5"}}, {    40, {"e4e5"}}}}},
    {"2q1rr1k/3bbnnp/p2p1pp1/2pPp3/PpP1P1P1/1P2BNNP/2BQ1PRK/7R b - -",     {{{     0, {"e7d8", "e8d8", "h8g8"}}, {     0, {"h8g8"}}, {     5, {"e7d8"}}, {     5, {"e7d8"}}, {     0, {"h8g8"}}}}},
    {"rnbqkb1r/p3pppp/1p6/2ppP3/3N4/2P5/PPP1QPPP/R1B1KB1R w KQkq -",       {{{    45, {"d4f3", "e5e6"}}, {    40, {"d4f3"}}, {    65, {"e5e6"}}, {    35, {"e5e6"}}, {    65, {"e5e6"}}}}},
    {"r1b2rk1/2q1b1pp/p2ppn2/1p6/3QP3/1BN1B3/PPP3PP/R4RK1 w - -",          {{{    75, {"c3d5"}}, {    75, {"a2a4"}}, {   145, {"c3d5"}}, {    95, {"c3d5"}}, {   115, {"c3d5"}}}}},
    {"2r3k1/pppR1pp1/4p3/4P1P1/5P2/1P4K1/P1P5/8 w - -",                    {{{    60, {"g3f3", "g5g6"}}, {   105, {"g5g6"}}, {   120, {"g5g6"}}, {   120, {"g5g6"}}, {   125, {"g5g6"}}}}},
    {"1nk1r1r1/pp2n1pp/4p3/q2pPp1N/b1pP1P2/B1P2R2/2P1B1PP/R2Q2K1 w - -",   {{{     5, {"a3b4", "f3g3", "h5f6"}}, {    30, {"h5f6"}}, {    45, {"h5f6"}}, {    35, {"h5f6"}}, {    50, {"h5f6"}}}}},
    {"4b3/p3kp2/6p1/3pP2p/2pP1P2/4K1P1/P3N2P/8 w - -",                     {{{    40, {"f4f5"}}, {    50, {"f4f5"}}, {    40, {"f4f5"}}, {    50, {"f4f5"}}, {    60, {"f4f5"}}}}},
    {"2kr1bnr/pbpq4/2n1pp2/3p3p/3P1P1B/2N2N1Q/PPP3PP/2KR1B1R w - -",       {{{   220, {"d1e1", "f1d3"}}, {   225, {"f1d3"}}, {   240, {"f1d3"}}, {   225, {"f1d3"}}, {   225, {"f1d3"}}}}},
    {"3rr1k1/pp3pp1/1qn2np1/8/3p4/PP1R1P2/2P1NQPP/R1B3K1 b - -",           {{{     0, {"b6c5", "c6e5", "f6d5"}}, {   140, {"c6e5"}}, {   260, {"c6e5"}}, {   270, {"c6e5"}}, {   265, {"c6e5"}}}}},
    {"2r1nrk1/p2q1ppp/bp1p4/n1pPp3/P1P1P3/2PBB1N1/4QPPP/R4RK1 w - -",      {{{    65, {"a1a2", "f2f3", "g3f5"}}, {    70, {"g3f5"}}, {    75, {"a1a2"}}, {    75, {"a1a2"}}, {    75, {"a1a2"}}}}},
    {"r3r1k1/ppqb1ppp/8/4p1NQ/8/2P5/PP3PPP/R3R1K1 b - -",                  {{{     5, {"d7f5"}}, {     5, {"d7f5"}}, {    10, {"d7f5"}}, {    10, {"d7f5"}}, {    15, {"d7f5"}}}}},
    {"r2q1rk1/4bppp/p2p4/2pP4/3pP3/3Q4/PP1B1PPP/R3R1K1 w - -",             {{{    50, {"a1c1", "b2b4"}}, {    50, {"a1c1"}}, {    45, {"a1c1"}}, {    45, {"a1c1"}}, {    45, {"a1c1"}}}}},
    {"rnb2r1k/pp2p2p/2pp2p1/q2P1p2/8/1Pb2NP1/PB2PPBP/R2Q1RK1 w - -",       {{{   360, {"d1d2", "d1e1"}}, {   365, {"d1e1"}}, {   375, {"d1e1"}}, {   470, {"d1d2"}}, {   470, {"d1d2"}}}}},
    {"2r3k1/1p2q1pp/2b1pr2/p1pp4/6Q1/1P1PP1R1/P1PN2PP/5RK1 w - -",         {{{    60, {"g4g7"}}, {    55, {"g4g7"}}, {    45, {"g4g7"}}, {    55, {"g4g7"}}, {    55, {"g4g7"}}}}},
    {"r1bqkb1r/4npp1/p1p4p/1p1pP1B1/8/1B6/PPPN1PPP/R2Q1RK1 w kq -",        {{{   125, {"d2e4"}}, {   120, {"d2e4"}}, {   115, {"d2e4"}}, {    95, {"d2e4"}}, {   115, {"d2e4"}}}}},
    {"r2q1rk1/1ppnbppp/p2p1nb1/3Pp3/2P1P1P1/2N2N1P/PPB1QP2/R1B2RK1 b - -", {{{   -30, {"a8c8", "c7c5", "h7h6"}}, {   -35, {"a8b8"}}, {   -35, {"a8b8"}}, {   -40, {"f6e8"}}, {   -30, {"h7h6"}}}}},
    {"r1bq1rk1/pp2ppbp/2np2p1/2n5/P3PP2/N1P2N2/1PB3PP/R1B1QRK1 b - -",     {{{    25, {"c5b3"}}, {     5, {"c5b3"}}, {     0, {"c5b3"}}, {     5, {"f7f5"}}, {     0, {"f7f5"}}}}},
    {"3rr3/2pq2pk/p2p1pnp/8/2QBPP2/1P6/P5PP/4RRK1 b - -",                  {{{   -65, {"e8e4"}}, {   -50, {"e8e4"}}, {   -50, {"e8e4"}}, {   -50, {"e8e4"}}, {   -50, {"e8e4"}}}}},
    {"r4k2/pb2bp1r/1p1qp2p/3pNp2/3P1P2/2N3P1/PPP1Q2P/2KRR3 w - -",         {{{    50, {"c3b5"}}, {    45, {"c3b5"}}, {    45, {"c3b5"}}, {    40, {"c3b5"}}, {    45, {"e1g1"}}}}},
    {"3rn2k/ppb2rpp/2ppqp2/5N2/2P1P3/1P5Q/PB3PPP/3RR1K1 w - -",            {{{   235, {"f5h6"}}, {   160, {"f5h6"}}, {   175, {"f5h6"}}, {   170, {"f5h6"}}, {   165, {"f5h6"}}}}},
    {"2r2rk1/1bqnbpp1/1p1ppn1p/pP6/N1P1P3/P2B1N1P/1B2QPP1/R2R2K1 b - -",   {{{    70, {"b7e4"}}, {    70, {"b7e4"}}, {    45, {"b7e4"}}, {    70, {"b7e4"}}, {    55, {"b7e4"}}}}},
    {"r1bqk2r/pp2bppp/2p5/3pP3/P2Q1P2/2N1B3/1PP3PP/R4RK1 b kq -",          {{{    35, {"c8f5"}}, {    40, {"c8f5"}}, {    35, {"c8f5"}}, {    40, {"c8f5"}}, {    35, {"c8f5"}}}}},
    {"r2qnrnk/p2b2b1/1p1p2pp/2pPpp2/1PP1P3/PRNBB3/3QNPPP/5RK1 w - -",      {{{   120, {"e4f5"}}, {   130, {"e4f5"}}, {   120, {"e4f5"}}, {   130, {"e4f5"}}, {   125, {"e4f5"}}}}},
}};
// clang-format on

/// @brief Format the non-empty reference moves of @p bd as a space-separated string.
inline std::string ReferenceMoves(const BkDepth &bd)
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

} // namespace bk

/// @file Pawn structure tests matching Go pawn_structure_test.go.

#include "evaluation.h"
#include "position.h"

#include <format>
#include <functional>
#include <iostream>
#include <string_view>
#include <vector>

struct Check
{
    bool        expected;
    std::string description;
};

static int total_failures = 0;
static int total_checks   = 0;

// has returns true if sq is a member of bb.
static bool has(Bitboard bb, Square sq)
{
    return (bb & Bitboard(sq)).IsNotEmpty();
}

static void run_test(std::string_view name, std::function<std::vector<Check>()> fn)
{
    auto   checks   = fn();
    bool   all_pass = true;
    for (const auto &c : checks)
    {
        ++total_checks;
        if (!c.expected)
        {
            all_pass = false;
            ++total_failures;
            std::cout << std::format("  [FAIL] {}\n", c.description);
        }
    }
    std::cout << std::format("[{}] {}\n", all_pass ? "PASS" : "FAIL", name);
}

int main()
{
    // ── Test 1: single white pawn — isolated, unsupported, passed ─────────
    run_test("single white pawn: isolated, unsupported, passed, not doubled, not defended", [] {
        Position      pos = Position::FromString("7k/8/8/8/4P3/8/8/K7 w - - 0 1");
        PawnStructure ps  = DeterminePawnStructure<kWhite>(pos);
        Square        e4("e4");
        return std::vector<Check>{
            { has(ps.passed_pawns, e4),      "e4 should be passed"},
            { has(ps.isolated_pawns, e4),    "e4 should be isolated"},
            { has(ps.unsupported_pawns, e4), "e4 should be unsupported"},
            {!has(ps.doubled_pawns, e4),     "e4 should not be doubled"},
            {!has(ps.defended_pawns, e4),    "e4 should not be defended"},
        };
    });

    // ── Test 2: defended and supported — d3 immediately defends e4 ────────
    run_test("defended and supported: d3 immediately defends e4", [] {
        Position      pos = Position::FromString("7k/8/8/8/4P3/3P4/8/K7 w - - 0 1");
        PawnStructure ps  = DeterminePawnStructure<kWhite>(pos);
        Square        e4("e4"), d3("d3");
        return std::vector<Check>{
            { has(ps.defended_pawns, e4),    "e4 should be defended by d3"},
            {!has(ps.unsupported_pawns, e4), "e4 should be supported (d3 is in its support window)"},
            {!has(ps.isolated_pawns, e4),    "e4 should not be isolated (d-file has a pawn)"},
            {!has(ps.defended_pawns, d3),    "d3 should not be defended"},
            { has(ps.unsupported_pawns, d3), "d3 should be unsupported (e4 is rank 4, above supportedMask[d3])"},
        };
    });

    // ── Test 3: supported but NOT defended (d2 in window but not immediate) ──
    run_test("supported but NOT defended: d2 is in support window but not immediately behind e4", [] {
        Position      pos = Position::FromString("7k/8/8/8/4P3/8/3P4/K7 w - - 0 1");
        PawnStructure ps  = DeterminePawnStructure<kWhite>(pos);
        Square        e4("e4");
        return std::vector<Check>{
            {!has(ps.unsupported_pawns, e4), "e4 should be supported (d2 is in its support window)"},
            {!has(ps.defended_pawns, e4),    "e4 should NOT be defended (d2 is two ranks behind)"},
            {!has(ps.isolated_pawns, e4),    "e4 should not be isolated (d-file has a pawn)"},
        };
    });

    // ── Test 4: doubled pawns ──────────────────────────────────────────────
    run_test("doubled pawns: back pawn is doubled and excluded from passed", [] {
        Position      pos = Position::FromString("7k/8/4P3/4P3/8/8/8/K7 w - - 0 1");
        PawnStructure ps  = DeterminePawnStructure<kWhite>(pos);
        Square        e5("e5"), e6("e6");
        return std::vector<Check>{
            { has(ps.doubled_pawns, e5),  "e5 should be doubled (e6 is ahead on same file)"},
            {!has(ps.doubled_pawns, e6),  "e6 should not be doubled (nothing ahead)"},
            {!has(ps.passed_pawns, e5),   "e5 should not be passed: doubled pawns are excluded from passed"},
            { has(ps.passed_pawns, e6),   "e6 should be passed (front pawn, not doubled, no blockers)"},
        };
    });

    // ── Test 5: not passed — enemy pawn on adjacent file ahead ────────────
    run_test("not passed: enemy pawn on adjacent file ahead", [] {
        Position      pos = Position::FromString("7k/8/8/5p2/4P3/8/8/K7 w - - 0 1");
        PawnStructure ps  = DeterminePawnStructure<kWhite>(pos);
        Square        e4("e4");
        return std::vector<Check>{
            {!has(ps.passed_pawns, e4),      "e4 should not be passed (black pawn on f5 is in the passed-pawn mask)"},
            { has(ps.unsupported_pawns, e4), "e4 should be unsupported"},
            { has(ps.isolated_pawns, e4),    "e4 should be isolated"},
        };
    });

    // ── Test 6: black pawn defended and supported ──────────────────────────
    run_test("black pawn defended and supported by pawn on adjacent file ahead (black's behind)", [] {
        Position      pos = Position::FromString("7k/8/3p4/4p3/8/8/8/K7 w - - 0 1");
        PawnStructure ps  = DeterminePawnStructure<kBlack>(pos);
        Square        e5("e5"), d6("d6");
        return std::vector<Check>{
            { has(ps.defended_pawns, e5),    "black e5 should be defended by d6"},
            {!has(ps.unsupported_pawns, e5), "black e5 should be supported (d6 is in its support window)"},
            {!has(ps.isolated_pawns, e5),    "black e5 should not be isolated"},
            {!has(ps.defended_pawns, d6),    "black d6 should not be defended"},
        };
    });

    // ── Test 7: black pawn supported but not defended (pawn two ranks behind) ──
    run_test("black pawn: supported-but-not-defended (pawn two ranks behind)", [] {
        Position      pos = Position::FromString("7k/3p4/8/4p3/8/8/8/K7 w - - 0 1");
        PawnStructure ps  = DeterminePawnStructure<kBlack>(pos);
        Square        e5("e5");
        return std::vector<Check>{
            {!has(ps.unsupported_pawns, e5), "black e5 should be supported (d7 is in its support window)"},
            {!has(ps.defended_pawns, e5),    "black e5 should NOT be defended (d7 is two ranks behind)"},
        };
    });

    // ── Test 8: chain c3–d4–e5 with mixed properties ──────────────────────
    run_test("multiple pawns: chain with mixed properties", [] {
        Position      pos = Position::FromString("7k/8/8/4P3/3P4/2P5/8/K7 w - - 0 1");
        PawnStructure ps  = DeterminePawnStructure<kWhite>(pos);
        Square        c3("c3"), d4("d4"), e5("e5");
        return std::vector<Check>{
            { has(ps.defended_pawns, d4),    "d4 should be defended by c3"},
            { has(ps.defended_pawns, e5),    "e5 should be defended by d4"},
            {!has(ps.defended_pawns, c3),    "c3 should not be defended"},
            { has(ps.unsupported_pawns, c3), "c3 should be unsupported (d4 is ahead, not behind)"},
            {!has(ps.unsupported_pawns, d4), "d4 should be supported by c3"},
            {!has(ps.unsupported_pawns, e5), "e5 should be supported by d4"},
            {!has(ps.doubled_pawns, c3),     "c3 should not be doubled"},
            {!has(ps.doubled_pawns, d4),     "d4 should not be doubled"},
            {!has(ps.doubled_pawns, e5),     "e5 should not be doubled"},
            { has(ps.passed_pawns, c3),      "c3 should be passed (no enemy pawns)"},
            { has(ps.passed_pawns, d4),      "d4 should be passed (no enemy pawns)"},
            { has(ps.passed_pawns, e5),      "e5 should be passed (no enemy pawns)"},
        };
    });

    std::cout << std::format("\n{} individual checks; {} failed\n", total_checks, total_failures);
    return total_failures > 0 ? 1 : 0;
}

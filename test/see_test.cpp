/// @file see.cpp Static exchange evaluation tests matching Go see_test.go.

#include "position.h"
#include "static_exchange_evaluation.h"
#include "test_report.h"

#include <array>
#include <format>
#include <iostream>
#include <string_view>

/// @brief A single static-exchange-evaluation test case.
struct SeeTest
{
    std::string_view name_;          ///< Human-readable case name.
    std::string_view fen_;           ///< Position FEN.
    Move             move_;          ///< Move to evaluate.
    int              want_score_;    ///< Expected SEE score.
    bool             want_checking_; ///< Expected give-check flag.
};

// clang-format off
/// @brief The full set of SEE test cases.
static const auto tests = std::to_array<SeeTest>({
    // ── C reference cases ─────────────────────────────────────────────────
    {
        "rook takes pawn, no defenders",
        "1k1r4/1pp4p/p7/4p3/8/P5P1/1PP4P/2K1R3 w - - 0 1",
        Move::Capture("E1", "E5", kRook, kPawn),
        100, false,
    },
    {
        "knight takes pawn, loses to defenders",
        "1k1r3q/1ppn3p/p4b2/4p3/8/P2N2P1/1PP1R1BP/2K1Q3 w - - 0 1",
        Move::Capture("D3", "E5", kKnight, kPawn),
        -200, false,
    },

    // ── Undefended captures ───────────────────────────────────────────────
    {
        "bishop takes undefended knight",
        "k7/8/8/8/3n4/2B5/8/K7 w - - 0 1",
        Move::Capture("C3", "D4", kBishop, kKnight),
        300, false,
    },

    // ── Equal and winning exchanges ───────────────────────────────────────
    {
        "equal exchange: pawn takes pawn defended by pawn",
        "7k/8/4p3/3p4/4P3/8/8/K7 w - - 0 1",
        Move::Capture("E4", "D5", kPawn, kPawn),
        0, false,
    },
    {
        "pawn takes knight defended only by pawn",
        "7k/8/4p3/3n4/4P3/8/8/K7 w - - 0 1",
        Move::Capture("E4", "D5", kPawn, kKnight),
        200, false,
    },
    {
        "bishop takes rook defended only by bishop",
        "7k/8/4b3/3r4/8/1B6/8/K7 w - - 0 1",
        Move::Capture("B3", "D5", kBishop, kRook),
        200, false,
    },

    // ── Losing captures ───────────────────────────────────────────────────
    {
        "rook takes pawn defended by rook",
        "2rk4/8/8/2p5/8/8/8/2RK4 w - - 0 1",
        Move::Capture("C1", "C5", kRook, kPawn),
        -400, false,
    },
    {
        "knight takes pawn defended by king",
        "8/8/8/4kp2/8/4N3/8/K7 w - - 0 1",
        Move::Capture("E3", "F5", kKnight, kPawn),
        -200, false,
    },
    {
        "queen takes pawn defended by queen",
        "7k/8/8/3p3q/8/8/Q7/K7 w - - 0 1",
        Move::Capture("A2", "D5", kQueen, kPawn),
        -800, false,
    },

    // ── X-ray attacker ────────────────────────────────────────────────────
    {
        "x-ray: pawn takes queen, bishop revealed behind",
        "7k/8/4p3/3q4/2P5/1B6/8/K7 w - - 0 1",
        Move::Capture("C4", "D5", kPawn, kQueen),
        900, false,
    },

    // ── Multi-piece exchange ──────────────────────────────────────────────
    {
        "rook+bishop exchange over a pawn",
        "4r1k1/8/8/4p3/3B4/8/8/4R1K1 w - - 0 1",
        Move::Capture("E1", "E5", kRook, kPawn),
        100, false,
    },

    // ── Promotion captures ────────────────────────────────────────────────
    {
        "promotion: pawn captures rook, promotes to queen, undefended",
        "1r6/P7/8/5k2/8/8/8/K7 w - - 0 1",
        Move::Promotion("A7", "B8", kQueen, kRook),
        1300, false,
    },
    {
        "promotion: pawn captures rook, promotes to queen, gives check",
        "7r/6P1/8/4k3/8/8/8/K7 w - - 0 1",
        Move::Promotion("G7", "H8", kQueen, kRook),
        1300, true,
    },

    // ── Check detection ───────────────────────────────────────────────────
    {
        "rook takes pawn, checks king on same rank",
        "8/p6k/8/8/8/8/8/RK6 w - - 0 1",
        Move::Capture("A1", "A7", kRook, kPawn),
        100, true,
    },

    // ── Additional promotion cases ────────────────────────────────────────
    {
        "promotion with recapture chain",
        "r6r/6P1/8/3k4/8/8/8/K6R w - - 0 1",
        Move::Promotion("G7", "H8", kQueen, kRook),
        900, false,
    },
    {
        "underpromotion: pawn captures bishop, promotes to knight",
        "k6b/6P1/8/8/8/8/8/K7 w - - 0 1",
        Move::Promotion("G7", "H8", kKnight, kBishop),
        500, false,
    },

    // ── Long recapture sequences ──────────────────────────────────────────
    {
        "3-rook chain: white has extra rook, gains the pawn",
        "4r2k/4r3/8/4p3/8/4R3/4R3/K3R3 w - - 0 1",
        Move::Capture("E1", "E5", kRook, kPawn),
        100, false,
    },
    {
        "5-piece exchange with stand-pat at two levels",
        "7k/8/3p1b2/4r3/2NP1B2/3n4/8/K7 w - - 0 1",
        Move::Capture("D4", "E5", kPawn, kRook),
        400, false,
    },

    // ── En passant captures ───────────────────────────────────────────────
    {
        "en passant: wins an undefended pawn",
        "7k/8/8/3Pp3/8/8/8/K7 w - e6 0 1",
        Move::EpCapture("D5", "E6"),
        100, false,
    },
    {
        "en passant: equal, recaptured by an adjacent pawn",
        "7k/5p2/8/3Pp3/8/8/8/K7 w - e6 0 1",
        Move::EpCapture("D5", "E6"),
        0, false,
    },

    // ── Battery / revealed attacker ───────────────────────────────────────
    {
        "battery: rook in front of queen wins the pawn after the trade",
        "4r2k/8/8/4p3/8/8/4R3/K3Q3 w - - 0 1",
        Move::Capture("E2", "E5", kRook, kPawn),
        100, false,
    },

    // ── Losing capture that gives check ───────────────────────────────────
    {
        "queen grabs a king-defended pawn, loses the queen but gives check",
        "k7/p7/8/8/8/8/8/Q6K w - - 0 1",
        Move::Capture("A1", "A7", kQueen, kPawn),
        -800, true,
    },

    // ── Battery vs a pawn-defended minor: rook behind bishop wins a pawn ───
    {
        "bishop+rook battery beats a pawn-defended knight",
        "7k/8/4p3/3n4/8/8/6B1/K2R4 w - - 0 1",
        Move::Capture("G2", "D5", kBishop, kKnight),
        100, false,
    },

    // ── Cross-color pawn recapture: two attackers vs one defender ─────────
    // White answers black's e6xd5 recapture with c4xd5 — the recapturing pawns
    // are of different colors, which the swap-off must follow across plies.
    {
        "two pawn attackers vs one defender wins a pawn",
        "7k/8/4p3/3p4/2P1P3/8/8/K7 w - - 0 1",
        Move::Capture("E4", "D5", kPawn, kPawn),
        100, false,
    },
});
// clang-format on

/// @brief Run all SEE test cases.
/// @return Non-zero if any case failed.
int main()
{
    for (const auto &tc : tests)
    {
        Position pos{Position::FromString(tc.fen_)};
        const auto [score, is_checking] = EvaluateStaticExchange(pos, tc.move_);
        bool ok                         = (score == tc.want_score_) && (is_checking == tc.want_checking_);
        if (!test_report::Check(ok, tc.name_))
        {
            if (score != tc.want_score_)
            {
                std::cout << std::format("       score: got {} want {}\n", score, tc.want_score_);
            }
            if (is_checking != tc.want_checking_)
            {
                std::cout << std::format("       givesCheck: got {} want {}\n", is_checking, tc.want_checking_);
            }
        }
    }
    const int total = (int)tests.size();
    return test_report::Summary(std::format("SEE: {}/{} cases passed", total - test_report::failures, total));
}

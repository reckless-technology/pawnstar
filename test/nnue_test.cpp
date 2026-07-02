/// @file nnue.cpp NNUE inference cross-check test.
///
/// Verifies that the in-engine NNUE forward pass reproduces the trainer's evaluation. The trainer-side
/// reference evals are produced by bullet (examples/pawnstar_eval.rs, see nnue/README.md), which feeds the
/// same net through bullet's own Chess768 feature extraction and `simple`-example inference. Agreement to
/// within a couple of centipawns (quantisation rounding) confirms our feature indexing, SCReLU activation,
/// dequantisation and file format all match bullet.
///
/// Usage:  test_nnue <net.bin> <reference.txt>
///   where reference.txt has one "<fen> | <expected_cp>" line per position.
///
/// With no arguments the test is skipped (returns success) so `make check` stays green when no net is built.

#include <memory>
#include "constants.h"
#include "nnue.h"
#include "position.h"
#include "test_report.h"

#include <cmath>
#include <cstdlib>
#include <format>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace
{
/// @brief Allowed engine-vs-reference deviation in centipawns (integer-quantisation rounding only).
constexpr int kTolerance = 2;
} // namespace

int main(int argc, char **argv)
{
    if (argc < 3)
    {
        return test_report::Summary("NNUE: skipped (no net/reference supplied)");
    }
    auto net_ptr = std::make_unique<nnue::Network>();
    nnue::Network &net = *net_ptr;
    if (!net.Load(argv[1]))
    {
        test_report::Check(false, std::format("NNUE: failed to load net '{}'", argv[1]));
        return test_report::ExitCode();
    }
    std::ifstream ref(argv[2]);
    if (!ref)
    {
        test_report::Check(false, std::format("NNUE: cannot open reference '{}'", argv[2]));
        return test_report::ExitCode();
    }

    // Two checks per position:
    //  (1) the exact int16 reference (EvaluateExact) matches bullet within ±kTolerance — verifies feature
    //      indexing, SCReLU and dequantisation; and
    //  (2) the shipped int8 Evaluate stays within kInt8Bound cp of that reference — guards the int8 kernel
    //      (the int8 output layer deliberately differs from bullet by the SCReLU-into-8-bits requant).
    constexpr int kInt8Bound = 40; // measured max ~26 cp on this set; a little headroom.
    int           checked = 0, max_abs_diff = 0, max_int8_dev = 0;
    for (std::string line; std::getline(ref, line);)
    {
        const auto bar = line.rfind('|');
        if (bar == std::string::npos)
        {
            continue;
        }
        const std::string fen      = line.substr(0, bar);
        const int         expected = std::atoi(line.substr(bar + 1).c_str());

        const Position position = Position::FromString(fen);
        const int      got      = net.EvaluateExact(position);
        const int      diff     = std::abs(got - expected);
        max_abs_diff            = std::max(max_abs_diff, diff);
        ++checked;
        if (diff > kTolerance)
        {
            test_report::Check(false,
                               std::format("NNUE ref16 {}  ref16={} bullet={} diff={}", fen, got, expected, diff));
        }
        const int int8_dev = std::abs(net.Evaluate(position) - got);
        max_int8_dev       = std::max(max_int8_dev, int8_dev);
        if (int8_dev > kInt8Bound)
        {
            test_report::Check(
                false, std::format("NNUE int8 {}  int8 deviates {} cp from ref16 (> {})", fen, int8_dev, kInt8Bound));
        }
    }

    return test_report::Summary(
        std::format("NNUE: {} positions, max |ref16-bullet|={}cp (tol {}), max |int8-ref16|={}cp (bound {})", checked,
                    max_abs_diff, kTolerance, max_int8_dev, kInt8Bound));
}

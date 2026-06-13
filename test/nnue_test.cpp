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

#include "constants.h"
#include "nnue.h"
#include "position.h"

#include <cmath>
#include <cstdlib>
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
        std::cout << "test_nnue skipped (no net/reference supplied)\n";
        return 0;
    }
    if (!nnue::LoadNetwork(argv[1]))
    {
        std::cerr << "test_nnue: failed to load net '" << argv[1] << "'\n";
        return 1;
    }
    std::ifstream ref(argv[2]);
    if (!ref)
    {
        std::cerr << "test_nnue: cannot open reference '" << argv[2] << "'\n";
        return 1;
    }

    int failures = 0, checked = 0, max_abs_diff = 0;
    for (std::string line; std::getline(ref, line);)
    {
        const auto bar = line.rfind('|');
        if (bar == std::string::npos)
        {
            continue;
        }
        const std::string fen = line.substr(0, bar);
        const int         expected = std::atoi(line.substr(bar + 1).c_str());

        const Position position = Position::FromString(fen);
        const int      got      = nnue::Evaluate(position);
        const int      diff     = std::abs(got - expected);
        max_abs_diff            = std::max(max_abs_diff, diff);
        ++checked;
        if (diff > kTolerance)
        {
            std::cout << "[FAIL] " << fen << "  engine=" << got << " ref=" << expected << " diff=" << diff << '\n';
            ++failures;
        }
    }

    std::cout << checked << " positions checked, " << failures << " failed, max |diff| = " << max_abs_diff
              << " cp (tolerance " << kTolerance << ")\n";
    if (failures == 0)
    {
        std::cout << "NNUE inference matches bullet reference.\n";
    }
    return failures == 0 ? 0 : 1;
}

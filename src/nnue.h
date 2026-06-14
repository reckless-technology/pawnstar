#pragma once
/// @file nnue.h Optional NNUE (efficiently-updatable neural network) evaluation.
///
/// This is an experimental alternative to the handcrafted evaluation in evaluation.cpp.
/// The network is a simple "perspective" net with the standard bullet `Chess768` feature set:
///
///   768 inputs per perspective  ->  kHiddenSize feature transformer  (x2 perspectives)
///   SCReLU activation, concat[own | opponent] = 2*kHiddenSize  ->  1 output (centipawns, stm relative)
///
/// The accumulator is recomputed from scratch on every evaluation (full refresh); incremental
/// updates are a later optimisation. The weights are produced by the `bullet` trainer and loaded
/// from bullet's *native* quantised .bin (see nnue/README.md), which is a headerless, tightly packed
/// little-endian struct in this exact order (matching bullet's `simple` example save_format):
///
///   feature_weights : int16[kInputSize * kHiddenSize]  (column-major: feature * kHiddenSize + i), scale QA
///   feature_bias    : int16[kHiddenSize]                                                          , scale QA
///   output_weights  : int16[2 * kHiddenSize]  (first kHiddenSize = stm, next = ntm)               , scale QB
///   output_bias     : int16 (scalar)                                                              , scale QA*QB
///
/// The activation and dequantisation below mirror bullet's `simple` example exactly so that an
/// engine evaluation reproduces the trainer's eval (verified by test_nnue).

#include "constants.h"

#include <array>
#include <cstdint>
#include <string>

class Position;

namespace nnue
{

constexpr int kInputSize  = 768; ///< Features per perspective: 2 colors * 6 piece types * 64 squares.
constexpr int kHiddenSize = 256; ///< Feature-transformer / accumulator width per perspective.

// Quantisation constants. These MUST match the bullet trainer configuration that produced the net.
constexpr int kQA    = 255; ///< Feature-transformer weight/activation scale (SCReLU clamp).
constexpr int kQB    = 64;  ///< Output-layer weight scale.
constexpr int kScale = 400; ///< Maps the raw network output to centipawns.

/// @brief Quantised network weights. Read-only after load and shared by all Lazy SMP threads.
/// Layout matches bullet's native quantised .bin exactly (see file header comment).
struct Network
{
    /// @brief Feature-transformer weights, column-major by feature then hidden: index = feature * kHiddenSize + i.
    alignas(64) std::array<int16_t, kInputSize * kHiddenSize> feature_weights;
    alignas(64) std::array<int16_t, kHiddenSize> feature_bias; ///< Feature-transformer biases.
    /// @brief Output weights: first kHiddenSize entries weight the side-to-move accumulator, next kHiddenSize the other.
    alignas(64) std::array<int16_t, 2 * kHiddenSize> output_weights;
    int16_t output_bias; ///< Output bias, in QA*QB units.
};

/// @brief Total size of a valid net file: every weight is a little-endian int16, tightly packed, no header.
constexpr std::size_t kNetFileBytes =
    (static_cast<std::size_t>(kInputSize) * kHiddenSize + kHiddenSize + 2 * kHiddenSize + 1) * sizeof(int16_t);

/// @brief The two perspective accumulators (each the feature-transformer output, width kHiddenSize).
/// Maintained incrementally across make/undo so a node's evaluation does not rebuild it from scratch.
struct Accumulator
{
    alignas(64) std::array<int16_t, kHiddenSize> white; ///< White-perspective accumulator.
    alignas(64) std::array<int16_t, kHiddenSize> black; ///< Black-perspective accumulator.
};

/// @brief Rebuild both perspectives of @p acc from scratch for @p position (seed bias + every piece).
void Refresh(Accumulator &acc, const Position &position);

/// @brief Incrementally move @p acc from matching @p from to matching @p to by applying only the
/// feature deltas for pieces whose placement differs. Reversible (swap @p from / @p to to undo), and a
/// no-op when the placements are identical (e.g. a null move). Robust to captures/castling/EP/promotion.
void Update(Accumulator &acc, const Position &from, const Position &to);

/// @brief Evaluate from a maintained accumulator. @param acc Accumulator matching the position.
/// @param side_to_move Side to move (selects which perspective feeds the first half of the output layer).
/// @return Score in centipawns from the side-to-move's perspective.
int EvaluateAccumulator(const Accumulator &acc, Color side_to_move);

/// @brief Whether NNUE evaluation is currently selected (set via the UseNNUE UCI option / env).
extern bool g_use_nnue;

/// @brief Whether a network has been successfully loaded. NNUE eval is only usable when true.
extern bool g_network_loaded;

/// @brief Load quantised weights from a net file.
/// @param path Path to the net file.
/// @return true on success; on failure logs to stderr and leaves g_network_loaded false.
bool LoadNetwork(const std::string &path);

/// @brief Whether NNUE evaluation can be used right now (selected and a net is loaded).
/// @return true if Evaluate() will use the network.
inline bool IsActive()
{
    return g_use_nnue && g_network_loaded;
}

/// @brief Evaluate a position with the loaded network (full accumulator refresh).
/// @param position Position to evaluate (assumed not a terminal/draw position; that is handled by the caller).
/// @return Score in centipawns from the side-to-move's perspective.
int Evaluate(const Position &position);

} // namespace nnue

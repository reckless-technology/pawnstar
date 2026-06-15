#pragma once
/// @file nnue.h Optional NNUE (efficiently-updatable neural network) evaluation.
///
/// This is an experimental alternative to the handcrafted evaluation in evaluation.cpp.
/// The network is a "perspective" net with bullet's king-bucketed `ChessBuckets` feature set:
///
///   768 inputs per (perspective, king bucket)  ->  kHiddenSize feature transformer  (x2 perspectives)
///   SCReLU activation, concat[own | opponent] = 2*kHiddenSize  ->  1 output (centipawns, stm relative)
///
/// Each perspective selects one of kNumKingBuckets weight banks by *its own* king's square (in that
/// perspective's orientation): the feature row is `bucket * 768 + chess768_index`. This matches bullet's
/// `ChessBuckets::new(kKingBucketMap)` exactly (the same 64-entry map; see nnue.cpp). A king move that
/// crosses a bucket boundary changes that whole perspective's bank, so its accumulator is refreshed
/// rather than diffed (at most one king moves per ply, so at most one perspective refreshes per Update).
///
/// A net is owned by the Game (one instance, read-only after Network::Load) and shared by all Lazy SMP
/// threads, which call only its const methods. The accumulator is maintained incrementally across
/// make/undo (see SearchState); Network::Refresh rebuilds it from scratch. The weights are produced by
/// the `bullet` trainer and loaded from bullet's *native* quantised .bin (see nnue/README.md), a
/// headerless, tightly packed little-endian struct in this exact order (bullet save_format):
///
///   feature_weights : int16[kFeatureRows * kHiddenSize]  (column-major: feature * kHiddenSize + i), scale QA
///   feature_bias    : int16[kHiddenSize]                                                           , scale QA
///   output_weights  : int16[2 * kHiddenSize]  (first kHiddenSize = stm, next = ntm)                , scale QB
///   output_bias     : int16 (scalar)                                                               , scale QA*QB
///
/// where kFeatureRows = 768 * kNumKingBuckets (the bucket banks are laid out bucket-major). The
/// activation and dequantisation mirror bullet's example exactly so that an engine evaluation reproduces
/// the trainer's eval (verified by test_nnue).

#include "constants.h"

#include <array>
#include <cstdint>
#include <string>

class Position;

namespace nnue
{

constexpr int kInputSize     = 768; ///< Features per perspective per bucket: 2 colors * 6 types * 64 squares.
constexpr int kNumKingBuckets = 4;  ///< King-square buckets; each perspective selects one weight bank.
constexpr int kFeatureRows   = kInputSize * kNumKingBuckets; ///< Feature-transformer input rows (bucket-major).
constexpr int kHiddenSize    = 512; ///< Feature-transformer / accumulator width per perspective.

// Quantisation constants. These MUST match the bullet trainer configuration that produced the net.
constexpr int kQA    = 255; ///< Feature-transformer weight/activation scale (SCReLU clamp).
constexpr int kQB    = 64;  ///< Output-layer weight scale.
constexpr int kScale = 400; ///< Maps the raw network output to centipawns.

/// @brief Total size of a valid net file: every weight is a little-endian int16, tightly packed, no header.
constexpr std::size_t kNetFileBytes =
    (static_cast<std::size_t>(kFeatureRows) * kHiddenSize + kHiddenSize + 2 * kHiddenSize + 1) * sizeof(int16_t);

/// @brief The two perspective accumulators (each the feature-transformer output, width kHiddenSize).
/// Maintained incrementally across make/undo so a node's evaluation does not rebuild it from scratch.
struct Accumulator
{
    alignas(64) std::array<int16_t, kHiddenSize> white; ///< White-perspective accumulator.
    alignas(64) std::array<int16_t, kHiddenSize> black; ///< Black-perspective accumulator.
};

/// @brief A loaded, quantised NNUE network plus its accumulator/evaluation operations.
/// Owned by the Game; read-only after Load() and shared by all Lazy SMP threads (only const methods are
/// called concurrently). Default-constructed instances are unloaded (IsLoaded() == false); the weight
/// arrays are left uninitialised until Load() fills them, so construction is cheap.
class Network
{
  public:
    /// @brief Load quantised weights from a bullet native .bin. On failure logs to stderr and stays unloaded.
    /// @param path Path to the net file. @return true on success.
    bool Load(const std::string &path);

    /// @brief Whether a network has been successfully loaded (evaluation is only usable when true).
    /// @return true if a net is loaded.
    bool IsLoaded() const
    {
        return loaded_;
    }

    /// @brief Rebuild both perspectives of @p acc from scratch for @p position (seed bias + every piece).
    void Refresh(Accumulator &acc, const Position &position) const;

    /// @brief Incrementally move @p acc from matching @p from to matching @p to by applying only the
    /// feature deltas for pieces whose placement differs. Reversible (swap @p from / @p to to undo) and a
    /// no-op when placements are identical (e.g. a null move). Robust to captures/castling/EP/promotion.
    void Update(Accumulator &acc, const Position &from, const Position &to) const;

    /// @brief Evaluate from a maintained accumulator. @param acc Accumulator matching the position.
    /// @param side_to_move Selects which perspective feeds the first half of the output layer.
    /// @return Score in centipawns from the side-to-move's perspective.
    int Evaluate(const Accumulator &acc, Color side_to_move) const;

    /// @brief Evaluate a position with a full accumulator refresh (diagnostics / reference path).
    /// @param position Position to evaluate. @return Score in centipawns, side-to-move relative.
    int Evaluate(const Position &position) const;

  private:
    /// @brief Rebuild a single perspective from scratch (bias + every piece) using its king bucket.
    /// @param side The perspective accumulator to rebuild (acc.white or acc.black).
    /// @param position Position to read pieces from. @param bucket That perspective's king bucket.
    /// @param black true for the black perspective (mirrors the square and swaps the colour offset).
    void RefreshSide(std::array<int16_t, kHiddenSize> &side, const Position &position, int bucket, bool black) const;

    /// @brief Apply the piece-placement delta from @p from to @p to to a single perspective, within one
    /// (unchanged) king bucket. @param side Perspective accumulator. @param bucket That perspective's bucket.
    /// @param black true for the black perspective.
    void DiffSide(std::array<int16_t, kHiddenSize> &side, const Position &from, const Position &to, int bucket,
                  bool black) const;

    /// @brief Feature-transformer weights, column-major by feature then hidden: index = feature * kHiddenSize + i.
    /// Feature rows are laid out bucket-major: row = bucket * 768 + chess768_index.
    alignas(64) std::array<int16_t, kFeatureRows * kHiddenSize> feature_weights_;
    alignas(64) std::array<int16_t, kHiddenSize> feature_bias_; ///< Feature-transformer biases.
    /// @brief Output weights: first kHiddenSize weight the side-to-move accumulator, next kHiddenSize the other.
    alignas(64) std::array<int16_t, 2 * kHiddenSize> output_weights_;
    int16_t output_bias_ = 0;     ///< Output bias, in QA*QB units.
    bool    loaded_      = false; ///< Whether Load() has succeeded.
};

} // namespace nnue

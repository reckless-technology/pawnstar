#pragma once
/// @file nnue.h NNUE (efficiently-updatable neural network) evaluation — the engine's only evaluator.
///
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
/// make/undo (see SearchState); Network::Refresh rebuilds it from scratch. A shipped net is prepended
/// with a self-describing NetHeader (see below) that the loader validates against this build; the weight
/// payload itself is the `bullet` trainer's native quantised output (see nnue/README.md), a tightly
/// packed little-endian struct in this exact order (bullet save_format):
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

constexpr int kInputSize      = 768; ///< Features per perspective per bucket: 2 colors * 6 types * 64 squares.
constexpr int kNumKingBuckets = 4;   ///< King-square buckets; each perspective selects one weight bank.
constexpr int kFeatureRows    = kInputSize * kNumKingBuckets; ///< Feature-transformer input rows (bucket-major).
constexpr int kHiddenSize     = 512; ///< Feature-transformer / accumulator width per perspective.

// Quantisation constants. These MUST match the bullet trainer configuration that produced the net.
constexpr int kQA    = 255; ///< Feature-transformer weight/activation scale (SCReLU clamp).
constexpr int kQB    = 64;  ///< Output-layer weight scale.
constexpr int kScale = 400; ///< Maps the raw network output to centipawns.

/// @brief Total size of the quantised weight payload: every weight a little-endian int16, tightly packed.
constexpr std::size_t kNetFileBytes =
    (static_cast<std::size_t>(kFeatureRows) * kHiddenSize + kHiddenSize + 2 * kHiddenSize + 1) * sizeof(int16_t);

/// @brief Magic identifying a Pawnstar-stamped NNUE file ("PSN" + format version byte).
constexpr char          kNetMagic[4]      = {'P', 'S', 'N', '1'};
constexpr std::uint16_t kNetFormatVersion = 1; ///< Header format version (bump on incompatible header changes).

/// @brief Self-describing 32-byte header prepended to a shipped net so an incompatible file is rejected
/// rather than silently misread. The architecture fields must equal the engine's compile-time constants;
/// the loader checks every one. Raw (headerless) bullet output is still accepted via an exact size check.
#pragma pack(push, 1)

struct NetHeader
{
    char          magic[4];       ///< kNetMagic.
    std::uint16_t format_version; ///< kNetFormatVersion.
    std::uint16_t input_size;     ///< kInputSize (features per perspective per bucket).
    std::uint16_t king_buckets;   ///< kNumKingBuckets.
    std::uint16_t hidden_size;    ///< kHiddenSize.
    std::int16_t  qa;             ///< kQA.
    std::int16_t  qb;             ///< kQB.
    std::int16_t  scale;          ///< kScale.
    std::uint8_t  reserved[14];   ///< Zero-padding to a round 32 bytes.
};

#pragma pack(pop)
static_assert(sizeof(NetHeader) == 32, "NetHeader must be exactly 32 bytes");

/// @brief The header describing the architecture this engine was compiled for.
/// @return A NetHeader populated from the compile-time NNUE constants.
inline NetHeader ExpectedNetHeader()
{
    NetHeader h{};
    h.magic[0]       = kNetMagic[0];
    h.magic[1]       = kNetMagic[1];
    h.magic[2]       = kNetMagic[2];
    h.magic[3]       = kNetMagic[3];
    h.format_version = kNetFormatVersion;
    h.input_size     = static_cast<std::uint16_t>(kInputSize);
    h.king_buckets   = static_cast<std::uint16_t>(kNumKingBuckets);
    h.hidden_size    = static_cast<std::uint16_t>(kHiddenSize);
    h.qa             = static_cast<std::int16_t>(kQA);
    h.qb             = static_cast<std::int16_t>(kQB);
    h.scale          = static_cast<std::int16_t>(kScale);
    return h;
}

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
    /// @brief Load quantised weights from a bullet native .bin file. On failure logs to stderr and stays
    /// unloaded. Reads the file into memory and defers to LoadFromMemory.
    /// @param path Path to the net file. @return true on success.
    bool Load(const std::string &path);

    /// @brief Load quantised weights from an in-memory image of a net file (stamped or raw), using the same
    /// header detection and architecture validation as Load. Used for the engine's embedded fallback net.
    /// @param data Pointer to the net image. @param size Image size in bytes.
    /// @param origin Label for log messages (e.g. a path or "embedded"). @return true on success.
    bool LoadFromMemory(const std::uint8_t *data, std::size_t size, const std::string &origin);

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

#pragma once
/// @file nnue.h NNUE (efficiently-updatable neural network) evaluation — the engine's only evaluator.
///
/// The network is a "perspective" net with bullet's `ChessBuckets` feature set:
///
///   768 inputs per (perspective, king bucket)  ->  kHiddenSize feature transformer  (x2 perspectives)
///   SCReLU activation, concat[own | opponent] = 2*kHiddenSize  ->  1 output (centipawns, stm relative)
///
/// The shipped net (**v12**) is **1024-wide with kNumKingBuckets = 8** — eight king-square weight banks
/// selected by king **file-pair × board-half** (`kKingBucketMap`, below): the file-pair `file/2` (a/b→0 …
/// g/h→3) plus +4 when the king is in the advanced half (its own ranks 5–8, per perspective). On the same
/// ~6B data this beat the 4-bucket v11 by **+17.3 ± 8.7 Elo at 8+0.08 and +11.0 ± 6.6 at 12+0.12** — king
/// buckets are NOT at diminishing returns (0→4 was +11.4; 4→8 is bigger), the rank-split capturing real
/// king-safety signal (own-half vs advanced king). Each perspective selects one of kNumKingBuckets banks by
/// *its own* king's square (in that perspective's orientation), the feature row being
/// `bucket * 768 + chess768_index`, matching bullet's `ChessBuckets::new(kKingBucketMap)` (the same 64-entry
/// map; see `kKingBucketMap` below). A king move that crosses a bucket boundary refreshes that whole
/// perspective's bank rather than diffing it.
///
/// **`kKingBucketMap` is the one place the bucketing is written down.** kNumKingBuckets is *derived* from it,
/// `MaxKingBucketIndex() + 1`, so the two cannot disagree: change the map and the count follows, and there is
/// no second constant to forget. It was a hand-set constant until 2026-10-27, and lowering it without editing
/// the map left WhiteBucket/BlackBucket returning the map's old bank indices, which reach past the end of
/// feature_weights_ (768 * kNumKingBuckets rows) - a buffer overrun, not a smaller net. This mirrors
/// tools/bullet/pawnstar.rs, which derives its own NUM_BUCKETS from KING_BUCKETS the same way. A
/// static_assert beside the map still checks what the derivation cannot see: that no entry is negative and
/// that the banks are numbered from 0 with no gaps, since a map selecting only banks {0, 3} would size the
/// weights for four and leave two of them untrained.
///
/// The map must also stay **byte-identical to the map in tools/bullet/pawnstar.rs that the net was trained
/// with**: a net trained with one map and evaluated with another loads, plays, and is silently wrong,
/// because a header that records only the bank *count* cannot tell the two apart. **The RNNUE1 header
/// closes that hole**: it carries `king_bucket_map_crc`, the CRC-64/XZ of this map's 64 entries as one
/// `uint8` each, and the loader refuses a net whose digest differs from this build's. That was deferred
/// until the next format change in nnue/README.md §2; this is that change, and the README now records it as
/// done rather than proposed. What the digest cannot prove is that the *trainer* used this map, since
/// tools/bullet/pawnstar.rs computes nothing from this header — it narrows the window to "stamped by a
/// build whose map differs", which is the likelier mistake.
///
/// A net is owned by the Game (one instance, read-only after Network::Load) and shared by all Lazy SMP
/// threads, which call only its const methods. The accumulator is maintained incrementally and **lazily**
/// across make/undo (the update is deferred until an eval reads it — see SearchState::CurrentAccumulator);
/// Network::Refresh rebuilds it from scratch.
///
/// **The net container is `RNNUE1`** — the one format every Reckless engine reads (skill `reckless-nnue`).
/// A 64-byte self-describing NetHeader (see below), then the weight payload: little-endian throughout,
/// tightly packed, in bullet's save order, bucket-major.
///
///   feature_weights : int16[kFeatureRows * kHiddenSize]  (column-major: feature * kHiddenSize + i), scale QA
///   feature_bias    : int16[kHiddenSize]                                                          , scale QA
///   output_weights  : int16[kNumOutputBuckets][2 * kHiddenSize]  (own perspective first, then the
///                     opponent's, per bucket)                                                     , scale QB
///   output_bias     : int32[kNumOutputBuckets]                                                 , scale QA*QB
///
/// where kFeatureRows = 768 * kNumKingBuckets (the bucket banks are laid out bucket-major). The
/// activation and dequantisation mirror bullet's example exactly so that an engine evaluation reproduces
/// the trainer's eval (verified by test_nnue).
///
/// The loader makes exactly two checks, in this order, and **refuses loudly without falling back** on any
/// failure: (1) the 64-byte header — magic, then `header_crc` over its first 56 bytes, then every
/// architecture field against this build's compile-time constants, then `king_bucket_map_crc` against this
/// build's own map; (2) the payload — `payload_bytes` against the bytes actually there, then `payload_crc`
/// over them. Each refusal names the field, the file's value and the engine's. A net that fails is not
/// loaded: silently playing on with a different net is the failure the whole container exists to stop.
///
/// RNNUE1 replaced the 32-byte `PSN1` header on 2026-10-27. PSN1 carried the architecture but neither the
/// bucket map's contents nor any checksum, and stored the output bias as a single int16; RNNUE1 adds the
/// map digest, the two CRCs, an output-bucket count and an int32 bias array. The two are told apart by
/// their magic, so a PSN1 net is rejected rather than misread.

#include "constants.h"
#include "crc64.h"
#include "platform.h"
#include "position.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <format>
#include <fstream>
#include <iostream>
#include <string>
#include <utility>
#include <vector>
#if defined(__AVX2__)
#include <immintrin.h>
#endif

/// @brief Horizontal king mirroring: 1 to mirror a perspective whose own king stands on files e-h, 0 for
/// v12's unmirrored shape. A build knob so that either shape can be built from one tree while the three
/// bit-exact ports are brought across one at a time; both are stamped into the net header, so a net can
/// only load into a build of its own shape.
#if !defined(PAWNSTAR_NNUE_KING_MIRRORING)
#define PAWNSTAR_NNUE_KING_MIRRORING 0
#endif

/// @brief Material output heads: 8 for the shared shape, 1 for v12's. The same build knob.
#if !defined(PAWNSTAR_NNUE_OUTPUT_BUCKETS)
#define PAWNSTAR_NNUE_OUTPUT_BUCKETS 1
#endif

namespace nnue
{

/// @brief Whether a perspective whose own king stands on files e-h is horizontally mirrored (`square ^ 7`)
/// before indexing. Measured in zenith at **+14.3 Elo** at fixed depth, and worth more than that number
/// suggests: halving the king-placement space gives twice the effective data per bucket at the same file
/// size, so it is a data multiplier as much as an Elo add. It only paid past ~1.4B training positions,
/// which is why v12 does not have it.
constexpr bool kIsKingMirroringEnabled = PAWNSTAR_NNUE_KING_MIRRORING != 0;

/// @brief King-square -> weight-bank map (bullet `ChessBuckets`). Indexed by a square in the *perspective's
/// own* orientation (white king square directly; black king square ^ kRankFlip), so the same 64-entry map
/// serves both perspectives. MUST be byte-identical to the array passed to ChessBuckets::new in
/// tools/bullet/*.rs. It is the one place the bucketing is written down: kNumKingBuckets is derived from it
/// below, so there is no second constant to keep in step.
constexpr std::array<int, 64> kKingBucketMapUnmirrored = {
    0, 0, 1, 1, 2, 2, 3, 3, //
    0, 0, 1, 1, 2, 2, 3, 3, //
    0, 0, 1, 1, 2, 2, 3, 3, //
    0, 0, 1, 1, 2, 2, 3, 3, //
    4, 4, 5, 5, 6, 6, 7, 7, //
    4, 4, 5, 5, 6, 6, 7, 7, //
    4, 4, 5, 5, 6, 6, 7, 7, //
    4, 4, 5, 5, 6, 6, 7, 7, //
};

/// @brief The mirrored shape's map, over **half** a board: 32 entries indexed `rank * 4 + mirrored_file`,
/// where `mirrored_file = min(file, 7 - file)`. `half * 4 + mirrored_file`, with `half = rank / 4`.
///
/// **This is the one place the mirrored bucketing is written down, and the engine's 64-entry map is
/// generated from it** by ExpandMirroredKingBucketMap below, using bullet's own expansion arithmetic. The
/// 32 entries are what `ChessBucketsMirrored::new` is handed in tools/bullet/pawnstar.rs, so trainer and
/// engine read the same 32 numbers rather than two 64-entry arrays that have to be compared by eye — a map
/// that drifts trains one network and runs another, and `king_bucket_map_crc` catches that only after the
/// training run is already spent. `test_primitives` reads the trainer's array out of the .rs file and
/// compares it with this one, so neither side can drift unnoticed.
constexpr std::array<int, 32> kKingBucketMapHalf = {
    0, 1, 2, 3, //
    0, 1, 2, 3, //
    0, 1, 2, 3, //
    0, 1, 2, 3, //
    4, 5, 6, 7, //
    4, 5, 6, 7, //
    4, 5, 6, 7, //
    4, 5, 6, 7, //
};

/// @brief XOR mask that mirrors a square across the d/e file boundary (it flips the three file bits).
constexpr int kFileFlip = 0x07;

/// @brief Expand the 32-entry half-board map to the 64-entry map the engine indexes, exactly as bullet's
/// `ChessBucketsMirrored::new` does: `expanded[sq] = half[(sq / 8) * 4 + [0,1,2,3,3,2,1,0][sq % 8]]`,
/// where that eight-entry table is `min(file, 7 - file)`. The expansion folds the file, so the expanded map
/// gives the same bank for a square and its mirror — which is what makes it safe to index with the king's
/// unmirrored square, as bullet does.
/// @return The 64-entry map.
consteval std::array<int, 64> ExpandMirroredKingBucketMap()
{
    std::array<int, 64> expanded{};
    for (std::size_t square = 0; square < expanded.size(); ++square)
    {
        const std::size_t file          = square % 8;
        const std::size_t mirrored_file = std::min(file, 7 - file);
        expanded[square]                = kKingBucketMapHalf[(square / 8) * 4 + mirrored_file];
    }
    return expanded;
}

/// @brief The map this build indexes. Mirroring halves the king-placement space, so the two shapes need
/// genuinely different maps: the mirrored one folds the file (four mirrored files x two halves), while
/// v12's pairs adjacent files (four file-pairs x two halves) and is not mirror-symmetric, so it has no
/// 32-entry form at all. Which one is compiled is stamped into the header twice over — in `flags` and in
/// `king_bucket_map_crc` — so a net trained for one can never load into a build of the other.
constexpr std::array<int, 64> kKingBucketMap =
    kIsKingMirroringEnabled ? ExpandMirroredKingBucketMap() : kKingBucketMapUnmirrored;

/// @brief The highest weight-bank index kKingBucketMap selects, i.e. the bank count it needs, less one.
consteval int MaxKingBucketIndex()
{
    int highest = 0;
    for (const int bucket : kKingBucketMap)
    {
        highest = std::max(highest, bucket);
    }
    return highest;
}

/// @brief Whether kKingBucketMap is a usable bank selector: every entry a bank index in range, and every bank
/// from 0 up to the highest actually selected by some square. The derivation of kNumKingBuckets cannot see
/// either fault - a negative entry would index feature_weights_ below its start, and a map selecting only
/// banks {0, 3} derives four banks while leaving two of them with no square to train them.
consteval bool IsKingBucketMapWellFormed()
{
    std::array<bool, 64> is_selected{};
    for (const int bucket : kKingBucketMap)
    {
        if (bucket < 0 || bucket >= static_cast<int>(is_selected.size()))
        {
            return false;
        }
        is_selected[static_cast<std::size_t>(bucket)] = true;
    }
    for (int bucket = 0; bucket <= MaxKingBucketIndex(); ++bucket)
    {
        if (!is_selected[static_cast<std::size_t>(bucket)])
        {
            return false;
        }
    }
    return true;
}

static_assert(IsKingBucketMapWellFormed(),
              "kKingBucketMap is not a usable bank selector: every one of its 64 entries must be a bank index "
              "in [0, 63], and every bank from 0 up to the highest entry must be selected by at least one "
              "square. A negative entry reads feature_weights_ below its start; a gap (banks {0, 3}, say) "
              "sizes the weights for four banks and leaves two of them untrained. Number the banks from 0 "
              "with no gaps, and keep the map byte-identical to KING_BUCKETS in tools/bullet/pawnstar.rs, "
              "which the net was trained with.");

constexpr int kInputSize      = 768; ///< Features per perspective per bucket: 2 colors * 6 types * 64 squares.
constexpr int kNumKingBuckets = MaxKingBucketIndex() + 1;     ///< King-square weight banks, per kKingBucketMap.
constexpr int kFeatureRows    = kInputSize * kNumKingBuckets; ///< Feature-transformer input rows (bucket-major).
constexpr int kHiddenSize     = 1024; ///< Feature-transformer / accumulator width per perspective.

/// @brief Material output heads, selected by OutputBucket() from the piece count. Eight in the shared
/// architecture, measured in zenith at **+14.7 Elo** at fixed depth; one in v12's shape. A compile-time
/// constant because it sizes the payload and is stamped into the header, so a net and a build can never
/// disagree about it silently.
constexpr int kNumOutputBuckets = PAWNSTAR_NNUE_OUTPUT_BUCKETS;
static_assert(kNumOutputBuckets >= 1 && kNumOutputBuckets <= 32,
              "PAWNSTAR_NNUE_OUTPUT_BUCKETS must be between 1 and 32: bullet's MaterialCount<N> divides the "
              "piece count by ceil(32 / N), so more than 32 heads would leave heads no position can select");

// Quantisation constants. These MUST match the bullet trainer configuration that produced the net.
constexpr int kQA    = 255; ///< Feature-transformer weight/activation scale (SCReLU clamp).
constexpr int kQB    = 64;  ///< Output-layer weight scale.
constexpr int kScale = 400; ///< Maps the raw network output to centipawns.

/// @brief Element counts of the four payload sections, in the order they are stored.
constexpr std::size_t kFeatureWeightCount = static_cast<std::size_t>(kFeatureRows) * kHiddenSize;
constexpr std::size_t kFeatureBiasCount   = kHiddenSize;
constexpr std::size_t kOutputWeightCount  = static_cast<std::size_t>(kNumOutputBuckets) * 2 * kHiddenSize;
constexpr std::size_t kOutputBiasCount    = kNumOutputBuckets;

/// @brief Exact size of the quantised weight payload: three int16 sections then an int32 bias per output
/// bucket, tightly packed and little-endian. This is what the header's payload_bytes must equal, and the
/// file carries nothing after it — bullet's 64-byte "bullet" tail padding is dropped at stamping time,
/// because an exact length is what lets a truncated or overlong file be told apart from a sound one.
constexpr std::size_t kNetPayloadBytes =
    (kFeatureWeightCount + kFeatureBiasCount + kOutputWeightCount) * sizeof(std::int16_t) +
    kOutputBiasCount * sizeof(std::int32_t);

/// @brief Magic identifying a net in the shared Reckless container: "RNNUE1" and two zero bytes.
constexpr char kNetMagic[8] = {'R', 'N', 'N', 'U', 'E', '1', '\0', '\0'};

/// @brief RNNUE1 format version. **2 when mirroring is on, 1 when it is off**, and that is not a quirk of
/// the switch: the map's contents are part of the architecture, and turning mirroring on changes what a
/// bucket *means* rather than only how many there are. v12's map pairs adjacent files
/// (`half * 4 + file / 2`); the mirrored map is per-file (`half * 4 + min(file, 7 - file)`) because
/// mirroring has already folded e-h onto d-a. By the container's own rule that is a format change, so it
/// bumps the version — which `king_bucket_map_crc` would catch anyway, but the version says plainly that
/// the two are different formats rather than two nets of one format.
constexpr std::uint16_t kNetFormatVersion = kIsKingMirroringEnabled ? 2 : 1;
constexpr std::uint16_t kNetHeaderBytes   = 64; ///< Header size, so a later version can extend it safely.

constexpr std::uint8_t kNetFlagKingMirroring = 0x01; ///< flags_ bit 0: horizontal king mirroring is on.
constexpr std::uint8_t kActivationScreelu    = 0;    ///< activation_ 0: SCReLU, clamp(x, 0, QA) then square.

/// @brief The 64-byte self-describing header every RNNUE1 net carries, little-endian and tightly packed.
/// Every architecture field must equal this build's compile-time constant and the map digest this build's
/// own map, or the net is refused; the three CRCs make a corrupt or truncated file refuse too.
#pragma pack(push, 1)

struct NetHeader
{
    char          magic_[8];            ///< kNetMagic, at offset 0.
    std::uint16_t format_version_;      ///< kNetFormatVersion, at 8.
    std::uint16_t header_bytes_;        ///< kNetHeaderBytes, at 10.
    std::uint16_t features_per_bucket_; ///< kInputSize, at 12.
    std::uint16_t king_buckets_;        ///< kNumKingBuckets, at 14.
    std::uint16_t hidden_size_;         ///< kHiddenSize, at 16.
    std::uint16_t output_buckets_;      ///< kNumOutputBuckets, at 18.
    std::uint8_t  flags_;               ///< kNetFlagKingMirroring, at 20; every other bit zero.
    std::uint8_t  activation_;          ///< kActivationScreelu, at 21.
    std::uint16_t qa_;                  ///< kQA, at 22.
    std::uint16_t qb_;                  ///< kQB, at 24.
    std::uint16_t eval_scale_;          ///< kScale, at 26.
    std::uint8_t  reserved_[4];         ///< Zero, at 28.
    std::uint64_t king_bucket_map_crc_; ///< CRC-64/XZ of kKingBucketMap's 64 entries as uint8, at 32.
    std::uint64_t payload_bytes_;       ///< kNetPayloadBytes, at 40.
    std::uint64_t payload_crc_;         ///< CRC-64/XZ over the payload, at 48.
    std::uint64_t header_crc_;          ///< CRC-64/XZ over this header's first 56 bytes, at 56.
};

#pragma pack(pop)
static_assert(sizeof(NetHeader) == kNetHeaderBytes, "NetHeader must be exactly 64 bytes");
static_assert(offsetof(NetHeader, king_bucket_map_crc_) == 32, "king_bucket_map_crc must sit at offset 32");
static_assert(offsetof(NetHeader, header_crc_) == 56, "header_crc must sit at offset 56");

/// @brief Bytes of the header that header_crc_ covers: everything before header_crc_ itself.
constexpr std::size_t kNetHeaderCrcBytes = offsetof(NetHeader, header_crc_);

/// @brief CRC-64/XZ of kKingBucketMap's 64 entries, one uint8 each — the digest the header carries so that
/// two maps of the same cardinality can no longer be mistaken for one another.
/// @return The digest, computed at compile time.
consteval std::uint64_t KingBucketMapCrc()
{
    std::array<std::uint8_t, kKingBucketMap.size()> entries{};
    for (std::size_t square = 0; square < kKingBucketMap.size(); ++square)
    {
        entries[square] = static_cast<std::uint8_t>(kKingBucketMap[square]);
    }
    return crc64::Checksum(entries.data(), entries.size());
}

/// @brief The architecture this engine was compiled for, as a header. payload_crc_ and header_crc_ are left
/// zero: only a stamping tool, which has the payload in hand, can fill them (see FinaliseNetHeader).
/// @return A NetHeader populated from the compile-time NNUE constants.
inline NetHeader ExpectedNetHeader()
{
    NetHeader header{};
    for (std::size_t index = 0; index < sizeof(kNetMagic); ++index)
    {
        header.magic_[index] = kNetMagic[index];
    }
    header.format_version_      = kNetFormatVersion;
    header.header_bytes_        = kNetHeaderBytes;
    header.features_per_bucket_ = static_cast<std::uint16_t>(kInputSize);
    header.king_buckets_        = static_cast<std::uint16_t>(kNumKingBuckets);
    header.hidden_size_         = static_cast<std::uint16_t>(kHiddenSize);
    header.output_buckets_      = static_cast<std::uint16_t>(kNumOutputBuckets);
    header.flags_               = kIsKingMirroringEnabled ? kNetFlagKingMirroring : 0U;
    header.activation_          = kActivationScreelu;
    header.qa_                  = static_cast<std::uint16_t>(kQA);
    header.qb_                  = static_cast<std::uint16_t>(kQB);
    header.eval_scale_          = static_cast<std::uint16_t>(kScale);
    header.king_bucket_map_crc_ = KingBucketMapCrc();
    header.payload_bytes_       = kNetPayloadBytes;
    return header;
}

/// @brief Fill in a header's two computed checksums, which need the payload and the rest of the header.
/// This is what a stamping tool calls after ExpectedNetHeader, so the header a net carries is written from
/// the stamping build's own compiled constants and never by hand.
/// @param header Header to complete, already populated by ExpectedNetHeader.
/// @param payload First byte of the weight payload. @param payload_bytes Payload length; must equal
/// header.payload_bytes_.
inline void FinaliseNetHeader(NetHeader &header, const std::uint8_t *payload, std::size_t payload_bytes)
{
    header.payload_crc_ = crc64::Checksum(payload, payload_bytes);
    header.header_crc_  = crc64::Checksum(reinterpret_cast<const std::uint8_t *>(&header), kNetHeaderCrcBytes);
}

/// @brief Everything one perspective needs to turn a (colour, piece, square) into a feature row: which
/// weight bank it reads, and how a square and a colour are oriented into its own view.
///
/// **The whole point of gathering these three into one value is the refresh rule.** A perspective can only
/// be updated incrementally while all three stay put; if any changes, every feature row it reads moves and
/// the perspective must be rebuilt from scratch. Before mirroring there was one way for that to happen — a
/// king crossing a bucket boundary — and the code compared bucket indices. Mirroring adds a second, and
/// the two are not the same: at the mirrored map a king stepping d1 -> e1 keeps bucket 3 (file 3 and file 4
/// both mirror to file 3) while `square_flip_` gains the file flip, so a comparison of buckets alone would
/// diff a perspective whose every row had just moved and be **silently** wrong — no crash, no assertion,
/// just a wrong evaluation. Comparing the whole Perspective cannot miss either case.
struct Perspective
{
    int bank_;        ///< First feature row of this perspective's bank: bucket * kInputSize.
    int square_flip_; ///< XOR applied to a square: kRankFlip for black's view, plus kFileFlip if mirrored.
    int color_flip_;  ///< XOR applied to a piece's colour: 0 for white's view, 1 for black's.

    constexpr bool operator==(const Perspective &) const = default;
};

/// @brief The two perspective accumulators (each the feature-transformer output, width kHiddenSize).
/// Maintained incrementally across make/undo so a node's evaluation does not rebuild it from scratch.
struct Accumulator
{
    alignas(64) std::array<int16_t, kHiddenSize> white_; ///< White-perspective accumulator.
    alignas(64) std::array<int16_t, kHiddenSize> black_; ///< Black-perspective accumulator.
};

/// @brief A loaded, quantised NNUE network plus its accumulator/evaluation operations.
/// Owned by the Game; read-only after Load() and shared by all Lazy SMP threads (only const methods are
/// called concurrently). Default-constructed instances are unloaded (loaded_ == false); the weight
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

    /// @brief Rebuild both perspectives of @p acc from scratch for @p position (seed bias + every piece).
    void Refresh(Accumulator &acc, const Position &position) const;

    /// @brief Incrementally move @p acc from matching @p from to matching @p to by applying only the
    /// feature deltas for pieces whose placement differs. Reversible (swap @p from / @p to to undo) and a
    /// no-op when placements are identical (e.g. a null move). Robust to captures/castling/EP/promotion.
    void Update(Accumulator &acc, const Position &from, const Position &to) const;

    /// @brief Evaluate from a maintained accumulator using the **int8** output layer — the engine's shipped
    /// evaluator (~1.86x faster than the exact int16 dot; a measured +31.8 Elo at 8+0.08), accurate to a
    /// bounded ~26 cp of EvaluateExact.
    /// @param acc Accumulator matching @p position.
    /// @param position The position @p acc was maintained for. It supplies the side to move, which selects
    /// which perspective feeds the first half of the output layer, and the piece count, which selects the
    /// output head — both are properties of the position, not of the accumulator, so passing the position
    /// is what stops the two being read from different places.
    /// @return Score in centipawns from the side-to-move's perspective.
    int Evaluate(const Accumulator &acc, const Position &position) const;

    /// @brief Evaluate a position with a full accumulator refresh, int8 output layer (the engine eval).
    /// @param position Position to evaluate. @return Score in centipawns, side-to-move relative.
    int Evaluate(const Position &position) const;

    /// @brief Exact int16 SCReLU forward pass — the **reference** evaluator that reproduces the trainer's
    /// eval (test_nnue cross-checks it to bullet within ±2 cp) and that the int8 Evaluate approximates. Not
    /// used on the search hot path. @param acc Accumulator matching @p position.
    /// @param position The position @p acc was maintained for: side to move and output head.
    /// @return Score in centipawns, side-to-move relative.
    int EvaluateExact(const Accumulator &acc, const Position &position) const;

    /// @brief Exact int16 evaluation with a full accumulator refresh (reference / diagnostics).
    /// @param position Position to evaluate. @return Score in centipawns, side-to-move relative.
    int EvaluateExact(const Position &position) const;

  private:
    /// @brief Rebuild a single perspective from scratch (bias + every piece) through its indexing.
    /// @param side The perspective accumulator to rebuild (acc.white_ or acc.black_).
    /// @param position Position to read pieces from.
    /// @param perspective That perspective's bank, square flip and colour flip.
    void RefreshSide(std::array<int16_t, kHiddenSize> &side, const Position &position,
                     const Perspective &perspective) const;

    /// @brief Apply the piece-placement delta from @p from to @p to to a single perspective, whose indexing
    /// is unchanged across the move. @param side Perspective accumulator.
    /// @param from Position before the move. @param to Position after it.
    /// @param perspective That perspective's (unchanged) bank, square flip and colour flip.
    void DiffSide(std::array<int16_t, kHiddenSize> &side, const Position &from, const Position &to,
                  const Perspective &perspective) const;

    /// @brief Feature-transformer weights, column-major by feature then hidden: index = feature * kHiddenSize + i.
    /// Feature rows are laid out bucket-major: row = bucket * 768 + chess768_index.
    alignas(64) std::array<int16_t, kFeatureRows * kHiddenSize> feature_weights_;
    alignas(64) std::array<int16_t, kHiddenSize> feature_bias_; ///< Feature-transformer biases (always int16).
    /// @brief Output weights, bucket-major: bucket * 2 * kHiddenSize + i, the first kHiddenSize of each
    /// bucket weighting the side-to-move accumulator and the next kHiddenSize the other.
    alignas(64) std::array<int16_t, kOutputWeightCount> output_weights_;
    /// @brief Output weights requantised to int8 for the shipped int8 forward pass (Network::Evaluate).
    /// w8 = round(output_weight / output_w_scale_) clamped to [-127,127]; the scale keeps the largest output
    /// weight in int8 range (1 for nets whose |weight| <= 127, e.g. the shipped net). Populated by Load so the
    /// int8 kernel needs no per-eval setup; the int16 reference (EvaluateExact) uses output_weights_ instead.
    /// One scale across every bucket, not one per bucket, so the dequantisation tail stays bucket-agnostic.
    alignas(64) std::array<int8_t, kOutputWeightCount> output_weights_i8_{};
    int output_w_scale_ = 1; ///< Divisor mapping the int16 output weights into int8 range.
    /// @brief Output bias per output bucket, in QA*QB units. int32 in the container even though the trained
    /// values comfortably fit int16: the width is part of RNNUE1, shared by all five engines.
    std::array<std::int32_t, kOutputBiasCount> output_bias_{};

  public:
    bool loaded_ = false; ///< Whether Load() has succeeded.
};

// ---- Out-of-class definitions (header-only build) ----

/// @brief Squared clipped ReLU: clamp to [0, kQA] then square (mirrors bullet's `simple` activation).
/// @param x Accumulator value (already in QA units). @return Activated value in QA*QA units.
/// (Used by the scalar forward pass; the AVX2 path inlines the equivalent arithmetic.)
[[maybe_unused]] inline int32_t SCReLU(int16_t x)
{
    const int32_t y = x < 0 ? 0 : (x > kQA ? kQA : x);
    return y * y;
}

// The feature column add/sub maintain the int16 accumulator from the int16 weight columns. The wrapping
// 16-bit add matches the scalar path exactly, so the AVX2 and scalar kernels are bit-identical.
#if defined(__AVX2__)
static_assert(kHiddenSize % 16 == 0, "AVX2 NNUE kernels need kHiddenSize to be a multiple of 16");

/// @brief Add a feature column to a single-perspective accumulator (AVX2: 16 int16 lanes/step).
inline void AddColumn(std::array<int16_t, kHiddenSize> &acc, const int16_t *column)
{
    for (int i = 0; i < kHiddenSize; i += 16)
    {
        const __m256i a = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(acc.data() + i));
        const __m256i c = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(column + i));
        _mm256_storeu_si256(reinterpret_cast<__m256i *>(acc.data() + i), _mm256_add_epi16(a, c));
    }
}

/// @brief Subtract a feature column from a single-perspective accumulator (AVX2; inverse of AddColumn).
inline void SubColumn(std::array<int16_t, kHiddenSize> &acc, const int16_t *column)
{
    for (int i = 0; i < kHiddenSize; i += 16)
    {
        const __m256i a = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(acc.data() + i));
        const __m256i c = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(column + i));
        _mm256_storeu_si256(reinterpret_cast<__m256i *>(acc.data() + i), _mm256_sub_epi16(a, c));
    }
}
#else // scalar fallback
/// @brief Add a feature column to a single-perspective accumulator.
inline void AddColumn(std::array<int16_t, kHiddenSize> &acc, const int16_t *column)
{
    for (int i = 0; i < kHiddenSize; ++i)
    {
        acc[i] = static_cast<int16_t>(acc[i] + column[i]);
    }
}

/// @brief Subtract a feature column from a single-perspective accumulator (the inverse of AddColumn).
inline void SubColumn(std::array<int16_t, kHiddenSize> &acc, const int16_t *column)
{
    for (int i = 0; i < kHiddenSize; ++i)
    {
        acc[i] = static_cast<int16_t>(acc[i] - column[i]);
    }
}
#endif

/// @brief Right shift applied to the SCReLU activation (clamp(x,0,QA)^2, in [0, QA^2=65025]) to fit uint8.
/// S=9 maps the range onto [0,127], which keeps each `pmaddubsw` adjacent-pair sum (2*127*127 < 2^15) inside
/// int16 — so the AVX2 (maddubs) and VNNI (vpdpbusd) kernels are saturation-free and bit-identical. The eval
/// scale below folds 2^kInt8Shift back out. (S=8 would be more accurate but lets a rare pair saturate int16
/// on AVX2; if a future SPRT wants S=8, pair it with output_w_scale_=2 to stay saturation-free.)
constexpr int kInt8Shift = 9;

#if defined(__AVX2__)
/// @brief Pack four int32 vectors (each 8 lanes, values in [0,255]) into 32 uint8 in *natural* order
/// [a0..7, b0..7, c0..7, d0..7]. packus works within 128-bit lanes, so a final permute restores order.
inline __m256i PackU8(__m256i a, __m256i b, __m256i c, __m256i d)
{
    const __m256i ab   = _mm256_packus_epi32(a, b);
    const __m256i cd   = _mm256_packus_epi32(c, d);
    const __m256i abcd = _mm256_packus_epi16(ab, cd);
    return _mm256_permutevar8x32_epi32(abcd, _mm256_setr_epi32(0, 4, 1, 5, 2, 6, 3, 7));
}

/// @brief SCReLU 32 accumulator int16 lanes -> 32 uint8 (natural order): clamp [0,QA], square, round, >>S.
inline __m256i ScReluU8(const int16_t *a)
{
    const __m256i zero  = _mm256_setzero_si256();
    const __m256i qa    = _mm256_set1_epi16(static_cast<int16_t>(kQA));
    const __m256i round = _mm256_set1_epi32(1 << (kInt8Shift - 1));
    const __m256i v0    = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(a));
    const __m256i v1    = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(a + 16));
    const __m256i c0    = _mm256_min_epi16(_mm256_max_epi16(v0, zero), qa);
    const __m256i c1    = _mm256_min_epi16(_mm256_max_epi16(v1, zero), qa);
    auto          sq    = [&](__m256i c, bool high) {
        const __m256i w = high ? _mm256_cvtepi16_epi32(_mm256_extracti128_si256(c, 1))
                               : _mm256_cvtepi16_epi32(_mm256_castsi256_si128(c));
        return _mm256_srli_epi32(_mm256_add_epi32(_mm256_mullo_epi32(w, w), round), kInt8Shift);
    };
    return PackU8(sq(c0, false), sq(c0, true), sq(c1, false), sq(c1, true));
}

/// @brief Horizontal sum of the 8 int32 lanes.
inline int32_t HsumI32(__m256i v)
{
    __m128i s = _mm_add_epi32(_mm256_castsi256_si128(v), _mm256_extracti128_si256(v, 1));
    s         = _mm_add_epi32(s, _mm_shuffle_epi32(s, _MM_SHUFFLE(1, 0, 3, 2)));
    s         = _mm_add_epi32(s, _mm_shuffle_epi32(s, _MM_SHUFFLE(2, 3, 0, 1)));
    return _mm_cvtsi128_si32(s);
}

/// @brief The int8 output dot over both perspectives -> int32, AVX2 maddubs version: uint8*int8 -> int16
/// pairwise (saturating) + madd widening to int32. With kInt8Shift=9 the maddubs intermediate never
/// saturates, so this is bit-identical to the VNNI version. @p stm/@p ntm are the two perspective
/// accumulators; @p w is the int8 output weights (kHiddenSize for stm, then kHiddenSize for ntm).
[[gnu::target("avx2")]] inline int32_t OutputDotInt8Avx2(const int16_t *stm, const int16_t *ntm, const int8_t *w)
{
    __m256i        acc     = _mm256_setzero_si256();
    const int16_t *acts[2] = {stm, ntm};
    for (int side = 0; side < 2; ++side)
    {
        const int16_t *a  = acts[side];
        const int8_t  *ww = w + side * kHiddenSize;
        for (int i = 0; i < kHiddenSize; i += 32)
        {
            const __m256i u8   = ScReluU8(a + i);
            const __m256i w8   = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(ww + i));
            const __m256i prod = _mm256_maddubs_epi16(u8, w8);
            acc                = _mm256_add_epi32(acc, _mm256_madd_epi16(prod, _mm256_set1_epi16(1)));
        }
    }
    return HsumI32(acc);
}

/// @brief Same dot via the AVX-VNNI vpdpbusd instruction (exact int32 accumulation in one op). Compiled with
/// avxvnni codegen for THIS FUNCTION ONLY (function-level target attribute), so the engine binary stays a
/// baseline-AVX2 build that loads and runs on AVX2-only CPUs — the function is simply never called there
/// (Network::Evaluate dispatches on the runtime cpuid check below). Bit-identical to OutputDotInt8Avx2.
[[gnu::target("avx2,avxvnni")]] inline int32_t OutputDotInt8Vnni(const int16_t *stm, const int16_t *ntm,
                                                                 const int8_t *w)
{
    __m256i        acc     = _mm256_setzero_si256();
    const int16_t *acts[2] = {stm, ntm};
    for (int side = 0; side < 2; ++side)
    {
        const int16_t *a  = acts[side];
        const int8_t  *ww = w + side * kHiddenSize;
        for (int i = 0; i < kHiddenSize; i += 32)
        {
            const __m256i u8 = ScReluU8(a + i);
            const __m256i w8 = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(ww + i));
            acc              = _mm256_dpbusd_avx_epi32(acc, u8, w8);
        }
    }
    return HsumI32(acc);
}

/// @brief Whether to use the AVX-VNNI output-dot kernel. Detected once at static init (via platform::
/// HasAvxVnni, which handles the GCC/Clang vs MSVC cpuid difference) and read-only afterwards, so the Lazy
/// SMP threads share it without synchronisation.
inline const bool kCpuHasAvxVnni = platform::HasAvxVnni();
#endif // __AVX2__

/// @brief One perspective's indexing state for a position.
/// @param position The position. @param black true for the black perspective.
/// @return Its Perspective. The map is indexed by the king's square oriented to this perspective but
/// **not** mirrored, matching bullet: the expanded map already folds the file, so a square and its mirror
/// select the same bank.
inline Perspective PerspectiveFor(const Position &position, bool black)
{
    const int king     = (position.pieces_[kKing] & position.colors_[black ? kBlack : kWhite]).Lsb();
    const int oriented = black ? (king ^ kRankFlip) : king;
    // bullet's test is `ksq % 8 > 3` on the perspective's own king square. A file is unchanged by the
    // rank flip, so the raw square answers it for either perspective.
    const int mirror = kIsKingMirroringEnabled && (king & kFileFlip) >= 4 ? kFileFlip : 0;
    return Perspective{kKingBucketMap[static_cast<std::size_t>(oriented)] * kInputSize,
                       (black ? kRankFlip : 0) | mirror, black ? 1 : 0};
}

/// @brief Feature row for a piece in one perspective's bank:
/// `bank + (colour ^ colour_flip)*384 + (type - 1)*64 + (square ^ square_flip)`.
inline int FeatureRow(const Perspective &perspective, int color, int piece, int sq)
{
    return perspective.bank_ + (color ^ perspective.color_flip_) * 384 + (piece - kPawn) * 64 +
           (sq ^ perspective.square_flip_);
}

/// @brief Offset into feature_weights_ of a piece's column in one perspective's bank.
inline std::size_t Row(const Perspective &perspective, int color, int piece, int sq)
{
    return static_cast<std::size_t>(FeatureRow(perspective, color, piece, sq)) * kHiddenSize;
}

/// @brief The output head a position selects, matching bullet's `MaterialCount<N>` exactly: the divisor is
/// `ceil(32 / N)` and the bucket `(piece_count - 2) / divisor`, so at N = 8 it is `(piece_count - 2) / 4`.
/// At N = 1 the divisor is 32 and every position selects head 0.
inline int OutputBucket(const Position &position)
{
    constexpr int divisor = (32 + kNumOutputBuckets - 1) / kNumOutputBuckets;
    return (position.pieces_[kNone].PopCount() - 2) / divisor;
}

inline void Network::RefreshSide(std::array<int16_t, kHiddenSize> &side, const Position &position,
                                 const Perspective &perspective) const
{
    side = feature_bias_;
    for (int color = kWhite; color <= kBlack; ++color)
    {
        const Bitboard friendly = position.colors_[color];
        for (int piece = kPawn; piece <= kKing; ++piece)
        {
            for (Square s : position.pieces_[piece] & friendly)
            {
                AddColumn(side, &feature_weights_[Row(perspective, color, piece, s)]);
            }
        }
    }
}

inline void Network::DiffSide(std::array<int16_t, kHiddenSize> &side, const Position &from, const Position &to,
                              const Perspective &perspective) const
{
    for (int color = kWhite; color <= kBlack; ++color)
    {
        for (int piece = kPawn; piece <= kKing; ++piece)
        {
            const Bitboard from_bb = from.pieces_[piece] & from.colors_[color];
            const Bitboard to_bb   = to.pieces_[piece] & to.colors_[color];
            for (Square s : from_bb & ~to_bb)
            {
                SubColumn(side, &feature_weights_[Row(perspective, color, piece, s)]);
            }
            for (Square s : to_bb & ~from_bb)
            {
                AddColumn(side, &feature_weights_[Row(perspective, color, piece, s)]);
            }
        }
    }
}

/// @brief Report a refused net. Every refusal goes through here, so each one names the net and the one
/// rule that matters is visible in one place: **a refused net is not loaded, and nothing falls back to
/// another net.** A stock engine that silently fell back to its embedded net once reported a plausible Elo
/// for a net that never played a move — the match looked fine and the number meant nothing.
/// @param origin Path or label of the net. @param reason Why it was refused, naming the field and both
/// values where there is a field to name. @return false, always, so a caller can `return RefuseNet(...)`.
[[nodiscard]] inline bool RefuseNet(const std::string &origin, const std::string &reason)
{
    // To **both** streams, on purpose. The `info string` on stdout is what a UCI GUI reads; the plain line
    // on stderr is what a match harness keeps, and harnesses routinely drop info lines. A refusal only a
    // GUI can see is a refusal nobody reads when it matters, which is during an unattended match.
    std::cout << std::format("info string NNUE: refusing net '{}': {}\n", origin, reason) << std::flush;
    std::fprintf(stderr, "NNUE: refusing net '%s': %s\n", origin.c_str(), reason.c_str());
    return false;
}

/// @brief Whether a net file can be opened and read at all — which is a different question from whether
/// the engine will accept it, and the two need telling apart exactly once: at startup, where a net file
/// that is simply not there is a deployment situation the embedded copy legitimately covers, while a net
/// that was read and then **refused** is a mistake nothing should paper over.
/// @param path Path to test. @return true if it opens and at least one byte reads.
inline bool IsNetFileReadable(const std::string &path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
    {
        return false;
    }
    char first_byte = 0;
    return static_cast<bool>(in.read(&first_byte, 1));
}

/// @brief Whether the net file at @p path exists and describes **this build's** NNUE shape.
///
/// A test suite whose expected values belong to a particular net — the trainer reference evaluations, the
/// Bratko-Kopec move set, the golden bench signature — has nothing to compare against when the build is at
/// another shape, because the engine rightly refuses that net. This extends those suites' existing "skipped
/// (no net supplied)" contract from *absent* to *absent or of another shape*, which is what the Rust and Go
/// ports already do (`common::is_net_this_builds_shape`, `skipWithoutTheShippedNet`). A shape with no
/// trained net is gated instead by the synthetic net: `test_nnue_incremental` builds one in memory, and
/// `tools/new-shape-signature.sh` searches one over six fixed positions.
///
/// Only the architecture fields and the bucket map's digest are compared, never the payload checksums: a net
/// **of this shape** that is corrupt or truncated must still fail its suite loudly rather than quietly skip
/// it. This answers "is this the right kind of net", not "is this net sound" — Load answers that.
/// @param path Path to test. @return true if the file's header describes this build's shape exactly.
inline bool IsNetFileThisBuildsShape(const std::string &path)
{
    std::ifstream in(path, std::ios::binary);
    NetHeader     header{};
    if (!in.read(reinterpret_cast<char *>(&header), sizeof(header)))
    {
        return false;
    }
    const NetHeader expected = ExpectedNetHeader();
    return std::memcmp(header.magic_, expected.magic_, sizeof(expected.magic_)) == 0 &&
           header.format_version_ == expected.format_version_ &&
           header.features_per_bucket_ == expected.features_per_bucket_ &&
           header.king_buckets_ == expected.king_buckets_ && header.hidden_size_ == expected.hidden_size_ &&
           header.output_buckets_ == expected.output_buckets_ && header.flags_ == expected.flags_ &&
           header.activation_ == expected.activation_ && header.qa_ == expected.qa_ && header.qb_ == expected.qb_ &&
           header.eval_scale_ == expected.eval_scale_ && header.king_bucket_map_crc_ == expected.king_bucket_map_crc_ &&
           header.payload_bytes_ == expected.payload_bytes_;
}

inline bool Network::Load(const std::string &path)
{
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in)
    {
        return RefuseNet(path, "it cannot be opened");
    }
    const std::streamoff size = in.tellg();
    in.seekg(0);
    std::vector<std::uint8_t> buffer(static_cast<std::size_t>(size < 0 ? 0 : size));
    if (!buffer.empty())
    {
        in.read(reinterpret_cast<char *>(buffer.data()), static_cast<std::streamsize>(buffer.size()));
    }
    if (!in)
    {
        return RefuseNet(path, "it opened but could not be read");
    }
    return LoadFromMemory(buffer.data(), buffer.size(), path);
}

inline bool Network::LoadFromMemory(const std::uint8_t *data, std::size_t size, const std::string &origin)
{
    // `loaded_` is deliberately **not** cleared here. Every check below happens before a single weight is
    // written, so a refused net leaves this Network exactly as it was — if a net was already loaded it is
    // still loaded and still the one playing. Clearing it up front was its own trap: a mismatched net set
    // through `setoption name EvalFile` left the network unloaded while the engine still answered `readyok`,
    // and the first `go` then read weight arrays that Load had never filled. A refused net must change
    // nothing, not leave the engine in a state it has no defined behaviour for.

    // --- Check 1: the header. 64 bytes; magic, then its own CRC, then every architecture field against
    // this build's compile-time constants, then the king-bucket map's digest. An unstamped (raw bullet)
    // net, a net in the retired 32-byte PSN1 container and a net for another architecture all land here.
    if (size < sizeof(NetHeader))
    {
        return RefuseNet(
            origin, std::format("it is {} bytes, too short for the {}-byte RNNUE1 header", size, sizeof(NetHeader)));
    }
    if (std::memcmp(data, kNetMagic, sizeof(kNetMagic)) != 0)
    {
        return RefuseNet(origin, "it carries no 'RNNUE1' magic, so it is not a net in the shared container — a "
                                 "raw bullet net must be stamped with tools/stamp_net, and a net in the retired "
                                 "PSN1 container must be re-stamped");
    }

    NetHeader header{};
    std::memcpy(&header, data, sizeof(header));

    const std::uint64_t header_crc = crc64::Checksum(data, kNetHeaderCrcBytes);
    if (header.header_crc_ != header_crc)
    {
        return RefuseNet(origin,
                         std::format("its header is corrupt: header_crc is {:#018x}, the bytes checksum to {:#018x}",
                                     header.header_crc_, header_crc));
    }

    const NetHeader expected = ExpectedNetHeader();

    struct FieldCheck
    {
        const char   *name_;
        std::uint64_t in_file_;
        std::uint64_t in_engine_;
    };

    const FieldCheck checks[] = {
        {"format_version", header.format_version_, expected.format_version_},
        {"header_bytes", header.header_bytes_, expected.header_bytes_},
        {"features_per_bucket", header.features_per_bucket_, expected.features_per_bucket_},
        {"king_buckets", header.king_buckets_, expected.king_buckets_},
        {"hidden_size", header.hidden_size_, expected.hidden_size_},
        {"output_buckets", header.output_buckets_, expected.output_buckets_},
        {"flags", header.flags_, expected.flags_},
        {"activation", header.activation_, expected.activation_},
        {"qa", header.qa_, expected.qa_},
        {"qb", header.qb_, expected.qb_},
        {"eval_scale", header.eval_scale_, expected.eval_scale_},
        {"king_bucket_map_crc", header.king_bucket_map_crc_, expected.king_bucket_map_crc_},
        {"payload_bytes", header.payload_bytes_, expected.payload_bytes_},
    };
    for (const FieldCheck &check : checks)
    {
        if (check.in_file_ != check.in_engine_)
        {
            return RefuseNet(origin, std::format("{} is {:#x} in the file, {:#x} in this engine", check.name_,
                                                 check.in_file_, check.in_engine_));
        }
    }

    // `reserved` is refused unless every byte is zero. It is not pedantry: those four bytes are the only
    // room a later format version has to add a field without moving anything, and that only works if a
    // reader of *this* version refuses a net that has already used them. Accepting a non-zero reserved
    // would mean a version-2 net quietly loading into a version-1 build with one field unread.
    for (const std::uint8_t reserved_byte : header.reserved_)
    {
        if (reserved_byte != 0)
        {
            return RefuseNet(origin, std::format("its reserved bytes are {:#04x} {:#04x} {:#04x} {:#04x}, and "
                                                 "every one must be zero",
                                                 header.reserved_[0], header.reserved_[1], header.reserved_[2],
                                                 header.reserved_[3]));
        }
    }

    // --- Check 2: the payload. Its length must be exactly what the header claims — no bullet tail padding,
    // nothing appended — and then its checksum must match.
    const std::size_t payload_bytes = size - sizeof(NetHeader);
    if (payload_bytes != kNetPayloadBytes)
    {
        return RefuseNet(origin,
                         std::format("it carries {} payload bytes after the header, not the {} its payload_bytes "
                                     "claims",
                                     payload_bytes, kNetPayloadBytes));
    }
    const std::uint8_t *p           = data + sizeof(NetHeader);
    const std::uint64_t payload_crc = crc64::Checksum(p, payload_bytes);
    if (header.payload_crc_ != payload_crc)
    {
        return RefuseNet(origin,
                         std::format("its payload is corrupt: payload_crc is {:#018x}, the bytes checksum to {:#018x}",
                                     header.payload_crc_, payload_crc));
    }

    // Copy in bullet's save order: feature_weights, feature_bias, output_weights, output_bias.
    std::memcpy(feature_weights_.data(), p, feature_weights_.size() * sizeof(int16_t));
    p += feature_weights_.size() * sizeof(int16_t);
    std::memcpy(feature_bias_.data(), p, feature_bias_.size() * sizeof(int16_t));
    p += feature_bias_.size() * sizeof(int16_t);
    std::memcpy(output_weights_.data(), p, output_weights_.size() * sizeof(int16_t));
    p += output_weights_.size() * sizeof(int16_t);
    std::memcpy(output_bias_.data(), p, output_bias_.size() * sizeof(std::int32_t));

    // Requantise the output weights to int8 for the optional int8 forward pass. The scale keeps the
    // largest-magnitude output weight inside [-127, 127]; for nets whose weights already fit (|w| <= 127,
    // e.g. the shipped net) the scale is 1 and the int8 copy is lossless. (Always computed — it is one-time,
    // ~2 KB, and lets the int8 kernel skip per-eval setup; the int16 path ignores it.)
    int max_ow = 0;
    for (int16_t v : output_weights_)
    {
        max_ow = std::max(max_ow, std::abs(static_cast<int>(v)));
    }
    output_w_scale_ = std::max(1, (max_ow + 126) / 127);
    for (std::size_t i = 0; i < output_weights_.size(); ++i)
    {
        const long q          = std::lround(static_cast<double>(output_weights_[i]) / output_w_scale_);
        output_weights_i8_[i] = static_cast<int8_t>(std::max<long>(-127, std::min<long>(127, q)));
    }

    loaded_ = true;
    std::fprintf(stderr, "info string NNUE: loaded net '%s'\n", origin.c_str());
    return true;
}

inline void Network::Refresh(Accumulator &acc, const Position &position) const
{
    // Each perspective uses its own king's bucket bank; seed bias and add every piece.
    RefreshSide(acc.white_, position, PerspectiveFor(position, /*black=*/false));
    RefreshSide(acc.black_, position, PerspectiveFor(position, /*black=*/true));
}

inline void Network::Update(Accumulator &acc, const Position &from, const Position &to) const
{
    // A king move that changes its perspective's bank or its mirroring moves every feature row that
    // perspective reads, so refresh it from scratch; otherwise apply only the piece-placement delta within
    // the unchanged indexing. At most one king moves per ply, so at most one perspective refreshes.
    const Perspective wf = PerspectiveFor(from, /*black=*/false), wt = PerspectiveFor(to, /*black=*/false);
    const Perspective bf = PerspectiveFor(from, /*black=*/true), bt = PerspectiveFor(to, /*black=*/true);

    // Common case (~98%): neither king changed bucket. Find the changed squares directly from the two
    // colour bitboards (a move touches only 2-4 squares — quiet, capture, en passant, castling and
    // promotion are all handled uniformly) instead of scanning all 12 piece types. For each square a colour
    // vacated we subtract the piece that was there (read from `from`); for each it now occupies we add the
    // piece now there (read from `to`) — so a promotion's pawn->queen and a capture's two pieces fall out
    // for free. Both perspectives are updated together. A null move (identical placements) does no work.
    // This is bit-identical to a per-piece-type diff: the same set of feature columns is added/subtracted,
    // and int16 accumulator add/sub is order-independent. (No square keeps a colour while changing piece
    // type without moving, so reading the type from `from`/`to` is exact.)
    if (wf == wt && bf == bt)
    {
        for (int color = kWhite; color <= kBlack; ++color)
        {
            const Bitboard from_bb = from.colors_[color];
            const Bitboard to_bb   = to.colors_[color];
            for (Square s : from_bb & ~to_bb)
            {
                const int piece = from.squares_[s];
                SubColumn(acc.white_, &feature_weights_[Row(wt, color, piece, s)]);
                SubColumn(acc.black_, &feature_weights_[Row(bt, color, piece, s)]);
            }
            for (Square s : to_bb & ~from_bb)
            {
                const int piece = to.squares_[s];
                AddColumn(acc.white_, &feature_weights_[Row(wt, color, piece, s)]);
                AddColumn(acc.black_, &feature_weights_[Row(bt, color, piece, s)]);
            }
        }
        return;
    }

    // Rare: one king changed its perspective's bank or its mirroring. Refresh that perspective; diff the
    // other. Both faults are caught by the one comparison — see Perspective.
    if (wf != wt)
    {
        RefreshSide(acc.white_, to, wt);
    }
    else
    {
        DiffSide(acc.white_, from, to, wt);
    }
    if (bf != bt)
    {
        RefreshSide(acc.black_, to, bt);
    }
    else
    {
        DiffSide(acc.black_, from, to, bt);
    }
}

/// @brief Shared dequantisation tail: SCReLU leaves the dot in QA*QA*QB units; reduce one QA, add the bias
/// (QA*QB units), apply the eval scale, then remove the remaining QA*QB quantisation -> centipawns. The
/// result is clamped to +/-kMaxEvaluation: pathological max-material positions (e.g. many promoted queens)
/// can push the raw net output past int16, which would wrap when stored in the int16 eval cache and could
/// collide with the mate band. Clamping keeps |eval| below kWinThreshold so eval and mate scores never mix.
inline int Dequant(int64_t output, std::int32_t bias)
{
    output /= kQA;
    output += bias;
    output *= kScale;
    output /= (kQA * kQB);
    return static_cast<int>(std::clamp<int64_t>(output, -kMaxEvaluation, kMaxEvaluation));
}

inline int Network::Evaluate(const Accumulator &acc, const Position &position) const
{
    // The engine's evaluation: the **int8** output layer (the shipped default). Activations are requantised
    // to uint8 (SCReLU >> kInt8Shift) and the output weights to int8; the dot accumulates in int32 (the
    // Phase-1 study showed |dot| <= 86M << 2^31). This is ~1.86x faster than the exact int16 dot (a measured
    // +31.8 Elo at 8+0.08) at a bounded ~26 cp deviation from EvaluateExact. The side-to-move accumulator
    // feeds the first half of the output layer.
    const bool                              white_to_move = position.color_to_move_ == kWhite;
    const std::array<int16_t, kHiddenSize> &stm_acc       = white_to_move ? acc.white_ : acc.black_;
    const std::array<int16_t, kHiddenSize> &ntm_acc       = white_to_move ? acc.black_ : acc.white_;
    // The output head the piece count selects. The weights are bucket-major, so a head is a contiguous
    // 2 * kHiddenSize run and selecting one is a pointer offset the kernels never need to know about.
    const int bucket = OutputBucket(position);

    int64_t output = 0;
#if defined(__AVX2__)
    // dot8 ~= raw_dot / (output_w_scale_ * 2^kInt8Shift), so multiply that factor back in before dequant.
    // Pick the AVX-VNNI kernel at runtime on capable CPUs (bit-identical, ~faster output dot), else AVX2.
    const int16_t *stm = stm_acc.data();
    const int16_t *ntm = ntm_acc.data();
    const int8_t  *w   = output_weights_i8_.data() + static_cast<std::size_t>(bucket) * 2 * kHiddenSize;
    const int32_t  dot = kCpuHasAvxVnni ? OutputDotInt8Vnni(stm, ntm, w) : OutputDotInt8Avx2(stm, ntm, w);
    output             = static_cast<int64_t>(dot) * output_w_scale_ * (1 << kInt8Shift);
#else
    // Scalar int8 fallback, bit-identical to the AVX2 path: clamp [0,QA], square, round, >>S to uint8.
    int64_t dot8 = 0;
    for (int i = 0; i < kHiddenSize; ++i)
    {
        const auto u8 = [&](int16_t x) {
            const int c = x < 0 ? 0 : (x > kQA ? kQA : x);
            const int a = (c * c + (1 << (kInt8Shift - 1))) >> kInt8Shift;
            return a > 255 ? 255 : a;
        };
        const std::size_t w = static_cast<std::size_t>(bucket) * 2 * kHiddenSize;
        dot8 += static_cast<int64_t>(u8(stm_acc[i])) * output_weights_i8_[w + i];
        dot8 += static_cast<int64_t>(u8(ntm_acc[i])) * output_weights_i8_[w + kHiddenSize + i];
    }
    output = dot8 * output_w_scale_ * (1 << kInt8Shift);
#endif
    return Dequant(output, output_bias_[static_cast<std::size_t>(bucket)]);
}

inline int Network::EvaluateExact(const Accumulator &acc, const Position &position) const
{
    // Exact int16 SCReLU forward pass mirroring bullet's `simple` example. This is the **reference**
    // evaluator: it reproduces the trainer's eval within quantisation rounding (test_nnue, ±2 cp) and is the
    // baseline the int8 Evaluate above approximates. Not used on the search hot path.
    const bool                              white_to_move = position.color_to_move_ == kWhite;
    const std::array<int16_t, kHiddenSize> &stm_acc       = white_to_move ? acc.white_ : acc.black_;
    const std::array<int16_t, kHiddenSize> &ntm_acc       = white_to_move ? acc.black_ : acc.white_;
    const int                               bucket        = OutputBucket(position);
    const int16_t *bucket_weights = output_weights_.data() + static_cast<std::size_t>(bucket) * 2 * kHiddenSize;

    int64_t output = 0;
#if defined(__AVX2__)
    // Per lane: clamp(x,0,QA)^2 (int32, <=65025) times the weight (low 32 bits, as the scalar int multiply);
    // products are sign-extended to 64-bit and summed (exact, no overflow), so it is bit-identical to scalar.
    const __m256i  zero       = _mm256_setzero_si256();
    const __m256i  qa         = _mm256_set1_epi16(static_cast<int16_t>(kQA));
    __m256i        acc64      = _mm256_setzero_si256();
    const int16_t *acts[2]    = {stm_acc.data(), ntm_acc.data()};
    const int16_t *weights[2] = {bucket_weights, bucket_weights + kHiddenSize};
    for (int side = 0; side < 2; ++side)
    {
        const int16_t *a = acts[side];
        const int16_t *w = weights[side];
        for (int i = 0; i < kHiddenSize; i += 16)
        {
            const __m256i x   = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(a + i));
            const __m256i c   = _mm256_min_epi16(_mm256_max_epi16(x, zero), qa); // clamp to [0, QA]
            const __m256i wv  = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(w + i));
            const __m256i clo = _mm256_cvtepi16_epi32(_mm256_castsi256_si128(c));
            const __m256i chi = _mm256_cvtepi16_epi32(_mm256_extracti128_si256(c, 1));
            const __m256i wlo = _mm256_cvtepi16_epi32(_mm256_castsi256_si128(wv));
            const __m256i whi = _mm256_cvtepi16_epi32(_mm256_extracti128_si256(wv, 1));
            const __m256i plo = _mm256_mullo_epi32(_mm256_mullo_epi32(clo, clo), wlo);
            const __m256i phi = _mm256_mullo_epi32(_mm256_mullo_epi32(chi, chi), whi);
            acc64             = _mm256_add_epi64(acc64, _mm256_cvtepi32_epi64(_mm256_castsi256_si128(plo)));
            acc64             = _mm256_add_epi64(acc64, _mm256_cvtepi32_epi64(_mm256_extracti128_si256(plo, 1)));
            acc64             = _mm256_add_epi64(acc64, _mm256_cvtepi32_epi64(_mm256_castsi256_si128(phi)));
            acc64             = _mm256_add_epi64(acc64, _mm256_cvtepi32_epi64(_mm256_extracti128_si256(phi, 1)));
        }
    }
    alignas(32) int64_t lanes[4];
    _mm256_store_si256(reinterpret_cast<__m256i *>(lanes), acc64);
    output = lanes[0] + lanes[1] + lanes[2] + lanes[3];
#else
    for (int i = 0; i < kHiddenSize; ++i)
    {
        output += SCReLU(stm_acc[i]) * bucket_weights[i];
        output += SCReLU(ntm_acc[i]) * bucket_weights[kHiddenSize + i];
    }
#endif
    return Dequant(output, output_bias_[static_cast<std::size_t>(bucket)]);
}

inline int Network::Evaluate(const Position &position) const
{
    Accumulator acc;
    Refresh(acc, position);
    return Evaluate(acc, position);
}

inline int Network::EvaluateExact(const Position &position) const
{
    Accumulator acc;
    Refresh(acc, position);
    return EvaluateExact(acc, position);
}

} // namespace nnue

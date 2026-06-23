#pragma once
/// @file nnue.h NNUE (efficiently-updatable neural network) evaluation — the engine's only evaluator.
///
/// The network is a "perspective" net with bullet's `ChessBuckets` feature set:
///
///   768 inputs per (perspective, king bucket)  ->  kHiddenSize feature transformer  (x2 perspectives)
///   SCReLU activation, concat[own | opponent] = 2*kHiddenSize  ->  1 output (centipawns, stm relative)
///
/// The shipped net is **1024-wide with kNumKingBuckets = 4** — four king-square weight banks selected by
/// king file-pair (`kKingBucketMap`, below). On ~2.25B positions this beat the single-bank 1024 net (v8)
/// by **+11.4 ± 6.75 Elo at 8+0.08** (the bucket advantage shrinks with data — it was +29 at 750M — but
/// stays positive at scale). The bucket mechanism generalises over kNumKingBuckets: each perspective
/// selects one of kNumKingBuckets banks by *its own* king's square (in that perspective's orientation), the
/// feature row being `bucket * 768 + chess768_index`, matching bullet's `ChessBuckets::new(kKingBucketMap)`
/// (the same 64-entry map; see nnue.cpp). A king move that crosses a file-pair bucket boundary refreshes
/// that whole perspective's bank rather than diffing it.
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

#include "constants.h"
#include "position.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <utility>
#include <vector>
#if defined(__AVX2__)
#include <immintrin.h>
#endif
namespace nnue
{

constexpr int kInputSize      = 768; ///< Features per perspective per bucket: 2 colors * 6 types * 64 squares.
constexpr int kNumKingBuckets = 4;   ///< King-square weight banks (1 = single bank, i.e. no king buckets).
constexpr int kFeatureRows    = kInputSize * kNumKingBuckets; ///< Feature-transformer input rows (bucket-major).
constexpr int kHiddenSize     = 1024; ///< Feature-transformer / accumulator width per perspective.

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
/// the loader checks every one. Raw (headerless) bullet output is rejected — stamp it with tools/stamp_net.
#pragma pack(push, 1)

struct NetHeader
{
    char          magic_[4];       ///< kNetMagic.
    std::uint16_t format_version_; ///< kNetFormatVersion.
    std::uint16_t input_size_;     ///< kInputSize (features per perspective per bucket).
    std::uint16_t king_buckets_;   ///< kNumKingBuckets.
    std::uint16_t hidden_size_;    ///< kHiddenSize.
    std::int16_t  qa_;             ///< kQA.
    std::int16_t  qb_;             ///< kQB.
    std::int16_t  scale_;          ///< kScale.
    std::uint8_t  reserved_[14];   ///< Zero-padding to a round 32 bytes.
};

#pragma pack(pop)
static_assert(sizeof(NetHeader) == 32, "NetHeader must be exactly 32 bytes");

/// @brief The header describing the architecture this engine was compiled for.
/// @return A NetHeader populated from the compile-time NNUE constants.
inline NetHeader ExpectedNetHeader()
{
    NetHeader h{};
    h.magic_[0]       = kNetMagic[0];
    h.magic_[1]       = kNetMagic[1];
    h.magic_[2]       = kNetMagic[2];
    h.magic_[3]       = kNetMagic[3];
    h.format_version_ = kNetFormatVersion;
    h.input_size_     = static_cast<std::uint16_t>(kInputSize);
    h.king_buckets_   = static_cast<std::uint16_t>(kNumKingBuckets);
    h.hidden_size_    = static_cast<std::uint16_t>(kHiddenSize);
    h.qa_             = static_cast<std::int16_t>(kQA);
    h.qb_             = static_cast<std::int16_t>(kQB);
    h.scale_          = static_cast<std::int16_t>(kScale);
    return h;
}

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
    /// bounded ~26 cp of EvaluateExact. @param acc Accumulator matching the position.
    /// @param side_to_move Selects which perspective feeds the first half of the output layer.
    /// @return Score in centipawns from the side-to-move's perspective.
    int Evaluate(const Accumulator &acc, Color side_to_move) const;

    /// @brief Evaluate a position with a full accumulator refresh, int8 output layer (the engine eval).
    /// @param position Position to evaluate. @return Score in centipawns, side-to-move relative.
    int Evaluate(const Position &position) const;

    /// @brief Exact int16 SCReLU forward pass — the **reference** evaluator that reproduces the trainer's
    /// eval (test_nnue cross-checks it to bullet within ±2 cp) and that the int8 Evaluate approximates. Not
    /// used on the search hot path. @param acc Accumulator matching the position. @param side_to_move stm.
    /// @return Score in centipawns, side-to-move relative.
    int EvaluateExact(const Accumulator &acc, Color side_to_move) const;

    /// @brief Exact int16 evaluation with a full accumulator refresh (reference / diagnostics).
    /// @param position Position to evaluate. @return Score in centipawns, side-to-move relative.
    int EvaluateExact(const Position &position) const;

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
    alignas(64) std::array<int16_t, kHiddenSize> feature_bias_; ///< Feature-transformer biases (always int16).
    /// @brief Output weights: first kHiddenSize weight the side-to-move accumulator, next kHiddenSize the other.
    alignas(64) std::array<int16_t, 2 * kHiddenSize> output_weights_;
    /// @brief Output weights requantised to int8 for the shipped int8 forward pass (Network::Evaluate).
    /// w8 = round(output_weight / output_w_scale_) clamped to [-127,127]; the scale keeps the largest output
    /// weight in int8 range (1 for nets whose |weight| <= 127, e.g. the shipped v8). Populated by Load so the
    /// int8 kernel needs no per-eval setup; the int16 reference (EvaluateExact) uses output_weights_ instead.
    alignas(64) std::array<int8_t, 2 * kHiddenSize> output_weights_i8_{};
    int     output_w_scale_ = 1; ///< Divisor mapping the int16 output weights into int8 range.
    int16_t output_bias_    = 0; ///< Output bias, in QA*QB units.

  public:
    bool loaded_ = false; ///< Whether Load() has succeeded.
};

// ---- Definitions moved from nnue.cpp (header-only) ----

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

/// @brief uint8 x int8 dot, accumulated into an int32 vector. Uses VNNI vpdpbusd where available (exact int32
/// accumulation); else AVX2 maddubs (uint8*int8 -> int16 pairwise, saturating) + madd widening to int32.
/// With kInt8Shift=9 the maddubs intermediate never saturates, so both forms give identical results.
inline __m256i DotU8I8(__m256i acc, __m256i u8, __m256i w8)
{
#if defined(__AVXVNNI__) || defined(__AVX512VNNI__)
    return _mm256_dpbusd_avx_epi32(acc, u8, w8);
#else
    const __m256i prod = _mm256_maddubs_epi16(u8, w8);
    return _mm256_add_epi32(acc, _mm256_madd_epi16(prod, _mm256_set1_epi16(1)));
#endif
}

/// @brief Horizontal sum of the 8 int32 lanes.
inline int32_t HsumI32(__m256i v)
{
    __m128i s = _mm_add_epi32(_mm256_castsi256_si128(v), _mm256_extracti128_si256(v, 1));
    s         = _mm_add_epi32(s, _mm_shuffle_epi32(s, _MM_SHUFFLE(1, 0, 3, 2)));
    s         = _mm_add_epi32(s, _mm_shuffle_epi32(s, _MM_SHUFFLE(2, 3, 0, 1)));
    return _mm_cvtsi128_si32(s);
}
#endif // __AVX2__

/// @brief King-square -> weight-bank map (bullet `ChessBuckets`). Indexed by a square in the *perspective's
/// own* orientation (white king square directly; black king square ^ kRankFlip), so the same 64-entry map
/// serves both perspectives. MUST be byte-identical to the array passed to ChessBuckets::new in
/// tools/bullet/*.rs. The shipped net is all-zero: a single weight bank (no king buckets, kNumKingBuckets=1),
/// i.e. plain Chess768. The map is retained so a future bucketed net only needs to change these values
/// (and kNumKingBuckets) to match.
constexpr std::array<int, 64> kKingBucketMap = {
    0, 0, 1, 1, 2, 2, 3, 3, //
    0, 0, 1, 1, 2, 2, 3, 3, //
    0, 0, 1, 1, 2, 2, 3, 3, //
    0, 0, 1, 1, 2, 2, 3, 3, //
    0, 0, 1, 1, 2, 2, 3, 3, //
    0, 0, 1, 1, 2, 2, 3, 3, //
    0, 0, 1, 1, 2, 2, 3, 3, //
    0, 0, 1, 1, 2, 2, 3, 3, //
};

/// @brief Feature row for a piece in one perspective's bucket bank.
///   white perspective: bucket*768 + color*384 + (type-1)*64 + square
///   black perspective: bucket*768 + (1-color)*384 + (type-1)*64 + (square ^ kRankFlip)
inline int FeatureRow(int bucket, int color, int piece, int sq, bool black)
{
    const int type_offset = (piece - kPawn) * 64;
    const int base = black ? ((1 - color) * 384 + type_offset + (sq ^ kRankFlip)) : (color * 384 + type_offset + sq);
    return bucket * kInputSize + base;
}

/// @brief Byte offset into feature_weights_ of a piece's column in one perspective's bucket bank.
inline std::size_t Row(int bucket, int color, int piece, int sq, bool black)
{
    return static_cast<std::size_t>(FeatureRow(bucket, color, piece, sq, black)) * kHiddenSize;
}

/// @brief A perspective's king bucket: index the map with that perspective's king square, oriented to it.
inline int WhiteBucket(const Position &p)
{
    return kKingBucketMap[(p.pieces_[kKing] & p.colors_[kWhite]).Lsb()];
}

inline int BlackBucket(const Position &p)
{
    return kKingBucketMap[(p.pieces_[kKing] & p.colors_[kBlack]).Lsb() ^ kRankFlip];
}

inline void Network::RefreshSide(std::array<int16_t, kHiddenSize> &side, const Position &position, int bucket,
                                 bool black) const
{
    side = feature_bias_;
    for (int color = kWhite; color <= kBlack; ++color)
    {
        const Bitboard friendly = position.colors_[color];
        for (int piece = kPawn; piece <= kKing; ++piece)
        {
            for (Square s : position.pieces_[piece] & friendly)
            {
                AddColumn(side, &feature_weights_[Row(bucket, color, piece, s, black)]);
            }
        }
    }
}

inline void Network::DiffSide(std::array<int16_t, kHiddenSize> &side, const Position &from, const Position &to,
                              int bucket, bool black) const
{
    for (int color = kWhite; color <= kBlack; ++color)
    {
        for (int piece = kPawn; piece <= kKing; ++piece)
        {
            const Bitboard from_bb = from.pieces_[piece] & from.colors_[color];
            const Bitboard to_bb   = to.pieces_[piece] & to.colors_[color];
            for (Square s : from_bb & ~to_bb)
            {
                SubColumn(side, &feature_weights_[Row(bucket, color, piece, s, black)]);
            }
            for (Square s : to_bb & ~from_bb)
            {
                AddColumn(side, &feature_weights_[Row(bucket, color, piece, s, black)]);
            }
        }
    }
}

inline bool Network::Load(const std::string &path)
{
    loaded_ = false;
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in)
    {
        std::fprintf(stderr, "info string NNUE: cannot open net file '%s'\n", path.c_str());
        return false;
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
        std::fprintf(stderr, "info string NNUE: error reading net file '%s'\n", path.c_str());
        return false;
    }
    return LoadFromMemory(buffer.data(), buffer.size(), path);
}

inline bool Network::LoadFromMemory(const std::uint8_t *data, std::size_t size, const std::string &origin)
{
    loaded_ = false;

    // Only a Pawnstar-stamped image is accepted: a 32-byte NetHeader (magic + architecture) immediately
    // followed by the payload. Every architecture field is checked against this engine's constants, so an
    // incompatible — or an unstamped/raw bullet — net is rejected with a clear message instead of being
    // silently misread. Stamp a raw bullet net with tools/stamp_net (which embeds the building engine's
    // constants) before loading it.
    NetHeader header{};
    if (size < sizeof(NetHeader) || std::memcmp(data, kNetMagic, sizeof(kNetMagic)) != 0)
    {
        std::fprintf(stderr,
                     "info string NNUE: net '%s' is not a stamped Pawnstar net (missing '%.4s' header) — "
                     "stamp it with tools/stamp_net\n",
                     origin.c_str(), kNetMagic);
        return false;
    }
    std::memcpy(&header, data, sizeof(header));

    // Validate the stamped architecture fields against this build.
    const NetHeader expected = ExpectedNetHeader();
    if (header.format_version_ != expected.format_version_ || header.input_size_ != expected.input_size_ ||
        header.king_buckets_ != expected.king_buckets_ || header.hidden_size_ != expected.hidden_size_ ||
        header.qa_ != expected.qa_ || header.qb_ != expected.qb_ || header.scale_ != expected.scale_)
    {
        std::fprintf(stderr,
                     "info string NNUE: net '%s' is incompatible — file is "
                     "v%u/in%u/buckets%u/hidden%u/qa%d/qb%d/scale%d, engine expects "
                     "v%u/in%u/buckets%u/hidden%u/qa%d/qb%d/scale%d\n",
                     origin.c_str(), header.format_version_, header.input_size_, header.king_buckets_,
                     header.hidden_size_, header.qa_, header.qb_, header.scale_, expected.format_version_,
                     expected.input_size_, expected.king_buckets_, expected.hidden_size_, expected.qa_, expected.qb_,
                     expected.scale_);
        return false;
    }
    const std::size_t payload_offset = sizeof(NetHeader);

    // The packed payload (feature_weights, feature_bias, output_weights, output_bias) is exactly
    // kNetFileBytes; the image may have a header before it and bullet padding after it.
    if (size < payload_offset + kNetFileBytes)
    {
        std::fprintf(stderr, "info string NNUE: net '%s' truncated\n", origin.c_str());
        return false;
    }

    // Copy in bullet's save order: feature_weights, feature_bias, output_weights, output_bias.
    const std::uint8_t *p = data + payload_offset;
    std::memcpy(feature_weights_.data(), p, feature_weights_.size() * sizeof(int16_t));
    p += feature_weights_.size() * sizeof(int16_t);
    std::memcpy(feature_bias_.data(), p, feature_bias_.size() * sizeof(int16_t));
    p += feature_bias_.size() * sizeof(int16_t);
    std::memcpy(output_weights_.data(), p, output_weights_.size() * sizeof(int16_t));
    p += output_weights_.size() * sizeof(int16_t);
    std::memcpy(&output_bias_, p, sizeof(output_bias_));

    // Requantise the output weights to int8 for the optional int8 forward pass. The scale keeps the
    // largest-magnitude output weight inside [-127, 127]; for nets whose weights already fit (|w| <= 127,
    // e.g. the shipped v8) the scale is 1 and the int8 copy is lossless. (Always computed — it is one-time,
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
    RefreshSide(acc.white_, position, WhiteBucket(position), /*black=*/false);
    RefreshSide(acc.black_, position, BlackBucket(position), /*black=*/true);
}

inline void Network::Update(Accumulator &acc, const Position &from, const Position &to) const
{
    // A king move that crosses a bucket boundary changes that whole perspective's bank, so refresh it from
    // scratch; otherwise apply only the piece-placement delta within the (unchanged) bank. At most one king
    // moves per ply, so at most one perspective refreshes.
    const int wf = WhiteBucket(from), wt = WhiteBucket(to);
    const int bf = BlackBucket(from), bt = BlackBucket(to);

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
                SubColumn(acc.white_, &feature_weights_[Row(wt, color, piece, s, false)]);
                SubColumn(acc.black_, &feature_weights_[Row(bt, color, piece, s, true)]);
            }
            for (Square s : to_bb & ~from_bb)
            {
                const int piece = to.squares_[s];
                AddColumn(acc.white_, &feature_weights_[Row(wt, color, piece, s, false)]);
                AddColumn(acc.black_, &feature_weights_[Row(bt, color, piece, s, true)]);
            }
        }
        return;
    }

    // Rare: one king crossed a bucket boundary. Refresh that perspective; diff the other.
    if (wf != wt)
    {
        RefreshSide(acc.white_, to, wt, /*black=*/false);
    }
    else
    {
        DiffSide(acc.white_, from, to, wt, /*black=*/false);
    }
    if (bf != bt)
    {
        RefreshSide(acc.black_, to, bt, /*black=*/true);
    }
    else
    {
        DiffSide(acc.black_, from, to, bt, /*black=*/true);
    }
}

/// @brief Shared dequantisation tail: SCReLU leaves the dot in QA*QA*QB units; reduce one QA, add the bias
/// (QA*QB units), apply the eval scale, then remove the remaining QA*QB quantisation -> centipawns.
inline int Dequant(int64_t output, int16_t bias)
{
    output /= kQA;
    output += bias;
    output *= kScale;
    output /= (kQA * kQB);
    return static_cast<int>(output);
}

inline int Network::Evaluate(const Accumulator &acc, Color side_to_move) const
{
    // The engine's evaluation: the **int8** output layer (the shipped default). Activations are requantised
    // to uint8 (SCReLU >> kInt8Shift) and the output weights to int8; the dot accumulates in int32 (the
    // Phase-1 study showed |dot| <= 86M << 2^31). This is ~1.86x faster than the exact int16 dot (a measured
    // +31.8 Elo at 8+0.08) at a bounded ~26 cp deviation from EvaluateExact. The side-to-move accumulator
    // feeds the first half of the output layer.
    const bool                              white_to_move = side_to_move == kWhite;
    const std::array<int16_t, kHiddenSize> &stm_acc       = white_to_move ? acc.white_ : acc.black_;
    const std::array<int16_t, kHiddenSize> &ntm_acc       = white_to_move ? acc.black_ : acc.white_;

    int64_t output = 0;
#if defined(__AVX2__)
    // dot8 ~= raw_dot / (output_w_scale_ * 2^kInt8Shift), so multiply that factor back in before dequant.
    __m256i        acc32      = _mm256_setzero_si256();
    const int16_t *acts[2]    = {stm_acc.data(), ntm_acc.data()};
    const int8_t  *weights[2] = {output_weights_i8_.data(), output_weights_i8_.data() + kHiddenSize};
    for (int side = 0; side < 2; ++side)
    {
        const int16_t *a = acts[side];
        const int8_t  *w = weights[side];
        for (int i = 0; i < kHiddenSize; i += 32)
        {
            const __m256i u8 = ScReluU8(a + i);
            const __m256i w8 = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(w + i));
            acc32            = DotU8I8(acc32, u8, w8);
        }
    }
    output = static_cast<int64_t>(HsumI32(acc32)) * output_w_scale_ * (1 << kInt8Shift);
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
        dot8 += static_cast<int64_t>(u8(stm_acc[i])) * output_weights_i8_[i];
        dot8 += static_cast<int64_t>(u8(ntm_acc[i])) * output_weights_i8_[kHiddenSize + i];
    }
    output = dot8 * output_w_scale_ * (1 << kInt8Shift);
#endif
    return Dequant(output, output_bias_);
}

inline int Network::EvaluateExact(const Accumulator &acc, Color side_to_move) const
{
    // Exact int16 SCReLU forward pass mirroring bullet's `simple` example. This is the **reference**
    // evaluator: it reproduces the trainer's eval within quantisation rounding (test_nnue, ±2 cp) and is the
    // baseline the int8 Evaluate above approximates. Not used on the search hot path.
    const bool                              white_to_move = side_to_move == kWhite;
    const std::array<int16_t, kHiddenSize> &stm_acc       = white_to_move ? acc.white_ : acc.black_;
    const std::array<int16_t, kHiddenSize> &ntm_acc       = white_to_move ? acc.black_ : acc.white_;

    int64_t output = 0;
#if defined(__AVX2__)
    // Per lane: clamp(x,0,QA)^2 (int32, <=65025) times the weight (low 32 bits, as the scalar int multiply);
    // products are sign-extended to 64-bit and summed (exact, no overflow), so it is bit-identical to scalar.
    const __m256i  zero       = _mm256_setzero_si256();
    const __m256i  qa         = _mm256_set1_epi16(static_cast<int16_t>(kQA));
    __m256i        acc64      = _mm256_setzero_si256();
    const int16_t *acts[2]    = {stm_acc.data(), ntm_acc.data()};
    const int16_t *weights[2] = {output_weights_.data(), output_weights_.data() + kHiddenSize};
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
        output += SCReLU(stm_acc[i]) * output_weights_[i];
        output += SCReLU(ntm_acc[i]) * output_weights_[kHiddenSize + i];
    }
#endif
    return Dequant(output, output_bias_);
}

inline int Network::Evaluate(const Position &position) const
{
    Accumulator acc;
    Refresh(acc, position);
    return Evaluate(acc, position.color_to_move_);
}

inline int Network::EvaluateExact(const Position &position) const
{
    Accumulator acc;
    Refresh(acc, position);
    return EvaluateExact(acc, position.color_to_move_);
}

} // namespace nnue

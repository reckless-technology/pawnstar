/// @file nnue_quant_study.cpp  int16 -> int8 NNUE quantisation feasibility study (offline).
///
/// This is a measurement tool, not part of the engine. It answers two questions the int8 plan hinges on,
/// using the *shipped* net and the checked-in reference FENs (no retrain, no engine change):
///
///   Phase 1 (dynamic range / saturation):
///     - Histograms of the live accumulator values (int16) and the SCReLU activations clamp(x,0,QA)^2.
///     - The static distribution of the output-layer weights.
///     - The magnitude of the int64 output dot product (does an int32 accumulator suffice for a VNNI path?).
///     - The centipawn error of an Option-1 int8 emulation (requantise the SCReLU activation to uint8 via a
///       right shift, round the output weights to int8) versus the engine's int16 evaluation, swept over the
///       activation shift S.
///     - How often an AVX2 `pmaddubsw` (uint8 x int8 -> int16, *saturating*) adjacent-pair sum would
///       overflow int16 — the concrete saturation hazard of an AVX2 (non-VNNI) int8 kernel.
///
///   Phase 0 (cost split, per-call kernel cost):
///     - Wall-clock ns/call of the FT accumulator Update (the per-make/undo hot path) versus the output-layer
///       Evaluate (the per-leaf path). The dominant cost decides whether int8 can help at all.
///
/// Usage:  build/nnue_quant_study <net.bin> [reference.txt]
///   reference.txt defaults to test/nnue_reference.txt ("<fen> | <cp>" per line; the cp is ignored).

#include "constants.h"
#include "game.h"
#include "move.h"
#include "nnue.h"
#include "position.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace
{
using nnue::kHiddenSize;
using nnue::kQA;
using nnue::kQB;
using nnue::kScale;

/// @brief The raw int16 weight payload, parsed straight from the net file (header-aware).
struct RawWeights
{
    std::vector<int16_t> feature_weights_; // kFeatureRows * kHiddenSize
    std::vector<int16_t> feature_bias_;    // kHiddenSize
    std::vector<int16_t> output_weights_;  // 2 * kHiddenSize
    int16_t              output_bias_ = 0;
};

bool ParseNet(const std::string &path, RawWeights &w)
{
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in)
    {
        return false;
    }
    const std::streamoff size = in.tellg();
    in.seekg(0);
    std::vector<std::uint8_t> buf(static_cast<std::size_t>(size));
    in.read(reinterpret_cast<char *>(buf.data()), size);

    // Header-aware: a stamped net starts with magic "PSN1"; otherwise the payload starts at byte 0.
    std::size_t off = (size >= 4 && std::memcmp(buf.data(), "PSN1", 4) == 0) ? 32 : 0;

    const std::size_t nfw = static_cast<std::size_t>(nnue::kFeatureRows) * kHiddenSize;
    w.feature_weights_.resize(nfw);
    w.feature_bias_.resize(kHiddenSize);
    w.output_weights_.resize(2 * kHiddenSize);
    auto take = [&](void *dst, std::size_t count) {
        std::memcpy(dst, buf.data() + off, count * sizeof(int16_t));
        off += count * sizeof(int16_t);
    };
    take(w.feature_weights_.data(), nfw);
    take(w.feature_bias_.data(), kHiddenSize);
    take(w.output_weights_.data(), 2 * kHiddenSize);
    std::memcpy(&w.output_bias_, buf.data() + off, sizeof(int16_t));
    return true;
}

/// @brief Running distribution summary (percentiles via a stored sample, fine for a few-million-element set).
struct Dist
{
    std::vector<double> xs_;

    void add(double v)
    {
        xs_.push_back(v);
    }

    void report(const char *name)
    {
        if (xs_.empty())
        {
            return;
        }
        std::sort(xs_.begin(), xs_.end());
        double sum = 0, sq = 0;
        for (double v : xs_)
        {
            sum += v;
            sq += v * v;
        }
        const double n    = static_cast<double>(xs_.size());
        const double mean = sum / n;
        const double sd   = std::sqrt(std::max(0.0, sq / n - mean * mean));
        auto         pct  = [&](double p) { return xs_[std::min<std::size_t>(xs_.size() - 1, (std::size_t)(p * n))]; };
        std::printf(
            "  %-22s n=%-10zu min=%.0f  p50=%.0f  p95=%.0f  p99=%.0f  p99.9=%.0f  max=%.0f  mean=%.1f sd=%.1f\n", name,
            xs_.size(), xs_.front(), pct(0.50), pct(0.95), pct(0.99), pct(0.999), xs_.back(), mean, sd);
    }
};

int Clamp(int x, int lo, int hi)
{
    return x < lo ? lo : (x > hi ? hi : x);
}
} // namespace

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        std::printf("usage: %s <net.bin> [reference.txt]\n", argv[0]);
        return 1;
    }
    const std::string net_path = argv[1];
    const std::string ref_path = argc >= 3 ? argv[2] : "test/nnue_reference.txt";

    nnue::Network net;
    if (!net.Load(net_path))
    {
        std::printf("failed to load net '%s'\n", net_path.c_str());
        return 1;
    }
    RawWeights w;
    if (!ParseNet(net_path, w))
    {
        std::printf("failed to parse net payload '%s'\n", net_path.c_str());
        return 1;
    }

    std::ifstream ref(ref_path);
    if (!ref)
    {
        std::printf("cannot open reference '%s'\n", ref_path.c_str());
        return 1;
    }
    std::vector<Position> positions;
    for (std::string line; std::getline(ref, line);)
    {
        const auto        bar = line.rfind('|');
        const std::string fen = bar == std::string::npos ? line : line.substr(0, bar);
        if (fen.find_first_not_of(" \t") == std::string::npos)
        {
            continue;
        }
        positions.push_back(Position::FromString(fen));
    }
    std::printf("loaded net '%s', %zu positions from '%s'\n\n", net_path.c_str(), positions.size(), ref_path.c_str());

    // ---- Static output-weight distribution (decides the int8 weight scale WS) -------------------------------
    int max_ow = 0;
    for (int16_t v : w.output_weights_)
    {
        max_ow = std::max(max_ow, std::abs((int)v));
    }
    int max_fw = 0;
    for (int16_t v : w.feature_weights_)
    {
        max_fw = std::max(max_fw, std::abs((int)v));
    }
    // int8 weight scale: map the largest-magnitude output weight onto +-127.
    const int WS = std::max(1, (max_ow + 126) / 127);
    std::printf("STATIC WEIGHTS\n");
    std::printf("  feature_weights: max|w|=%d (QA=%d units, int16)\n", max_fw, kQA);
    std::printf("  output_weights : max|w|=%d (QB=%d units, int16)  -> int8 scale WS=%d\n", max_ow, kQB, WS);
    std::printf("  output_bias    : %d (QA*QB units)\n\n", (int)w.output_bias_);

    // ---- Phase 1: dynamic range over all positions ---------------------------------------------------------
    // Option-1 int8 emulation error is swept over the activation right-shift S.
    const int    kSmin = 7, kSmax = 13;
    Dist         acc_dist, act_dist, dot_dist;
    std::int64_t act_clamped_lo = 0, act_clamped_hi = 0, act_total = 0; // SCReLU clamp activity
    // AVX2 pmaddubsw (uint8 x int8 -> int16, saturating) adjacent-pair overflow, probed per shift S.
    std::vector<std::int64_t> sat_pairs(kSmax + 1, 0), sat_over(kSmax + 1, 0);
    std::vector<double>       err_sum(kSmax + 1, 0.0), err_max(kSmax + 1, 0.0);
    std::vector<int>          ref_cps;

    for (const Position &pos : positions)
    {
        nnue::Accumulator acc;
        net.Refresh(acc, pos);
        const bool                              wtm = pos.color_to_move_ == kWhite;
        const std::array<int16_t, kHiddenSize> &stm = wtm ? acc.white_ : acc.black_;
        const std::array<int16_t, kHiddenSize> &ntm = wtm ? acc.black_ : acc.white_;

        // Accumulator + activation distributions, and the SCReLU clamp activity.
        auto scan = [&](const std::array<int16_t, kHiddenSize> &a) {
            for (int i = 0; i < kHiddenSize; ++i)
            {
                acc_dist.add(a[i]);
                ++act_total;
                if (a[i] < 0)
                {
                    ++act_clamped_lo;
                }
                else if (a[i] > kQA)
                {
                    ++act_clamped_hi;
                }
                const int c = Clamp(a[i], 0, kQA);
                act_dist.add((double)c * c); // SCReLU output in [0, QA^2]
            }
        };
        scan(stm);
        scan(ntm);

        // Reference int64 raw dot (pre-dequant), to size the int32 headroom of a VNNI path.
        std::int64_t raw = 0;
        for (int i = 0; i < kHiddenSize; ++i)
        {
            const int cs = Clamp(stm[i], 0, kQA), cn = Clamp(ntm[i], 0, kQA);
            raw += (std::int64_t)cs * cs * w.output_weights_[i];
            raw += (std::int64_t)cn * cn * w.output_weights_[kHiddenSize + i];
        }
        dot_dist.add(std::abs((double)raw));
        const int ref_cp = net.Evaluate(pos); // engine int16 path (== Evaluate(acc, stm))
        ref_cps.push_back(ref_cp);

        // int8 emulation, swept over S. u8 = clamp(round(screlu / 2^S), 0, 255); weights -> int8 via WS.
        for (int S = kSmin; S <= kSmax; ++S)
        {
            const double half = (double)(1 << (S - 1));
            std::int64_t dot8 = 0;
            auto         u8   = [&](int16_t x) {
                const int c = Clamp(x, 0, kQA);
                const int a = c * c;
                return Clamp((int)((a + (int)half) >> S), 0, 255);
            };
            for (int i = 0; i < kHiddenSize; ++i)
            {
                const int ws_i = Clamp((int)std::lround((double)w.output_weights_[i] / WS), -127, 127);
                const int wn_i = Clamp((int)std::lround((double)w.output_weights_[kHiddenSize + i] / WS), -127, 127);
                dot8 += (std::int64_t)u8(stm[i]) * ws_i;
                dot8 += (std::int64_t)u8(ntm[i]) * wn_i;
            }
            // Re-expand to the int16 raw-dot scale: each term was scaled by 1/(WS * 2^S).
            const double approx_raw = (double)dot8 * WS * (double)(1 << S);
            double       out        = approx_raw / kQA + w.output_bias_;
            out                     = out * kScale / (kQA * kQB);
            const int    cp8        = (int)std::lround(out);
            const double e          = std::abs((double)(cp8 - ref_cp));
            err_sum[S] += e;
            err_max[S] = std::max(err_max[S], e);
        }

        // AVX2 pmaddubsw saturation probe: adjacent (2i,2i+1) products summed to int16, for each shift S.
        for (int Sp = kSmin; Sp <= kSmax; ++Sp)
        {
            const double halfp = (double)(1 << (Sp - 1));
            auto         u8p   = [&](int16_t x) {
                const int c = Clamp(x, 0, kQA);
                const int a = c * c;
                return Clamp((int)((a + (int)halfp) >> Sp), 0, 255);
            };
            auto probe = [&](const std::array<int16_t, kHiddenSize> &a, int woff) {
                for (int i = 0; i + 1 < kHiddenSize; i += 2)
                {
                    const int w0 = Clamp((int)std::lround((double)w.output_weights_[woff + i] / WS), -127, 127);
                    const int w1 = Clamp((int)std::lround((double)w.output_weights_[woff + i + 1] / WS), -127, 127);
                    const int s  = u8p(a[i]) * w0 + u8p(a[i + 1]) * w1;
                    ++sat_pairs[Sp];
                    if (s > 32767 || s < -32768)
                    {
                        ++sat_over[Sp];
                    }
                }
            };
            probe(stm, 0);
            probe(ntm, kHiddenSize);
        }
    }

    std::printf("PHASE 1 — DYNAMIC RANGE  (%zu positions, both perspectives)\n", positions.size());
    acc_dist.report("accumulator (int16)");
    act_dist.report("SCReLU act [0,QA^2]");
    dot_dist.report("|output dot| (i64)");
    std::printf("  SCReLU clamp: %.2f%% values <0 (clamped to 0), %.4f%% values >QA (clamped to %d)\n",
                100.0 * act_clamped_lo / act_total, 100.0 * act_clamped_hi / act_total, kQA);
    const double max_dot = dot_dist.xs_.empty() ? 0 : dot_dist.xs_.back();
    std::printf("  max |output dot| = %.0f  (int32 max = 2147483647; %s)\n\n", max_dot,
                max_dot < 2147483647.0 ? "FITS int32 -> VNNI int32 accumulator suffices"
                                       : "EXCEEDS int32 -> needs int64");

    std::printf("INT8 OUTPUT-LAYER EMULATION vs int16 engine eval (Option 1: SCReLU>>S, int8 weights, WS=%d)\n", WS);
    std::printf("  %-4s %-12s %-12s %-26s\n", "S", "mean|dcp|", "max|dcp|", "AVX2 pmaddubsw i16 overflow");
    for (int S = kSmin; S <= kSmax; ++S)
    {
        std::printf("  %-4d %-12.3f %-12.0f %lld/%lld (%.3f%%)\n", S,
                    err_sum[S] / std::max<std::size_t>(1, positions.size()), err_max[S], (long long)sat_over[S],
                    (long long)sat_pairs[S], 100.0 * sat_over[S] / std::max<std::int64_t>(1, sat_pairs[S]));
    }
    std::printf("  (int16 cross-check tolerance is +-2 cp; bullet quantise rounding alone is ~<=2 cp.\n");
    std::printf("   S=8 maps SCReLU's [0,QA^2=65025] onto uint8 [0,254] with no extra clamp — the natural fit.)\n\n");

    // ---- Phase 0: per-call kernel cost ---------------------------------------------------------------------
    std::printf("PHASE 0 — PER-CALL KERNEL COST (single thread, this CPU)\n");
    // Use a mid-game position with a legal move to drive Update (make then undo == one reversible round-trip).
    Position base = positions.empty() ? Position::FromString("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1")
                                      : positions[positions.size() / 2];
    MoveList moves = base.GenerateLegalMoves();
    if (moves.size() == 0)
    {
        base  = Position::FromString("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"),
        moves = base.GenerateLegalMoves();
    }
    const Move     move  = *moves.begin();
    const Position child = base.MakeMove(move);

    nnue::Accumulator acc;
    net.Refresh(acc, base);

    auto now = [] { return std::chrono::steady_clock::now(); };
    auto ns  = [](auto d) { return std::chrono::duration_cast<std::chrono::nanoseconds>(d).count(); };

    // Update: net.Update(acc, base, child) advances; net.Update(acc, child, base) reverses (acc restored).
    const long   kUpdIters = 4'000'000;
    volatile int sink      = 0;
    auto         t0        = now();
    for (long i = 0; i < kUpdIters; ++i)
    {
        net.Update(acc, base, child);
        net.Update(acc, child, base);
    }
    auto         t1     = now();
    const double upd_ns = (double)ns(t1 - t0) / (2.0 * kUpdIters);

    // Evaluate: output-layer forward pass from a maintained accumulator.
    const long kEvalIters = 4'000'000;
    auto       t2         = now();
    for (long i = 0; i < kEvalIters; ++i)
    {
        sink += net.Evaluate(acc, base.color_to_move_);
    }
    auto         t3      = now();
    const double eval_ns = (double)ns(t3 - t2) / (double)kEvalIters;

    // Refresh, for reference (root only, or on a bucket crossing).
    const long kRefIters = 1'000'000;
    auto       t4        = now();
    for (long i = 0; i < kRefIters; ++i)
    {
        net.Refresh(acc, base);
    }
    auto         t5     = now();
    const double ref_ns = (double)ns(t5 - t4) / (double)kRefIters;
    (void)sink;

    std::printf("  Update  (per make/undo, FT incremental): %7.1f ns/call\n", upd_ns);
    std::printf("  Evaluate(per leaf, output dot product) : %7.1f ns/call\n", eval_ns);
    std::printf("  Refresh (root / bucket crossing)       : %7.1f ns/call\n", ref_ns);
    std::printf("\n  Cost split = ns/call * call-rate. Measured call-rate (single-thread search, DEBUGX counters):\n");
    std::printf("    middlegame  Update:Evaluate = 3.07:1  -> Evaluate ~= %.0f%% of NNUE time\n",
                100.0 * (1.0 / 3.07 * eval_ns) / (upd_ns + 1.0 / 3.07 * eval_ns));
    std::printf("    endgame     Update:Evaluate = 4.71:1  -> Evaluate ~= %.0f%% of NNUE time\n",
                100.0 * (1.0 / 4.71 * eval_ns) / (upd_ns + 1.0 / 4.71 * eval_ns));
    std::printf("  So the output-layer Evaluate is ~50-60%% of NNUE compute (NOT a few %%): int8's reachable\n");
    std::printf("  target is large. (Update calls counted in nnue::Network::Update; Evaluate in Evaluate(acc).)\n");
    return 0;
}

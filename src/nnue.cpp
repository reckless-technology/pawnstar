#include "nnue.h"

#include "constants.h"
#include "position.h"

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <utility>

namespace nnue
{

namespace
{
/// @brief Squared clipped ReLU: clamp to [0, kQA] then square (mirrors bullet's `simple` activation).
/// @param x Accumulator value (already in QA units). @return Activated value in QA*QA units.
inline int32_t SCReLU(int16_t x)
{
    const int32_t y = x < 0 ? 0 : (x > kQA ? kQA : x);
    return y * y;
}

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

/// @brief The bullet Chess768 feature indices for a piece, one per perspective.
///   white-perspective index = color * 384 + (piece_type - 1) * 64 + square
///   black-perspective index = (1 - color) * 384 + (piece_type - 1) * 64 + (square ^ kRankFlip)
inline std::pair<int, int> FeatureIndices(int color, int piece, int sq)
{
    const int type_offset = (piece - kPawn) * 64;
    return {color * 384 + type_offset + sq, (1 - color) * 384 + type_offset + (sq ^ kRankFlip)};
}
} // namespace

void Network::AddPiece(Accumulator &acc, int color, int piece, int sq) const
{
    const auto [wi, bi] = FeatureIndices(color, piece, sq);
    AddColumn(acc.white, &feature_weights_[static_cast<std::size_t>(wi) * kHiddenSize]);
    AddColumn(acc.black, &feature_weights_[static_cast<std::size_t>(bi) * kHiddenSize]);
}

void Network::RemovePiece(Accumulator &acc, int color, int piece, int sq) const
{
    const auto [wi, bi] = FeatureIndices(color, piece, sq);
    SubColumn(acc.white, &feature_weights_[static_cast<std::size_t>(wi) * kHiddenSize]);
    SubColumn(acc.black, &feature_weights_[static_cast<std::size_t>(bi) * kHiddenSize]);
}

bool Network::Load(const std::string &path)
{
    loaded_ = false;
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in)
    {
        std::fprintf(stderr, "info string NNUE: cannot open net file '%s'\n", path.c_str());
        return false;
    }
    // The file is bullet's headerless quantised .bin; the size guards against arch/format mismatch. bullet
    // pads the trailing 1-element output_bias tensor up to a 64-byte boundary, so the file may be a little
    // larger than the tightly packed weights; we read the weights and ignore any trailing padding.
    const std::streamoff size = in.tellg();
    if (static_cast<std::size_t>(size) < kNetFileBytes)
    {
        std::fprintf(stderr,
                     "info string NNUE: net '%s' is %lld bytes, expected at least %zu (wrong hidden size / format)\n",
                     path.c_str(), static_cast<long long>(size), kNetFileBytes);
        return false;
    }
    in.seekg(0);
    // Read in bullet's save order: feature_weights, feature_bias, output_weights, output_bias.
    in.read(reinterpret_cast<char *>(feature_weights_.data()), feature_weights_.size() * sizeof(int16_t));
    in.read(reinterpret_cast<char *>(feature_bias_.data()), feature_bias_.size() * sizeof(int16_t));
    in.read(reinterpret_cast<char *>(output_weights_.data()), output_weights_.size() * sizeof(int16_t));
    in.read(reinterpret_cast<char *>(&output_bias_), sizeof(output_bias_));
    if (!in)
    {
        std::fprintf(stderr, "info string NNUE: net file '%s' truncated\n", path.c_str());
        return false;
    }
    loaded_ = true;
    std::fprintf(stderr, "info string NNUE: loaded net '%s'\n", path.c_str());
    return true;
}

void Network::Refresh(Accumulator &acc, const Position &position) const
{
    // Seed both perspectives with the feature-transformer bias, then add every piece.
    acc.white = feature_bias_;
    acc.black = feature_bias_;
    for (int color = kWhite; color <= kBlack; ++color)
    {
        const Bitboard friendly = position.colors_[color];
        for (int piece = kPawn; piece <= kKing; ++piece)
        {
            for (Square s : position.pieces_[piece] & friendly)
            {
                AddPiece(acc, color, piece, s);
            }
        }
    }
}

void Network::Update(Accumulator &acc, const Position &from, const Position &to) const
{
    // Diff piece placement per (color, type): remove pieces that left a square, add pieces that arrived.
    // A handful of squares change per move; identical placements (e.g. a null move) yield no work.
    for (int color = kWhite; color <= kBlack; ++color)
    {
        for (int piece = kPawn; piece <= kKing; ++piece)
        {
            const Bitboard from_bb = from.pieces_[piece] & from.colors_[color];
            const Bitboard to_bb   = to.pieces_[piece] & to.colors_[color];
            for (Square s : from_bb & ~to_bb)
            {
                RemovePiece(acc, color, piece, s);
            }
            for (Square s : to_bb & ~from_bb)
            {
                AddPiece(acc, color, piece, s);
            }
        }
    }
}

int Network::Evaluate(const Accumulator &acc, Color side_to_move) const
{
    // The side-to-move accumulator feeds the first half of the output layer.
    const bool                              white_to_move = side_to_move == kWhite;
    const std::array<int16_t, kHiddenSize> &stm_acc       = white_to_move ? acc.white : acc.black;
    const std::array<int16_t, kHiddenSize> &ntm_acc       = white_to_move ? acc.black : acc.white;

    // Forward pass mirroring bullet's `simple` example exactly (SCReLU activation).
    int64_t output = 0;
    for (int i = 0; i < kHiddenSize; ++i)
    {
        output += SCReLU(stm_acc[i]) * output_weights_[i];
        output += SCReLU(ntm_acc[i]) * output_weights_[kHiddenSize + i];
    }
    // SCReLU leaves the dot product in QA*QA*QB units; reduce one QA, add the bias (QA*QB units), apply the
    // eval scale, then remove the remaining QA*QB quantisation.
    output /= kQA;
    output += output_bias_;
    output *= kScale;
    output /= (kQA * kQB);
    return static_cast<int>(output);
}

int Network::Evaluate(const Position &position) const
{
    // Full-refresh evaluation (diagnostics/tests and the incremental path's reference): build a fresh
    // accumulator then run the shared forward pass, so both paths are guaranteed identical.
    Accumulator acc;
    Refresh(acc, position);
    return Evaluate(acc, position.ColorToMove());
}

} // namespace nnue

#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <string_view>

#include "move.h"

/// @brief Encapsulates the four castling-rights flags (white/black × kingside/queenside).
/// The value is kept as a 4-bit field in a uint8_t so it can be used directly as an index
/// into the Zobrist castling-rights hash table (entries 0–15).
class CastlingRights
{
  public:
    constexpr CastlingRights() : rights_{0}
    {
    }
    constexpr explicit CastlingRights(uint8_t rights) : rights_{rights}
    {
    }

    constexpr bool MayWhiteCastleKingside() const
    {
        return !!(rights_ & kWhiteKingside);
    }
    constexpr bool MayWhiteCastleQueenside() const
    {
        return !!(rights_ & kWhiteQueenside);
    }
    constexpr bool MayBlackCastleKingside() const
    {
        return !!(rights_ & kBlackKingside);
    }
    constexpr bool MayBlackCastleQueenside() const
    {
        return !!(rights_ & kBlackQueenside);
    }
    constexpr bool IsNone() const
    {
        return rights_ == 0;
    }

    /// @brief Raw value for use as a Zobrist hash table index (0–15).
    constexpr uint8_t Value() const
    {
        return rights_;
    }

    constexpr CastlingRights &operator&=(uint8_t mask)
    {
        rights_ &= mask;
        return *this;
    }
    constexpr CastlingRights &operator|=(uint8_t flags)
    {
        rights_ |= flags;
        return *this;
    }

    /// @brief Return the castling rights that result after making a move.
    constexpr CastlingRights AfterMove(const Move &move) const
    {
        return CastlingRights{static_cast<uint8_t>(rights_ & kMoveMasks[move.from()] & kMoveMasks[move.to()])};
    }

    /// @brief Parse the castling field from a FEN string (e.g. "KQkq" or "-").
    static constexpr CastlingRights FromFen(std::string_view fen)
    {
        if (fen == "-")
            return CastlingRights{};
        CastlingRights rights;
        for (char c : fen)
        {
            switch (c)
            {
            case 'K':
                rights |= kWhiteKingside;
                break;
            case 'Q':
                rights |= kWhiteQueenside;
                break;
            case 'k':
                rights |= kBlackKingside;
                break;
            case 'q':
                rights |= kBlackQueenside;
                break;
            default:
                break;
            }
        }
        return rights;
    }

    /// @brief Serialize to a FEN castling field (e.g. "KQkq" or "-").
    std::string ToFenString() const
    {
        if (rights_ == 0)
            return "-";
        std::string s;
        if (rights_ & kWhiteKingside)
            s += 'K';
        if (rights_ & kWhiteQueenside)
            s += 'Q';
        if (rights_ & kBlackKingside)
            s += 'k';
        if (rights_ & kBlackQueenside)
            s += 'q';
        return s;
    }

  private:
    uint8_t rights_; ///< The actual flag value.

    static constexpr uint8_t kWhiteKingside  = 0x01;
    static constexpr uint8_t kWhiteQueenside = 0x02;
    static constexpr uint8_t kBlackKingside  = 0x04;
    static constexpr uint8_t kBlackQueenside = 0x08;
    static constexpr uint8_t kOkm            = ~0;
    static constexpr uint8_t kA1m            = ~kWhiteQueenside;
    static constexpr uint8_t kE1m            = ~(kWhiteKingside | kWhiteQueenside);
    static constexpr uint8_t kH1m            = ~kWhiteKingside;
    static constexpr uint8_t kA8m            = ~kBlackQueenside;
    static constexpr uint8_t kE8m            = ~(kBlackKingside | kBlackQueenside);
    static constexpr uint8_t kH8m            = ~kBlackKingside;

    // clang-format off
    static constexpr std::array<uint8_t, 64> kMoveMasks
    {
        kA1m, kOkm, kOkm, kOkm, kE1m, kOkm, kOkm, kH1m,
        kOkm, kOkm, kOkm, kOkm, kOkm, kOkm, kOkm, kOkm,
        kOkm, kOkm, kOkm, kOkm, kOkm, kOkm, kOkm, kOkm,
        kOkm, kOkm, kOkm, kOkm, kOkm, kOkm, kOkm, kOkm,
        kOkm, kOkm, kOkm, kOkm, kOkm, kOkm, kOkm, kOkm,
        kOkm, kOkm, kOkm, kOkm, kOkm, kOkm, kOkm, kOkm,
        kOkm, kOkm, kOkm, kOkm, kOkm, kOkm, kOkm, kOkm,
        kA8m, kOkm, kOkm, kOkm, kE8m, kOkm, kOkm, kH8m,
    };
    // clang-format on
};

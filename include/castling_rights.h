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
    constexpr CastlingRights() : value_{0}
    {
    }
    constexpr CastlingRights(uint8_t rights) : value_{rights}
    {
    }

    constexpr bool MayWhiteCastleKingside() const
    {
        return !!(value_ & kWhiteKingside);
    }
    constexpr bool MayWhiteCastleQueenside() const
    {
        return !!(value_ & kWhiteQueenside);
    }
    constexpr bool MayBlackCastleKingside() const
    {
        return !!(value_ & kBlackKingside);
    }
    constexpr bool MayBlackCastleQueenside() const
    {
        return !!(value_ & kBlackQueenside);
    }

    /// @brief Zobrist hash associated with these castling rights.
    constexpr zobrist_t Hash() const
    {
        return kCastlingRightsHashes[value_];
    }

    /// @brief Return the castling rights that result after making a move.
    constexpr CastlingRights &AfterMove(const Move &move)
    {
        value_ &= kMoveMasks[move.from()] & kMoveMasks[move.to()];
        return *this;
    }

    /// @brief Parse the castling field from a FEN string (e.g. "KQkq" or "-").
    static constexpr CastlingRights FromFen(std::string_view fen)
    {
        uint8_t result = 0;
        if (fen == "-")
        {
            return result;
        }
        for (char c : fen)
        {
            switch (c)
            {
            case 'K':
                result |= kWhiteKingside;
                break;
            case 'Q':
                result |= kWhiteQueenside;
                break;
            case 'k':
                result |= kBlackKingside;
                break;
            case 'q':
                result |= kBlackQueenside;
                break;
            default:
                break;
            }
        }
        return CastlingRights{result};
    }

    /// @brief Serialize castling rights to FEN format.
    /// @return string containing FEN of castling rights (e.g. "KQkq" or "-").
    std::string ToFenString() const
    {
        if (value_ == 0)
            return "-";
        std::string s;
        if (value_ & kWhiteKingside)
            s += 'K';
        if (value_ & kWhiteQueenside)
            s += 'Q';
        if (value_ & kBlackKingside)
            s += 'k';
        if (value_ & kBlackQueenside)
            s += 'q';
        return s;
    }

  private:
    uint8_t value_; ///< The actual flag value.

    enum Flags : uint8_t
    {
        kWhiteKingside  = 0x01,
        kWhiteQueenside = 0x02,
        kBlackKingside  = 0x04,
        kBlackQueenside = 0x08,
        kOK             = (uint8_t)(~0),
        kA1             = (uint8_t)(~kWhiteQueenside),
        kE1             = (uint8_t)(~(kWhiteKingside | kWhiteQueenside)),
        kH1             = (uint8_t)(~kWhiteKingside),
        kA8             = (uint8_t)(~kBlackQueenside),
        kE8             = (uint8_t)(~(kBlackKingside | kBlackQueenside)),
        kH8             = (uint8_t)(~kBlackKingside),
    };

    /// @brief Mask values ANDed with move from and to squares to determine new castling rights after a move.
    /// Only squares A1, E1, H1, A8, E8, H8 affect castling rights.
    // clang-format off
    static constexpr std::array<uint8_t, 64> kMoveMasks
    {
        kA1, kOK, kOK, kOK, kE1, kOK, kOK, kH1,
        kOK, kOK, kOK, kOK, kOK, kOK, kOK, kOK,
        kOK, kOK, kOK, kOK, kOK, kOK, kOK, kOK,
        kOK, kOK, kOK, kOK, kOK, kOK, kOK, kOK,
        kOK, kOK, kOK, kOK, kOK, kOK, kOK, kOK,
        kOK, kOK, kOK, kOK, kOK, kOK, kOK, kOK,
        kOK, kOK, kOK, kOK, kOK, kOK, kOK, kOK,
        kA8, kOK, kOK, kOK, kE8, kOK, kOK, kH8,
    };
    // clang-format on
};

#pragma once
/// @file castling_rights.h Castling-rights flags and their effect on hashing and move making.
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
    /// @brief Default constructor; no castling rights.
    constexpr CastlingRights() : value_{0}
    {
    }
    /// @brief Construct from a raw 4-bit flag value.
    /// @param rights Bitwise OR of the Flags enumerators.
    constexpr CastlingRights(uint8_t rights) : value_{rights}
    {
    }

    /// @brief Whether white may still castle kingside.
    /// @return true if the white kingside right is set.
    constexpr bool MayWhiteCastleKingside() const
    {
        return !!(value_ & kWhiteKingside);
    }
    /// @brief Whether white may still castle queenside.
    /// @return true if the white queenside right is set.
    constexpr bool MayWhiteCastleQueenside() const
    {
        return !!(value_ & kWhiteQueenside);
    }
    /// @brief Whether black may still castle kingside.
    /// @return true if the black kingside right is set.
    constexpr bool MayBlackCastleKingside() const
    {
        return !!(value_ & kBlackKingside);
    }
    /// @brief Whether black may still castle queenside.
    /// @return true if the black queenside right is set.
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
    /// @param move Move being made (its from/to squares may revoke rights).
    /// @return Reference to this updated castling-rights object.
    constexpr CastlingRights &AfterMove(const Move &move)
    {
        value_ &= kMoveMasks[move.from()] & kMoveMasks[move.to()];
        return *this;
    }

    /// @brief Parse the castling field from a FEN string (e.g. "KQkq" or "-").
    /// @param fen FEN castling field.
    /// @return The parsed castling rights.
    static constexpr CastlingRights FromFen(std::string_view fen)
    {
        uint8_t result = 0;
        if (fen == "-")
        {
            return CastlingRights{result};
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

    /// @brief Castling-rights bit flags, and the AND masks applied when a key square is vacated/entered.
    enum Flags : uint8_t
    {
        kWhiteKingside  = 0x01,                                           ///< White may castle kingside.
        kWhiteQueenside = 0x02,                                           ///< White may castle queenside.
        kBlackKingside  = 0x04,                                           ///< Black may castle kingside.
        kBlackQueenside = 0x08,                                           ///< Black may castle queenside.
        kOK             = (uint8_t)(~0),                                  ///< Mask that preserves all rights.
        kA1             = (uint8_t)(~kWhiteQueenside),                    ///< Mask applied for the a1 square.
        kE1             = (uint8_t)(~(kWhiteKingside | kWhiteQueenside)), ///< Mask applied for the e1 square.
        kH1             = (uint8_t)(~kWhiteKingside),                     ///< Mask applied for the h1 square.
        kA8             = (uint8_t)(~kBlackQueenside),                    ///< Mask applied for the a8 square.
        kE8             = (uint8_t)(~(kBlackKingside | kBlackQueenside)), ///< Mask applied for the e8 square.
        kH8             = (uint8_t)(~kBlackKingside),                     ///< Mask applied for the h8 square.
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

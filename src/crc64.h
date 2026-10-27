#pragma once
/// @file crc64.h CRC-64/XZ — the integrity check the shared Reckless NNUE net container (`RNNUE1`) carries.
///
/// One algorithm, five engines: `pawnstar`, `pawnstar-rs`, `pawnstar-go`, `zenith` and `stoopid-chess` all
/// stamp and verify a net with this same checksum, so an implementation that differs by a bit rejects every
/// net the others accept. It is **CRC-64/XZ**, also called CRC-64/GO-ECMA:
///
///   polynomial  0x42F0E1EBA9EA3693, reflected (so the table constant is 0xC96C5795D7870F42)
///   init        0xFFFFFFFFFFFFFFFF
///   reflect     in and out
///   final XOR   0xFFFFFFFFFFFFFFFF
///
/// The **check value** every implementation must reproduce — CRC-64/XZ of the nine ASCII bytes `123456789`
/// is `0x995DC9BBDF1939FA` — is asserted twice here: as a `static_assert` at the bottom of this file, so a
/// wrong table cannot compile, and again at run time as the first check `test_primitives` makes.
///
/// A 64-bit CRC and not a 32-bit one because the payload is ~12.6 MB; a CRC and not a cryptographic hash
/// because this guards against accidental corruption (a truncated download, a half-written file, a net
/// stamped by the wrong build), not against tampering. The 256-entry table is built by a `consteval`
/// function, so there is no generated table to keep in step and no dependency to add; the whole thing is
/// `constexpr`, which is what lets the king-bucket map's digest be a compile-time constant.

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace crc64
{

/// @brief The reflected form of the CRC-64/XZ polynomial 0x42F0E1EBA9EA3693.
constexpr std::uint64_t kReflectedPolynomial = 0xC96C5795D7870F42ULL;

/// @brief The value a CRC-64/XZ register starts at, and the value it is XORed with at the end.
constexpr std::uint64_t kInitialAndFinalXor = 0xFFFFFFFFFFFFFFFFULL;

/// @brief Build the 256-entry byte-at-a-time lookup table for the reflected polynomial.
/// @return Entry @c i is the CRC register state after feeding byte @c i into a zeroed register.
consteval std::array<std::uint64_t, 256> MakeTable()
{
    std::array<std::uint64_t, 256> table{};
    for (std::size_t byte = 0; byte < table.size(); ++byte)
    {
        std::uint64_t remainder = static_cast<std::uint64_t>(byte);
        for (int bit = 0; bit < 8; ++bit)
        {
            remainder = (remainder & 1U) != 0U ? (remainder >> 1) ^ kReflectedPolynomial : remainder >> 1;
        }
        table[byte] = remainder;
    }
    return table;
}

/// @brief The byte-at-a-time CRC-64/XZ table, computed once at compile time.
inline constexpr std::array<std::uint64_t, 256> kTable = MakeTable();

/// @brief Feed @p length bytes of @p data into a running CRC register.
/// @param running The register, as returned by a previous Update (start from kInitialAndFinalXor).
/// @param data First byte to feed. May be null only when @p length is zero.
/// @param length Number of bytes to feed.
/// @return The updated register — still pre-final-XOR, so it can be fed again.
constexpr std::uint64_t Update(std::uint64_t running, const std::uint8_t *data, std::size_t length)
{
    for (std::size_t index = 0; index < length; ++index)
    {
        running = kTable[(running ^ data[index]) & 0xFFU] ^ (running >> 8);
    }
    return running;
}

/// @brief CRC-64/XZ of a byte range.
/// @param data First byte. May be null only when @p length is zero.
/// @param length Number of bytes.
/// @return The checksum, init and final XOR applied.
constexpr std::uint64_t Checksum(const std::uint8_t *data, std::size_t length)
{
    return Update(kInitialAndFinalXor, data, length) ^ kInitialAndFinalXor;
}

/// @brief CRC-64/XZ of the bytes of a string view — the form the check value is written in.
/// @param text Bytes to checksum. @return The checksum.
constexpr std::uint64_t Checksum(std::string_view text)
{
    std::uint64_t running = kInitialAndFinalXor;
    for (const char character : text)
    {
        running = kTable[(running ^ static_cast<std::uint8_t>(character)) & 0xFFU] ^ (running >> 8);
    }
    return running ^ kInitialAndFinalXor;
}

/// @brief The published CRC-64/XZ check value: the checksum of the nine ASCII bytes "123456789".
constexpr std::uint64_t kCheckValueInput = 0x995DC9BBDF1939FAULL;

static_assert(Checksum(std::string_view{"123456789"}) == kCheckValueInput,
              "CRC-64/XZ is wrong: the checksum of the nine ASCII bytes \"123456789\" must be "
              "0x995DC9BBDF1939FA. Every Reckless engine asserts this same value, so a table or a "
              "reflection that disagrees here will reject every net the other engines accept.");

static_assert(Checksum(nullptr, 0) == 0ULL, "CRC-64/XZ of an empty range must be zero");

} // namespace crc64

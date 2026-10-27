#pragma once
/// @file sha256.h FIPS 180-4 SHA-256, for the provenance sidecars the training-data tools write.
///
/// A data shard's lineage has to be auditable later - which net labelled it, or which input bytes it was
/// extracted from - and a filename says nothing about contents. This is the whole of SHA-256 in one header
/// with no dependency: the engine links no crypto library and should not start now for one hash of one file.
///
/// Two forms, because the two tools need different ones. `Hasher` is incremental, for a stream that is
/// consumed once and never seekable (tools/lichess_extract.cpp reads its PGN from stdin, so the only chance
/// to hash the input is as it goes past). `OfFile` is the one-shot form for a file on disk
/// (tools/gen_data.cpp hashes the labelling net).
///
/// tools/gen_data.cpp still carries its own copy of this algorithm inline; it should be switched over to
/// this header, which was not done here only because that file was being edited concurrently.

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>
#include <string_view>

namespace sha256
{
/// @brief The SHA-256 round constants (FIPS 180-4 §4.2.2).
constexpr std::array<uint32_t, 64> kRoundConstants{
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
    0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
    0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
    0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
    0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
    0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u, 0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u};

/// @brief The initial hash value (FIPS 180-4 §5.3.3).
constexpr std::array<uint32_t, 8> kInitialState{0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
                                                0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u};

/// @brief Rotate a 32-bit word right. @param x Word. @param n Bits. @return The rotated word.
constexpr uint32_t RotateRight(uint32_t x, int n)
{
    return (x >> n) | (x << (32 - n));
}

/// @brief Compress one 64-byte block into the running state (FIPS 180-4 §6.2.2).
/// @param state The eight-word hash state, updated in place. @param block The 64 message bytes.
inline void CompressBlock(std::array<uint32_t, 8> &state, const unsigned char *block)
{
    std::array<uint32_t, 64> schedule{};

    for (int i = 0; i < 16; ++i)
    {
        schedule[(std::size_t)i] = ((uint32_t)block[i * 4] << 24) | ((uint32_t)block[i * 4 + 1] << 16) |
                                   ((uint32_t)block[i * 4 + 2] << 8) | (uint32_t)block[i * 4 + 3];
    }

    for (int i = 16; i < 64; ++i)
    {
        const uint32_t s0 = RotateRight(schedule[(std::size_t)i - 15], 7) ^
                            RotateRight(schedule[(std::size_t)i - 15], 18) ^ (schedule[(std::size_t)i - 15] >> 3);
        const uint32_t s1 = RotateRight(schedule[(std::size_t)i - 2], 17) ^
                            RotateRight(schedule[(std::size_t)i - 2], 19) ^ (schedule[(std::size_t)i - 2] >> 10);
        schedule[(std::size_t)i] = schedule[(std::size_t)i - 16] + s0 + schedule[(std::size_t)i - 7] + s1;
    }

    std::array<uint32_t, 8> working = state;

    for (int i = 0; i < 64; ++i)
    {
        const uint32_t s1    = RotateRight(working[4], 6) ^ RotateRight(working[4], 11) ^ RotateRight(working[4], 25);
        const uint32_t ch    = (working[4] & working[5]) ^ (~working[4] & working[6]);
        const uint32_t temp1 = working[7] + s1 + ch + kRoundConstants[(std::size_t)i] + schedule[(std::size_t)i];
        const uint32_t s0    = RotateRight(working[0], 2) ^ RotateRight(working[0], 13) ^ RotateRight(working[0], 22);
        const uint32_t maj   = (working[0] & working[1]) ^ (working[0] & working[2]) ^ (working[1] & working[2]);
        const uint32_t temp2 = s0 + maj;

        working[7] = working[6];
        working[6] = working[5];
        working[5] = working[4];
        working[4] = working[3] + temp1;
        working[3] = working[2];
        working[2] = working[1];
        working[1] = working[0];
        working[0] = temp1 + temp2;
    }

    for (std::size_t i = 0; i < state.size(); ++i)
    {
        state[i] += working[i];
    }
}

/// @brief An incremental SHA-256 over a byte stream: Update as the bytes go past, Digest once at the end.
class Hasher
{
  public:
    void        Update(const void *data, std::size_t length);
    std::string Digest() const;
    uint64_t    bytes_hashed() const;

  private:
    std::array<uint32_t, 8>       state_ = kInitialState; ///< The running hash state.
    std::array<unsigned char, 64> block_{};               ///< The partial block not yet compressed.
    std::size_t                   block_filled_ = 0;      ///< Bytes held in block_.
    uint64_t                      total_bytes_  = 0;      ///< Every byte seen, for the length padding.
};

/// @brief Absorb more of the message.
/// @param data The bytes. @param length How many.
inline void Hasher::Update(const void *data, std::size_t length)
{
    const unsigned char *bytes = static_cast<const unsigned char *>(data);
    total_bytes_ += (uint64_t)length;

    while (length > 0)
    {
        const std::size_t take = std::min(length, 64 - block_filled_);
        std::memcpy(block_.data() + block_filled_, bytes, take);
        block_filled_ += take;
        bytes += take;
        length -= take;
        if (block_filled_ == 64)
        {
            CompressBlock(state_, block_.data());
            block_filled_ = 0;
        }
    }
}

/// @brief The digest of everything absorbed so far, as 64 lowercase hex digits.
///
/// Const, and so repeatable: the padding is applied to a copy of the state, leaving the hasher able to
/// absorb more. That is what lets a caller report a running digest and then a final one.
///
/// @return The 64-character digest.
inline std::string Hasher::Digest() const
{
    // Pad: a 0x80 byte, zeroes, then the message length in bits as a big-endian 64-bit word. A final block
    // with no room for the length (56 bytes or more of message) spills into one extra block.
    std::array<uint32_t, 8>        state = state_;
    std::array<unsigned char, 128> tail{};
    std::memcpy(tail.data(), block_.data(), block_filled_);
    tail[block_filled_]           = 0x80;
    const std::size_t tail_blocks = block_filled_ >= 56 ? 2 : 1;
    const uint64_t    bit_length  = total_bytes_ * 8;

    for (int i = 0; i < 8; ++i)
    {
        tail[tail_blocks * 64 - 1 - (std::size_t)i] = (unsigned char)((bit_length >> (8 * i)) & 0xff);
    }

    for (std::size_t block = 0; block < tail_blocks; ++block)
    {
        CompressBlock(state, tail.data() + block * 64);
    }

    std::string digest;
    digest.reserve(64);

    for (const uint32_t word : state)
    {
        for (int shift = 28; shift >= 0; shift -= 4)
        {
            digest.push_back("0123456789abcdef"[(word >> shift) & 0xf]);
        }
    }

    return digest;
}

/// @brief How many bytes have been absorbed. @return The byte count.
inline uint64_t Hasher::bytes_hashed() const
{
    return total_bytes_;
}

/// @brief The SHA-256 of a file, as 64 lowercase hex digits.
/// @param path File to hash.
/// @return The digest, or an empty string if the file could not be read.
inline std::string OfFile(const std::string &path)
{
    std::ifstream in{path, std::ios::binary};

    if (!in)
    {
        return {};
    }

    Hasher                           hasher;
    std::array<unsigned char, 65536> buffer{};

    while (in.read(reinterpret_cast<char *>(buffer.data()), (std::streamsize)buffer.size()) || in.gcount() > 0)
    {
        hasher.Update(buffer.data(), (std::size_t)in.gcount());
    }

    return hasher.Digest();
}

/// @brief The SHA-256 of a string, as 64 lowercase hex digits. @param text The bytes. @return The digest.
inline std::string OfString(std::string_view text)
{
    Hasher hasher;
    hasher.Update(text.data(), text.size());
    return hasher.Digest();
}
} // namespace sha256

/// @file stamp_net.cpp Wrap a raw bullet NNUE payload in the shared Reckless `RNNUE1` container.
///
/// The bullet trainer writes a headerless quantised net. This tool prepends the 64-byte NetHeader that
/// every Reckless engine reads (src/nnue.h, skill `reckless-nnue`), **written entirely from this build's
/// own compiled constants** — the architecture fields, the king-bucket map's CRC-64, and then the payload's
/// and the header's own CRC-64. So the tool can only produce a header describing the engine it was built
/// as: a 512-wide net stamped by a 1024-wide tool gets a 1024-wide header and is refused on first load,
/// which is the point. Never hand-edit a net file; stamp it.
///
/// It also **drops bullet's tail padding**. bullet pads its quantised output up to a 64-byte boundary with
/// repeats of the word "bullet"; RNNUE1 records an exact payload length and the loader checks the file has
/// nothing after it, which is what lets a truncated or an appended-to file be told apart from a sound one.
///
/// Usage: stamp_net <raw_in.bin> <stamped_out.bin>

#include "nnue.h"

#include <cstdio>
#include <fstream>
#include <iterator>
#include <vector>

int main(int argc, char **argv)
{
    if (argc != 3)
    {
        std::fprintf(stderr, "usage: %s <raw_in.bin> <stamped_out.bin>\n", argv[0]);
        return 2;
    }
    std::ifstream in(argv[1], std::ios::binary);
    if (!in)
    {
        std::fprintf(stderr, "stamp_net: cannot open '%s'\n", argv[1]);
        return 1;
    }
    std::vector<char> payload((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

    // The raw payload must match this build's architecture, give or take bullet's < 64 bytes of tail
    // padding. Anything else is a net for another architecture, or an already-stamped file.
    if (payload.size() < nnue::kNetPayloadBytes || payload.size() >= nnue::kNetPayloadBytes + 64)
    {
        std::fprintf(stderr,
                     "stamp_net: '%s' is %zu bytes, expected %zu raw (plus at most 63 bytes of bullet "
                     "padding) — wrong architecture for this build, or already stamped?\n",
                     argv[1], payload.size(), nnue::kNetPayloadBytes);
        return 1;
    }
    payload.resize(nnue::kNetPayloadBytes); // drop bullet's tail padding

    nnue::NetHeader header = nnue::ExpectedNetHeader();
    nnue::FinaliseNetHeader(header, reinterpret_cast<const std::uint8_t *>(payload.data()), payload.size());

    std::ofstream out(argv[2], std::ios::binary);
    if (!out)
    {
        std::fprintf(stderr, "stamp_net: cannot write '%s'\n", argv[2]);
        return 1;
    }
    out.write(reinterpret_cast<const char *>(&header), sizeof(header));
    out.write(payload.data(), static_cast<std::streamsize>(payload.size()));
    if (!out)
    {
        std::fprintf(stderr, "stamp_net: write to '%s' failed\n", argv[2]);
        return 1;
    }
    // Print every stamped value: the net table in nnue/README.md §7 records these, and a disagreement
    // between two nets of the same size is then a number to compare rather than two arrays to read.
    std::fprintf(stderr,
                 "stamp_net: wrote '%s' — %zu-byte RNNUE1 header + %zu-byte payload\n"
                 "  features_per_bucket %u  king_buckets %u  hidden_size %u  output_buckets %u\n"
                 "  flags 0x%02X (king mirroring %s)  activation %u  qa %u  qb %u  eval_scale %u\n"
                 "  king_bucket_map_crc 0x%016llX\n"
                 "  payload_crc         0x%016llX\n"
                 "  header_crc          0x%016llX\n",
                 argv[2], sizeof(header), payload.size(), header.features_per_bucket_, header.king_buckets_,
                 header.hidden_size_, header.output_buckets_, header.flags_,
                 (header.flags_ & nnue::kNetFlagKingMirroring) != 0 ? "on" : "off", header.activation_, header.qa_,
                 header.qb_, header.eval_scale_, static_cast<unsigned long long>(header.king_bucket_map_crc_),
                 static_cast<unsigned long long>(header.payload_crc_),
                 static_cast<unsigned long long>(header.header_crc_));
    return 0;
}

/// @file stamp_net.cpp Prepend a self-describing NetHeader to a raw bullet NNUE .bin.
///
/// The bullet trainer writes a headerless quantised net; this tool stamps it with the 32-byte
/// NetHeader (magic + architecture parameters from this build's nnue.h constants) so the engine can
/// reject an incompatible file instead of silently misreading it. Run it on the exported quantised.bin
/// before shipping a net (see nnue/README.md §7).
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
    const std::vector<char> payload((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

    // The raw payload must match this build's architecture (bullet pads the trailing tensor < 64 bytes).
    if (payload.size() < nnue::kNetFileBytes || payload.size() >= nnue::kNetFileBytes + 64)
    {
        std::fprintf(stderr, "stamp_net: '%s' is %zu bytes, expected %zu (raw) — wrong architecture for this build?\n",
                     argv[1], payload.size(), nnue::kNetFileBytes);
        return 1;
    }

    const nnue::NetHeader header = nnue::ExpectedNetHeader();
    std::ofstream         out(argv[2], std::ios::binary);
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
    std::fprintf(stderr, "stamp_net: wrote '%s' (%zu-byte header + %zu-byte payload)\n", argv[2], sizeof(header),
                 payload.size());
    return 0;
}

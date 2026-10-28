/// @file print_net_header.cpp Print the `RNNUE1` header of any net file, and say whether it is intact.
///
/// The reader that pairs with `stamp_net`'s writer (the logic is `net_header_report.h`, tested by
/// `test_net_header`). It decodes against the engine's own `nnue::NetHeader`, prints every field as `key=value`,
/// verifies `header_crc` and `payload_crc`, and reports whether the file's shape is this build's — but never
/// refuses a file for the shape, so it describes a net this build could not load.
///
/// Usage: print_net_header <net.bin>

#include "net_header_report.h"

#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

int main(int argc, char **argv)
{
#ifdef _WIN32
    // The report is read by shell scripts (Git Bash on Windows among them), where a carriage return at the end of
    // every value would make `[ "$buckets" = 8 ]` false for a net that has 8. Text mode would add one to each line.
    _setmode(_fileno(stdout), _O_BINARY);
    _setmode(_fileno(stderr), _O_BINARY);
#endif
    return net_header::Run(std::vector<std::string>(argv + 1, argv + argc), std::cout, std::cerr);
}

#pragma once
/// @file net_header_report.h Read an `RNNUE1` net file and describe its header — any net, loadable or not.
///
/// This is the **reader** the repository lacked. `tools/stamp_net.cpp` writes a header and `Network::Load`
/// refuses a net that does not match this build, but nothing would simply *say what a file's header holds*,
/// so decoders grew outside the tree (an `od` script read its `uint16` fields from byte 4 when the magic is 8
/// bytes, and printed every label two fields out for as long as nobody checked). One decoder, here, tested.
///
/// **The decode is `nnue::NetHeader`, the engine's own struct** (src/nnue.h), copied out of the file bytes — no
/// table of offsets is transcribed. The struct is `static_assert`ed to 64 bytes with its CRC fields at 32 and
/// 56, and test/net_header_test.cpp pins every other offset against the container's specification.
///
/// **It refuses nothing.** A report is made for a file of any shape, a file that fails its CRCs, a file too
/// short to hold a header, a file with no magic at all: the moment a header is most worth reading is when the
/// engine will not load it. What it does is *judge*, separately and in plain keys:
///
///  * integrity — the magic, `header_crc`, `payload_bytes` against the file's actual length, `payload_crc`;
///  * shape — whether every architecture field equals **this build's** compile-time constant, naming each one
///    that does not. A mismatch is reported, never fatal: a 4-king-bucket net is a perfectly sound net that this
///    build could not load.
///
/// The output is `key=value` lines in a fixed order (see FormatReport), so a shell script can `grep '^key='`.

#include "crc64.h"
#include "nnue.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

namespace net_header
{

/// @brief Everything known about one file's `RNNUE1` header, decoded and checked. Fields that cannot be
/// determined (a file shorter than a header) keep their zero value and are reported `unavailable`.
struct Report
{
    std::string   path;
    std::uint64_t file_bytes = 0;

    /// @brief True when the file holds at least sizeof(nnue::NetHeader) bytes, so `header` was decoded.
    bool            has_complete_header = false;
    nnue::NetHeader header{};

    bool          has_valid_magic     = false;
    std::uint64_t computed_header_crc = 0;
    bool          is_header_crc_valid = false;
    bool          has_zero_reserved   = false;

    /// @brief True when the header's `header_bytes` places the payload inside the file: at least the 64 bytes this
    /// struct occupies, and no further than the end of the file. Without it there is no payload to measure.
    bool          has_payload_range     = false;
    std::uint64_t actual_payload_bytes  = 0;     ///< file_bytes - header_bytes, when has_payload_range.
    bool          is_payload_size_valid = false; ///< header.payload_bytes_ == actual_payload_bytes.
    std::uint64_t computed_payload_crc  = 0;     ///< Over every byte after the header, when has_payload_range.
    bool          is_payload_crc_valid  = false;

    /// @brief The payload size the header's own architecture fields imply, and whether `payload_bytes` agrees: a
    /// header can be internally consistent with its CRC and still describe a payload its fields cannot produce.
    std::uint64_t payload_bytes_from_fields   = 0;
    bool          is_payload_bytes_consistent = false;

    /// @brief Names of the architecture fields that differ from this build's constants; empty when the file's shape
    /// is this build's. The details are the same differences with both values, for a human to read.
    std::vector<std::string> shape_mismatches;
    std::vector<std::string> shape_mismatch_details;

    /// @brief True when the magic, header CRC, payload length, payload CRC and the header's own arithmetic (its
    /// payload_bytes is what its architecture fields imply) all check out. Independent of shape.
    bool is_intact = false;
};

namespace detail
{

inline std::string Unsigned(std::uint64_t value)
{
    return std::to_string(value);
}

inline std::string Hex16(std::uint64_t value)
{
    char text[24];
    std::snprintf(text, sizeof(text), "0x%016llX", static_cast<unsigned long long>(value));
    return text;
}

inline std::string Hex2(std::uint8_t value)
{
    char text[8];
    std::snprintf(text, sizeof(text), "0x%02X", static_cast<unsigned>(value));
    return text;
}

inline std::string YesNo(bool value)
{
    return value ? "yes" : "no";
}

/// @brief Bytes as text a line can carry: printable ASCII as itself, anything else as \xNN, so "RNNUE1\0\0" reads
/// `RNNUE1\x00\x00` and a file with no magic still prints something a log can hold.
inline std::string EscapedBytes(const char *bytes, std::size_t count)
{
    std::string text;
    for (std::size_t index = 0; index < count; ++index)
    {
        const auto byte = static_cast<unsigned char>(bytes[index]);
        if (byte >= 0x20 && byte < 0x7F && byte != '\\')
        {
            text += static_cast<char>(byte);
        }
        else
        {
            char escape[8];
            std::snprintf(escape, sizeof(escape), "\\x%02X", static_cast<unsigned>(byte));
            text += escape;
        }
    }
    return text;
}

} // namespace detail

/// @brief The payload size an `RNNUE1` header's own architecture fields imply: feature weights and bias and output
/// weights as int16, the output bias as int32.
inline std::uint64_t PayloadBytesFromFields(const nnue::NetHeader &header)
{
    const std::uint64_t hidden = header.hidden_size_;
    const std::uint64_t rows   = static_cast<std::uint64_t>(header.king_buckets_) * header.features_per_bucket_;
    const std::uint64_t int16s =
        rows * hidden + hidden + static_cast<std::uint64_t>(header.output_buckets_) * 2 * hidden;
    return int16s * sizeof(std::int16_t) + static_cast<std::uint64_t>(header.output_buckets_) * sizeof(std::int32_t);
}

/// @brief Decode and check a net file already in memory. Never fails: every outcome is a field of the report.
/// @param data First byte of the file; may be null only when @p size is zero. @param size File length.
/// @param path What to call the file in the report.
inline Report Describe(const std::uint8_t *data, std::size_t size, const std::string &path)
{
    Report report;
    report.path       = path;
    report.file_bytes = size;
    if (size < sizeof(nnue::NetHeader))
    {
        return report;
    }
    report.has_complete_header = true;
    std::memcpy(&report.header, data, sizeof(report.header));
    const nnue::NetHeader &header = report.header;

    report.has_valid_magic     = std::memcmp(header.magic_, nnue::kNetMagic, sizeof(nnue::kNetMagic)) == 0;
    report.computed_header_crc = crc64::Checksum(data, nnue::kNetHeaderCrcBytes);
    report.is_header_crc_valid = report.computed_header_crc == header.header_crc_;
    report.has_zero_reserved   = true;
    for (const std::uint8_t reserved_byte : header.reserved_)
    {
        report.has_zero_reserved = report.has_zero_reserved && reserved_byte == 0;
    }

    // The payload starts where the file's own header_bytes says, which is 64 for every version so far. A value that
    // would put it outside the file leaves nothing to measure, and the report says so rather than guessing.
    report.has_payload_range = header.header_bytes_ >= sizeof(nnue::NetHeader) && header.header_bytes_ <= size;
    if (report.has_payload_range)
    {
        report.actual_payload_bytes  = size - header.header_bytes_;
        report.is_payload_size_valid = header.payload_bytes_ == report.actual_payload_bytes;
        report.computed_payload_crc  = crc64::Checksum(data + header.header_bytes_, report.actual_payload_bytes);
        report.is_payload_crc_valid  = report.computed_payload_crc == header.payload_crc_;
    }

    report.payload_bytes_from_fields   = PayloadBytesFromFields(header);
    report.is_payload_bytes_consistent = report.payload_bytes_from_fields == header.payload_bytes_;

    const nnue::NetHeader expected = nnue::ExpectedNetHeader();

    const struct
    {
        const char   *name;
        std::uint64_t in_file;
        std::uint64_t in_build;
        bool          is_checksum; ///< Shown in hex: a CRC read as a decimal is unreadable.
    } fields[] = {
        {"format_version", header.format_version_, expected.format_version_, false},
        {"header_bytes", header.header_bytes_, expected.header_bytes_, false},
        {"features_per_bucket", header.features_per_bucket_, expected.features_per_bucket_, false},
        {"king_buckets", header.king_buckets_, expected.king_buckets_, false},
        {"hidden_size", header.hidden_size_, expected.hidden_size_, false},
        {"output_buckets", header.output_buckets_, expected.output_buckets_, false},
        {"flags", header.flags_, expected.flags_, false},
        {"activation", header.activation_, expected.activation_, false},
        {"qa", header.qa_, expected.qa_, false},
        {"qb", header.qb_, expected.qb_, false},
        {"eval_scale", header.eval_scale_, expected.eval_scale_, false},
        {"king_bucket_map_crc", header.king_bucket_map_crc_, expected.king_bucket_map_crc_, true},
        {"payload_bytes", header.payload_bytes_, expected.payload_bytes_, false},
    };

    for (const auto &field : fields)
    {
        if (field.in_file != field.in_build)
        {
            report.shape_mismatches.emplace_back(field.name);
            const std::string in_file =
                field.is_checksum ? detail::Hex16(field.in_file) : std::to_string(field.in_file);
            const std::string in_build =
                field.is_checksum ? detail::Hex16(field.in_build) : std::to_string(field.in_build);
            report.shape_mismatch_details.push_back(std::string(field.name) + ": file " + in_file + ", this build " +
                                                    in_build);
        }
    }

    report.is_intact = report.has_valid_magic && report.is_header_crc_valid && report.has_payload_range &&
                       report.is_payload_size_valid && report.is_payload_crc_valid &&
                       report.is_payload_bytes_consistent;
    return report;
}

/// @brief Read a whole file into memory.
/// @return false when it cannot be opened or read.
inline bool ReadWholeFile(const std::string &path, std::vector<std::uint8_t> &bytes)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
    {
        return false;
    }
    bytes.assign((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return !in.bad();
}

/// @brief The report as ordered key/value pairs: the one definition of the output, which FormatReport prints and
/// the test parses. Every key is always present, in this order; a value that cannot be known is `unavailable`.
///
/// **The format is `key=value`, one per line, because a shell reads it with no parser:**
/// `sed -n 's/^king_buckets=//p'`, `grep -q '^integrity=ok'`, or `while IFS== read -r key value`. Values never
/// contain a newline. Integers are decimal; CRCs and flags are `0x` hex, upper case, CRCs 16 digits wide — the
/// spelling `stamp_net` and the container's documentation use, so they compare as text. Booleans are `yes`/`no`.
inline std::vector<std::pair<std::string, std::string>> KeyValues(const Report &report)
{
    using namespace detail;
    const std::string      unavailable = "unavailable";
    const bool             has_header  = report.has_complete_header;
    const nnue::NetHeader &header      = report.header;

    std::vector<std::pair<std::string, std::string>> pairs;
    const auto add = [&pairs](const char *key, std::string value) { pairs.emplace_back(key, std::move(value)); };
    // A decoded header field, or `unavailable` for a file too short to hold one.
    const auto field = [&](const char *key, auto value) { add(key, has_header ? Unsigned(value) : unavailable); };

    add("file", report.path);
    add("file_bytes", Unsigned(report.file_bytes));
    add("header_complete", YesNo(has_header));
    add("magic", has_header ? EscapedBytes(header.magic_, sizeof(header.magic_)) : unavailable);
    add("magic_ok", YesNo(report.has_valid_magic));
    field("format_version", header.format_version_);
    field("header_bytes", header.header_bytes_);
    field("features_per_bucket", header.features_per_bucket_);
    field("king_buckets", header.king_buckets_);
    field("hidden_size", header.hidden_size_);
    field("output_buckets", header.output_buckets_);
    add("flags", has_header ? Hex2(header.flags_) : unavailable);
    add("king_mirroring",
        has_header ? ((header.flags_ & nnue::kNetFlagKingMirroring) != 0 ? "on" : "off") : unavailable);
    add("flags_unknown_bits",
        has_header ? Hex2(static_cast<std::uint8_t>(header.flags_ & ~nnue::kNetFlagKingMirroring)) : unavailable);
    field("activation", header.activation_);
    add("activation_name",
        has_header ? (header.activation_ == nnue::kActivationScreelu ? "screlu" : "unknown") : unavailable);
    field("qa", header.qa_);
    field("qb", header.qb_);
    field("eval_scale", header.eval_scale_);
    add("reserved_zero", has_header ? YesNo(report.has_zero_reserved) : unavailable);
    add("king_bucket_map_crc", has_header ? Hex16(header.king_bucket_map_crc_) : unavailable);
    add("payload_bytes", has_header ? Unsigned(header.payload_bytes_) : unavailable);
    add("payload_crc", has_header ? Hex16(header.payload_crc_) : unavailable);
    add("header_crc", has_header ? Hex16(header.header_crc_) : unavailable);

    add("header_crc_computed", has_header ? Hex16(report.computed_header_crc) : unavailable);
    add("header_crc_ok", YesNo(report.is_header_crc_valid));
    add("actual_payload_bytes", report.has_payload_range ? Unsigned(report.actual_payload_bytes) : unavailable);
    add("payload_size_ok", YesNo(report.is_payload_size_valid));
    add("payload_crc_computed", report.has_payload_range ? Hex16(report.computed_payload_crc) : unavailable);
    add("payload_crc_ok", YesNo(report.is_payload_crc_valid));
    add("payload_bytes_from_fields", has_header ? Unsigned(report.payload_bytes_from_fields) : unavailable);
    add("payload_bytes_from_fields_ok", has_header ? YesNo(report.is_payload_bytes_consistent) : unavailable);
    add("integrity", report.is_intact ? "ok" : "fail");

    std::string mismatches;
    for (const std::string &name : report.shape_mismatches)
    {
        mismatches += (mismatches.empty() ? "" : ",") + name;
    }
    add("shape_matches_build", has_header ? YesNo(report.shape_mismatches.empty()) : unavailable);
    add("shape_mismatches", has_header ? mismatches : unavailable);
    return pairs;
}

/// @brief Write the report as `key=value` lines on @p out (see KeyValues).
inline void FormatReport(const Report &report, std::ostream &out)
{
    for (const auto &[key, value] : KeyValues(report))
    {
        out << key << '=' << value << '\n';
    }
}

/// @brief The command line: `print_net_header <net.bin>`.
///
/// Standard output carries the `key=value` report and nothing else, so a script can capture it whole. Standard
/// error carries the human reading of it: one line per failed integrity check, and one when the file's shape is
/// not this build's, with both values.
///
/// Exit status: 0 when the file is intact (magic, both CRCs, payload length and arithmetic), **whatever its shape**; 1
/// when it is not, with the report still printed in full; 2 on a usage error or a file that cannot be read.
inline int Run(const std::vector<std::string> &arguments, std::ostream &out, std::ostream &error)
{
    if (arguments.size() != 1 || arguments[0] == "-h" || arguments[0] == "--help")
    {
        error << "usage: print_net_header <net.bin>\n"
                 "  Decode and check an RNNUE1 net file's header against the engine's own NetHeader.\n"
                 "  Prints key=value lines on stdout; reads any net, including one this build could not load.\n"
                 "  exit 0: intact (magic, header_crc, payload length, payload_crc); 1: not intact; 2: usage/read "
                 "error\n";
        return 2;
    }
    const std::string        &path = arguments[0];
    std::vector<std::uint8_t> bytes;
    if (!ReadWholeFile(path, bytes))
    {
        error << "print_net_header: cannot read '" << path << "'\n";
        return 2;
    }

    const Report report = Describe(bytes.data(), bytes.size(), path);
    FormatReport(report, out);

    if (!report.has_complete_header)
    {
        error << "print_net_header: '" << path << "' is " << report.file_bytes << " bytes, too short for the "
              << sizeof(nnue::NetHeader) << "-byte RNNUE1 header\n";
    }
    else
    {
        const auto fail = [&](const std::string &message) { error << "print_net_header: FAIL " << message << '\n'; };
        if (!report.has_valid_magic)
        {
            fail("magic is not 'RNNUE1': not a net in the shared container");
        }
        if (!report.is_header_crc_valid)
        {
            fail("header_crc is " + detail::Hex16(report.header.header_crc_) + ", the header bytes checksum to " +
                 detail::Hex16(report.computed_header_crc));
        }
        if (!report.has_payload_range)
        {
            fail("header_bytes " + std::to_string(report.header.header_bytes_) +
                 " puts the payload outside the file; payload checks not made");
        }
        else
        {
            if (!report.is_payload_size_valid)
            {
                fail("payload_bytes is " + std::to_string(report.header.payload_bytes_) + ", the file carries " +
                     std::to_string(report.actual_payload_bytes) + " bytes after the header");
            }
            if (!report.is_payload_crc_valid)
            {
                fail("payload_crc is " + detail::Hex16(report.header.payload_crc_) +
                     ", the payload bytes checksum to " + detail::Hex16(report.computed_payload_crc));
            }
        }
        if (!report.shape_mismatches.empty())
        {
            // WORDED ABOUT THIS TOOL'S OWN BUILD, DELIBERATELY. `shape_matches_build` compares the file against
            // the compile-time constants of whichever build of *this tool* is running, which is very often not
            // the engine that is about to load the net: on the CI runners the decoder a match harness finds is
            // ~/pawnstar/build/print_net_header, compiled at the old architecture, so every new-shape net it
            // reads lands here. The note this replaced said "this build would refuse it", which is true of this
            // decoder and reads as a refusal of the NET - it would stop a human mid-match-setup on a run that is
            // fine. The verdict on the file is `integrity`, which is what tools/pawnstar-match.sh gates on, so
            // integrity says itself again here, above the shape note and in the same stream.
            error
                << "print_net_header: integrity=" << (report.is_intact ? "ok" : "fail")
                << " - that is the verdict on the FILE, and nothing below changes it.\n"
                << "print_net_header: NOTE the shape fields above are the FILE'S OWN. They are not the shape THIS\n"
                   "print_net_header:   COPY of print_net_header was compiled at, so this copy could not load this\n"
                   "print_net_header:   net - which is a statement about the decoder's build, not about the net. An\n"
                   "print_net_header:   engine built at the net's own shape loads it; decode with a print_net_header\n"
                   "print_net_header:   from that engine's tree for a verdict about it. The fields that differ:\n";
            for (const std::string &detail_line : report.shape_mismatch_details)
            {
                error << "print_net_header:   " << detail_line << '\n';
            }
        }
    }
    return report.is_intact ? 0 : 1;
}

} // namespace net_header

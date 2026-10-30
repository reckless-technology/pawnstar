/// @file net_header_test.cpp Tests of the `RNNUE1` header reader (tools/net_header_report.h, `print_net_header`).
///
/// **Why this test exists.** A decoder written outside the tree read its `uint16` fields from byte offset 4 when
/// the magic is 8 bytes, so every label it printed was two fields out — it showed `buckets2` for an 8-bucket net
/// and the real bucket count under `qb` — and nothing noticed through several multi-hour matches, because
/// nothing tested it. This test is built to be the thing that would have noticed.
///
/// **Its expected values are written down here, not derived from the code under test.** Each fixture is laid
/// out byte by byte at the container's *documented* offsets (skill `reckless-nnue`, "The RNNUE1 container";
/// nnue/README.md §2) with literal numbers, and the three checksums each carries were computed by a separate
/// Python implementation of CRC-64/XZ, not by `crc64.h`. A reader that is off by two bytes, or that swaps two
/// fields, or whose CRC differs by a bit, then disagrees with a number this file states in plain text.
///
/// The fixtures:
///
///  * `kEightBuckets`  — the shared shape's header: 8 king buckets, 8 output heads, 12,617,760 payload bytes.
///  * `kFourBuckets`   — the case that caught the old script out: 4 king buckets, map CRC 0x79FB2D554FABC9C6
///                       (the CRC of `min(file, 7 - file)` for every square), 6,326,304 payload bytes.
///  * `kDistinct`      — a header in which **every field holds a value no other field holds**, so any shift or
///                       swap of offsets changes a label's value. The first two share values (768 and 8 and 8 ...)
///                       that a mis-ordered decoder could land on by luck; this one cannot.
///
/// The payload of each is `byte(i) = (i * 167 + (i >> 11) * 13 + 5) & 0xFF`, a recipe cheap enough to state and
/// to reproduce in another language.
///
/// Then it checks that every kind of damage is *seen*: a flipped header byte, a flipped payload byte, a
/// truncated file, bullet's tail padding left on, a wrong magic, a file shorter than a header, a header whose
/// CRC was recomputed over fields that cannot produce its payload size. A check that has never been seen to
/// fail is not a check, so each of these asserts the failing verdict, not merely the absence of a crash.
///
/// Last, with a net path argument, the real file the build's other suites are given — the shipped v20 net at the
/// shared shape, or the synthetic net at whichever shape the build is — is decoded against literals for those two
/// files.
///
/// Usage:  test_net_header [net.bin]

#include "net_header_report.h"

#include "test_report.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace
{

// --- The container's documented layout, pinned against the engine's own struct at compile time. If NetHeader
// moves a field, this file stops compiling, which is earlier than any test could say so.
static_assert(sizeof(nnue::NetHeader) == 64);
static_assert(offsetof(nnue::NetHeader, magic_) == 0);
static_assert(offsetof(nnue::NetHeader, format_version_) == 8);
static_assert(offsetof(nnue::NetHeader, header_bytes_) == 10);
static_assert(offsetof(nnue::NetHeader, features_per_bucket_) == 12);
static_assert(offsetof(nnue::NetHeader, king_buckets_) == 14);
static_assert(offsetof(nnue::NetHeader, hidden_size_) == 16);
static_assert(offsetof(nnue::NetHeader, output_buckets_) == 18);
static_assert(offsetof(nnue::NetHeader, flags_) == 20);
static_assert(offsetof(nnue::NetHeader, activation_) == 21);
static_assert(offsetof(nnue::NetHeader, qa_) == 22);
static_assert(offsetof(nnue::NetHeader, qb_) == 24);
static_assert(offsetof(nnue::NetHeader, eval_scale_) == 26);
static_assert(offsetof(nnue::NetHeader, reserved_) == 28);
static_assert(offsetof(nnue::NetHeader, king_bucket_map_crc_) == 32);
static_assert(offsetof(nnue::NetHeader, payload_bytes_) == 40);
static_assert(offsetof(nnue::NetHeader, payload_crc_) == 48);
static_assert(offsetof(nnue::NetHeader, header_crc_) == 56);
static_assert(sizeof(nnue::NetHeader::flags_) == 1 && sizeof(nnue::NetHeader::activation_) == 1,
              "flags and activation are uint8, not uint16");

using Bytes = std::vector<std::uint8_t>;

void Check(bool condition, const std::string &message)
{
    test_report::Check(condition, message);
}

/// @brief A header laid out with literal values at the documented offsets, by hand.
struct Fields
{
    std::uint16_t format_version;
    std::uint16_t header_bytes;
    std::uint16_t features_per_bucket;
    std::uint16_t king_buckets;
    std::uint16_t hidden_size;
    std::uint16_t output_buckets;
    std::uint8_t  flags;
    std::uint8_t  activation;
    std::uint16_t qa;
    std::uint16_t qb;
    std::uint16_t eval_scale;
    std::uint64_t king_bucket_map_crc;
    std::uint64_t payload_bytes;
    std::uint64_t payload_crc; ///< Computed by the separate Python implementation.
    std::uint64_t header_crc;  ///< Likewise.
};

void PutU16(Bytes &bytes, std::size_t offset, std::uint16_t value)
{
    bytes[offset]     = static_cast<std::uint8_t>(value & 0xFF);
    bytes[offset + 1] = static_cast<std::uint8_t>(value >> 8);
}

void PutU64(Bytes &bytes, std::size_t offset, std::uint64_t value)
{
    for (std::size_t index = 0; index < 8; ++index)
    {
        bytes[offset + index] = static_cast<std::uint8_t>(value >> (8 * index));
    }
}

/// @brief The fixture payload: byte(i) = (i * 167 + (i >> 11) * 13 + 5) & 0xFF.
std::uint8_t PayloadByte(std::uint64_t index)
{
    return static_cast<std::uint8_t>((index * 167 + (index >> 11) * 13 + 5) & 0xFF);
}

/// @brief Lay a whole net file out: the header at its documented offsets, then the recipe payload.
Bytes BuildNet(const Fields &fields)
{
    Bytes      bytes(fields.header_bytes + fields.payload_bytes, 0);
    const char magic[8] = {'R', 'N', 'N', 'U', 'E', '1', '\0', '\0'};
    for (std::size_t index = 0; index < 8; ++index)
    {
        bytes[index] = static_cast<std::uint8_t>(magic[index]);
    }
    PutU16(bytes, 8, fields.format_version);
    PutU16(bytes, 10, fields.header_bytes);
    PutU16(bytes, 12, fields.features_per_bucket);
    PutU16(bytes, 14, fields.king_buckets);
    PutU16(bytes, 16, fields.hidden_size);
    PutU16(bytes, 18, fields.output_buckets);
    bytes[20] = fields.flags;
    bytes[21] = fields.activation;
    PutU16(bytes, 22, fields.qa);
    PutU16(bytes, 24, fields.qb);
    PutU16(bytes, 26, fields.eval_scale);
    // 28..31 reserved, zero.
    PutU64(bytes, 32, fields.king_bucket_map_crc);
    PutU64(bytes, 40, fields.payload_bytes);
    PutU64(bytes, 48, fields.payload_crc);
    PutU64(bytes, 56, fields.header_crc);
    for (std::uint64_t index = 0; index < fields.payload_bytes; ++index)
    {
        bytes[fields.header_bytes + index] = PayloadByte(index);
    }
    return bytes;
}

// The shared shape. CRCs from an independent Python CRC-64/XZ over the recipe payload and the 56 header bytes.
const Fields kEightBuckets{
    .format_version      = 2,
    .header_bytes        = 64,
    .features_per_bucket = 768,
    .king_buckets        = 8,
    .hidden_size         = 1024,
    .output_buckets      = 8,
    .flags               = 0x01,
    .activation          = 0,
    .qa                  = 255,
    .qb                  = 64,
    .eval_scale          = 400,
    .king_bucket_map_crc = 0x0A20060D4E9586E0ULL,
    .payload_bytes       = 12617760,
    .payload_crc         = 0xB98E8442DA1D0230ULL,
    .header_crc          = 0x217476A2E6F04700ULL,
};

// Four king buckets: the header the old script mislabelled.
const Fields kFourBuckets{
    .format_version      = 2,
    .header_bytes        = 64,
    .features_per_bucket = 768,
    .king_buckets        = 4,
    .hidden_size         = 1024,
    .output_buckets      = 8,
    .flags               = 0x01,
    .activation          = 0,
    .qa                  = 255,
    .qb                  = 64,
    .eval_scale          = 400,
    .king_bucket_map_crc = 0x79FB2D554FABC9C6ULL,
    .payload_bytes       = 6326304,
    .payload_crc         = 0x0AB8FEB8076003FDULL,
    .header_crc          = 0xEC396838E57DFBB8ULL,
};

// Every field distinct: no two fields share a value, so no two labels can be swapped unseen.
const Fields kDistinct{
    .format_version      = 3,
    .header_bytes        = 64,
    .features_per_bucket = 770,
    .king_buckets        = 5,
    .hidden_size         = 96,
    .output_buckets      = 6,
    .flags               = 0x03,
    .activation          = 2,
    .qa                  = 301,
    .qb                  = 77,
    .eval_scale          = 555,
    .king_bucket_map_crc = 0x1122334455667788ULL,
    .payload_bytes       = 741720,
    .payload_crc         = 0x952A408BEF291F35ULL,
    .header_crc          = 0x247318F73B538341ULL,
};

/// @brief What running the tool on a file produced.
struct Outcome
{
    int                                exit_code = -1;
    std::map<std::string, std::string> keys;
    std::string                        standard_error;
    std::size_t                        line_count                  = 0;
    bool                               is_shape_probe_in_agreement = true;

    /// @brief A key's value, or `<missing>` so an absent key can never equal an expected one.
    std::string Get(const std::string &key) const
    {
        const auto found = keys.find(key);
        return found == keys.end() ? "<missing>" : found->second;
    }
};

/// @brief Run the command line on the file at @p path, in process, and collect what it printed.
Outcome RunToolOnFile(const std::filesystem::path &path)
{
    Outcome            outcome;
    std::ostringstream standard_out;
    std::ostringstream standard_error;
    outcome.exit_code      = net_header::Run({path.string()}, standard_out, standard_error);
    outcome.standard_error = standard_error.str();

    std::istringstream lines(standard_out.str());
    std::string        line;
    while (std::getline(lines, line))
    {
        const std::size_t equals = line.find('=');
        if (equals != std::string::npos)
        {
            outcome.keys[line.substr(0, equals)] = line.substr(equals + 1);
        }
        ++outcome.line_count;
    }

    // The engine has its own "is this file my shape" probe (nnue::IsNetFileThisBuildsShape). Where the magic is
    // right the reader and the probe must reach the same verdict; a disagreement means one of them is wrong.
    if (outcome.Get("magic_ok") == "yes")
    {
        outcome.is_shape_probe_in_agreement =
            nnue::IsNetFileThisBuildsShape(path.string()) == (outcome.Get("shape_matches_build") == "yes");
    }
    return outcome;
}

/// @brief Write @p bytes to a scratch file and run the command line on it.
Outcome RunTool(const Bytes &bytes)
{
    const std::filesystem::path path = std::filesystem::temp_directory_path() / "pawnstar_net_header_test.bin";
    {
        std::ofstream out(path, std::ios::binary);
        out.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }
    const Outcome   outcome = RunToolOnFile(path);
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
    return outcome;
}

/// @brief Assert that one key of the output holds exactly @p expected.
void ExpectKey(const Outcome &outcome, const std::string &label, const std::string &key, const std::string &expected)
{
    const auto found      = outcome.keys.find(key);
    const bool is_present = found != outcome.keys.end();
    Check(is_present && found->second == expected, label + ": " + key + " is " +
                                                       (is_present ? "'" + found->second + "'" : "missing") +
                                                       ", expected '" + expected + "'");
}

/// @brief Assert every decoded field of a fixture, by its literal values, in the output a script would read.
void ExpectFixtureDecoded(const std::string &label, const Outcome &outcome, const Fields &fields,
                          const std::string &file_bytes_text, const std::string &payload_bytes_text,
                          const std::string &map_crc_text, const std::string &payload_crc_text,
                          const std::string &header_crc_text)
{
    ExpectKey(outcome, label, "file_bytes", file_bytes_text);
    ExpectKey(outcome, label, "header_complete", "yes");
    ExpectKey(outcome, label, "magic", "RNNUE1\\x00\\x00");
    ExpectKey(outcome, label, "magic_ok", "yes");
    ExpectKey(outcome, label, "format_version", std::to_string(fields.format_version));
    ExpectKey(outcome, label, "header_bytes", std::to_string(fields.header_bytes));
    ExpectKey(outcome, label, "features_per_bucket", std::to_string(fields.features_per_bucket));
    ExpectKey(outcome, label, "king_buckets", std::to_string(fields.king_buckets));
    ExpectKey(outcome, label, "hidden_size", std::to_string(fields.hidden_size));
    ExpectKey(outcome, label, "output_buckets", std::to_string(fields.output_buckets));
    ExpectKey(outcome, label, "activation", std::to_string(fields.activation));
    ExpectKey(outcome, label, "qa", std::to_string(fields.qa));
    ExpectKey(outcome, label, "qb", std::to_string(fields.qb));
    ExpectKey(outcome, label, "eval_scale", std::to_string(fields.eval_scale));
    ExpectKey(outcome, label, "reserved_zero", "yes");
    ExpectKey(outcome, label, "king_bucket_map_crc", map_crc_text);
    ExpectKey(outcome, label, "payload_bytes", payload_bytes_text);
    ExpectKey(outcome, label, "payload_crc", payload_crc_text);
    ExpectKey(outcome, label, "header_crc", header_crc_text);
    ExpectKey(outcome, label, "header_crc_computed", header_crc_text);
    ExpectKey(outcome, label, "header_crc_ok", "yes");
    ExpectKey(outcome, label, "actual_payload_bytes", payload_bytes_text);
    ExpectKey(outcome, label, "payload_size_ok", "yes");
    ExpectKey(outcome, label, "payload_crc_computed", payload_crc_text);
    ExpectKey(outcome, label, "payload_crc_ok", "yes");
    ExpectKey(outcome, label, "payload_bytes_from_fields", payload_bytes_text);
    ExpectKey(outcome, label, "payload_bytes_from_fields_ok", "yes");
    ExpectKey(outcome, label, "integrity", "ok");
    Check(outcome.exit_code == 0,
          label + ": exits 0 for an intact net, whatever its shape (got " + std::to_string(outcome.exit_code) + ")");
    Check(outcome.is_shape_probe_in_agreement,
          label + ": agrees with nnue::IsNetFileThisBuildsShape about whether the shape is this build's");
}

/// @brief True when @p list names @p field (a comma-separated shape_mismatches value).
bool NamesField(const std::string &list, const std::string &field)
{
    std::istringstream stream(list);
    std::string        item;
    while (std::getline(stream, item, ','))
    {
        if (item == field)
        {
            return true;
        }
    }
    return false;
}

// ---------------------------------------------------------------------------------------------------------------

void TestEightBucketNet()
{
    const Bytes   bytes   = BuildNet(kEightBuckets);
    const Outcome outcome = RunTool(bytes);
    ExpectFixtureDecoded("8-bucket", outcome, kEightBuckets, "12617824", "12617760", "0x0A20060D4E9586E0",
                         "0xB98E8442DA1D0230", "0x217476A2E6F04700");
    ExpectKey(outcome, "8-bucket", "flags", "0x01");
    ExpectKey(outcome, "8-bucket", "king_mirroring", "on");
    ExpectKey(outcome, "8-bucket", "flags_unknown_bits", "0x00");
    ExpectKey(outcome, "8-bucket", "activation_name", "screlu");
    // Not a coincidence of the labels: the four 8s and the 768 and 1024 sit in four different fields, so a reader
    // two bytes out would print 8 where 1024 belongs and 1024 where 8 belongs. Name the tell-tale directly.
    Check(outcome.Get("hidden_size") == "1024" && outcome.Get("king_buckets") == "8" &&
              outcome.Get("output_buckets") == "8" && outcome.Get("features_per_bucket") == "768",
          "8-bucket: features_per_bucket 768, king_buckets 8, hidden_size 1024, output_buckets 8");
    Check(outcome.Get("qa") == "255" && outcome.Get("qb") == "64" && outcome.Get("eval_scale") == "400",
          "8-bucket: qa 255, qb 64, eval_scale 400, each under its own label");
}

void TestFourBucketNet()
{
    const Bytes   bytes   = BuildNet(kFourBuckets);
    const Outcome outcome = RunTool(bytes);
    ExpectFixtureDecoded("4-bucket", outcome, kFourBuckets, "6326368", "6326304", "0x79FB2D554FABC9C6",
                         "0x0AB8FEB8076003FD", "0xEC396838E57DFBB8");
    // The case that caught the old script out: it printed the bucket count under the wrong label. Not refused,
    // and said plainly not to be this build's shape — king_buckets always differs from a build of 8 or 4.
    ExpectKey(outcome, "4-bucket", "king_buckets", "4");
    ExpectKey(outcome, "4-bucket", "qb", "64");
    ExpectKey(outcome, "4-bucket", "qa", "255");
    Check(outcome.Get("shape_matches_build") == "no" || nnue::kNumKingBuckets == 4,
          "4-bucket: reported as not this build's shape (this build has " + std::to_string(nnue::kNumKingBuckets) +
              " king buckets)");
    Check(NamesField(outcome.Get("shape_mismatches"), "king_buckets") == (nnue::kNumKingBuckets != 4),
          "4-bucket: king_buckets is among the mismatches exactly when this build does not have 4");
    Check(NamesField(outcome.Get("shape_mismatches"), "payload_bytes") ==
              (nnue::kNetPayloadBytes != kFourBuckets.payload_bytes),
          "4-bucket: payload_bytes is among the mismatches exactly when this build's payload differs");
    if (nnue::kNumKingBuckets != 4)
    {
        Check(outcome.standard_error.find("COPY of print_net_header was compiled at") != std::string::npos &&
                  outcome.standard_error.find("king_buckets: file 4") != std::string::npos,
              "4-bucket: stderr says the shape differs, with the file's value beside the build's");
        // The wording is the fix for a real operational trap, so the test holds it to what it must say. A note
        // about a shape must be unmistakably about THIS DECODER's build (the decoder a match harness finds is
        // routinely not the engine that will play), and must not read as a refusal of a sound net - while the
        // verdict that does matter, integrity, stays in front of it.
        Check(outcome.standard_error.find("integrity=ok - that is the verdict on the FILE") != std::string::npos,
              "4-bucket: stderr leads with integrity, the verdict on the file, before the shape note");
        Check(outcome.standard_error.find("would refuse it") == std::string::npos &&
                  outcome.standard_error.find("not this build's, so") == std::string::npos,
              "4-bucket: stderr does not read as a refusal of a net that is perfectly sound");
        Check(outcome.standard_error.find("a statement about the decoder's build, not about the net") !=
                  std::string::npos,
              "4-bucket: stderr says whose build the shape verdict is about");
    }
}

void TestEveryFieldDistinct()
{
    const Bytes   bytes   = BuildNet(kDistinct);
    const Outcome outcome = RunTool(bytes);
    ExpectFixtureDecoded("distinct", outcome, kDistinct, "741784", "741720", "0x1122334455667788", "0x952A408BEF291F35",
                         "0x247318F73B538341");
    ExpectKey(outcome, "distinct", "flags", "0x03");
    ExpectKey(outcome, "distinct", "king_mirroring", "on");
    ExpectKey(outcome, "distinct", "flags_unknown_bits", "0x02");
    ExpectKey(outcome, "distinct", "activation", "2");
    ExpectKey(outcome, "distinct", "activation_name", "unknown");
    // 20 and 21 are single bytes: a reader treating them as uint16 would print flags 0x0203 and swallow qa's low
    // byte, which this file's qa (301 = 0x012D) would then betray.
    ExpectKey(outcome, "distinct", "qa", "301");
}

/// @brief A build whose own shape this is: stamped by the engine's own functions, it must read as a match.
void TestThisBuildsShapeReadsAsMatch()
{
    nnue::NetHeader header = nnue::ExpectedNetHeader();
    Bytes           payload(nnue::kNetPayloadBytes, 0);
    nnue::FinaliseNetHeader(header, payload.data(), payload.size());
    Bytes bytes(sizeof(header) + payload.size());
    std::memcpy(bytes.data(), &header, sizeof(header));
    std::memcpy(bytes.data() + sizeof(header), payload.data(), payload.size());

    const Outcome outcome = RunTool(bytes);
    ExpectKey(outcome, "this-build", "shape_matches_build", "yes");
    ExpectKey(outcome, "this-build", "shape_mismatches", "");
    ExpectKey(outcome, "this-build", "integrity", "ok");
    Check(outcome.exit_code == 0 && outcome.standard_error.empty(),
          "this-build: exit 0 and nothing on stderr for a net of this build's own shape");
    Check(outcome.is_shape_probe_in_agreement, "this-build: agrees with nnue::IsNetFileThisBuildsShape");
}

/// @brief Every output has the same keys in the same order, so a script can rely on the shape of the report.
void TestOutputShapeIsStable()
{
    const Outcome intact = RunTool(BuildNet(kDistinct));
    const Outcome empty  = RunTool(Bytes{});
    Check(intact.line_count == empty.line_count && intact.keys.size() == empty.keys.size() && intact.line_count > 25,
          "output: the same " + std::to_string(intact.line_count) + " keys for an intact net and an empty file");
    bool is_same = true;
    for (const auto &entry : intact.keys)
    {
        is_same = is_same && empty.keys.count(entry.first) == 1;
    }
    Check(is_same, "output: an empty file yields every key the intact net does");
}

// --- Damage. Each asserts the FAILING verdict, in the field that should carry it, and that the other checks
// still say what is true: one fault must not paint the whole report red.

void TestCorruptedHeaderIsCaught()
{
    // qb 64 -> 65 with header_crc left as it was. qb is chosen because nothing else in the report depends on it,
    // so the header CRC is the only check that can notice (hidden_size would also trip the payload arithmetic).
    Bytes bytes = BuildNet(kEightBuckets);
    PutU16(bytes, 24, 65);
    const Outcome outcome = RunTool(bytes);
    ExpectKey(outcome, "corrupt header", "qb", "65"); // still decoded and printed
    ExpectKey(outcome, "corrupt header", "header_crc_ok", "no");
    ExpectKey(outcome, "corrupt header", "header_crc", "0x217476A2E6F04700"); // the stored value, unchanged
    ExpectKey(outcome, "corrupt header", "payload_crc_ok", "yes");
    ExpectKey(outcome, "corrupt header", "payload_size_ok", "yes");
    ExpectKey(outcome, "corrupt header", "payload_bytes_from_fields_ok", "yes");
    ExpectKey(outcome, "corrupt header", "integrity", "fail");
    Check(outcome.Get("header_crc_computed") != outcome.Get("header_crc"),
          "corrupt header: the computed header CRC differs from the stored one");
    Check(outcome.exit_code == 1, "corrupt header: exit status 1 (got " + std::to_string(outcome.exit_code) + ")");
    Check(outcome.standard_error.find("FAIL header_crc") != std::string::npos,
          "corrupt header: stderr names the failing check");
}

void TestCorruptedPayloadIsCaught()
{
    Bytes bytes = BuildNet(kEightBuckets);
    bytes[64 + 6'000'000] ^= 0x01U; // one bit, deep in the payload
    const Outcome outcome = RunTool(bytes);
    ExpectKey(outcome, "corrupt payload", "header_crc_ok", "yes");
    ExpectKey(outcome, "corrupt payload", "payload_size_ok", "yes");
    ExpectKey(outcome, "corrupt payload", "payload_crc_ok", "no");
    ExpectKey(outcome, "corrupt payload", "integrity", "fail");
    Check(outcome.exit_code == 1, "corrupt payload: exit status 1 (got " + std::to_string(outcome.exit_code) + ")");
    Check(outcome.Get("payload_crc_computed") != outcome.Get("payload_crc"),
          "corrupt payload: the computed payload CRC differs from the header's");
    Check(outcome.standard_error.find("FAIL payload_crc") != std::string::npos,
          "corrupt payload: stderr names the failing check");
}

void TestTruncationAndPaddingAreCaught()
{
    Bytes truncated = BuildNet(kDistinct);
    truncated.pop_back();
    const Outcome short_outcome = RunTool(truncated);
    ExpectKey(short_outcome, "truncated", "payload_size_ok", "no");
    ExpectKey(short_outcome, "truncated", "payload_crc_ok", "no");
    ExpectKey(short_outcome, "truncated", "header_crc_ok", "yes");
    ExpectKey(short_outcome, "truncated", "actual_payload_bytes", "741719");
    ExpectKey(short_outcome, "truncated", "integrity", "fail");

    // bullet's tail padding: the ASCII word "bullet" repeated. An exact length is the container's rule.
    Bytes padded = BuildNet(kDistinct);
    for (std::size_t index = 0; index < 62; ++index)
    {
        padded.push_back(static_cast<std::uint8_t>("bullet"[index % 6]));
    }
    const Outcome padded_outcome = RunTool(padded);
    ExpectKey(padded_outcome, "padded", "payload_size_ok", "no");
    ExpectKey(padded_outcome, "padded", "actual_payload_bytes", "741782");
    ExpectKey(padded_outcome, "padded", "integrity", "fail");
    Check(padded_outcome.exit_code == 1, "padded: exit status 1");
}

void TestNotANetIsStillDescribed()
{
    Bytes no_magic = BuildNet(kDistinct);
    no_magic[0]    = 'P';
    no_magic[1]    = 'S';
    no_magic[2]    = 'N';
    no_magic[3]    = '1';
    PutU64(no_magic, 56, crc64::Checksum(no_magic.data(), 56)); // so only the magic can give it away
    const Outcome no_magic_outcome = RunTool(no_magic);
    ExpectKey(no_magic_outcome, "no magic", "magic", "PSN1E1\\x00\\x00");
    ExpectKey(no_magic_outcome, "no magic", "magic_ok", "no");
    ExpectKey(no_magic_outcome, "no magic", "integrity", "fail");
    Check(no_magic_outcome.exit_code == 1, "no magic: exit status 1");

    const Outcome tiny = RunTool(Bytes(40, 0x55));
    ExpectKey(tiny, "40-byte file", "header_complete", "no");
    ExpectKey(tiny, "40-byte file", "king_buckets", "unavailable");
    ExpectKey(tiny, "40-byte file", "integrity", "fail");
    Check(tiny.exit_code == 1, "40-byte file: exit status 1, and no crash");

    const Outcome empty = RunTool(Bytes{});
    ExpectKey(empty, "empty file", "file_bytes", "0");
    ExpectKey(empty, "empty file", "integrity", "fail");
}

/// @brief A header whose CRC is right but whose fields cannot produce its payload size: sound as bytes, wrong
/// as a description. Recompute header_crc over the lie, so only the arithmetic can give it away.
void TestHeaderThatContradictsItselfIsCaught()
{
    Bytes bytes = BuildNet(kDistinct);
    PutU16(bytes, 14, 6); // king_buckets 5 -> 6: payload_bytes no longer follows from the fields
    PutU64(bytes, 56, crc64::Checksum(bytes.data(), 56));
    const Outcome outcome = RunTool(bytes);
    ExpectKey(outcome, "self-contradicting", "header_crc_ok", "yes");
    ExpectKey(outcome, "self-contradicting", "payload_crc_ok", "yes");
    ExpectKey(outcome, "self-contradicting", "payload_bytes_from_fields_ok", "no");
    ExpectKey(outcome, "self-contradicting", "integrity", "fail");
}

void TestHeaderBytesOutsideFile()
{
    // header_bytes below the 64 bytes the struct occupies, and header_bytes past the end of a short file: either way
    // the payload has no place to be, so no payload verdict is given - and no out-of-bounds read is made.
    Bytes too_small = BuildNet(kDistinct);
    PutU16(too_small, 10, 63);
    PutU64(too_small, 56, crc64::Checksum(too_small.data(), 56));
    const Outcome small_outcome = RunTool(too_small);
    ExpectKey(small_outcome, "header_bytes 63", "header_bytes", "63");
    ExpectKey(small_outcome, "header_bytes 63", "header_crc_ok", "yes");
    ExpectKey(small_outcome, "header_bytes 63", "actual_payload_bytes", "unavailable");
    ExpectKey(small_outcome, "header_bytes 63", "payload_crc_ok", "no");
    ExpectKey(small_outcome, "header_bytes 63", "integrity", "fail");

    Bytes too_large = BuildNet(kDistinct);
    too_large.resize(100);
    PutU16(too_large, 10, 200);
    PutU64(too_large, 56, crc64::Checksum(too_large.data(), 56));
    const Outcome large_outcome = RunTool(too_large);
    ExpectKey(large_outcome, "header_bytes 200 in a 100-byte file", "actual_payload_bytes", "unavailable");
    ExpectKey(large_outcome, "header_bytes 200 in a 100-byte file", "payload_size_ok", "no");
    ExpectKey(large_outcome, "header_bytes 200 in a 100-byte file", "integrity", "fail");
    Check(large_outcome.standard_error.find("outside the file") != std::string::npos,
          "header_bytes 200 in a 100-byte file: stderr says the payload checks were not made");
}

void TestUnreadableFileAndUsage()
{
    std::ostringstream out;
    std::ostringstream error;
    const int          missing = net_header::Run({"definitely/not/a/file.bin"}, out, error);
    Check(missing == 2 && out.str().empty(), "missing file: exit status 2 and nothing on stdout");
    std::ostringstream usage_out;
    std::ostringstream usage_error;
    const int          usage = net_header::Run({}, usage_out, usage_error);
    Check(usage == 2 && usage_out.str().empty() && usage_error.str().find("usage:") != std::string::npos,
          "no argument: exit status 2 and a usage line on stderr");
}

// --- The real files the build's other suites are given.

/// @brief The shipped net's own CRCs - the only two fields that tell it from the synthetic net of the same
/// shape and the same size. Re-bless both together whenever a new net ships.
constexpr std::string_view kShippedNetPayloadCrc = "0x62C460241DDC3EF7";
constexpr std::string_view kShippedNetHeaderCrc  = "0x5A2DFC8FA67F2B2C";

/// @brief The synthetic net at the shared shape, written by test_nnue_incremental from a fixed seed; its
/// size and SHA-256 are what tools/new-shape-signature.sh asserts, and these CRCs were computed by a
/// separate Python implementation rather than by this build.
constexpr std::string_view kSyntheticSharedShapePayloadCrc = "0xE6F3FB70D96D876A";
constexpr std::string_view kSyntheticSharedShapeHeaderCrc  = "0x7B18813893FDE2D9";

/// @brief The synthetic net at the retired v12 shape - mirroring off, one output head - which the build
/// knobs still reach. No net of that shape is shipped, so this is the only file of that size a build is
/// ever handed. Its payload is the same fixed-seed stream as the shared shape's up to the smaller output
/// layer, which is why only its header CRC differs from that net's.
constexpr std::string_view kSyntheticV12ShapePayloadCrc = "0x710AB57A62B2043D";
constexpr std::string_view kSyntheticV12ShapeHeaderCrc  = "0xB207CEAD671664E1";

/// @brief Literal facts about the real nets this repository's builds are handed, keyed by file size and,
/// where a size no longer identifies one file, by payload CRC.
void TestRealNet(const std::string &path)
{
    Bytes bytes;
    if (!net_header::ReadWholeFile(path, bytes))
    {
        Check(false, "real net: cannot read '" + path + "'");
        return;
    }
    const Outcome outcome = RunToolOnFile(path);

    const std::string label = "real net (" + std::to_string(bytes.size()) + " bytes)";
    ExpectKey(outcome, label, "integrity", "ok");
    ExpectKey(outcome, label, "shape_matches_build", "yes"); // the build is handed a net of its own shape
    Check(outcome.is_shape_probe_in_agreement, label + ": agrees with nnue::IsNetFileThisBuildsShape");

    if (bytes.size() == 12'617'824)
    {
        // The shared shape, which is the default and the shipped net's. TWO files have this size: the
        // shipped pawnstar-v20.bin and the synthetic net every port writes from one fixed seed (SHA-256
        // 4abfdb7b...ad4871b, which tools/new-shape-signature.sh checks). Their architecture fields are
        // identical - that is what "the same shape" means - so the shape is asserted once here and the two
        // are then told apart by payload_crc, which pins the header_crc that belongs with it. A net of this
        // size that is neither fails, rather than passing on the shape alone.
        ExpectKey(outcome, label, "format_version", "2");
        ExpectKey(outcome, label, "features_per_bucket", "768");
        ExpectKey(outcome, label, "king_buckets", "8");
        ExpectKey(outcome, label, "hidden_size", "1024");
        ExpectKey(outcome, label, "output_buckets", "8");
        ExpectKey(outcome, label, "flags", "0x01");
        ExpectKey(outcome, label, "king_mirroring", "on");
        ExpectKey(outcome, label, "qa", "255");
        ExpectKey(outcome, label, "qb", "64");
        ExpectKey(outcome, label, "eval_scale", "400");
        ExpectKey(outcome, label, "king_bucket_map_crc", "0x0A20060D4E9586E0");
        ExpectKey(outcome, label, "payload_bytes", "12617760");

        const std::string payload_crc = outcome.Get("payload_crc");
        if (payload_crc == kShippedNetPayloadCrc)
        {
            ExpectKey(outcome, label, "header_crc", std::string(kShippedNetHeaderCrc));
            std::cout << "[INFO] " << label << ": the shipped pawnstar-v20.bin\n";
        }
        else if (payload_crc == kSyntheticSharedShapePayloadCrc)
        {
            ExpectKey(outcome, label, "header_crc", std::string(kSyntheticSharedShapeHeaderCrc));
            std::cout << "[INFO] " << label << ": the synthetic net at the shared shape\n";
        }
        else
        {
            Check(false, label + ": payload_crc is " + payload_crc + ", which is neither the shipped net's " +
                             std::string(kShippedNetPayloadCrc) + " nor the synthetic net's " +
                             std::string(kSyntheticSharedShapePayloadCrc));
        }
    }
    else if (bytes.size() == 12'589'124)
    {
        // The retired v12 shape - mirroring off, one output head - which the build knobs still reach. No
        // net of that shape is shipped any more, so the only file of this size a build is handed is the
        // synthetic net written at that shape.
        ExpectKey(outcome, label, "format_version", "1");
        ExpectKey(outcome, label, "features_per_bucket", "768");
        ExpectKey(outcome, label, "king_buckets", "8");
        ExpectKey(outcome, label, "hidden_size", "1024");
        ExpectKey(outcome, label, "output_buckets", "1");
        ExpectKey(outcome, label, "flags", "0x00");
        ExpectKey(outcome, label, "king_mirroring", "off");
        ExpectKey(outcome, label, "qa", "255");
        ExpectKey(outcome, label, "qb", "64");
        ExpectKey(outcome, label, "eval_scale", "400");
        ExpectKey(outcome, label, "king_bucket_map_crc", "0xCC6DDB5DAF0AB18D");
        ExpectKey(outcome, label, "payload_bytes", "12589060");
        ExpectKey(outcome, label, "payload_crc", std::string(kSyntheticV12ShapePayloadCrc));
        ExpectKey(outcome, label, "header_crc", std::string(kSyntheticV12ShapeHeaderCrc));
    }
    else
    {
        std::cout << "[INFO] " << label << ": not one of the known nets, so only its integrity and shape were "
                  << "checked\n";
    }
}

} // namespace

int main(int argc, char **argv)
{
    TestEightBucketNet();
    TestFourBucketNet();
    TestEveryFieldDistinct();
    TestThisBuildsShapeReadsAsMatch();
    TestOutputShapeIsStable();
    TestCorruptedHeaderIsCaught();
    TestCorruptedPayloadIsCaught();
    TestTruncationAndPaddingAreCaught();
    TestNotANetIsStillDescribed();
    TestHeaderThatContradictsItselfIsCaught();
    TestHeaderBytesOutsideFile();
    TestUnreadableFileAndUsage();
    if (argc > 1)
    {
        TestRealNet(argv[1]);
    }
    return test_report::Summary(test_report::failures == 0
                                    ? "net header: every check passed"
                                    : "net header: " + std::to_string(test_report::failures) + " check(s) failed");
}

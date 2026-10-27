/// @file lc0_extract.cpp Turn Leela Chess Zero's v6 self-play training data into NNUE training shards.
///
/// The extraction rules all live in tools/lc0_extract.h, which test/lc0_extract_test.cpp drives directly;
/// this file is the command line, the shard writer and the provenance sidecar around them.
///
/// ## What this is for
///
/// The two Lichess sources (tools/lichess_extract.cpp) are **human** games, and that costs twice over: a
/// human game's result is nearly unpredictable from the position, so the `ConstantWDL { value: 0.5 }` half
/// of the trainer's target is largely noise, and human games end in resignation, so decisive positions are
/// scarce. Lc0's test80 archive is **engine self-play with engine results**: the value estimate is an MCTS
/// search by a net far stronger than ours, the result is what that engine actually scored from the
/// position, and the games are adjudicated by a threshold rather than by a human giving up.
///
/// ## Usage
///
/// A tar is a gigabyte and its members are gzip streams, so the input is a stream and never a file on disk.
/// A gzip decompressor reads the concatenation of a tar's members as one stream, which is the whole of the
/// pipeline:
///
/// ```sh
/// curl -fsS --retry 3 -o chunk.tar \
///   https://storage.lczero.org/files/training_data/test80/training-run1-test80-20250820-1017.tar
/// tar -xOf chunk.tar --exclude LICENSE \
///   | gzip -dc \
///   | lc0_extract --out-dir pool/ \
///       --source-url https://storage.lczero.org/files/training_data/test80/training-run1-test80-20250820-1017.tar
/// ```
///
/// Download the tar to a file first and check its status rather than piping `curl` straight in: a 429 or a
/// 503 body piped into a decompressor looks exactly like a tar that produced no records.
///
/// Options, all optional but `--out-dir`:
///
///   --out-dir <dir>         where the shards go (default `.`). Written as `data_0000.txt`, `data_0001.txt`
///                           ..., which is the name tools/check_duplicates.sh and tools/train_pipeline.sh's
///                           text-directory mode already glob for.
///   --shard-positions <n>   records per shard (default 2000000). A shard only ever rotates on a game
///                           boundary, so the figure is a target and not a hard size.
///   --skip-plies <n>        drop this many plies from the start of each game (default 8).
///   --max-plies <n>         keep nothing past this ply (default 200, 0 = no cap).
///   --eval-cap <cp>         drop a position whose |eval| is at or above this (default 3000).
///   --eval-scale <cp>       the sigmoid scale the mapping inverts (default 400, which is `eval_scale` in
///                           tools/bullet/pawnstar.rs). It scales the score column linearly, so it is also
///                           the knob for putting this pool on another pool's numeric scale - 276 is what
///                           the measured slope against this engine's own search implies. Leaving it at 400
///                           is the self-consistent choice; see the value-head section of
///                           tools/lc0_extract.h before changing it.
///   --q-scale <f>           multiply lc0's `q` by this before the mapping (default 1.0). This is *not* a
///                           linear rescale of the score - it compresses the saturated ends much more than
///                           the middle - so it is for an over-confident value head, not for a scale
///                           mismatch. Use --eval-scale for that.
///   --min-visits <n>        drop a record whose root search had fewer visits than this (default 100).
///   --value-head best|root  which of lc0's estimates becomes the label (default best).
///   --skip-adjudicated      drop every record of a game lc0 adjudicated rather than played out.
///   --no-quiet-filter       keep a position whose next move was a capture or a promotion.
///   --max-bytes <n>         stop after this many bytes of input (0 = to end of input). Checked on a game
///                           boundary, so a little more than this is consumed.
///   --source-url <url>      which tar this is, for the sidecar.
///   --source-sha256 <hex>   the SHA-256 of that tar, for the sidecar - `sha256sum` of the file that was
///                           downloaded. This is the provenance worth recording, and it is cheap: one pass
///                           over a gigabyte rather than forty, since a tar inflates about 41x.
///   --no-input-digest       do not SHA-256 the decompressed stream. That digest is the whole per-byte cost
///                           of a run (44.3 s of a measured 52.7 s, against 8.4 s for every decode and
///                           filter), so a harvest that records --source-sha256 instead should switch it
///                           off. A run with neither records no provenance at all, and says so.
///   --source-run <name>     which training run, for the sidecar (default `test80`).
///   --no-dedup              keep repeated positions. By default a position whose Zobrist key has already
///                           been written to the current shard is dropped, which is gen_data's rule too.
///
/// ## The provenance sidecar, and the attribution it exists to make writable
///
/// Each shard gets a `<shard>.meta` beside it, in the same `key value` shape tools/gen_data.cpp and
/// tools/lichess_extract.cpp write (so tools/check_duplicates.sh reads the `games_completed` line out of
/// any of them without caring which tool wrote it). It carries the licence, the source URL and run, the
/// filter settings in full, the counts, and the SHA-256 of the exact input prefix the shard's records came
/// from.
///
/// The licence line is the point of the file. The archive is **ODbL 1.0** with its contents under **DbCL
/// 1.0**, and ODbL obliges a notice naming the source whenever a Produced Work - a trained net - is
/// distributed. The sidecar is meant to be the thing that notice is written from without going back to the
/// web: `source_licence`, `source_licence_url`, `contents_licence`, `contents_licence_url` and a ready-made
/// `attribution` line are all there.
///
/// The digest is the other part worth being careful about. The input is a pipe, so it can never be re-read
/// to check it afterwards, and a tar is far too large to keep: the digest taken as the bytes went past is
/// the *only* evidence of what was read. It covers exactly the records handed out as part of a game, not
/// the one-record lookahead the reader holds to find the next game boundary, and each shard records the
/// byte range it covers as well.
///
/// One expected piece of noise: tools/check_duplicates.sh audits the *campaign identity* of the sidecars it
/// finds (which worker of which partitioned self-play campaign wrote each shard) and warns that a shard
/// recording none "cannot be audited". A shard from here records none, and should not: there is no opening
/// pool to partition and no worker index, because a run reads one tar front to back. The partition of an
/// lc0 harvest is the tar, which `source_url` names. The warning is accurate and the gate still passes.

#include "lc0_extract.h"

#include "version.h"

#include <cstdio>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_set>

#ifdef _WIN32
// The input is binary, and on Windows stdin is not. See the note in main(). This is kept here rather than
// routed through src/platform.h because it is a property of this tool's stdin, not of the engine.
#include <fcntl.h>
#include <io.h>
#endif

namespace
{
/// @brief The archive's licence, taken from the `LICENSE` member every test80 tar carries. It is recorded
/// in every shard rather than assumed, because ODbL obliges a notice naming it when a net trained on these
/// positions is distributed - see the file comment.
constexpr const char *kSourceLicence      = "ODbL-1.0";
constexpr const char *kSourceLicenceUrl   = "http://opendatacommons.org/licenses/odbl/1.0/";
constexpr const char *kContentsLicence    = "DbCL-1.0";
constexpr const char *kContentsLicenceUrl = "http://opendatacommons.org/licenses/dbcl/1.0/";
constexpr const char *kSourceName         = "Leela Chess Zero self-play training data";
constexpr const char *kSourceHomeUrl      = "https://storage.lczero.org/files/training_data/";

/// @brief The current UTC time as an ISO-8601 second, for the sidecar.
/// strftime rather than chrono's formatters, which the oldest standard library this repo builds against
/// does not carry for a system_clock time point - the same reason tools/gen_data.cpp uses it.
/// @return e.g. "2026-10-27T18:04:11Z", or an empty string if the clock could not be read.
std::string UtcTimestamp()
{
    const std::time_t now      = std::time(nullptr);
    const std::tm    *utc      = std::gmtime(&now);
    char              text[32] = {};

    if (utc == nullptr || std::strftime(text, sizeof(text), "%Y-%m-%dT%H:%M:%SZ", utc) == 0)
    {
        return {};
    }

    return text;
}

/// @brief Read a non-negative integer argument. @param text The token. @return Its value, or -1 if malformed.
int64_t ParseNonNegative(const char *text)
{
    char          *end   = nullptr;
    const uint64_t value = std::strtoull(text, &end, 10);

    if (end == text || *end != '\0' || value > (uint64_t)INT64_MAX)
    {
        return -1;
    }

    return (int64_t)value;
}

/// @brief Everything the sidecar says about where the data came from, as given on the command line.
struct SourceDescription
{
    std::string url;            ///< The exact tar URL, if it was given.
    std::string sha256;         ///< The SHA-256 of that tar, if it was given.
    std::string run = "test80"; ///< Which training run the tar belongs to.
};

/// @brief Writes the records out as `data_%04d.txt` shards, each with its provenance sidecar.
///
/// The dedup is **per shard**, which is gen_data's rule and the reason RecordSink exists as an interface at
/// all: only whoever owns the shards knows what is in the current one. tools/check_duplicates.sh is what
/// measures duplication across an assembled pool, and it needs the per-shard behaviour to be this way round
/// to be meaningful - a globally deduplicated pool would always measure as perfectly unique and the gate
/// would never fire.
///
/// A shard rotates in EndGame() and nowhere else, so a game's records never straddle two shards. That
/// matters to check_duplicates.sh, whose sampling reads the first N records of every shard and relies on a
/// duplicate being a contiguous run of records inside one of them.
class ShardWriter final : public lc0::RecordSink
{
  public:
    ShardWriter(std::string out_dir, const lc0::Options &options, const SourceDescription &source,
                const lc0::RecordReader &reader, const lc0::Statistics &statistics, bool does_deduplicate);
    ~ShardWriter() override;

    bool IsNewToShard(zobrist_t hash) override;
    void Write(const std::string &fen, int white_score, const char *wdl_text) override;
    void EndGame() override;

    /// @brief Close the open shard and write its sidecar. Call once, before reporting. @return true on success.
    bool Close();

    uint64_t shards_written() const
    {
        return shards_written_;
    }

    uint64_t records_written() const
    {
        return records_written_total_;
    }

    bool has_failed() const
    {
        return has_failed_;
    }

  private:
    bool        OpenShard();
    bool        CloseShard();
    std::string ShardPath(uint64_t index) const;

    std::string              out_dir_;
    const lc0::Options      &options_;
    const SourceDescription &source_;
    const lc0::RecordReader &reader_;
    const lc0::Statistics   &statistics_;
    bool                     does_deduplicate_ = true;

    std::ofstream                 out_;
    std::unordered_set<zobrist_t> seen_in_shard_;
    uint64_t                      shard_index_             = 0;
    uint64_t                      records_in_shard_        = 0;
    uint64_t                      games_in_shard_          = 0;
    uint64_t                      duplicates_in_shard_     = 0;
    uint64_t                      input_bytes_shard_start_ = 0;
    bool                          has_game_contributed_    = false;

    uint64_t    shards_written_        = 0;
    uint64_t    records_written_total_ = 0;
    bool        has_failed_            = false;
    bool        is_open_               = false;
    std::string last_shard_path_;
};

ShardWriter::ShardWriter(std::string out_dir, const lc0::Options &options, const SourceDescription &source,
                         const lc0::RecordReader &reader, const lc0::Statistics &statistics, bool does_deduplicate)
    : out_dir_{std::move(out_dir)}, options_{options}, source_{source}, reader_{reader}, statistics_{statistics},
      does_deduplicate_{does_deduplicate}
{
}

ShardWriter::~ShardWriter()
{
    // Close() is the supported path and reports failures; this only catches an early return.
    if (is_open_)
    {
        CloseShard();
    }
}

/// @brief Where shard @p index is written. @param index The shard number. @return The path.
std::string ShardWriter::ShardPath(uint64_t index) const
{
    char name[32] = {};
    std::snprintf(name, sizeof(name), "data_%04llu.txt", (unsigned long long)index);
    return out_dir_.empty() ? std::string{name} : out_dir_ + "/" + name;
}

/// @brief Open the next shard for writing. @return true on success.
bool ShardWriter::OpenShard()
{
    const std::string path = ShardPath(shard_index_);
    out_.open(path, std::ios::binary | std::ios::trunc);

    if (!out_)
    {
        std::cerr << "lc0_extract: cannot write '" << path << "'\n";
        has_failed_ = true;
        return false;
    }

    is_open_                 = true;
    records_in_shard_        = 0;
    games_in_shard_          = 0;
    duplicates_in_shard_     = 0;
    input_bytes_shard_start_ = reader_.attributable_bytes();
    seen_in_shard_.clear();
    last_shard_path_ = path;
    return true;
}

/// @brief Flush the open shard and write its `.meta` provenance sidecar. @return true on success.
bool ShardWriter::CloseShard()
{
    if (!is_open_)
    {
        return true;
    }

    const std::string path = ShardPath(shard_index_);
    out_.flush();
    const bool is_shard_good = out_.good();
    out_.close();
    is_open_ = false;

    if (!is_shard_good)
    {
        std::cerr << "lc0_extract: failed writing '" << path << "'\n";
        has_failed_ = true;
        return false;
    }

    std::ofstream meta{path + ".meta"};

    if (!meta)
    {
        std::cerr << "lc0_extract: cannot write '" << path << ".meta'\n";
        has_failed_ = true;
        return false;
    }

    const uint64_t input_bytes_end = reader_.attributable_bytes();

    meta << "# Pawnstar NNUE training-data shard provenance (tools/lc0_extract.cpp).\n"
         << "# Extracted from the Leela Chess Zero self-play training data, which is published under the\n"
         << "# Open Database License 1.0, its contents under the Database Contents License 1.0. A net\n"
         << "# trained on these positions is an ODbL Produced Work: distributing it obliges the notice on\n"
         << "# the attribution line below, and does not oblige the net itself to be ODbL.\n"
         << "shard " << path << '\n'
         << "extractor lc0_extract\n"
         << "engine_version " << VersionString() << '\n'
         << "source_name " << kSourceName << '\n'
         << "source_home " << kSourceHomeUrl << '\n'
         << "source_run " << source_.run << '\n'
         << "source_url " << (source_.url.empty() ? "unrecorded" : source_.url) << '\n'
         << "source_tar_sha256 " << (source_.sha256.empty() ? "unrecorded" : source_.sha256) << '\n'
         << "source_licence " << kSourceLicence << '\n'
         << "source_licence_url " << kSourceLicenceUrl << '\n'
         << "contents_licence " << kContentsLicence << '\n'
         << "contents_licence_url " << kContentsLicenceUrl << '\n'
         << "attribution Position data from the " << kSourceName << " (" << kSourceHomeUrl << "), run " << source_.run
         << ", used under the Open Database License 1.0 (" << kSourceLicenceUrl << ").\n"
         << "record_format lc0 v6 (" << lc0::kRecordBytes << " bytes, input format " << lc0::kInputFormatClassical
         << ")\n"
         << "input_sha256 " << reader_.Digest() << '\n'
         << "input_sha256_covers bytes 0.." << input_bytes_end << " (records handed out in a complete game)\n"
         << "input_bytes_shard_start " << input_bytes_shard_start_ << '\n'
         << "input_bytes_shard_end " << input_bytes_end << '\n'
         << "value_head " << (options_.value_head == lc0::ValueHead::kRootQ ? "root_q" : "best_q") << '\n'
         << "value_mapping score_cp = eval_scale * ln((1 + q_scale * q) / (1 - q_scale * q))\n"
         << "eval_scale " << options_.eval_scale << '\n'
         << "q_scale " << options_.q_scale << '\n'
         << "skip_plies " << options_.skip_plies << '\n'
         << "max_plies " << options_.max_plies << '\n'
         << "eval_cap " << options_.eval_cap << '\n'
         << "min_visits " << options_.min_visits << '\n'
         << "quiet_filter " << (options_.does_require_quiet ? "played_move" : "off") << '\n'
         << "skip_adjudicated " << (options_.does_skip_adjudicated ? "yes" : "no") << '\n'
         << "shard_positions_target " << options_.shard_positions << '\n'
         << "deduplicated " << (does_deduplicate_ ? "yes" : "no") << '\n'
         << "games_completed " << games_in_shard_ << '\n'
         << "games_non_standard_dropped " << statistics_.games_non_standard << '\n'
         << "records " << records_in_shard_ << '\n'
         << "dropped_duplicate " << duplicates_in_shard_ << '\n'
         << "boundary_disagreements " << reader_.boundary_disagreements() << '\n'
         << "generated_utc " << UtcTimestamp() << '\n';
    meta.flush();

    if (!meta.good())
    {
        std::cerr << "lc0_extract: failed writing '" << path << ".meta'\n";
        has_failed_ = true;
        return false;
    }

    ++shards_written_;
    return true;
}

/// @brief Whether this position is new to the current shard, claiming it if so.
/// @param hash The Zobrist key. @return true if it had not been written to this shard.
bool ShardWriter::IsNewToShard(zobrist_t hash)
{
    if (!does_deduplicate_)
    {
        return true;
    }

    const bool is_new = seen_in_shard_.insert(hash).second;

    if (!is_new)
    {
        ++duplicates_in_shard_;
    }

    return is_new;
}

/// @brief Write one record. @param fen The position. @param white_score Centipawns, White-relative.
/// @param wdl_text One of "1.0" / "0.5" / "0.0".
void ShardWriter::Write(const std::string &fen, int white_score, const char *wdl_text)
{
    if (has_failed_)
    {
        return;
    }

    if (!is_open_ && !OpenShard())
    {
        return;
    }

    out_ << fen << " | " << white_score << " | " << wdl_text << '\n';
    ++records_in_shard_;
    ++records_written_total_;
    has_game_contributed_ = true;
}

/// @brief Rotate the shard if it is full. Called after each game walked, so a game never straddles two.
void ShardWriter::EndGame()
{
    if (has_game_contributed_)
    {
        ++games_in_shard_;
        has_game_contributed_ = false;
    }

    if (has_failed_ || !is_open_)
    {
        return;
    }

    if (options_.shard_positions > 0 && records_in_shard_ >= options_.shard_positions)
    {
        if (!CloseShard())
        {
            return;
        }

        ++shard_index_;
    }
}

/// @brief Close the open shard and write its sidecar. @return true on success.
bool ShardWriter::Close()
{
    return CloseShard() && !has_failed_;
}

/// @brief Print the usage text. @param program argv[0].
void PrintUsage(const char *program)
{
    std::cerr << "usage: " << program << " [options] < v6-records-on-stdin\n"
              << "       see the comment at the top of tools/lc0_extract.cpp for every option\n"
              << "  --out-dir <dir>  --shard-positions <n>  --skip-plies <n>  --max-plies <n>\n"
              << "  --eval-cap <cp>  --eval-scale <cp>  --q-scale <f>  --min-visits <n>\n"
              << "  --value-head best|root  --skip-adjudicated  --no-quiet-filter\n"
              << "  --max-bytes <n>  --source-url <url>  --source-sha256 <hex>  --source-run <name>\n"
              << "  --no-input-digest  --no-dedup\n";
}
} // namespace

int main(int argc, char **argv)
{
    lc0::Options      options;
    SourceDescription source;
    std::string       out_dir           = ".";
    uint64_t          max_bytes         = 0;
    bool              does_deduplicate  = true;
    bool              does_digest_input = true;

    for (int i = 1; i < argc; ++i)
    {
        const std::string flag = argv[i];
        // Every flag below this point takes a value unless it is one of these; collect it once so each case
        // can assume it exists.
        const bool does_take_value = flag != "--no-dedup" && flag != "--no-quiet-filter" &&
                                     flag != "--skip-adjudicated" && flag != "--no-input-digest" && flag != "--help" &&
                                     flag != "-h";

        if (does_take_value && i + 1 >= argc)
        {
            std::cerr << "lc0_extract: " << flag << " needs a value\n";
            return 2;
        }

        const char *value = does_take_value ? argv[++i] : "";

        if (flag == "--help" || flag == "-h")
        {
            PrintUsage(argv[0]);
            return 0;
        }
        else if (flag == "--no-dedup")
        {
            does_deduplicate = false;
        }
        else if (flag == "--no-quiet-filter")
        {
            options.does_require_quiet = false;
        }
        else if (flag == "--skip-adjudicated")
        {
            options.does_skip_adjudicated = true;
        }
        else if (flag == "--no-input-digest")
        {
            does_digest_input = false;
        }
        else if (flag == "--out-dir")
        {
            out_dir = value;
        }
        else if (flag == "--source-url")
        {
            source.url = value;
        }
        else if (flag == "--source-sha256")
        {
            source.sha256 = value;
        }
        else if (flag == "--source-run")
        {
            source.run = value;
        }
        else if (flag == "--value-head")
        {
            if (std::strcmp(value, "best") == 0)
            {
                options.value_head = lc0::ValueHead::kBestQ;
            }
            else if (std::strcmp(value, "root") == 0)
            {
                options.value_head = lc0::ValueHead::kRootQ;
            }
            else
            {
                std::cerr << "lc0_extract: --value-head must be best or root\n";
                return 2;
            }
        }
        else if (flag == "--q-scale" || flag == "--eval-scale")
        {
            char        *end    = nullptr;
            const double number = std::strtod(value, &end);

            if (end == value || *end != '\0' || !(number > 0.0))
            {
                std::cerr << "lc0_extract: " << flag << " needs a positive number, got '" << value << "'\n";
                return 2;
            }

            if (flag == "--q-scale")
            {
                options.q_scale = number;
            }
            else
            {
                options.eval_scale = number;
            }
        }
        else
        {
            const int64_t number = ParseNonNegative(value);

            if (number < 0)
            {
                std::cerr << "lc0_extract: " << flag << " needs a non-negative integer, got '" << value << "'\n";
                return 2;
            }

            if (flag == "--skip-plies")
            {
                options.skip_plies = (int)number;
            }
            else if (flag == "--max-plies")
            {
                options.max_plies = (int)number;
            }
            else if (flag == "--eval-cap")
            {
                options.eval_cap = (int)number;
            }
            else if (flag == "--min-visits")
            {
                options.min_visits = (uint32_t)number;
            }
            else if (flag == "--shard-positions")
            {
                options.shard_positions = (std::size_t)number;
            }
            else if (flag == "--max-bytes")
            {
                max_bytes = (uint64_t)number;
            }
            else
            {
                std::cerr << "lc0_extract: unknown option '" << flag << "'\n";
                PrintUsage(argv[0]);
                return 2;
            }
        }
    }

#ifdef _WIN32
    // A v6 record stream is binary, and Windows opens stdin as a text stream: it would translate every
    // 0x0d 0x0a pair to one byte and treat the first 0x1a as end of input. Both bytes occur by the
    // thousand in a plane block, so without this the reader stops a few records in - which is exactly what
    // it did, reporting a truncated stream after 58,830 bytes of a 1.4 GB one.
    _setmode(_fileno(stdin), _O_BINARY);
#endif

    std::ios::sync_with_stdio(false);

    lc0::Statistics   statistics;
    lc0::RecordReader reader{std::cin, does_digest_input};
    ShardWriter       writer{out_dir, options, source, reader, statistics, does_deduplicate};
    lc0::GameRecords  game;

    while (reader.NextGame(game))
    {
        statistics.records_seen += game.size() / lc0::kRecordBytes;
        lc0::ExtractGame(game, options, writer, statistics);

        if (writer.has_failed())
        {
            return 1;
        }

        // Bounded on a game boundary: the point of the bound is a reproducible prefix, and cutting inside a
        // game would leave the digest covering bytes no record came from.
        if (max_bytes != 0 && reader.attributable_bytes() >= max_bytes)
        {
            break;
        }
    }

    if (!writer.Close())
    {
        return 1;
    }

    if (reader.has_failed())
    {
        std::cerr << "lc0_extract: the input was not a whole number of " << lc0::kRecordBytes
                  << "-byte records - it ended mid-record after " << reader.bytes_consumed()
                  << " bytes. A truncated tar, or an HTTP error body fed to the decompressor.\n";
        return 1;
    }

    // A run that read records but wrote nothing is the shape a wrong input format or a stray download takes,
    // and it must not look like success.
    if (statistics.records_seen == 0)
    {
        std::cerr << "lc0_extract: the input held no v6 records at all. Check the download: an HTTP error "
                     "body piped into a decompressor produces exactly this.\n";
        return 1;
    }

    std::cerr << "lc0_extract: wrote " << writer.records_written() << " records in " << writer.shards_written()
              << " shard(s) to " << out_dir << '\n'
              << "lc0_extract: read " << reader.bytes_consumed() << " bytes (" << statistics.records_seen
              << " records, " << statistics.games_seen << " games, " << reader.attributable_bytes()
              << " bytes attributable, sha256 " << reader.Digest() << ")\n"
              << "lc0_extract: " << statistics.games_non_standard << " game(s) dropped as non-standard (Chess960), "
              << statistics.games_used << " walked, " << statistics.games_contributing << " contributed\n";

    // Every drop reason, because the reasons are what size the next run.
    std::cerr << "lc0_extract: plies " << statistics.plies_seen << "; dropped skip_plies "
              << statistics.dropped_skip_plies << ", max_plies " << statistics.dropped_max_plies << ", min_visits "
              << statistics.dropped_min_visits << ", adjudicated " << statistics.dropped_adjudicated << ", castling "
              << statistics.dropped_castling << ", unreadable " << statistics.dropped_unreadable << ", in_check "
              << statistics.dropped_in_check << ", not_quiet " << statistics.dropped_not_quiet << ", last_of_game "
              << statistics.dropped_last_of_game << ", eval_cap " << statistics.dropped_eval_cap << ", duplicate "
              << statistics.dropped_duplicate << '\n';

    if (statistics.records_bad_version > 0 || statistics.records_bad_format > 0)
    {
        std::cerr << "lc0_extract: " << statistics.records_bad_version << " record(s) were not version "
                  << lc0::kTrainingDataVersion << " and " << statistics.records_bad_format << " were not input format "
                  << lc0::kInputFormatClassical << "; those games were skipped\n";
    }

    if (!does_digest_input && source.sha256.empty())
    {
        std::cerr << "lc0_extract: WARNING - --no-input-digest was given and --source-sha256 was not, so "
                     "these shards record no provenance of their input at all.\n";
    }

    if (reader.boundary_disagreements() > 0)
    {
        std::cerr << "lc0_extract: WARNING - the two game-boundary signals disagreed about "
                  << reader.boundary_disagreements()
                  << " record(s). Every ply number, and so --skip-plies, "
                     "is suspect for the games involved.\n";
    }

    return 0;
}

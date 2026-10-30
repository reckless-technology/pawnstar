/// @file lichess_extract.cpp Turn the Lichess open database's monthly PGN into NNUE training shards.
///
/// The extraction rules all live in tools/lichess_extract.h, which test/lichess_extract_test.cpp drives
/// directly; this file is the command line, the shard writer and the provenance sidecar around them.
///
/// ## What this is for
///
/// The nets up to and including v12 were trained on the PlentyChess bulletformat dataset, by Yoshie2000,
/// which is no longer used (nnue/README.md §7, "Provenance of the earlier networks"). The shipped net is
/// trained entirely on Leela Chess Zero self-play data (tools/lc0_extract), and the Lichess open database
/// is the third source this one adds: the monthly standard-chess exports
/// are published **CC0 1.0** (public domain dedication), about 10% of their games carry inline Stockfish
/// `[%eval]` comments, and every game carries its result - which is exactly the pair tools/bullet/pawnstar.rs
/// wants, since it labels with `ConstantWDL { value: 0.5 }`, half game result and half eval sigmoid.
///
/// ## Usage
///
/// The monthly files are tens of gigabytes compressed, so the input is a stream and never a file on disk:
///
/// ```sh
/// curl -s https://database.lichess.org/standard/lichess_db_standard_rated_2026-09.pgn.zst \
///   | zstd -dc \
///   | lichess_extract --out-dir pool/ --month 2026-09 \
///       --source-url https://database.lichess.org/standard/lichess_db_standard_rated_2026-09.pgn.zst
/// ```
///
/// Options, all optional but `--out-dir`:
///
///   --out-dir <dir>         where the shards go (default `.`). Written as `data_0000.txt`, `data_0001.txt`
///                           ..., which is the name tools/check_duplicates.sh and tools/train_pipeline.sh's
///                           text-directory mode already glob for.
///   --shard-positions <n>   records per shard (default 2000000). A shard only ever rotates on a game
///                           boundary, so the figure is a target and not a hard size.
///   --min-elo <n>           both players must be rated at least this (default 0). A `?` rating counts 0.
///   --skip-plies <n>        drop this many plies from the start of each game (default 8).
///   --max-plies <n>         keep nothing past this ply (default 200, 0 = no cap).
///   --eval-cap <cp>         drop a position whose |eval| is at or above this (default 3000).
///   --resign-cp <cp>        adjudication threshold (default 1500, 0 = no adjudication).
///   --resign-plies <n>      consecutive decisive plies before the harvest of a game stops (default 4).
///   --mate-score-cp <cp>    0 (the default) skips a `#N` mate eval; otherwise clamp it to ±this.
///   --quiet-mode <which>    `next` (default), `played` or `both` - see QuietMode in the header, and read
///                           its comment before changing the default.
///   --max-bytes <n>         stop after this many bytes of PGN (0 = to end of input). Checked on a game
///                           boundary, so a little more than this is consumed.
///   --month <YYYY-MM>       which monthly file this is, for the sidecar.
///   --source-url <url>      where it came from, for the sidecar.
///   --no-dedup              keep repeated positions. By default a position whose Zobrist key has already
///                           been written to the current shard is dropped, which is gen_data's rule too.
///
/// ## The provenance sidecar, and why it is exact
///
/// Each shard gets a `<shard>.meta` beside it, in the same `key value` shape tools/gen_data.cpp writes (so
/// tools/check_duplicates.sh reads the `games_completed` line out of either without caring which tool wrote
/// it). It carries the licence, the source URL and month, the filter settings in full, the counts, and the
/// SHA-256 of the exact input prefix the shard's records came from.
///
/// That digest is the part worth being careful about. The input is a pipe, so it can never be re-read to
/// check it afterwards, and a monthly file is far too large to keep: the digest taken as the bytes went past
/// is the *only* evidence of what was read. So it covers exactly the prefix ending at the last complete game
/// (PgnReader::Digest), not the bytes that happened to sit in the reader's 64 KB readahead, and each shard
/// records the byte range it covers as well. A digest over a slightly different prefix than the records came
/// from could not be checked against anything, which would make the whole sidecar decorative.
///
/// The licence is recorded because we owe the Lichess database an attribution line when a net trained on it
/// ships, and the sidecar is meant to be the thing that line is written from without going back to the web:
/// `source_licence`, `source_licence_url` and a ready-made `attribution` line are all there.
///
/// One expected piece of noise: tools/check_duplicates.sh audits the *campaign identity* of the sidecars it
/// finds (which worker of which partitioned self-play campaign wrote each shard) and warns that a shard
/// recording none "cannot be audited". A shard from here records none, and should not: there is no opening
/// pool to partition and no worker index, because a run reads one monthly file front to back. The partition
/// of a Lichess campaign is the month, which `source_month` names. The warning is accurate and the gate
/// still passes; it is not a sign of a malformed sidecar.

#include "lichess_extract.h"

#include "version.h"

#include <cstdio>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_set>
#include <vector>

namespace
{
/// @brief The Lichess open database's licence. The monthly exports are a CC0 public-domain dedication,
/// which is what makes them usable here at all - so it is recorded in every shard rather than assumed.
constexpr const char *kSourceLicence    = "CC0-1.0";
constexpr const char *kSourceLicenceUrl = "https://creativecommons.org/publicdomain/zero/1.0/";
constexpr const char *kSourceName       = "Lichess open database (standard rated games)";
constexpr const char *kSourceHomeUrl    = "https://database.lichess.org/";

/// @brief The current UTC time as an ISO-8601 second, for the sidecar.
/// strftime rather than chrono's formatters, which the oldest standard library this repo builds against
/// does not carry for a system_clock time point - the same reason tools/gen_data.cpp uses it.
/// @return e.g. "2026-10-26T18:04:11Z", or an empty string if the clock could not be read.
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

/// @brief Spell a QuietMode for the sidecar. @param mode The mode. @return Its command-line spelling.
const char *DescribeQuietMode(lichess::QuietMode mode)
{
    switch (mode)
    {
    case lichess::QuietMode::kNextMove:
        return "next";
    case lichess::QuietMode::kBoth:
        return "both";
    case lichess::QuietMode::kPlayedMove:
    default:
        return "played";
    }
}

/// @brief Everything the sidecar says about where the data came from, as given on the command line.
struct SourceDescription
{
    std::string url;   ///< The exact URL streamed, if it was given.
    std::string month; ///< The `YYYY-MM` of the monthly file, if it was given.
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
class ShardWriter final : public lichess::RecordSink
{
  public:
    ShardWriter(std::string out_dir, const lichess::Options &options, const SourceDescription &source,
                const lichess::PgnReader &reader, bool does_deduplicate);
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

    std::string last_shard_path() const
    {
        return last_shard_path_;
    }

  private:
    bool        OpenShard();
    bool        CloseShard();
    std::string ShardPath(uint64_t index) const;

    std::string               out_dir_;
    const lichess::Options   &options_;
    const SourceDescription  &source_;
    const lichess::PgnReader &reader_;
    bool                      does_deduplicate_ = true;

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
    uint64_t    duplicates_total_      = 0;
    bool        has_failed_            = false;
    bool        is_open_               = false;
    std::string last_shard_path_;
};

ShardWriter::ShardWriter(std::string out_dir, const lichess::Options &options, const SourceDescription &source,
                         const lichess::PgnReader &reader, bool does_deduplicate)
    : out_dir_{std::move(out_dir)}, options_{options}, source_{source}, reader_{reader},
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
        std::cerr << "lichess_extract: cannot write '" << path << "'\n";
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
        std::cerr << "lichess_extract: failed writing '" << path << "'\n";
        has_failed_ = true;
        return false;
    }

    std::ofstream meta{path + ".meta"};

    if (!meta)
    {
        std::cerr << "lichess_extract: cannot write '" << path << ".meta'\n";
        has_failed_ = true;
        return false;
    }

    // The byte range and digest are of the input prefix ending at the last complete game, which is exactly
    // what the records in this shard were extracted from. See the file comment.
    const uint64_t input_bytes_end = reader_.attributable_bytes();

    meta << "# Pawnstar NNUE training-data shard provenance (tools/lichess_extract.cpp).\n"
         << "# Extracted from the Lichess open database, which is published CC0 1.0 (public domain).\n"
         << "shard " << path << '\n'
         << "extractor lichess_extract\n"
         << "engine_version " << VersionString() << '\n'
         << "source_name " << kSourceName << '\n'
         << "source_home " << kSourceHomeUrl << '\n'
         << "source_url " << (source_.url.empty() ? "unrecorded" : source_.url) << '\n'
         << "source_month " << (source_.month.empty() ? "unrecorded" : source_.month) << '\n'
         << "source_licence " << kSourceLicence << '\n'
         << "source_licence_url " << kSourceLicenceUrl << '\n'
         << "attribution Game data from the " << kSourceName << " (" << kSourceHomeUrl << "), "
         << (source_.month.empty() ? std::string{"monthly export"} : source_.month) << ", used under " << kSourceLicence
         << ".\n"
         << "input_sha256 " << reader_.Digest() << '\n'
         << "input_sha256_covers bytes 0.." << input_bytes_end << " (to the end of the last complete game)\n"
         << "input_bytes_shard_start " << input_bytes_shard_start_ << '\n'
         << "input_bytes_shard_end " << input_bytes_end << '\n'
         << "min_elo " << options_.min_elo << '\n'
         << "skip_plies " << options_.skip_plies << '\n'
         << "max_plies " << options_.max_plies << '\n'
         << "eval_cap " << options_.eval_cap << '\n'
         << "resign_centipawns " << options_.resign_cp << '\n'
         << "resign_plies " << options_.resign_plies << '\n'
         << "mate_score_centipawns " << options_.mate_score_cp << '\n'
         << "quiet_mode " << DescribeQuietMode(options_.quiet_mode) << '\n'
         << "shard_positions_target " << options_.shard_positions << '\n'
         << "deduplicated " << (does_deduplicate_ ? "yes" : "no") << '\n'
         << "games_completed " << games_in_shard_ << '\n'
         << "records " << records_in_shard_ << '\n'
         << "dropped_duplicate " << duplicates_in_shard_ << '\n'
         << "generated_utc " << UtcTimestamp() << '\n';
    meta.flush();

    if (!meta.good())
    {
        std::cerr << "lichess_extract: failed writing '" << path << ".meta'\n";
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
        ++duplicates_total_;
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
    std::cerr << "usage: " << program << " [options] < pgn-on-stdin\n"
              << "       see the comment at the top of tools/lichess_extract.cpp for every option\n"
              << "  --out-dir <dir>  --shard-positions <n>  --min-elo <n>  --skip-plies <n>\n"
              << "  --max-plies <n>  --eval-cap <cp>  --resign-cp <cp>  --resign-plies <n>\n"
              << "  --mate-score-cp <cp>  --quiet-mode next|played|both  --max-bytes <n>\n"
              << "  --month <YYYY-MM>  --source-url <url>  --no-dedup\n";
}
} // namespace

int main(int argc, char **argv)
{
    lichess::Options  options;
    SourceDescription source;
    std::string       out_dir          = ".";
    uint64_t          max_bytes        = 0;
    bool              does_deduplicate = true;

    for (int i = 1; i < argc; ++i)
    {
        const std::string flag = argv[i];
        // Every flag below this point takes a value; collect it once so each case can assume it exists.
        const bool does_take_value = flag != "--no-dedup" && flag != "--help" && flag != "-h";

        if (does_take_value && i + 1 >= argc)
        {
            std::cerr << "lichess_extract: " << flag << " needs a value\n";
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
        else if (flag == "--out-dir")
        {
            out_dir = value;
        }
        else if (flag == "--month")
        {
            source.month = value;
        }
        else if (flag == "--source-url")
        {
            source.url = value;
        }
        else if (flag == "--quiet-mode")
        {
            if (std::strcmp(value, "next") == 0)
            {
                options.quiet_mode = lichess::QuietMode::kNextMove;
            }
            else if (std::strcmp(value, "played") == 0)
            {
                options.quiet_mode = lichess::QuietMode::kPlayedMove;
            }
            else if (std::strcmp(value, "both") == 0)
            {
                options.quiet_mode = lichess::QuietMode::kBoth;
            }
            else
            {
                std::cerr << "lichess_extract: --quiet-mode must be next, played or both\n";
                return 2;
            }
        }
        else
        {
            const int64_t number = ParseNonNegative(value);

            if (number < 0)
            {
                std::cerr << "lichess_extract: " << flag << " needs a non-negative integer, got '" << value << "'\n";
                return 2;
            }

            if (flag == "--min-elo")
            {
                options.min_elo = (int)number;
            }
            else if (flag == "--skip-plies")
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
            else if (flag == "--resign-cp")
            {
                options.resign_cp = (int)number;
            }
            else if (flag == "--resign-plies")
            {
                options.resign_plies = (int)number;
            }
            else if (flag == "--mate-score-cp")
            {
                options.mate_score_cp = (int)number;
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
                std::cerr << "lichess_extract: unknown option '" << flag << "'\n";
                PrintUsage(argv[0]);
                return 2;
            }
        }
    }

    // A mate eval clamped at or above the cap would be dropped again by the cap, which would look like the
    // clamp silently not working. Refuse the combination rather than let it mislead a run.
    if (options.mate_score_cp > 0 && options.mate_score_cp >= options.eval_cap)
    {
        std::cerr << "lichess_extract: --mate-score-cp " << options.mate_score_cp << " is at or above --eval-cap "
                  << options.eval_cap << ", so every mate position would be dropped by the cap\n";
        return 2;
    }

    std::ios::sync_with_stdio(false);

    lichess::Statistics statistics;
    lichess::PgnReader  reader{std::cin};
    ShardWriter         writer{out_dir, options, source, reader, does_deduplicate};
    lichess::PgnGame    game;

    while (reader.NextGame(game))
    {
        lichess::ExtractGame(game, options, writer, statistics);

        if (writer.has_failed())
        {
            return 1;
        }

        // Bounded on a game boundary: the point of the bound is a reproducible prefix, and cutting inside a
        // game would leave the digest covering bytes no record came from.
        if (max_bytes != 0 && reader.bytes_consumed() >= max_bytes)
        {
            break;
        }
    }

    if (!writer.Close())
    {
        return 1;
    }

    const uint64_t games_considered =
        statistics.games_seen - statistics.games_truncated - statistics.games_non_standard;

    std::cerr << "lichess_extract: wrote " << writer.records_written() << " records in " << writer.shards_written()
              << " shard(s) to " << out_dir << '\n'
              << "lichess_extract: read " << reader.bytes_consumed() << " bytes of PGN (" << statistics.games_seen
              << " games, " << reader.attributable_bytes() << " bytes attributable, sha256 " << reader.Digest() << ")\n"
              << "lichess_extract: " << statistics.games_with_evals << " of " << games_considered
              << " complete standard games carried evals, " << statistics.games_passing_elo << " passed --min-elo "
              << options.min_elo << ", " << statistics.games_used << " were walked, " << statistics.games_contributing
              << " contributed\n";

    // Every drop reason, because the reasons are what size the next run.
    std::cerr << "lichess_extract: plies " << statistics.plies_seen << " (" << statistics.plies_with_eval
              << " with evals); dropped skip_plies " << statistics.dropped_skip_plies << ", max_plies "
              << statistics.dropped_max_plies << ", adjudicated " << statistics.dropped_after_adjudication
              << ", no_eval " << statistics.dropped_no_eval << ", mate_eval " << statistics.dropped_mate_eval
              << ", in_check " << statistics.dropped_in_check << ", not_quiet " << statistics.dropped_not_quiet
              << ", eval_cap " << statistics.dropped_eval_cap << ", duplicate " << statistics.dropped_duplicate << '\n';

    if (statistics.games_with_unparsed_san > 0)
    {
        std::cerr << "lichess_extract: " << statistics.games_with_unparsed_san << " game(s) abandoned at an "
                  << "unresolved SAN token (" << statistics.plies_unparsed_san << " plies)\n";
    }

    return 0;
}

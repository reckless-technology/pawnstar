/// @file make_book.cpp Build pawnstar.book, the shipped opening book, from the Lichess open database.
///
/// ## Why this exists
///
/// The book this replaced was derived from TSCP's `book.txt`, and was asserted throughout this repository to
/// be in the public domain. **It was not.** TSCP is Copyright 1997 Tom Kerrigan; the only permission anyone
/// has ever located for it is an informal 2002 email allowing use "as long as you're not making any money
/// off of it", and a non-commercial permission is incompatible with the GPLv3 this engine is distributed
/// under. The repository the documents linked to carries no licence file at all. So the material had to go,
/// and no rewording of the claim could have fixed it: what was needed was a book whose provenance claim is
/// true.
///
/// The Lichess open database is that source. Its monthly standard-chess exports are released under the
/// **Creative Commons CC0 1.0 Universal public-domain dedication**, which is an actual dedication by the
/// rights holder rather than a belief about one - so "this book carries no third party's restriction" is a
/// statement that can be checked. See THIRD-PARTY-NOTICES.md.
///
/// ## What it reads, and what it reuses
///
/// Lichess PGN, on standard input, exactly as tools/lichess_extract does:
///
/// ```sh
/// curl -s https://database.lichess.org/standard/lichess_db_standard_rated_2015-01.pgn.zst \
///   | zstd -dc \
///   | make_book --out pawnstar.book --month 2015-01 \
///       --source-url https://database.lichess.org/standard/lichess_db_standard_rated_2015-01.pgn.zst
/// ```
///
/// **There is one PGN reader in this repository and this is not a second one.** The SAN resolver, the line
/// reader, the game splitter, the Elo tag reader and the byte-exact digest all come from
/// tools/lichess_extract.h, which test/lichess_extract_test.cpp drives directly. Only the part that is
/// genuinely this tool's own is here: the move tree, its frequency pruning, and the book writer.
///
/// ## How a book is built from games
///
/// Every accepted game's first `--plies` plies are walked into a prefix tree, each node counting the games
/// that reached it. A move is then **kept only if at least `--min-count` games played it from that
/// position**, which is the whole quality filter: a move that many strong players chose is a move worth
/// having in a book, and one that a single player tried once is not. Everything else about the source -
/// which openings are fashionable, how wide the repertoire is - is left to the data.
///
/// Each root-to-leaf path of the *pruned* tree is written as one book line. So no line is a prefix of
/// another, and the file holds each distinct line exactly once.
///
/// **That is also where the frequency weighting comes from, and it is worth being explicit about it because
/// it is not the obvious thing.** `OpeningBook` stores a position's moves *with repeats* and picks among
/// them uniformly, so a move's weight is the number of book lines that pass through it. Writing one line per
/// pruned leaf therefore weights a move by **how many distinct continuations survive behind it**, not by how
/// many games played it. The two correlate strongly - a popular move is popular because it has playable
/// continuations - but they are not the same number, and a reader expecting game counts would be wrong.
/// Writing each line once per game that played it would reproduce game counts exactly and would also make
/// the file tens of megabytes, which the engine embeds in its own binary (src/embedded_book.S), so it is not
/// a trade worth making for a weighting the engine uses only to break ties between sound opening moves.
///
/// ## The filters
///
///   --min-elo <n>           both players must be rated at least this (default 2000). A `?` rating counts 0.
///   --min-base-seconds <n>  the game's TimeControl base must be at least this (default 180, i.e. 3+0 and
///                           slower). This is what excludes bullet, where the opening is played on reflex
///                           and the move choice says more about the clock than about the position. An
///                           unlimited or unparseable TimeControl passes.
///   --plies <n>             how many plies of each game to walk (default 16, i.e. eight moves a side).
///   --min-game-plies <n>    a game shorter than this is skipped (default: the same as --plies, so every
///                           contributing game actually reached the end of the window). A game that ended
///                           inside the window ended in a blunder, and its moves are not book moves.
///   --min-count <n>         a move is kept only if this many accepted games played it from that position
///                           (default 12).
///   --min-share-percent <n> a move must also be this per cent of the games that reached the position
///                           (default 1). This is what keeps `h2h4` out of the first few plies, where tens
///                           of thousands of games stand behind a position and no absolute count filters
///                           anything; see CollectLines, which measures it.
///   --max-bytes <n>         stop after this many bytes of PGN (0 = to end of input), checked on a game
///                           boundary.
///   --out <path>            where to write (default `pawnstar.book`).
///   --month <YYYY-MM>       which monthly file this is, for the provenance header.
///   --source-url <url>      where it came from, for the provenance header.
///
/// ## The provenance header
///
/// The book file itself carries it, as `#` comments - which `OpeningBook::ParseLineOfPlay` already treats as
/// comment to end of line, so no parser change was needed. It names the source, the CC0 dedication and its
/// URL, the month, every filter setting, the counts, and the **SHA-256 of exactly the input bytes the lines
/// were built from** (`PgnReader::Digest`, which hashes bytes as they are handed out rather than as they are
/// read, so the digest covers the prefix the book came from and nothing else). A monthly file is far too
/// large to keep and is read from a pipe that can never be re-read, so that digest is the only evidence of
/// what was consumed. The whole point of a provenance claim is that someone can check it.
///
/// ## What this does not write: the '?' marks
///
/// `OpeningBook` understands a trailing `?` on a move - *questionable*: replayed so the rest of the line
/// still parses, never chosen - and the book screener is what puts them there, by searching every book move
/// and flagging the ones that lose more than a margin against the engine's best. This tool writes none, and
/// its own output has none: the frequency threshold is already a strong filter, a move that a dozen 2200+
/// players chose from the same position needing no second opinion. **The shipped book has since been
/// screened anyway** (2026-10-31, 100 cp against the engine's own choice at depth 12 with pawnstar-v20.bin),
/// so `pawnstar.book` does carry `?` marks even though nothing here wrote them - regenerating with this tool
/// means screening it afterwards, and `test_opening_book` is what catches forgetting that second step:
///
/// ```sh
/// amphitheater-cli screenbook -engine cmd=build/pawnstar -book pawnstar.book -depth 12 -margin 100
/// ```
///
/// The screener is `screenbook` in the public `amphitheater-chess` repository, which drives any engine over
/// UCI; its header comment (`src/match/BookScreener.h`) records why a weak move is marked rather than
/// deleted, and what makes one move's score comparable with another's. The predecessor that lived here,
/// `tools/filter_book.cpp`, was compiled against this engine's own evaluation - so it needed one copy per
/// engine, and it had stopped compiling unnoticed when the eight-bucket net landed; it was retired on
/// 2026-10-31. Screening preserves lines, comments and move numbers verbatim and only ever adds a `?` or,
/// where a position's every move was marked, one line ending in the engine's own unmarked choice - and
/// `tools/make_openings.sh` strips the mark either way.
///
/// Build:  make tools   ->   build/make_book

#include "lichess_extract.h"

#include "version.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace
{
/// @brief The Lichess open database's licence, as tools/lichess_extract.cpp records it in every shard.
constexpr const char *kSourceLicence     = "CC0-1.0";
constexpr const char *kSourceLicenceUrl  = "https://creativecommons.org/publicdomain/zero/1.0/";
constexpr const char *kSourceName        = "Lichess open database (standard rated games)";
constexpr const char *kSourceHomeUrl     = "https://database.lichess.org/";
constexpr const char *kSourceLicenceName = "Creative Commons CC0 1.0 Universal public-domain dedication";

/// @brief What the run was asked for.
struct Options
{
    int         min_elo           = 2000;            ///< Both players must be rated at least this.
    int         min_base_seconds  = 180;             ///< The TimeControl base must be at least this.
    int         plies             = 16;              ///< Plies of each game walked into the tree.
    int         min_game_plies    = -1;              ///< Shorter games are skipped (-1 = same as plies).
    uint64_t    min_count         = 12;              ///< Games that must have played a move for it to be kept.
    uint64_t    min_share_percent = 1;               ///< Per cent of a position's games a move must also be.
    uint64_t    max_bytes         = 0;               ///< Stop after this many bytes (0 = all of them).
    std::string out_path          = "pawnstar.book"; ///< Where the book goes.
    std::string source_month      = {};              ///< Which monthly file, for the header.
    std::string source_url        = {};              ///< Where it came from, for the header.
};

/// @brief What the run counted. Every rejection has a reason, because the reasons are what size a run: a
/// yield that is mostly `games_below_elo` means the threshold is too high for this month, and one that is
/// mostly `games_too_short` means the ply window is deeper than the source's games reach.
struct Statistics
{
    uint64_t games_seen         = 0; ///< Games read from the stream.
    uint64_t games_truncated    = 0; ///< Games whose movetext ran off the end of a bounded stream.
    uint64_t games_non_standard = 0; ///< Games with a FEN / non-standard Variant tag.
    uint64_t games_unfinished   = 0; ///< Games whose Result is `*`.
    uint64_t games_below_elo    = 0; ///< Games where either player is under --min-elo.
    uint64_t games_too_fast     = 0; ///< Games whose TimeControl base is under --min-base-seconds.
    uint64_t games_too_short    = 0; ///< Games with fewer than --min-game-plies plies.
    uint64_t games_used         = 0; ///< Games walked into the tree.
    uint64_t games_unparsed_san = 0; ///< Games abandoned part-way at a SAN token that did not resolve.
    uint64_t plies_walked       = 0; ///< Plies replayed into the tree.
};

/// @brief One node of the prefix tree: the games that reached it, and the moves played from it.
///
/// The children are a flat vector searched linearly rather than a map. A chess position has at most a few
/// dozen legal moves and an opening position far fewer distinct *played* ones, so the scan is shorter than a
/// hash would be - and it keeps the children in first-seen order, which the writer then sorts explicitly so
/// the output does not depend on the order games arrived in.
struct Node
{
    uint64_t                                         games    = 0;  ///< Accepted games that reached here.
    std::vector<std::pair<std::string, std::size_t>> children = {}; ///< Coordinate move -> node index.
};

/// @brief The current UTC time as an ISO-8601 second, for the provenance header.
/// strftime rather than chrono's formatters, which the oldest standard library this repo builds against does
/// not carry for a system_clock time point - the same reason tools/lichess_extract.cpp uses it.
/// @return e.g. "2026-10-30T18:04:11Z", or an empty string if the clock could not be read.
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

/// @brief The base time of a Lichess TimeControl tag, in seconds.
///
/// Lichess writes `<base>+<increment>`, or `-` for a game with no clock. Anything that is not a number is
/// treated as unlimited and passes every threshold: refusing a game because its clock could not be parsed
/// would silently narrow the source for a reason that has nothing to do with the chess.
///
/// @param text The tag's value. @return The base in seconds, or -1 for unlimited / unparseable.
int ParseTimeControlBase(std::string_view text)
{
    if (text.empty() || text == "-")
    {
        return -1;
    }

    std::string value{text};
    char       *end  = nullptr;
    const long  base = std::strtol(value.c_str(), &end, 10);

    if (end == value.c_str() || base < 0 || base > 1000000)
    {
        return -1;
    }

    return (int)base;
}

/// @brief Whether a game is one this book may learn from, counting the reason if it is not.
/// @param game The game. @param options The filters. @param statistics Counted into. @return true to walk it.
bool PassesFilters(const lichess::PgnGame &game, const Options &options, Statistics &statistics)
{
    if (!game.is_complete)
    {
        ++statistics.games_truncated;
        return false;
    }

    const std::string_view variant = game.Tag("Variant");

    if (game.HasTag("FEN") || (!variant.empty() && variant != "Standard"))
    {
        ++statistics.games_non_standard;
        return false;
    }

    // An unfinished game is dropped not because the book needs a result - it does not, the moves are the
    // whole of what is wanted - but because `*` on Lichess means abandoned, and an abandoned game's moves
    // are as likely to be a disconnection as a decision.
    if (lichess::ResultToWdlToken(game.Tag("Result")) == nullptr)
    {
        ++statistics.games_unfinished;
        return false;
    }

    if (lichess::ParseElo(game.Tag("WhiteElo")) < options.min_elo ||
        lichess::ParseElo(game.Tag("BlackElo")) < options.min_elo)
    {
        ++statistics.games_below_elo;
        return false;
    }

    const int base_seconds = ParseTimeControlBase(game.Tag("TimeControl"));

    if (base_seconds >= 0 && base_seconds < options.min_base_seconds)
    {
        ++statistics.games_too_fast;
        return false;
    }

    const int min_game_plies = options.min_game_plies >= 0 ? options.min_game_plies : options.plies;

    if ((int)game.plies.size() < min_game_plies)
    {
        ++statistics.games_too_short;
        return false;
    }

    return true;
}

/// @brief Walk one game's opening into the tree.
/// @param game The game. @param options The settings. @param tree The arena, node 0 being the root.
/// @param statistics Counted into.
void WalkGame(const lichess::PgnGame &game, const Options &options, std::vector<Node> &tree, Statistics &statistics)
{
    Position    position{Position::FromString(std::string(lichess::kStartPositionFen))};
    std::size_t node   = 0;
    const int   window = std::min<int>(options.plies, (int)game.plies.size());

    ++tree[node].games;

    for (int ply = 0; ply < window; ++ply)
    {
        const Move move = lichess::ParseSanMove(position, game.plies[(std::size_t)ply].san);

        if (move == Move::None())
        {
            // The game is abandoned from here: every position after an unresolved move would be wrong, and
            // what was already counted stands - those nodes were reached by moves that did resolve.
            ++statistics.games_unparsed_san;
            return;
        }

        const std::string coordinate = move.ToString();
        std::size_t       child      = 0;

        for (const auto &candidate : tree[node].children)
        {
            if (candidate.first == coordinate)
            {
                child = candidate.second;
                break;
            }
        }

        if (child == 0)
        {
            tree.push_back(Node{});
            child = tree.size() - 1;
            tree[node].children.emplace_back(coordinate, child);
        }

        ++tree[child].games;
        ++statistics.plies_walked;
        position = position.MakeMove(move);
        node     = child;
    }
}

/// @brief Collect every root-to-leaf path of the pruned tree, deepest-first in move order.
///
/// A node's child survives two thresholds, and it needs both.
///
///  * **An absolute count**, `min_count` games having played the move. This is the quality filter deep in
///    the tree, where every count is small.
///  * **A share of its parent**, `min_share_percent` per cent of the games that reached the position. This
///    one only ever bites in the first few plies, where a position has tens of thousands of games behind it
///    and the absolute threshold is therefore no filter at all. **Without it the book plays `h2h4` and
///    `g2g4`:** measured on the 2017-01 file at `--min-elo 2200` and `--min-count 8`, eight of the start
///    position's eighteen surviving first moves were fringe choices accounting for 0.56% of the book's
///    lines - 73,128 games is enough that even a bad move clears any sane absolute bar. A move has to be
///    both common enough to be real and not a fringe choice at its own position.
///
/// A node with no surviving child is a leaf, and its path is one book line. A short leaf is kept rather than
/// dropped: it means the move reaching it was itself played often enough but nothing after it was, and
/// dropping the path would remove that move from the book altogether rather than merely shortening the line.
///
/// @param tree The arena. @param node The node to walk. @param min_count The absolute threshold.
/// @param min_share_percent The share-of-parent threshold, in per cent.
/// @param path The moves above this node, appended to in place. @param lines Collected into.
void CollectLines(const std::vector<Node> &tree, std::size_t node, uint64_t min_count, uint64_t min_share_percent,
                  std::vector<std::string> &path, std::vector<std::string> &lines)
{
    std::vector<std::pair<std::string, std::size_t>> survivors;

    for (const auto &child : tree[node].children)
    {
        const uint64_t games = tree[child.second].games;

        if (games >= min_count && games * 100 >= tree[node].games * min_share_percent)
        {
            survivors.push_back(child);
        }
    }

    if (survivors.empty())
    {
        if (!path.empty())
        {
            std::string line = path[0];

            for (std::size_t index = 1; index < path.size(); ++index)
            {
                line += " ";
                line += path[index];
            }

            lines.push_back(line);
        }

        return;
    }

    // Sorted by move, so the file is a function of the games and the settings and not of the order the games
    // happened to arrive in - two runs over the same input write the same bytes.
    std::sort(survivors.begin(), survivors.end(),
              [](const auto &left, const auto &right) { return left.first < right.first; });

    for (const auto &child : survivors)
    {
        path.push_back(child.first);
        CollectLines(tree, child.second, min_count, min_share_percent, path, lines);
        path.pop_back();
    }
}

/// @brief Print the usage and the defaults. @param stream Where to print it.
void Usage(std::ostream &stream)
{
    stream << "make_book: build pawnstar.book from Lichess PGN on standard input.\n"
           << "  --out <path>             (default pawnstar.book)\n"
           << "  --min-elo <n>            (default 2000)\n"
           << "  --min-base-seconds <n>   (default 180)\n"
           << "  --plies <n>              (default 16)\n"
           << "  --min-game-plies <n>     (default: --plies)\n"
           << "  --min-count <n>          (default 12)\n"
           << "  --min-share-percent <n>  (default 1)\n"
           << "  --max-bytes <n>          (default 0, meaning all of the input)\n"
           << "  --month <YYYY-MM>\n"
           << "  --source-url <url>\n";
}
} // namespace

/// @brief Entry point. @param argc Argument count. @param argv Arguments. @return 0 on success.
int main(int argc, char **argv)
{
    Options options;

    for (int index = 1; index < argc; ++index)
    {
        const std::string_view flag  = argv[index];
        const bool             wants = index + 1 < argc;

        if (flag == "--help" || flag == "-h")
        {
            Usage(std::cout);
            return 0;
        }

        if (!wants)
        {
            std::cerr << "make_book: " << flag << " needs a value\n";
            return 2;
        }

        const char *value = argv[++index];

        if (flag == "--out")
        {
            options.out_path = value;
        }
        else if (flag == "--month")
        {
            options.source_month = value;
        }
        else if (flag == "--source-url")
        {
            options.source_url = value;
        }
        else if (flag == "--min-elo")
        {
            options.min_elo = (int)ParseNonNegative(value);
        }
        else if (flag == "--min-base-seconds")
        {
            options.min_base_seconds = (int)ParseNonNegative(value);
        }
        else if (flag == "--plies")
        {
            options.plies = (int)ParseNonNegative(value);
        }
        else if (flag == "--min-game-plies")
        {
            options.min_game_plies = (int)ParseNonNegative(value);
        }
        else if (flag == "--min-count")
        {
            options.min_count = (uint64_t)ParseNonNegative(value);
        }
        else if (flag == "--min-share-percent")
        {
            options.min_share_percent = (uint64_t)ParseNonNegative(value);
        }
        else if (flag == "--max-bytes")
        {
            options.max_bytes = (uint64_t)ParseNonNegative(value);
        }
        else
        {
            std::cerr << "make_book: unknown option " << flag << "\n";
            Usage(std::cerr);
            return 2;
        }
    }

    if (options.plies <= 0 || options.min_count == 0 || options.min_elo < 0 || options.min_base_seconds < 0)
    {
        std::cerr << "make_book: --plies, --min-count, --min-elo and --min-base-seconds must be positive\n";
        return 2;
    }

    std::vector<Node>  tree{Node{}};
    Statistics         statistics;
    lichess::PgnReader reader{std::cin};
    lichess::PgnGame   game;

    while (reader.NextGame(game))
    {
        ++statistics.games_seen;

        if (PassesFilters(game, options, statistics))
        {
            ++statistics.games_used;
            WalkGame(game, options, tree, statistics);
        }

        if (options.max_bytes != 0 && reader.attributable_bytes() >= options.max_bytes)
        {
            break;
        }

        if ((statistics.games_seen % 200000) == 0)
        {
            std::cerr << "make_book: " << statistics.games_seen << " games seen, " << statistics.games_used << " used, "
                      << tree.size() << " tree nodes\n";
        }
    }

    std::vector<std::string> path;
    std::vector<std::string> lines;
    CollectLines(tree, 0, options.min_count, options.min_share_percent, path, lines);

    if (lines.empty())
    {
        std::cerr << "make_book: no line survived --min-count " << options.min_count << "; nothing written\n";
        return 1;
    }

    std::ofstream out{options.out_path, std::ios::binary};

    if (!out)
    {
        std::cerr << "make_book: cannot write " << options.out_path << "\n";
        return 1;
    }

    const int min_game_plies = options.min_game_plies >= 0 ? options.min_game_plies : options.plies;

    out << "# Pawnstar's opening book.\n"
        << "#\n"
        << "# SOURCE AND LICENCE. Every line below was built from the " << kSourceName << ",\n"
        << "# " << kSourceHomeUrl << ", which is published under the\n"
        << "# " << kSourceLicenceName << " (" << kSourceLicence << ",\n"
        << "# " << kSourceLicenceUrl << "). CC0 places the work in the public\n"
        << "# domain as far as the law allows and asks for no attribution, so this book carries no\n"
        << "# restriction of its own and no third party's. Lichess is credited in THIRD-PARTY-NOTICES.md\n"
        << "# anyway, because it is the right thing to do and not because anything obliges it.\n"
        << "#\n"
        << "# Generated by tools/make_book.cpp (Pawnstar " << VersionString() << ") on " << UtcTimestamp() << ".\n"
        << "#   source month      " << (options.source_month.empty() ? "(unrecorded)" : options.source_month) << "\n"
        << "#   source url        " << (options.source_url.empty() ? "(unrecorded)" : options.source_url) << "\n"
        << "#   input sha256      " << reader.Digest() << "   (of the " << reader.attributable_bytes()
        << " bytes read)\n"
        << "#   min elo           " << options.min_elo << "   (both players)\n"
        << "#   min base seconds  " << options.min_base_seconds << "   (the TimeControl base)\n"
        << "#   plies per line    " << options.plies << "\n"
        << "#   min game plies    " << min_game_plies << "\n"
        << "#   min games a move  " << options.min_count << "\n"
        << "#   min share a move  " << options.min_share_percent << "%   (of the games reaching the position)\n"
        << "#   games seen        " << statistics.games_seen << "\n"
        << "#   games used        " << statistics.games_used << "\n"
        << "#   book lines        " << lines.size() << "\n"
        << "#\n"
        << "# Each line is one opening, in coordinate notation, replayed from the start position by\n"
        << "# OpeningBook::ParseLineOfPlay. A move was kept only where at least " << options.min_count
        << " of the games above\n"
        << "# played it from that position AND it was at least " << options.min_share_percent
        << "% of the games that reached that\n"
        << "# position - the second rule being what keeps a fringe move out of the first few plies, where\n"
        << "# tens of thousands of games stand behind a position and no absolute count filters anything.\n"
        << "# Each line runs to where those thresholds stopped it, so no line is a prefix of another and\n"
        << "# each appears exactly once. A move's weight in the engine's choice is therefore the number of\n"
        << "# lines passing through it - see tools/make_book.cpp, which explains why that is the weighting\n"
        << "# and not the game count.\n"
        << "#\n"
        << "# DO NOT EDIT BY HAND. Regenerate with tools/make_book.cpp, and regenerate nnue/openings.epd\n"
        << "# with tools/make_openings.sh afterwards - it is derived from this file.\n";

    for (const std::string &line : lines)
    {
        out << line << "\n";
    }

    out.flush();

    if (!out)
    {
        std::cerr << "make_book: write failed on " << options.out_path << "\n";
        return 1;
    }

    std::cerr << "make_book: " << statistics.games_seen << " games seen (" << statistics.games_truncated
              << " truncated, " << statistics.games_non_standard << " non-standard, " << statistics.games_unfinished
              << " unfinished, " << statistics.games_below_elo << " below elo, " << statistics.games_too_fast
              << " too fast, " << statistics.games_too_short << " too short), " << statistics.games_used << " used, "
              << statistics.games_unparsed_san << " abandoned at an unresolved SAN, " << statistics.plies_walked
              << " plies walked, " << tree.size() << " tree nodes, " << lines.size() << " lines -> " << options.out_path
              << "\n";

    return 0;
}

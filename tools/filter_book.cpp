/// @file filter_book.cpp Offline opening-book filter: flag questionable book moves with a '?' suffix.
///
/// For every move in the book, this searches the position *before* the move with the engine's own NNUE
/// evaluation and compares the book move's value to the engine's best move. A move is "questionable" when it
/// loses more than a margin (default 75 cp) relative to the best move; such moves get a trailing '?' in the
/// output so the engine recognises and replays them (to stay in book as the defender) but never plays them
/// itself (see opening_book.cpp).
///
/// The book file structure is preserved: lines, move numbers, and '#' comments are passed through verbatim;
/// only move tokens are (potentially) rewritten with a '?'. Re-running on an already-filtered book is safe —
/// an existing '?' is stripped and recomputed.
///
/// Parallelism: book *lines* are processed across a thread pool (one Game per worker). Each individual search
/// is forced single-threaded (PAWNSTAR_THREADS=1) so it stays deterministic; concurrency comes from running
/// many lines at once. Output is buffered per line and written in the original order, so the result is
/// independent of scheduling. The per-(position, move) verdict cache is shared across workers so common
/// opening prefixes are (mostly) scored once.
///
/// Usage:  filter_book <net.bin> <in.book> [out.book] [--depth N] [--margin CP] [--threads T]
///   out.book defaults to "<in.book>.filtered" so the input is never clobbered.
///   --depth   fixed search depth for the parent position (default 12; the child is searched at depth-1).
///   --margin  centipawn loss-vs-best threshold above which a move is flagged (default 75).
///   --threads number of book lines to process simultaneously (default: hardware concurrency). Each worker
///             holds its own ~80 MB of hash tables, so high thread counts cost memory.
///
/// Build:  make tools   ->   build/filter_book

#include "chess_clock.h"
#include "game.h"
#include "move.h"
#include "position.h"
#include "search.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <ranges>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace
{
constexpr const char *kStartFen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

struct SearchResult
{
    bool found_;
    int  score_; ///< Side-to-move-relative centipawns of the best move.
};

/// @brief Search a position to a fixed depth and return its best move's score.
///
/// Determinism matters here: the same position must always produce the same verdict regardless of how the
/// worker threads happen to interleave. SearchRootNode is only deterministic when it runs single-threaded
/// (under Lazy SMP the helper threads race through the shared TT, making the exact score non-reproducible),
/// so the caller sets PAWNSTAR_THREADS=1 once at startup; with that, this whole search runs in the calling
/// worker thread with no nested threads spawned.
///
/// The returned score is side-to-move relative (negamax convention) in NNUE centipawns: positive means the
/// side to move is better. Mates are returned as large near-±kMateScore values, so a missed mate shows up as
/// a huge loss below.
///
/// @param game  A worker-owned Game (its own TT/eval cache/net). Mutated: its position is reset to @p fen.
/// @param fen   The position to search, in FEN.
/// @param depth Fixed search depth.
/// @return {found, score}. found is false only if the position has no legal move (mate/stalemate at the root).
SearchResult Search(Game &game, const std::string &fen, int depth)
{
    game.SetPosition(fen);
    game.book_.Free(); // never return a book move - we want the engine's own judgement, not a canned reply
    game.time_control_.clock_type_ = ChessClock::kFixedDepth;
    game.time_control_.depth_      = depth;
    Move m                         = game.SearchRootNode();
    return {m != Move::None(), m.score()};
}

/// @brief State shared by all worker threads.
struct Shared
{
    int                         depth_;
    int                         child_depth_;
    int                         margin_;
    std::map<std::string, bool> verdict_; ///< (position, move) -> questionable. Guarded by verdict_mtx.
    std::mutex                  verdict_mtx_;
    std::mutex                  log_mtx_;         ///< Serialises stderr flag messages.
    std::atomic<int>            moves_scored_{0}; ///< Search pairs computed (may double-count racing duplicates).
    std::atomic<int>            moves_flagged_{0};
};

/// @brief Decide whether a book move is questionable, consulting/updating the shared verdict cache.
///
/// The core idea: a book move is "questionable" if the engine, on its own, would do meaningfully better by
/// playing something else. We measure that as the move's centipawn *loss versus the best move*, computed with
/// two fixed-depth searches:
///
///   * best  = score of the engine's best move from @p pos, searched to `depth`. This is what the engine
///             would score the position at if it were free to choose.
///   * child = score of the position *after* the book move, searched to `depth-1`. This is from the opponent's
///             (the now-side-to-move's) perspective, so the book move's value to the mover is its negation.
///             Using depth-1 keeps the two searches at the same leaf horizon (1 ply for the move + depth-1
///             below it == depth), so the comparison is apples-to-apples.
///
/// Then loss = best - (-child). If the book move *is* the engine's best move, loss is ~0; the worse the book
/// move, the larger the loss. We flag when loss exceeds the margin. Because mates score near ±kMateScore, a
/// book move that walks past a forced mate shows up as an enormous loss and is (correctly) flagged.
///
/// Both searches share this worker's Game/TT but operate on freshly-set positions, so the result depends only
/// on (@p pos, @p mv) and the fixed depth - never on neighbouring work. That makes the verdict cacheable.
///
/// Threading: the cache is shared across all workers (common opening prefixes recur in many lines, so this is
/// where most of the duplicate-work saving comes from). We hold verdict_mtx only for the two short lookups and
/// the insert, never across the (multi-second) searches. Two workers can therefore both miss the cache for the
/// same key and both compute it - that wastes a little work but is harmless, since the verdict is deterministic
/// and both stores write the same value.
///
/// @return true if the move should be suffixed with '?' (questionable).
bool JudgeQuestionable(Game &game, const Position &pos, const Move &mv, Shared &sh)
{
    // The key identifies the (position-before-the-move, move) pair. The Zobrist hash pins down the position and
    // mv.ToString() (coordinate notation, never carrying a '?') pins down the move.
    const std::string key = std::to_string(pos.hash_) + ":" + mv.ToString();
    {
        std::lock_guard<std::mutex> lk{sh.verdict_mtx_};
        if (auto it = sh.verdict_.find(key); it != sh.verdict_.end())
        {
            return it->second; // already judged (by this or another worker) - reuse it
        }
    }
    // Cache miss: run the two scoring searches with no lock held (see threading note above).
    const std::string  parent_fen = pos.ToString();
    const std::string  child_fen  = pos.MakeMove(mv).ToString();
    const SearchResult best       = Search(game, parent_fen, sh.depth_);
    const SearchResult child      = Search(game, child_fen, sh.child_depth_);

    bool questionable = false;
    // If the child has no legal move, the book move delivered mate or stalemate. A mating move is the best move
    // there is and must never be flagged; a stalemating move is a rare opening curiosity. Either way we leave it
    // unflagged rather than risk suffixing a strong move with '?'.
    if (best.found_ && child.found_)
    {
        const int book_value = -child.score_;            // the book move's value from the mover's perspective
        const int loss       = best.score_ - book_value; // how much worse than the engine's own best choice
        questionable         = loss > sh.margin_;
        sh.moves_scored_.fetch_add(1, std::memory_order_relaxed);
        if (questionable)
        {
            sh.moves_flagged_.fetch_add(1, std::memory_order_relaxed);
            // Log every flag so the run can be reviewed: the move, the two scores, the loss, and the FEN of the
            // position it was played from. log_mtx keeps concurrent messages from interleaving on stderr.
            std::lock_guard<std::mutex> lk{sh.log_mtx_};
            std::cerr << "  flag " << mv.ToString() << "  best=" << best.score_ << " book=" << book_value
                      << " loss=" << loss << "  " << parent_fen << "\n";
        }
    }
    {
        std::lock_guard<std::mutex> lk{sh.verdict_mtx_};
        sh.verdict_[key] = questionable;
    }
    return questionable;
}

/// @brief Rewrite a single book line, flagging questionable moves with '?'. Returns the rebuilt line.
///
/// A book line is a whitespace-separated sequence of coordinate moves (e.g. "e2e4 e7e5 g1f3"), optionally with
/// move-number tokens ("1.") and a trailing '#' comment - exactly the grammar OpeningBook::ParseLineOfPlay
/// accepts. This mirrors that replay so it walks the same positions, and judges each *move* token against the
/// position reached just before it. The line's structure is preserved on output: move numbers and the comment
/// pass through untouched, and only move tokens may gain a '?'.
///
/// @param game    The worker's Game, used for the scoring searches.
/// @param line    The raw input line.
/// @param line_no Zero-based line index, used only for diagnostics.
/// @param sh      Shared verdict cache / counters / logging.
/// @return The rewritten line (single-spaced tokens, comment re-appended).
std::string ProcessLine(Game &game, const std::string &line, int line_no, Shared &sh)
{
    // Split off any trailing '#' comment (to end of line) and keep it verbatim to re-append at the end.
    std::string body = line;
    std::string comment;
    if (const auto h = line.find('#'); h != std::string::npos)
    {
        body    = line.substr(0, h);
        comment = line.substr(h);
    }

    // Every line is replayed from the standard start position, just like the book loader does.
    Position                 pos = Position::FromString(kStartFen);
    std::vector<std::string> out_tokens;
    std::stringstream        ss{body};
    std::string              tok;

    while (ss >> tok)
    {
        // Move numbers ("1.", "12.") start with a digit and are not moves - copy them through unchanged.
        if (std::isdigit(static_cast<unsigned char>(tok[0])))
        {
            out_tokens.push_back(tok);
            continue;
        }
        // Strip any pre-existing '?' so re-running the filter recomputes the verdict from scratch (idempotent
        // on a book that was already filtered, and lets you re-run with a different margin).
        const std::string coord = (tok.back() == '?') ? tok.substr(0, tok.size() - 1) : tok;

        // Resolve the coordinate token to the actual legal Move from this position (needed to advance and to
        // build the cache key). A token that matches no legal move means the book line is corrupt from here on.
        auto moves = pos.GenerateLegalMoves();
        auto it    = std::ranges::find_if(moves, [&](const Move &m) { return m.ToString() == coord; });
        if (it == moves.end())
        {
            // Don't drop data: warn, copy the original token verbatim, and stop processing this line (the
            // remaining tokens depend on a position we can no longer reconstruct).
            std::lock_guard<std::mutex> lk{sh.log_mtx_};
            std::cerr << "filter_book: illegal move '" << coord << "' in line " << line_no + 1 << "; passing through\n";
            out_tokens.push_back(tok);
            break;
        }
        const Move mv           = *it;
        const bool questionable = JudgeQuestionable(game, pos, mv, sh);
        out_tokens.push_back(questionable ? coord + "?" : coord);
        pos = pos.MakeMove(mv); // advance to judge the next move from the resulting position
    }

    // Reassemble: single-spaced tokens, then the preserved comment (if any).
    std::string rebuilt;
    for (size_t i = 0; i < out_tokens.size(); ++i)
    {
        if (i)
        {
            rebuilt += ' ';
        }
        rebuilt += out_tokens[i];
    }
    if (!comment.empty())
    {
        if (!rebuilt.empty())
        {
            rebuilt += ' ';
        }
        rebuilt += comment;
    }
    return rebuilt;
}
} // namespace

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        std::cerr << "usage: filter_book <net.bin> <in.book> [out.book] [--depth N] [--margin CP] [--threads T]\n";
        return 1;
    }
    const std::string net_path = argv[1];
    const std::string in_path  = argv[2];
    std::string       out_path;
    int               depth   = 12;
    int               margin  = 75;
    int               threads = 0; // 0 => hardware concurrency

    for (int i = 3; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (arg == "--depth" && i + 1 < argc)
        {
            depth = std::atoi(argv[++i]);
        }
        else if (arg == "--margin" && i + 1 < argc)
        {
            margin = std::atoi(argv[++i]);
        }
        else if (arg == "--threads" && i + 1 < argc)
        {
            threads = std::atoi(argv[++i]);
        }
        else if (!arg.empty() && arg[0] != '-')
        {
            out_path = arg;
        }
        else
        {
            std::cerr << "filter_book: unrecognised argument '" << arg << "'\n";
            return 1;
        }
    }
    if (out_path.empty())
    {
        // Default to a sibling file so the input book is never clobbered - the caller reviews the diff and
        // promotes it deliberately.
        out_path = in_path + ".filtered";
    }

    // Force every SearchRootNode call single-threaded so each search is deterministic (see Search()). All the
    // parallelism in this tool is at the book-line level instead. Set once, before any Game is constructed.
    setenv("PAWNSTAR_THREADS", "1", 1);

    // Read the whole book into memory up front. It is tiny (hundreds of lines) and keeping the inputs indexed
    // lets workers grab lines in any order while we still emit output in the original order.
    std::ifstream in{in_path};
    if (!in)
    {
        std::cerr << "filter_book: cannot open '" << in_path << "'\n";
        return 1;
    }
    std::vector<std::string> input_lines;
    for (std::string line; std::getline(in, line);)
    {
        input_lines.push_back(line);
    }

    std::ofstream out{out_path};
    if (!out)
    {
        std::cerr << "filter_book: cannot write '" << out_path << "'\n";
        return 1;
    }

    // Default the worker count to the machine's hardware concurrency, and never spawn more workers than there
    // are lines to process (extra workers would just idle).
    if (threads <= 0)
    {
        const unsigned hw = std::thread::hardware_concurrency();
        threads           = hw == 0 ? 1 : (int)hw;
    }
    threads = std::clamp<int>(threads, 1, (int)std::max<size_t>(1, input_lines.size()));

    Shared sh;
    sh.depth_       = depth;
    sh.child_depth_ = std::max(1, depth - 1);
    sh.margin_      = margin;

    std::cerr << "filter_book: net=" << net_path << " depth=" << depth << " margin=" << margin
              << "cp threads=" << threads << " lines=" << input_lines.size() << "\n";

    // The engine's diagnostic counters (DEBUGX builds) live in a single process-global map that INCREMENT
    // populates lazily on first use, caching a slot pointer in a function-local static per call site. Once a
    // slot exists, updating it is just a (benign, racy) integer increment - the same races Lazy SMP already
    // tolerates. The risky window is the *first* touch of each call site concurrently inserting into the map.
    // A single serial search here visits the hot search/eval/TT call sites first, so by the time workers start
    // those statics are populated and no concurrent map insertion happens. It also validates the net loads.
    {
        Game warm;
        if (!warm.nnue_network_.Load(net_path))
        {
            std::cerr << "filter_book: failed to load net '" << net_path << "'\n";
            return 1;
        }
        Search(warm, kStartFen, std::min(depth, 6)); // shallow is enough to touch the call sites
    }

    // Per-line outputs, written back at their original index so the final file order matches the input order
    // regardless of which worker finished which line when.
    std::vector<std::string> output_lines(input_lines.size());
    std::atomic<size_t>      next{0}; // shared cursor: the next line index to claim (work-stealing)
    std::atomic<bool>        load_failed{false};

    auto worker = [&] {
        // Each worker owns a Game (its own ~80 MB of hash tables + its own net), so the only state shared with
        // other workers is the verdict cache/counters in `sh` - no contention on the hot search path.
        Game game;
        if (!game.nnue_network_.Load(net_path))
        {
            load_failed.store(true);
            return;
        }
        // Claim line indices atomically until they run out; lines are independent units of work.
        for (size_t idx = next.fetch_add(1); idx < input_lines.size(); idx = next.fetch_add(1))
        {
            output_lines[idx] = ProcessLine(game, input_lines[idx], (int)idx, sh);
            const size_t done = idx + 1;
            if (done % 20 == 0) // periodic progress; "~" because claim order is not completion order
            {
                std::lock_guard<std::mutex> lk{sh.log_mtx_};
                std::cerr << "filter_book: ~" << done << "/" << input_lines.size() << " lines, "
                          << sh.moves_flagged_.load() << " flagged so far\n";
            }
        }
    };

    std::vector<std::thread> pool;
    pool.reserve(threads);
    for (int i = 0; i < threads; ++i)
    {
        pool.emplace_back(worker);
    }
    for (auto &t : pool)
    {
        t.join();
    }
    if (load_failed.load())
    {
        std::cerr << "filter_book: a worker failed to load net '" << net_path << "'\n";
        return 1;
    }

    // All workers done: flush the rewritten lines in input order.
    for (const auto &l : output_lines)
    {
        out << l << '\n';
    }

    std::cerr << "filter_book: done. " << input_lines.size() << " lines, " << sh.moves_scored_.load()
              << " move searches, " << sh.moves_flagged_.load() << " flagged questionable. Wrote " << out_path << "\n";
    return 0;
}

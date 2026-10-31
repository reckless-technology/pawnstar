/// @file filter_book.cpp Offline opening-book filter: flag questionable book moves with a '?' suffix.
///
/// For every move in the book, this searches the position *before* the move twice with the engine's own NNUE
/// evaluation - once unrestricted, once restricted to that one move - and compares the two scores, taken at the
/// same depth from the same root. A move is "questionable" when it
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
///   --depth   fixed search depth, used for both scores (default 12).
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
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <ranges>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace
{
constexpr const char *kStartFen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

// This tool's own comment block is fenced by these, so re-running on a book it already pruned rewrites that
// block instead of stacking a second copy above it. Re-running is a documented thing to do - it is how a
// different margin is tried - and a committed book must not grow a header every time.
constexpr const char *kBlockStart = "# ---- filter_book ----";
constexpr const char *kBlockEnd   = "# ---- end filter_book ----";
constexpr const char *kRefillMark = "# ---- filter_book: the engine's own moves ----";

/// @brief True for a comment line belonging to a previous run's own block, which this run has rewritten.
/// @param line         The input line.
/// @param inside_block Carried between calls: true while between the fence markers.
bool IsOurOwnComment(const std::string &line, bool &inside_block)
{
    if (line.starts_with(kBlockStart))
    {
        inside_block = true;
        return true;
    }
    if (line.starts_with(kBlockEnd))
    {
        inside_block = false;
        return true;
    }
    return inside_block || line.starts_with(kRefillMark);
}

struct SearchResult
{
    bool        found_;
    int         score_; ///< Side-to-move-relative centipawns of the move below.
    std::string move_;  ///< Coordinate notation of the move searched, so a caller can re-score it restricted.
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
/// @param game      A worker-owned Game (its own TT/eval cache/net). Mutated: its position is reset to @p fen.
/// @param fen       The position to search, in FEN.
/// @param depth     Fixed search depth.
/// @param only_move Coordinate notation of the single root move to allow, or empty for an unrestricted search.
///                  This is UCI `go searchmoves` with one move: the engine searches that move to the same
///                  depth as an unrestricted search, so the two scores are directly comparable.
/// @return {found, score}. found is false only if the position has no legal move (mate/stalemate at the root).
SearchResult Search(Game &game, const std::string &fen, int depth, const std::string &only_move = {})
{
    game.SetPosition(fen);
    // SetPosition clears the eval cache but NOT the transposition table, and a stale table is not a mere
    // speed-up here: the parent search writes fail-low bounds for every move it rejects, including the book
    // move whose child we are about to search, and that child search then reads its own rejection back as a
    // bound and confirms it. The verdict became a function of whatever the worker had searched before, so one
    // (position, move) pair scored differently in different workers - 'f8g7' after 1.c4 g6 2.Nc3 came back at
    // losses of 90, 98, 106, 111 and 126 cp in one run - and the cache below stored whichever arrived first.
    // Clearing makes a search a pure function of (fen, depth), which is what the caching and the '?' set in a
    // committed book both require.
    game.transposition_table_.Clear();
    game.book_.Free(); // never return a book move - we want the engine's own judgement, not a canned reply
    game.search_moves_.clear();
    if (!only_move.empty())
    {
        game.search_moves_.push_back(only_move);
    }
    game.time_control_.clock_type_ = ChessClock::kFixedDepth;
    game.time_control_.depth_      = depth;
    Move m                         = game.SearchRootNode();
    return {m != Move::None(), m.score(), m.ToString()};
}

/// @brief State shared by all worker threads.
struct Shared
{
    int                         depth_;
    int                         margin_;
    std::map<std::string, bool> verdict_; ///< (position, move) -> questionable. Guarded by verdict_mtx.
    std::mutex                  verdict_mtx_;

    /// A position's reference: the engine's own choice there, and that move's score measured the same
    /// restricted way every book move is. The move is kept because it is what refills the position if the
    /// margin removes everything the book offered.
    struct Reference
    {
        int         score_;
        std::string move_;
    };

    std::map<zobrist_t, Reference> reference_;
    std::mutex                     reference_mtx_;
    std::mutex                     log_mtx_;         ///< Serialises stderr flag messages.
    std::atomic<int>               moves_scored_{0}; ///< Search pairs computed (may double-count racing duplicates).
    std::atomic<int>               moves_flagged_{0};
};

/// @brief The score to measure this position's book moves against: the engine's own preferred move, scored the
/// same restricted way they are.
///
/// This exists because an unrestricted search's score and a restricted search's score are NOT comparable, and
/// subtracting one from the other was flagging moves that are perfectly sound. After 1.Nf3 g6 the unrestricted
/// depth-12 search reports `e2e4` at +190; searching `e2e4` alone at the same depth reports +102. One move, one
/// position, one depth, 88 cp apart - because a root holding twenty-odd moves prunes, orders and re-searches
/// quite differently from a root holding one, and "depth 12" does not mean the same effort in both. Measured
/// against the +190, every one of the seven book replies to 1.Nf3 g6 came out more than 75 cp short and the
/// position lost every move it had, which is an artefact of the instrument and not an opinion about chess.
///
/// So every number in the comparison now comes from a restricted search: the engine picks its move with an
/// ordinary search, and that move is then re-scored alone, exactly as each book move is. Like against like.
///
/// One conservatism is left in deliberately. The reference is the engine's *chosen* move re-scored, not the
/// highest score over all legal moves re-scored - that would be another ~30 searches per position for a
/// reference that can only move up. Where some third move would have scored higher restricted, the reference
/// is a little low and a borderline book move escapes the flag. Under-flagging is the right way to be wrong
/// here: a '?' on a sound move costs the engine a line it should have played.
///
/// @return The reference score, or nullopt if the position has no legal move at all.
std::optional<Shared::Reference> ReferenceScore(Game &game, const Position &pos, Shared &sh)
{
    {
        std::lock_guard<std::mutex> lk{sh.reference_mtx_};
        if (auto it = sh.reference_.find(pos.hash_); it != sh.reference_.end())
        {
            return it->second;
        }
    }
    const std::string  fen    = pos.ToString();
    const SearchResult chosen = Search(game, fen, sh.depth_);
    if (!chosen.found_)
    {
        return std::nullopt;
    }
    const SearchResult      scored    = Search(game, fen, sh.depth_, chosen.move_);
    const Shared::Reference reference = {scored.score_, chosen.move_};
    {
        std::lock_guard<std::mutex> lk{sh.reference_mtx_};
        sh.reference_[pos.hash_] = reference;
    }
    return reference;
}

/// @brief Decide whether a book move is questionable, consulting/updating the shared verdict cache.
///
/// The core idea: a book move is "questionable" if the engine, on its own, would do meaningfully better by
/// playing something else. We measure that as the move's centipawn *loss versus the best move*, computed with
/// two fixed-depth searches:
///
///   * best = score of the engine's best move from @p pos, searched to `depth`, unrestricted. This is what
///            the engine scores the position at when it is free to choose.
///   * book = score of @p mv from the SAME position at the SAME depth, with the root restricted to that one
///            move (`searchmoves`). Both numbers are side-to-move relative at one root and come from the same
///            kind of search, so `loss = best - book` is the sentence the margin states: how far below the
///            best scoring move this move scores.
///
/// It used to search the position *after* the move at `depth - 1` and negate that. The leaf horizon is the
/// same, but it is not the same measurement: a fresh search of the child reaches its score through its own
/// iterative deepening and aspiration windows, and it disagreed with the restricted search at the parent by
/// tens of centipawns. For `f8g7` after 1.e4 g6 2.f4, both runs agreeing that the best move scores +46: the
/// child at depth 11 valued the move at -108, a loss of 154 cp, and the restricted search at depth 12 values
/// it at -60, a loss of 106. The engine reproduces the restricted pair exactly when driven by hand with one
/// thread (`go depth 12` then `go depth 12 searchmoves f8g7`), which the child route cannot be checked against
/// so directly. On a 75 cp margin a 48 cp disagreement decides verdicts, so both scores now come from one
/// root at one depth.
///
/// If the book move *is* the engine's best move, loss is ~0; the worse the move, the larger the loss. We flag
/// when loss exceeds the margin. Because mates score near ±kMateScore, a book move that walks past a forced
/// mate shows up as an enormous loss and is (correctly) flagged, while a mating move scores far above any
/// alternative and never is.
///
/// Each search clears the transposition table (see Search), so a verdict depends only on (@p pos, @p mv) and
/// the depth, never on what the worker searched before it. That is what makes it cacheable, and what makes a
/// run reproducible: one book at one depth gives the same '?' set at any thread count.
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
    const std::string                      parent_fen = pos.ToString();
    const std::optional<Shared::Reference> reference  = ReferenceScore(game, pos, sh);
    const SearchResult                     book       = Search(game, parent_fen, sh.depth_, mv.ToString());

    bool questionable = false;
    // Either search failing to find a move means the root has no legal move at all, so the book line has ended
    // in mate or stalemate - a curiosity rather than a bad move. Leave it unflagged rather than risk suffixing
    // a strong move with '?'.
    if (reference.has_value() && book.found_)
    {
        const int book_value = book.score_;                    // the book move's own score, restricted
        const int loss       = reference->score_ - book_value; // how far below the engine's own move, alike
        questionable         = loss > sh.margin_;
        sh.moves_scored_.fetch_add(1, std::memory_order_relaxed);
        if (questionable)
        {
            sh.moves_flagged_.fetch_add(1, std::memory_order_relaxed);
            // Log every flag so the run can be reviewed: the move, the two scores, the loss, and the FEN of the
            // position it was played from. log_mtx keeps concurrent messages from interleaving on stderr.
            std::lock_guard<std::mutex> lk{sh.log_mtx_};
            std::cerr << "  drop " << mv.ToString() << "  best=" << reference->score_ << " book=" << book_value
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
/// @brief What judging one book line produced.
struct LineOutcome
{
    std::vector<std::string> kept_;        ///< The line's tokens, each move carrying a '?' if it was marked.
    std::string              comment_;     ///< Any trailing '#' comment, preserved verbatim.
    bool                     has_moves_{}; ///< False for a blank or comment-only line, which passes through.
};

/// @brief Judge one book line, marking each below-margin move with a trailing '?'.
///
/// A move more than the margin below its position's reference gets a '?', which `OpeningBook::ParseLineOfPlay`
/// replays - so the rest of the line still parses - but never selects. The line keeps its full length, and
/// that is the point of marking rather than deleting: the moves after a marked move are the replies to it, and
/// the engine needs them when an opponent plays the move it declined itself. Deleting the move would take the
/// whole continuation with it and leave the engine out of book exactly where an opponent had surprised it.
///
/// A marked move is not selectable, though, so a position all of whose moves are marked has nothing to play.
/// The caller puts the engine's own choice at those positions, unmarked. See main.
LineOutcome ProcessLine(Game &game, const std::string &line, int line_no, Shared &sh)
{
    // Split off any trailing '#' comment (to end of line) and keep it verbatim to re-append at the end.
    std::string body = line;
    LineOutcome outcome;
    if (const auto h = line.find('#'); h != std::string::npos)
    {
        body             = line.substr(0, h);
        outcome.comment_ = line.substr(h);
    }

    // Every line is replayed from the standard start position, just like the book loader does.
    Position          pos = Position::FromString(kStartFen);
    std::stringstream ss{body};
    std::string       tok;

    while (ss >> tok)
    {
        // Move numbers ("1.", "12.") start with a digit and are not moves - copy them through unchanged.
        if (std::isdigit(static_cast<unsigned char>(tok[0])))
        {
            outcome.kept_.push_back(tok);
            continue;
        }
        outcome.has_moves_ = true;
        // Strip any pre-existing '?' so a book that was already marked still reads, and re-running recomputes
        // the verdict from scratch: idempotent, and it lets you re-run with a different margin.
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
            outcome.kept_.push_back(tok);
            break;
        }
        const Move mv           = *it;
        const bool questionable = JudgeQuestionable(game, pos, mv, sh);
        outcome.kept_.push_back(questionable ? coord + "?" : coord);
        pos = pos.MakeMove(mv); // advance to judge the next move from the resulting position
    }

    return outcome;
}

/// @brief The move tokens of a line, with any move-number tokens dropped - what makes two lines the same line.
std::vector<std::string> MoveTokensOf(const std::vector<std::string> &tokens)
{
    std::vector<std::string> moves;
    for (const std::string &t : tokens)
    {
        if (!std::isdigit(static_cast<unsigned char>(t[0])))
        {
            moves.push_back(t);
        }
    }
    return moves;
}

/// @brief Join move tokens with single spaces.
std::string JoinMoves(const std::vector<std::string> &moves)
{
    std::string joined;
    for (const std::string &m : moves)
    {
        if (!joined.empty())
        {
            joined += ' ';
        }
        joined += m;
    }
    return joined;
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
#if defined(_WIN32)
    _putenv_s("PAWNSTAR_THREADS", "1");
#else
    setenv("PAWNSTAR_THREADS", "1", 1);
#endif

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

    // Binary, so a newline stays one byte. In text mode on Windows the runtime writes CRLF, and the same
    // book filtered on Windows and on Linux would then differ in every line ending - which matters more
    // here than it looks: this file is copied byte for byte into pawnstar-rs and pawnstar-go, where the
    // three copies having one SHA-256 is the check that the copy actually happened. The book is marked
    // `-text` in .gitattributes for the same reason, so git will not quietly normalise this either.
    std::ofstream out{out_path, std::ios::binary};
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
    sh.depth_  = depth;
    sh.margin_ = margin;

    std::cerr << "filter_book: net=" << net_path << " depth=" << depth << " margin=" << margin
              << "cp threads=" << threads << " lines=" << input_lines.size() << "\n";

    // The engine's diagnostic counters (DEBUGX builds) live in a single process-global map that INCREMENT
    // populates lazily on first use, caching a slot pointer in a function-local static per call site. Once a
    // slot exists, updating it is just a (benign, racy) integer increment - the same races Lazy SMP already
    // tolerates. The risky window is the *first* touch of each call site concurrently inserting into the map.
    // A single serial search here visits the hot search/eval/TT call sites first, so by the time workers start
    // those statics are populated and no concurrent map insertion happens. It also validates the net loads.
    {
        // On the heap, like main.cpp's: a Game carries the net's weights by value, so sizeof(Game) is
        // the architecture's payload plus change - 12 MB at 8 king buckets x 1024 - and a Game declared
        // as a local overflows the stack before its first search. It overflows a 1 MB Windows thread and
        // an 8 MB Linux one alike, so this is not a platform quirk to work around on one side.
        auto  warm_ptr = std::make_unique<Game>();
        Game &warm     = *warm_ptr;
        if (!warm.nnue_network_.Load(net_path))
        {
            std::cerr << "filter_book: failed to load net '" << net_path << "'\n";
            return 1;
        }
        Search(warm, kStartFen, std::min(depth, 6)); // shallow is enough to touch the call sites
    }

    // Per-line outputs, written back at their original index so the final file order matches the input order
    // regardless of which worker finished which line when.
    std::vector<LineOutcome> outcomes(input_lines.size());
    std::atomic<size_t>      next{0}; // shared cursor: the next line index to claim (work-stealing)
    std::atomic<bool>        load_failed{false};

    auto worker = [&] {
        // Each worker owns a Game - its own hash tables, and its own copy of the net by value - so the
        // only state shared with other workers is the verdict cache/counters in `sh`, and there is no
        // contention on the hot search path. Heap-allocated for the reason above, and the reason it has
        // to be counted: a worker costs the net's payload plus its tables, so --threads buys parallelism
        // with memory.
        auto  game_ptr = std::make_unique<Game>();
        Game &game     = *game_ptr;
        if (!game.nnue_network_.Load(net_path))
        {
            load_failed.store(true);
            return;
        }
        // Claim line indices atomically until they run out; lines are independent units of work.
        for (size_t idx = next.fetch_add(1); idx < input_lines.size(); idx = next.fetch_add(1))
        {
            outcomes[idx]     = ProcessLine(game, input_lines[idx], (int)idx, sh);
            const size_t done = idx + 1;
            if (done % 20 == 0) // periodic progress; "~" because claim order is not completion order
            {
                std::lock_guard<std::mutex> lk{sh.log_mtx_};
                std::cerr << "filter_book: ~" << done << "/" << input_lines.size() << " lines, "
                          << sh.moves_flagged_.load() << " marked so far\n";
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

    // ---- Which positions have nothing selectable? ----
    //
    // A verdict is per (position, move), but "this position has nothing to play" is a property of the whole
    // book: the same position is reached by several lines and by several move orders, and a move marked in one
    // line is the same move in another. So replay every line - no searching, just the moves - and collect the
    // positions that have at least one move the engine may actually choose, and the way each was reached.
    std::set<zobrist_t>                           selectable;
    std::map<zobrist_t, std::vector<std::string>> reached_by;
    for (const LineOutcome &outcome : outcomes)
    {
        Position                 pos = Position::FromString(kStartFen);
        std::vector<std::string> prefix;
        for (const std::string &token : MoveTokensOf(outcome.kept_))
        {
            const bool        marked = token.back() == '?';
            const std::string coord  = marked ? token.substr(0, token.size() - 1) : token;
            auto              moves  = pos.GenerateLegalMoves();
            auto              it = std::ranges::find_if(moves, [&](const Move &m) { return m.ToString() == coord; });
            if (it == moves.end())
            {
                break; // an illegal token passed through above, and was reported there
            }
            reached_by.try_emplace(pos.hash_, prefix);
            if (!marked)
            {
                selectable.insert(pos.hash_);
            }
            // The prefix keeps each move's mark. Writing it bare would put every marked move along the way
            // back into the selectable set, because the parser selects whatever carries no '?' - so the
            // lines added to stop a position dead-ending would undo the marking that created it. Only the
            // move appended at the end is unmarked, and only there because the engine chose it.
            prefix.push_back(token);
            pos = pos.MakeMove(*it);
        }
    }

    // ---- Refill the emptied positions with the engine's own choice ----
    //
    // The reference move is at zero loss by construction - it IS the move the margin is measured from - so a
    // refilled position offers exactly what the engine would have found by searching. The book keeps its job
    // without ever offering a move the engine would decline.
    // The chosen move is never itself marked - its loss is zero by construction, being what the margin is
    // measured from - so a position whose every move is marked is a position the book does not offer it.
    std::vector<std::vector<std::string>> refills;
    for (const auto &[hash, prefix] : reached_by)
    {
        if (selectable.count(hash) != 0)
        {
            continue;
        }
        const auto it = sh.reference_.find(hash);
        if (it == sh.reference_.end())
        {
            std::cerr << "filter_book: no reference move cached for a position with nothing to play\n";
            continue;
        }
        std::vector<std::string> line = prefix;
        line.push_back(it->second.move_);
        refills.push_back(line);
    }

    // Marking leaves every original line exactly as long as it was, so the input's structure is untouched and
    // only the refills could repeat something. A refill cannot duplicate an existing line - the move it adds
    // is unmarked where every move the book had there is marked - but a cheap guard costs nothing.
    std::set<std::string> existing;
    for (const LineOutcome &outcome : outcomes)
    {
        existing.insert(JoinMoves(MoveTokensOf(outcome.kept_)));
    }

    // ---- Write ----
    //
    // This tool's record goes first, ahead of the generator's header, because that header says every line
    // below came from the database and that is no longer the whole truth: the refilled lines end in a move
    // this engine chose. A reader should meet the qualification before the claim it qualifies.
    // The net by its file name, not the path as it was typed: a Windows invocation gives
    // "nnue\\pawnstar-v20.bin" and a Linux one "nnue/pawnstar-v20.bin", and the book would then differ by
    // platform in its own header - exactly what writing LF above is for. The name identifies the net; the
    // path only says where it sat on one machine.
    out << kBlockStart << "\n"
        << "# SCREENED AGAINST THE ENGINE'S OWN JUDGEMENT by tools/filter_book, with "
        << std::filesystem::path{net_path}.filename().string() << " at depth " << depth << ".\n"
        << "#\n"
        << "# Any move scoring more than " << margin << " cp below its position's reference carries a\n"
        << "# trailing '?'. OpeningBook::ParseLineOfPlay replays such a move - so the rest of its line still\n"
        << "# parses, and the engine keeps its reply to it - but never selects it. So the engine answers a\n"
        << "# move it would not play itself, which is the whole reason for marking rather than deleting.\n"
        << "#\n"
        << "# The reference is the engine's own choice at that position, re-scored the same restricted way\n"
        << "# every book move is, so that the two numbers are comparable; tools/filter_book.cpp says why that\n"
        << "# matters more than it sounds.\n"
        << "#\n"
        << "# A marked move cannot be chosen, so a position whose every move is marked would have nothing to\n"
        << "# play. Those positions were given the engine's own choice, unmarked - zero loss by construction,\n"
        << "# being the move the margin is measured from. THOSE MOVES ARE THIS ENGINE'S, NOT THE DATABASE'S,\n"
        << "# and each is the last move of a line in the marked section at the end of this file. Nothing else\n"
        << "# here was added and nothing was taken away: every database line is still present in full.\n"
        << kBlockEnd << "\n";
    int  written          = 0;
    bool inside_our_block = false;
    for (size_t idx = 0; idx < outcomes.size(); ++idx)
    {
        const LineOutcome &outcome = outcomes[idx];
        if (!outcome.has_moves_)
        {
            // A blank or comment-only line: the generator's own header, passed through - unless it is this
            // tool's own block from an earlier run, which has just been rewritten above.
            if (!IsOurOwnComment(input_lines[idx], inside_our_block))
            {
                out << input_lines[idx] << '\n';
            }
            continue;
        }
        out << JoinMoves(outcome.kept_);
        if (!outcome.comment_.empty())
        {
            out << ' ' << outcome.comment_;
        }
        out << '\n';
        ++written;
    }
    int refills_written = 0;
    for (const auto &line : refills)
    {
        const std::string joined = JoinMoves(line);
        if (existing.count(joined) != 0)
        {
            continue;
        }
        if (refills_written == 0)
        {
            out << kRefillMark << "\n"
                << "# THE ENGINE'S OWN MOVES. Each line below ends in a move this engine chose, because every\n"
                << "# database move at that position is marked '?' and so cannot be played. The moves before\n"
                << "# it are the database's, exactly as above.\n";
        }
        out << joined << '\n';
        ++refills_written;
    }

    std::cerr << "filter_book: done. " << input_lines.size() << " input lines, " << sh.moves_scored_.load()
              << " move searches, " << sh.moves_flagged_.load() << " moves marked '?', " << refills_written
              << " positions given the engine's own move because every move they had was marked, "
              << (written + refills_written) << " lines written. Wrote " << out_path << "\n";
    return 0;
}

/// @file uci_commands_test.cpp The UCI front end, driven in process through ProcessInput.
///
/// test/uci_test.sh drives the built engine as a GUI does; this suite drives the same command table
/// (input_handling.h) and Game (game.h) inside one process, where it can read every line the engine writes and
/// reach the paths a short GUI session never takes: every `setoption`, every form of `position`, `go` with each
/// of its parameters (depth, nodes, movetime, the clock and increments for either side, movestogo, infinite,
/// ponder with ponderhit and stop, searchmoves), the diagnostic commands (eval, nnue, getboard, bookmoves,
/// freebook, dbg, dbgclear, help, bench), malformed and unknown input, the root search's short cuts (a book
/// move, a forced move, no legal move) and the net loader's refusals (a missing file, an unstamped one, a
/// different architecture, a truncated one).
///
/// std::cout is swapped for a locked string buffer while a command runs, so the search thread's `info` and
/// `bestmove` lines are captured too; a search is always joined (StopThinking) before the buffer is read for the
/// last time and std::cout is given back, so no thread writes to it afterwards. The checks only assert what the
/// protocol promises (a bestmove, a legal one, from the restricted list, ...), never a particular search result,
/// so the suite is independent of the net's choices and of the machine's speed.
///
/// Usage:  test_uci_commands <net.bin>   (run from the repository root, where pawnstar.book is)
///   With no argument the suite is skipped, as the other net-dependent suites are.

#include "game.h"
#include "input_handling.h"
#include "test_report.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <mutex>
#include <sstream>
#include <streambuf>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace
{
using namespace std::chrono_literals;

/// @brief A stream buffer that appends everything written to it to a string under a mutex, so the test thread can
/// read what the search thread has written so far while it is still writing.
class LockedStringBuffer : public std::streambuf
{
  public:
    /// @brief Everything written so far.
    std::string Text() const
    {
        std::lock_guard lock{mutex_};
        return text_;
    }

  protected:
    int_type overflow(int_type character) override
    {
        if (!traits_type::eq_int_type(character, traits_type::eof()))
        {
            std::lock_guard lock{mutex_};
            text_.push_back(traits_type::to_char_type(character));
        }
        return traits_type::not_eof(character);
    }

    std::streamsize xsputn(const char *characters, std::streamsize count) override
    {
        std::lock_guard lock{mutex_};
        text_.append(characters, static_cast<std::size_t>(count));
        return count;
    }

  private:
    mutable std::mutex mutex_; ///< Guards text_.
    std::string        text_;  ///< The captured output.
};

/// @brief While alive, std::cout writes into a LockedStringBuffer; the destructor gives std::cout its own buffer back.
class CapturedOutput
{
  public:
    CapturedOutput() : saved_{std::cout.rdbuf(&buffer_)}
    {
    }

    ~CapturedOutput()
    {
        std::cout.rdbuf(saved_);
    }

    CapturedOutput(const CapturedOutput &)            = delete;
    CapturedOutput &operator=(const CapturedOutput &) = delete;

    /// @brief Everything written to std::cout since construction.
    std::string Text() const
    {
        return buffer_.Text();
    }

    /// @brief Wait until the captured text contains @p needle, or @p timeout passes.
    /// @return Whether the text appeared.
    bool WaitFor(std::string_view needle, std::chrono::milliseconds timeout) const
    {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (Text().find(needle) == std::string::npos)
        {
            if (std::chrono::steady_clock::now() > deadline)
            {
                return false;
            }
            std::this_thread::sleep_for(2ms);
        }
        return true;
    }

  private:
    LockedStringBuffer buffer_; ///< Where std::cout writes while captured.
    std::streambuf    *saved_;  ///< std::cout's own buffer, restored on destruction.
};

/// @brief Generous ceiling for a search the test waits on: instrumented and sanitized builds run several times
/// slower than a release build, and a loaded CI runner slower again.
constexpr std::chrono::milliseconds kSearchTimeout = 120s;

bool Contains(std::string_view text, std::string_view needle)
{
    return text.find(needle) != std::string_view::npos;
}

/// @brief The lines of @p text that start with @p prefix.
std::vector<std::string> LinesStartingWith(const std::string &text, std::string_view prefix)
{
    std::vector<std::string> lines;
    std::istringstream       stream{text};
    for (std::string line; std::getline(stream, line);)
    {
        if (line.starts_with(prefix))
        {
            lines.push_back(line);
        }
    }
    return lines;
}

/// @brief The move a `bestmove <move> [ponder <move>]` line names, or empty if @p text holds no bestmove.
std::string BestMove(const std::string &text)
{
    const auto lines = LinesStartingWith(text, "bestmove ");
    if (lines.empty())
    {
        return {};
    }
    std::istringstream stream{lines.back()};
    std::string        keyword, move;
    stream >> keyword >> move;
    return move;
}

/// @brief Whether @p move (UCI notation) was legal in the position the search started from and is the move the game
/// went on to: the search thread plays its bestmove into the game before it prints it.
bool IsLegal(Game &game, const std::string &move)
{
    if (game.positions_.size() < 2)
    {
        return false;
    }
    const Position &searched = game.positions_[game.positions_.size() - 2];
    for (const Move &candidate : searched.GenerateLegalMoves())
    {
        if (candidate.ToString() == move)
        {
            return searched.MakeMove(candidate).hash_ == game.CurrentPosition().hash_;
        }
    }
    return false;
}

/// @brief Run synchronous commands and return what they wrote.
std::string Run(Game &game, std::initializer_list<std::string_view> lines)
{
    CapturedOutput output;
    for (std::string_view line : lines)
    {
        ProcessInput(game, line);
    }
    return output.Text();
}

/// @brief Send a `go` line, wait for its bestmove, join the search and return everything it wrote.
/// @param is_complete Set to whether a bestmove arrived before the timeout.
std::string SearchToBestMove(Game &game, std::string_view go_line, bool &is_complete)
{
    CapturedOutput output;
    ProcessInput(game, go_line);
    is_complete = output.WaitFor("bestmove", kSearchTimeout);
    game.StopThinking();
    return output.Text();
}

/// @brief Send a `go` line, let the search run for @p run_time, send @p resolving_line (stop or ponderhit), wait for
/// the bestmove, join the search and return everything written.
std::string SearchThenSend(Game &game, std::string_view go_line, std::chrono::milliseconds run_time,
                           std::string_view resolving_line, bool &is_complete)
{
    CapturedOutput output;
    ProcessInput(game, go_line);
    std::this_thread::sleep_for(run_time);
    ProcessInput(game, resolving_line);
    is_complete = output.WaitFor("bestmove", kSearchTimeout);
    game.StopThinking();
    return output.Text();
}

/// @brief Set or (with an empty value) remove an environment variable for this process.
void SetEnvironment(const char *name, const char *value)
{
#if defined(_WIN32)
    _putenv_s(name, value); // an empty value removes it
#else
    if (value[0] == '\0')
    {
        unsetenv(name);
    }
    else
    {
        setenv(name, value, 1);
    }
#endif
}

/// @brief Write @p bytes to a file in the temporary directory and return its path.
std::string WriteTempFile(const std::string &name, const std::string &bytes)
{
    const std::filesystem::path path = std::filesystem::temp_directory_path() / name;
    std::ofstream               out{path, std::ios::binary};
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    return path.string();
}

std::string ReadFile(const std::string &path)
{
    std::ifstream in{path, std::ios::binary};
    return std::string{std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
}

void Check(bool condition, const std::string &message)
{
    test_report::Check(condition, message);
}

/// @brief Check, and on a failure print what the engine wrote, for the log.
void Check(bool condition, const std::string &message, const std::string &engine_output)
{
    if (!test_report::Check(condition, message))
    {
        std::cout << "  engine output was:\n" << engine_output << "  (end of engine output)\n";
    }
}

// ---- The scenarios -------------------------------------------------------------------------------------------

void HandshakesAndDiagnostics(Game &game)
{
    const std::string uci = Run(game, {"uci"});
    Check(Contains(uci, "id name Pawnstar ") && Contains(uci, "id author ") && uci.ends_with("uciok\n"),
          "uci: identifies the engine and ends with uciok");
    for (const char *option : {"Hash", "Threads", "Ponder", "EvalFile", "Clear Hash", "Move Overhead", "OwnBook"})
    {
        Check(Contains(uci, std::format("option name {} type", option)), std::format("uci: advertises {}", option));
    }
    Check(Run(game, {"isready"}) == "readyok\n", "isready: readyok");
    Check(Run(game, {"", "   ", "xyzzy plugh", "Go depth 1"}).empty(),
          "blank lines, unknown and wrongly-cased commands are ignored silently");

    const std::string help                = Run(game, {"help"});
    bool              lists_every_command = Contains(help, "Available commands:");
    for (const Handler &handler : handlers)
    {
        lists_every_command = lists_every_command && Contains(help, handler.name_);
    }
    Check(lists_every_command, "help: lists every command in the table");

    const std::string dbg = Run(game, {"dbgclear", "dbg"});
#if DEBUGX
    Check(Contains(dbg, "DEBUGX") && Contains(dbg, "*****"), "dbgclear + dbg: prints the counter banner");
#else
    Check(dbg.empty(), "dbgclear + dbg: silent with the DEBUGX counters compiled out");
#endif

    const std::string eval = Run(game, {"position startpos", "eval", "nnue"});
    Check(Contains(eval, "evaluation ") && Contains(eval, "(nnue)"), "eval: prints the static evaluation");
    Check(Contains(eval, "\nnnue ") || eval.starts_with("nnue "), "nnue: prints the raw network evaluation");
}

void Positions(Game &game)
{
    constexpr std::string_view kStart = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    Check(Run(game, {"position startpos", "getboard"}) == std::format("{}\n", kStart),
          "position startpos + getboard: the start position's FEN");

    Check(Run(game, {"position startpos moves e2e4 e7e5 g1f3", "getboard"}) ==
              "rnbqkbnr/pppp1ppp/8/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R b KQkq - 1 2\n",
          "position startpos moves ...: the moves are played in order");

    // FENs round-trip through getboard: castling subsets and none, an en-passant square, the clocks.
    for (const char *fen :
         {"r3k2r/8/8/8/8/8/8/R3K2R w Kq - 7 31", "4k3/8/8/8/8/8/8/4K3 b - - 0 60",
          "rnbqkbnr/ppp1pppp/8/8/3pP3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 3", "r3k2r/8/8/8/8/8/8/R3K2R b Qk - 0 1"})
    {
        Check(Run(game, {std::format("position fen {}", fen), "getboard"}) == std::format("{}\n", fen),
              std::format("position fen + getboard round trip: {}", fen));
    }

    Check(Run(game, {"position fen 4k3/8/8/8/8/8/8/R3K3 w Q - 0 1 moves e1c1", "getboard"}) ==
              "4k3/8/8/8/8/8/8/2KR4 b - - 0 1\n",
          "position fen <six fields> moves ...: castles queenside");
    Check(Run(game, {"position fen 4k3/8/8/8/8/8/8/4K2R w K moves e1g1", "getboard"}) ==
              "4k3/8/8/8/8/8/8/5RK1 b - - 0 1\n",
          "position fen <three fields> moves ...: a short FEN ended by the moves keyword");
    Check(Run(game, {"position fen 4k3/8/8/8/8/8/8/4K2R w K", "getboard"}) == "4k3/8/8/8/8/8/8/4K2R w K - 0 1\n",
          "position fen <three fields>: a short FEN at the end of the line is still applied");
    Check(Run(game, {"position fen 4k3/P7/8/8/8/8/8/4K3 w - - 0 1 moves a7a8q", "getboard"}) ==
              "Q3k3/8/8/8/8/8/8/4K3 b - - 0 1\n",
          "position ... moves a7a8q: a promotion is played by its suffixed name");

    Run(game, {"position startpos moves e2e4"});
    Check(Run(game, {"position moves e7e5", "getboard"}) ==
              "rnbqkbnr/pppp1ppp/8/4p3/4P3/8/PPPP1PPP/RNBQKBNR w KQkq e6 0 2\n",
          "position moves ... with no source: the moves are applied to the current position");
    Check(Run(game, {"position startpos moves e2e5 e2e4", "getboard"}) ==
              "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1\n",
          "position ... moves: an illegal move is skipped and the rest are played");
    Check(Run(game, {"position fen garbage", "getboard"}) == std::format("{}\n", kStart),
          "position fen <garbage>: falls back to the start position");
    Check(Run(game, {"position fen 8/8/8/8/8/8/8/8 w - - 0 1", "getboard"}) == std::format("{}\n", kStart),
          "position fen <no kings>: falls back to the start position");
    Run(game, {"position fen 4k3/8/8/8/8/8/8/4K3 w - - 0 1"});
    Check(Run(game, {"position nonsense", "getboard"}) == "4k3/8/8/8/8/8/8/4K3 w - - 0 1\n",
          "position <unknown source>: leaves the position alone");

    // Game's own move stack: a null move and its undo leave the position as it was.
    Run(game, {"position startpos"});
    const std::size_t depth_before = game.positions_.size();
    game.MakeNullMove();
    Check(game.positions_.size() == depth_before + 1 && game.CurrentPosition().color_to_move_ == kBlack,
          "Game::MakeNullMove passes the move to the other side");
    game.UndoMove();
    const Game &const_game = game;
    Check(game.positions_.size() == depth_before && const_game.CurrentPosition().ToString() == kStart,
          "Game::UndoMove restores the previous position");
    Check(game.PlayMove(std::string_view{"e2e5"}) == Move::None(), "Game::PlayMove(\"e2e5\") refuses an illegal move");
}

void Options(Game &game)
{
    Run(game, {"setoption name Threads value 3"});
    Check(game.thread_count_ == 3, "setoption Threads 3");
    Run(game, {"setoption name Threads value 0"});
    Check(game.thread_count_ == 1, "setoption Threads 0 clamps to 1");
    Run(game, {"setoption name Threads value 100000"});
    Check(game.thread_count_ == kMaxSearchThreads, "setoption Threads 100000 clamps to the maximum");
    Run(game, {"setoption name Threads value 1"});

    Run(game, {"setoption name Move Overhead value 120"});
    Check(game.move_overhead_ == ChessClock::Duration{120}, "setoption Move Overhead 120 (a two-word name)");
    Run(game, {"setoption name Move Overhead value 99999"});
    Check(game.move_overhead_ == ChessClock::Duration{5000}, "setoption Move Overhead clamps to 5000");
    Run(game, {"setoption name Move Overhead value 30"});

    Run(game, {"setoption name OwnBook value false"});
    Check(!game.use_own_book_, "setoption OwnBook false");
    Run(game, {"setoption name OwnBook value true"});
    Check(game.use_own_book_, "setoption OwnBook true");

    Run(game, {"setoption name Hash value 0"});
    Check(game.transposition_table_.UsageStats().first == 0, "setoption Hash 0 clamps to 1 MB, empty");
    Run(game, {"setoption name Hash value 16"});
    // Put something in the table, then press the Clear Hash button.
    Run(game, {"setoption name OwnBook value false", "position startpos"});
    bool is_complete = false;
    SearchToBestMove(game, "go depth 4", is_complete);
    const std::size_t filled = game.transposition_table_.UsageStats().first;
    Run(game, {"setoption name Clear Hash"});
    Check(filled > 0 && game.transposition_table_.UsageStats().first == 0,
          "setoption Clear Hash empties a filled transposition table");

    const int threads_before = game.thread_count_;
    Run(game, {"setoption name NoSuchOption value 7", "setoption", "setoption value 3 name", "setoption Threads 5"});
    Check(game.thread_count_ == threads_before, "setoption: unknown names and malformed lines change nothing");
}

void NetLoading(Game &game, const std::string &net_path)
{
    Run(game, {"setoption name EvalFile value this/file/does/not/exist.bin"});
    Check(!game.nnue_network_.loaded_, "setoption EvalFile <missing file>: refused, no net loaded");
    const std::string without_net = Run(game, {"position startpos", "eval", "nnue"});
    Check(Contains(without_net, "info string no NNUE net loaded"), "eval without a net: says so");
    Check(Contains(without_net, "info string NNUE: no net loaded"), "nnue without a net: says so");

    const std::string net_bytes = ReadFile(net_path);
    auto              scratch   = std::make_unique<nnue::Network>(); // megabytes of weights: never on the stack
    Check(scratch->LoadFromMemory(reinterpret_cast<const std::uint8_t *>(net_bytes.data()), net_bytes.size(), "copy"),
          "Network::LoadFromMemory accepts the shipped net's bytes");

    const std::string unstamped = WriteTempFile("pawnstar_uci_unstamped.bin", "not a net, just some bytes here....");
    Run(game, {std::format("setoption name EvalFile value {}", unstamped)});
    Check(!game.nnue_network_.loaded_, "setoption EvalFile <unstamped file>: refused");

    std::string other_architecture = net_bytes.substr(0, 4096);
    other_architecture[6] ^= 0x01; // input_size_, the field after the magic and the format version
    const std::string incompatible = WriteTempFile("pawnstar_uci_incompatible.bin", other_architecture);
    Run(game, {std::format("setoption name EvalFile value {}", incompatible)});
    Check(!game.nnue_network_.loaded_, "setoption EvalFile <another architecture>: refused");

    const std::string truncated = WriteTempFile("pawnstar_uci_truncated.bin", net_bytes.substr(0, 4096));
    Run(game, {std::format("setoption name EvalFile value {}", truncated)});
    Check(!game.nnue_network_.loaded_, "setoption EvalFile <truncated file>: refused");

    Run(game, {std::format("setoption name EvalFile value {}", net_path)});
    Check(game.nnue_network_.loaded_, "setoption EvalFile <the shipped net>: loaded again");
    Check(Contains(Run(game, {"eval"}), "evaluation "), "eval with the net reloaded");

    for (const std::string &path : {unstamped, incompatible, truncated})
    {
        std::remove(path.c_str());
    }
}

void Searches(Game &game)
{
    bool is_complete = false;
    Run(game, {"setoption name OwnBook value false", "setoption name Threads value 1", "ucinewgame"});

    std::string out = SearchToBestMove(game, "go depth 5", is_complete);
    Check(is_complete && IsLegal(game, BestMove(out)) && Contains(out, "info depth"),
          "go depth 5: info lines (iterations past the shallow depth 3), then a legal bestmove", out);

    // Each search plays its bestmove into the game, so every search below sets its position first.
    Run(game, {"position startpos"});
    out = SearchToBestMove(game, "go nodes 3000", is_complete);
    Check(is_complete && IsLegal(game, BestMove(out)), "go nodes 3000: stops at the node budget with a bestmove", out);

    Run(game, {"position startpos"});
    out = SearchToBestMove(game, "go movetime 150", is_complete);
    Check(is_complete && IsLegal(game, BestMove(out)), "go movetime 150: a bestmove within the fixed time", out);

    Run(game, {"position startpos"});
    out = SearchToBestMove(game, "go wtime 3000 btime 1000 winc 20 binc 0 movestogo 12", is_complete);
    Check(is_complete && IsLegal(game, BestMove(out)), "go wtime/btime/winc/binc/movestogo (white to move)", out);

    Run(game, {"position startpos moves e2e4"});
    out = SearchToBestMove(game, "go wtime 1000 btime 4000 winc 0 binc 30", is_complete);
    Check(is_complete && IsLegal(game, BestMove(out)), "go wtime/btime/winc/binc (black to move, sudden death)", out);

    Run(game, {"position startpos"});
    out = SearchThenSend(game, "go infinite", 300ms, "stop", is_complete);
    Check(is_complete && IsLegal(game, BestMove(out)), "go infinite + stop: stop ends it with a bestmove", out);

    Run(game, {"position startpos"});
    out = SearchThenSend(game, "go ponder wtime 4000 btime 4000", 300ms, "ponderhit", is_complete);
    Check(is_complete && IsLegal(game, BestMove(out)), "go ponder + ponderhit: the ponder search becomes timed", out);

    // A ponder search that finishes on its own must hold its bestmove until the GUI resolves the ponder.
    {
        Run(game, {"position startpos"});
        CapturedOutput output;
        ProcessInput(game, "go ponder depth 2");
        std::this_thread::sleep_for(500ms);
        const bool is_held = !Contains(output.Text(), "bestmove");
        ProcessInput(game, "stop");
        is_complete = output.WaitFor("bestmove", kSearchTimeout);
        game.StopThinking();
        out = output.Text();
        Check(is_held && is_complete, "go ponder depth 2: the finished search waits for stop before its bestmove", out);
    }

    Run(game, {"position startpos"});
    out = SearchToBestMove(game, "go searchmoves h2h3 a2a3 depth 4", is_complete);
    Check(is_complete && (BestMove(out) == "h2h3" || BestMove(out) == "a2a3"),
          "go searchmoves h2h3 a2a3 depth 4: the bestmove is from the list (a keyword ends it)", out);
    Run(game, {"position startpos"});
    out = SearchToBestMove(game, "go depth 3 searchmoves b1a3", is_complete);
    Check(is_complete && BestMove(out) == "b1a3", "go depth 3 searchmoves b1a3: a single searched move", out);
    Run(game, {"position startpos"});
    out = SearchToBestMove(game, "go searchmoves z9z9 depth 2", is_complete);
    Check(is_complete && IsLegal(game, BestMove(out)), "go searchmoves <no legal move>: the list is ignored", out);
    Run(game, {"position startpos"});
    out = SearchToBestMove(game, "go depth 2 frobnicate 7", is_complete);
    Check(is_complete && IsLegal(game, BestMove(out)), "go with an unknown token: the token is ignored", out);

    // Lazy SMP: helper threads alongside the main thread.
    Run(game, {"setoption name Threads value 3", "position startpos"});
    out = SearchToBestMove(game, "go depth 7", is_complete);
    Check(is_complete && IsLegal(game, BestMove(out)), "Threads 3, go depth 7: helpers run and the main thread answers",
          out);
    Run(game, {"setoption name Threads value 1"});

    // A game history behind the root (the search seeds its repetition stack from it).
    Run(game, {"position startpos moves g1f3 g8f6 f3g1 f6g8 g1f3"});
    out = SearchToBestMove(game, "go depth 4", is_complete);
    Check(is_complete && IsLegal(game, BestMove(out)), "go after a move history with a repetition", out);
}

void RootShortcuts(Game &game, const std::string &book_path)
{
    bool is_complete = false;

    // A book move: returned at once, with no search and so no ponder move.
    Check(game.book_.Initialize(book_path), "the shipped opening book loads");
    Run(game, {"setoption name OwnBook value true", "position startpos"});
    std::string out = SearchToBestMove(game, "go depth 20", is_complete);
    Check(is_complete && IsLegal(game, BestMove(out)) && !Contains(out, "info depth") && !Contains(out, " ponder "),
          "OwnBook true at the start: a book move at once, no search, no ponder move", out);
    Run(game, {"setoption name OwnBook value false"});

    // A forced move: the only legal move is played without a search.
    Run(game, {"position fen k7/8/1K6/8/8/8/8/7R b - - 0 1"});
    out = SearchToBestMove(game, "go depth 20", is_complete);
    Check(is_complete && BestMove(out) == "a8b8" && !Contains(out, " ponder ") && !Contains(out, "info depth"),
          "a single legal move: bestmove a8b8 at once, with no search", out);

    // No legal move (checkmated): the search ends without a bestmove.
    {
        CapturedOutput output;
        ProcessInput(game, "position fen k7/1Q6/1K6/8/8/8/8/8 b - - 0 1");
        ProcessInput(game, "go depth 5");
        game.StopThinking();
        Check(!Contains(output.Text(), "bestmove"), "checkmated at the root: no bestmove");
    }

    // Mate scores either way round, and a stalemating move inside the tree.
    Run(game, {"position fen k7/6p1/1K6/8/8/8/8/7R b - - 0 1"});
    out = SearchToBestMove(game, "go depth 4", is_complete);
    Check(is_complete && Contains(out, "score mate -"), "being mated: the score is reported as a negative mate", out);
    Run(game, {"position fen k7/8/1K6/8/8/8/8/7R w - - 0 1"});
    out = SearchToBestMove(game, "go depth 4", is_complete);
    Check(is_complete && BestMove(out) == "h1h8" && Contains(out, "score mate  1"), "mate in one: h1h8, score mate 1",
          out);
    Run(game, {"position fen k7/8/1K6/8/8/8/8/2Q5 w - - 0 1"});
    out = SearchToBestMove(game, "go depth 3", is_complete);
    Check(is_complete && BestMove(out) != "c1c7", "a stalemating move is searched and not chosen", out);

    // Insufficient material: every leaf is a draw.
    Run(game, {"position fen 8/8/4k3/8/8/2N1K3/8/8 w - - 0 1"});
    out = SearchToBestMove(game, "go depth 4", is_complete);
    Check(is_complete && Contains(out, "score cp    0"), "king and knight against king: scored as a draw", out);
}

void BookCommands(Game &game)
{
    const std::string book_path = WriteTempFile("pawnstar_uci_book.book", "e2e4 e7e5 f2f4? e5f4\ne2e4 e7e5 g1f3\n");
    game.book_.Free();
    Check(game.book_.Initialize(book_path), "a small book with a questionable move loads");
    const std::string at_start = Run(game, {"position startpos", "bookmoves"});
    Check(Contains(at_start, "MOVE   COUNT") && Contains(at_start, "e2e4") && Contains(at_start, "  2"),
          "bookmoves at the start: e2e4, seen twice");
    const std::string after_e5 = Run(game, {"position startpos moves e2e4 e7e5", "bookmoves"});
    Check(Contains(after_e5, "g1f3") && Contains(after_e5, "f2f4?") && Contains(after_e5, "questionable"),
          "bookmoves after 1.e4 e5: g1f3, and f2f4? flagged questionable");
    Check(Run(game, {"position fen 4k3/8/8/8/8/8/8/4K3 w - - 0 1", "bookmoves"}).empty(),
          "bookmoves out of the book: prints nothing");
    Check(Run(game, {"freebook", "position startpos", "bookmoves"}).empty(), "freebook: the book is gone");
    std::remove(book_path.c_str());
}

void Bench(Game &game)
{
    const std::string              plain = Run(game, {"bench 3"});
    const std::vector<std::string> lines = LinesStartingWith(plain, "");
    Check(!lines.empty() && Contains(lines.back(), " nodes ") && lines.back().ends_with(" nps"),
          "bench 3: ends with '<nodes> nodes <nps> nps'");
    SetEnvironment("PAWNSTAR_BENCH_DEBUG", "1");
    const std::string debug = Run(game, {"bench 2"});
    SetEnvironment("PAWNSTAR_BENCH_DEBUG", "");
    Check(Contains(debug, "### pos 01 bm=") && Contains(debug, "### pos 16 bm="),
          "PAWNSTAR_BENCH_DEBUG: each position's bestmove and node count");
#if DEBUGX
    Check(Contains(debug, "DEBUGX"), "PAWNSTAR_BENCH_DEBUG: each position's diagnostic counters");
#endif
    Check(game.thread_count_ == 1 && !game.use_own_book_, "bench restores Threads and OwnBook");
}

void DefaultThreads()
{
    SetEnvironment("PAWNSTAR_THREADS", "3");
    Check(Game::ComputeDefaultThreads() == 3, "PAWNSTAR_THREADS=3: three threads by default");
    SetEnvironment("PAWNSTAR_THREADS", "100000");
    Check(Game::ComputeDefaultThreads() == kMaxSearchThreads, "PAWNSTAR_THREADS=100000: clamped to the maximum");
    SetEnvironment("PAWNSTAR_THREADS", "0");
    const int hardware = Game::ComputeDefaultThreads();
    Check(hardware >= 1 && hardware <= kMaxSearchThreads, "PAWNSTAR_THREADS=0: ignored, the hardware's count");
    SetEnvironment("PAWNSTAR_THREADS", "");
}

void NewGame(Game &game)
{
    Run(game, {"position startpos moves e2e4"});
    bool is_complete = false;
    SearchToBestMove(game, "go depth 3", is_complete);
    Run(game, {"ucinewgame"});
    Check(game.transposition_table_.UsageStats().first == 0 && game.positions_.size() == 1 &&
              game.CurrentPosition().ToString() == "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
          "ucinewgame: an empty table and the start position");
}
} // namespace

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        return test_report::Summary("UCI COMMANDS: skipped (no net supplied)");
    }
    const std::string net_path = argv[1];
    auto              game_ptr = std::make_unique<Game>();
    Game             &game     = *game_ptr;
    if (!game.nnue_network_.Load(net_path))
    {
        test_report::Check(false, std::format("UCI COMMANDS: failed to load net '{}'", net_path));
        return test_report::ExitCode();
    }
    game.thread_count_ = 1;

    HandshakesAndDiagnostics(game);
    Positions(game);
    Options(game);
    NetLoading(game, net_path);
    Searches(game);
    RootShortcuts(game, "pawnstar.book");
    BookCommands(game);
    Bench(game);
    DefaultThreads();
    NewGame(game);

    return test_report::Summary("UCI COMMANDS: every command, option and go parameter through ProcessInput");
}

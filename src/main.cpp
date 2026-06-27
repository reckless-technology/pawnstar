#include "constants.h"
#include "debug_hashtable.h"
#include "game.h"
#include "input_handling.h"
#include "nnue.h"
#include "opening_book.h"
#include "position.h"
#include "transposition_table.h"
#include <memory>

#include <execinfo.h>
#include <filesystem>
#include <iostream>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <string_view>
#include <unistd.h>

namespace
{
/// @brief Locate a resource file shipped with the engine (the net or the opening book).
/// Tries the path as given (relative to the current working directory) first, then relative to the
/// running executable's directory and that directory's parent (so the engine finds nnue/ and the book
/// when launched from anywhere, e.g. build/pawnstar living one level below the repo-root resources).
/// Returns the first candidate that exists, or the original path if none do (so the caller's existing
/// not-found handling — and the embedded-net fallback — still applies).
/// @param relpath Resource path relative to the project root (e.g. "pawnstar.book").
/// @return A path that exists if one was found, otherwise @p relpath unchanged.
std::string LocateResource(const std::string &relpath)
{
    namespace fs = std::filesystem;
    std::error_code ec;
    if (fs::exists(relpath, ec))
    {
        return relpath;
    }
    const fs::path exe = fs::read_symlink("/proc/self/exe", ec);
    if (!ec)
    {
        const fs::path exe_dir = exe.parent_path();
        for (const fs::path &base : {exe_dir, exe_dir.parent_path()})
        {
            const fs::path candidate = base / relpath;
            if (fs::exists(candidate, ec))
            {
                return candidate.string();
            }
        }
    }
    return relpath;
}
} // namespace

// The shipped NNUE net embedded into the binary (see src/embedded_net.S), used as a fallback when the
// on-disk net cannot be loaded. Linked into the engine executable only (not the test binaries).
extern "C" const unsigned char pawnstar_embedded_net[];
extern "C" const unsigned char pawnstar_embedded_net_end[];

// The shipped opening book embedded into the binary (see src/embedded_book.S), used as a fallback when the
// on-disk pawnstar.book cannot be loaded. Linked into the engine executable only (not the test binaries).
extern "C" const char pawnstar_embedded_book[];
extern "C" const char pawnstar_embedded_book_end[];

/// @brief Program entry point. Prints the banner, loads the book, then runs the UCI input loop.
/// @return Process exit code.
int main()
{
    setbuf(stdin, NULL);
    setbuf(stdout, NULL);
    std::cout << std::unitbuf;
    std::cout << "   .::.    \n"
                 "   _::_    \n"
                 " _/____\\_ \n"
                 " \\      / \n"
                 "  \\____/  \n"
                 "  (____)   \n"
                 "   |  |    \n"
                 "   |__|    \n"
                 "  /    \\  \n"
                 " (______)  \n"
                 "(________) \n"
                 "/________\\\n"
                 "Pawnstar: a UCI compatible chess engine\n"
                 "(C) Jonny Reckless 2009 - 2026\n"
                 "Compiled: " __DATE__ " " __TIME__ "\n";

    auto game_ptr = std::make_unique<Game>();

    Game &game = *game_ptr;
    // Load the opening book from the working directory; if the file can't be loaded (wrong cwd, missing/
    // renamed file), fall back to the copy embedded in the binary (src/embedded_book.S) so the engine always
    // has a book. The book is optional, so a failure of both is only a warning, not fatal.
    if (!game.book_.Initialize(LocateResource("pawnstar.book")))
    {
        const std::size_t embedded_size = static_cast<std::size_t>(pawnstar_embedded_book_end - pawnstar_embedded_book);
        if (game.book_.InitializeFromMemory(pawnstar_embedded_book, embedded_size))
        {
            std::cout << "info string book file load failed; using the embedded book (" << embedded_size << " bytes)\n";
        }
        else
        {
            std::cout << "info string Unable to open book file (and the embedded fallback failed).\n";
        }
    }
    // NNUE is the only evaluator, so a net is required. Load the shipped net from the working directory
    // (like the opening book); PAWNSTAR_EVALFILE=<path> overrides the path, and UCI `setoption name
    // EvalFile value <path>` loads a different net at runtime. If the on-disk net cannot be loaded (wrong
    // cwd, missing/renamed file), fall back to the copy embedded in the binary (src/embedded_net.S) so the
    // engine always has a working evaluator. Only if even that fails — which would mean the build is
    // corrupt — does the engine give up.
    const char       *eval_file = std::getenv("PAWNSTAR_EVALFILE");
    const std::string net_path  = eval_file ? eval_file : LocateResource("nnue/pawnstar-v12.bin");
    if (!game.nnue_network_.Load(net_path))
    {
        const std::size_t embedded_size = static_cast<std::size_t>(pawnstar_embedded_net_end - pawnstar_embedded_net);
        std::cout << "info string NNUE: file load failed; falling back to the embedded net (" << embedded_size
                  << " bytes)\n";
        if (!game.nnue_network_.LoadFromMemory(pawnstar_embedded_net, embedded_size, "embedded"))
        {
            std::cout << "info string FATAL: could not load the NNUE net from '" << net_path
                      << "' or the embedded fallback. The build may be corrupt.\n";
            return 1;
        }
    }
    DebugXClear();
    std::cout << "ready\n";
    for (std::string line; std::getline(std::cin, line);)
    {
        ProcessInput(game, line);
    }
}

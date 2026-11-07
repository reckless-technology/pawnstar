#pragma once
/// @file no_search_moves.h Which moves in a match's PGN were played with no search, and could they have been forced?
///
/// fastchess writes one comment per move, and a move the engine reported no search for reads
/// `{/0 0.000s, n=0, sd=0, nps=0}`. An engine that replies at once to the only legal move does that properly. A book
/// move does it too, and so does a move an engine played without reporting anything, which is why the count alone
/// says little: the first check on a match that was to be played with books off is whether each such move had any
/// alternative. That takes the board, so this replays every game, with the engine's own move generator and the SAN
/// reader in `lichess_extract.h`, rather than carrying a third copy of the chess rules or a Python package CI
/// does not install. It replaces `forced.py`, kept beside the Trebuchet 1.0 strength matches of 2026-11-07, which
/// needed python-chess on the machine that ran it.
///
/// Measured on those matches: Pawnstar 2.2.0 made 1,309 such moves in 4,000 games against Trebuchet 1.0.0, 1,242 of
/// them the only legal move and 67 not, with between 2 and 45 legal moves on the board; Trebuchet made none. The tool
/// reports that. It does not explain it.
///
/// `tools/pgn_nps.py` counts the same moves from their comments alone; this adds what the comments cannot say.
///
/// A move counts as played with no search when its comment states `n=0` or states no `n=` at all. `nps=0` is not
/// `n=0`: a searched move can round to it.
///
/// Usage: no_search_moves <match.pgn>... ; exits non-zero when a game is cut short or holds a move that is not legal
/// where it stands, because every position after such a move would be wrong.

#include "lichess_extract.h"

#include <cstdint>
#include <fstream>
#include <map>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

namespace no_search_moves
{
/// @brief What one engine did across every game it played in the files read.
struct EngineTally
{
    uint64_t                moves                = 0;     ///< Every move attributed to it.
    uint64_t                moves_without_search = 0;     ///< Of those, the moves with `n=0` or no `n=` at all.
    uint64_t                only_legal_moves     = 0;     ///< Of those, the moves that had no alternative.
    std::map<int, uint64_t> unforced_by_legal_move_count; ///< The rest, by how many legal moves were on the board.

    /// @brief The moves played with no search although another was legal. @return The count.
    uint64_t UnforcedMoves() const
    {
        return moves_without_search - only_legal_moves;
    }
};

/// @brief Everything read from the files, the games that could not be used counted rather than passed over.
struct ScanReport
{
    uint64_t                           files            = 0;
    uint64_t                           games            = 0;
    uint64_t                           games_incomplete = 0; ///< No result token: the file stops inside the game.
    uint64_t                           games_unresolved = 0; ///< A move is not legal where it stands.
    uint64_t                           games_nameless   = 0; ///< No `White` or `Black` tag to attribute moves to.
    std::map<std::string, EngineTally> engines;

    /// @brief Whether every game could be replayed to its end. @return true if so.
    bool IsWhole() const
    {
        return games_incomplete == 0 && games_unresolved == 0 && games_nameless == 0;
    }
};

/// @brief The node count a move comment states.
/// @param comment The comment, without its braces. @return The count, or nothing when it states none. `n=` must
/// start a word, so `nps=` and the like cannot be taken for it.
inline std::optional<uint64_t> NodeCountOfComment(std::string_view comment)
{
    const auto is_word_character = [](char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
    };

    for (std::size_t at = comment.find("n="); at != std::string_view::npos; at = comment.find("n=", at + 1))
    {
        if (at > 0 && is_word_character(comment[at - 1]))
        {
            continue;
        }

        std::size_t digit = at + 2;
        uint64_t    count = 0;

        while (digit < comment.size() && comment[digit] >= '0' && comment[digit] <= '9')
        {
            count = count * 10 + static_cast<uint64_t>(comment[digit] - '0');
            ++digit;
        }

        if (digit > at + 2)
        {
            return count;
        }
    }

    return std::nullopt;
}

/// @brief Whether a move comment says nothing was searched.
/// @param comment The comment, without its braces. @return true for `n=0` or no node count at all.
inline bool IsMoveWithoutSearch(std::string_view comment)
{
    const std::optional<uint64_t> count = NodeCountOfComment(comment);
    return !count.has_value() || *count == 0;
}

/// @brief Replay one game, adding what it held to the report.
/// @param game The game. @param report The report to add to. A game that stops short or holds an unresolvable move
/// is counted as such, and what it held up to that point stands: those positions were reached by legal moves.
inline void ScanGame(const lichess::PgnGame &game, ScanReport &report)
{
    ++report.games;

    if (!game.is_complete)
    {
        ++report.games_incomplete;
        return;
    }

    const std::string white{game.Tag("White")};
    const std::string black{game.Tag("Black")};

    if (white.empty() || black.empty())
    {
        ++report.games_nameless;
        return;
    }

    Position position = Position::FromString(game.HasTag("FEN") ? game.Tag("FEN") : lichess::kStartPositionFen);

    for (const lichess::PlyToken &ply : game.plies)
    {
        const Move move = lichess::ParseSanMove(position, ply.san);

        if (move == Move::None())
        {
            ++report.games_unresolved;
            return;
        }

        EngineTally &tally = report.engines[position.color_to_move_ == Color::kWhite ? white : black];
        ++tally.moves;

        if (IsMoveWithoutSearch(ply.comment))
        {
            ++tally.moves_without_search;
            const int legal_moves = static_cast<int>(position.GenerateLegalMoves().size());

            if (legal_moves == 1)
            {
                ++tally.only_legal_moves;
            }
            else
            {
                ++tally.unforced_by_legal_move_count[legal_moves];
            }
        }

        position = position.MakeMove(move);
    }
}

/// @brief Replay every game in a stream. @param in The PGN. @param report The report to add to.
inline void ScanStream(std::istream &in, ScanReport &report)
{
    lichess::PgnReader reader{in};
    lichess::PgnGame   game;

    while (reader.NextGame(game))
    {
        ScanGame(game, report);
    }
}

/// @brief Print the report. @param report What was read. @param out Where to print it.
inline void PrintReport(const ScanReport &report, std::ostream &out)
{
    out << "games read: " << report.games << " in " << report.files << " file(s)\n";

    if (report.games_incomplete > 0)
    {
        out << "games cut short: " << report.games_incomplete << "\n";
    }

    if (report.games_unresolved > 0)
    {
        out << "games abandoned at a move that is not legal where it stands: " << report.games_unresolved << "\n";
    }

    if (report.games_nameless > 0)
    {
        out << "games without White and Black tags: " << report.games_nameless << "\n";
    }

    for (const auto &[name, tally] : report.engines)
    {
        out << name << ": " << tally.moves << " moves, " << tally.moves_without_search
            << " with no search: " << tally.only_legal_moves << " the only legal move, " << tally.UnforcedMoves()
            << " NOT forced\n";

        if (!tally.unforced_by_legal_move_count.empty())
        {
            out << "  not forced, legal moves on the board x moves:";

            for (const auto &[legal_moves, count] : tally.unforced_by_legal_move_count)
            {
                out << " " << legal_moves << "x" << count;
            }

            out << "\n";
        }
    }
}

/// @brief The command line. @param paths The PGN files. @param out The report. @param error Diagnostics.
/// @return 0 when every game was replayed to its end, 1 when one was not, 2 on bad usage or an unreadable file.
inline int Run(const std::vector<std::string> &paths, std::ostream &out, std::ostream &error)
{
    if (paths.empty())
    {
        error << "usage: no_search_moves <match.pgn>...\n";
        return 2;
    }

    ScanReport report;

    for (const std::string &path : paths)
    {
        std::ifstream file{path, std::ios::binary};

        if (!file)
        {
            error << "no_search_moves: cannot read " << path << "\n";
            return 2;
        }

        ++report.files;
        ScanStream(file, report);
    }

    PrintReport(report, out);
    return report.IsWhole() ? 0 : 1;
}
} // namespace no_search_moves

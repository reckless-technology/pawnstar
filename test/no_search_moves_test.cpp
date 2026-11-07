/// @file no_search_moves_test.cpp Unit tests of the no-search move scan (tools/no_search_moves.h).
///
/// The expected figures are worked by hand from the two games in test/fixtures/no_search_moves.pgn, with the legal
/// moves counted square by square and then again by python-chess, a route that shares no code with this engine:
///
///   Game 1, from k7/8/2K5/8/8/8/8/7R w, alpha White and beta Black:
///     1. Kc7  searched
///        Ka7  n=0, the only legal move: the king on c7 takes b7 and b8.
///     2. Rh8  searched, reporting nps=0, which must not be read as n=0
///        Ka6  n=0, the only legal move: the rook takes a8 and the king takes b8, b7 and b6.
///     3. Ra8+ n=0, NOT forced: 20 legal moves, the rook's 14 and the king's 6 (not b6 or b7, beside a6).
///        Kb5  n=0, the only legal move: the a-file is the rook's and the king takes b6 and b7.
///   Game 2, from a BLACK-to-move position, beta White and alpha Black, so attribution has to read the FEN and the
///   tags rather than assume White moves first or that a name keeps one colour:
///     1... Kh7 n=0, the only legal move: the rook takes the g-file and the king takes g7.
///     2. Kf7  searched
///        Kh8  `{book}`, no node count, NOT forced: 2 legal moves, h6 and h8.
///     3. Rh1# searched
///
///   alpha: 5 moves, 3 with no search (Ra8+, Kh7, Kh8), 1 the only legal move, 2 not forced: 20 legal x1, 2 legal x1.
///   beta:  5 moves, 3 with no search (Ka7, Ka6, Kb5), all three the only legal move.
///
/// test/fixtures/no_search_moves_broken.pgn holds one of each game the scan cannot use: a move that is not legal
/// where it stands, a game with no White tag, and a file that stops inside a game.
///
/// Usage:  test_no_search_moves   (run from the repository root, which is where ctest runs it)

#include "no_search_moves.h"

#include "test_report.h"

#include <sstream>
#include <string>
#include <vector>

namespace
{
const std::string kGoodFixture   = "test/fixtures/no_search_moves.pgn";
const std::string kBrokenFixture = "test/fixtures/no_search_moves_broken.pgn";

void TestReadingAComment()
{
    using no_search_moves::IsMoveWithoutSearch;
    using no_search_moves::NodeCountOfComment;

    test_report::Check(IsMoveWithoutSearch("/0 0.000s, n=0, sd=0, nps=0"), "comment: fastchess's own n=0 line");
    test_report::Check(IsMoveWithoutSearch("/0 0.081s, n=0, sd=0, nps=0"), "comment: n=0 with time on the clock");
    test_report::Check(IsMoveWithoutSearch("book"), "comment: a book comment states no node count");
    test_report::Check(IsMoveWithoutSearch(""), "comment: nor does no comment at all");
    test_report::Check(!IsMoveWithoutSearch("+0.10/5 0.010s, n=100, sd=5, nps=10000"), "comment: a searched move");
    test_report::Check(!IsMoveWithoutSearch("+0.10/5 0.010s, n=100, sd=5, nps=0"), "comment: nps=0 is not n=0");
    test_report::Check(!IsMoveWithoutSearch("+0.10/5 0.010s, n=05, sd=5"), "comment: n=05 is five nodes, not none");
    test_report::Check(!IsMoveWithoutSearch("min=0, n=7"), "comment: min=0 is not n=0, and n=7 still follows it");
    test_report::Check(IsMoveWithoutSearch("min=7"), "comment: min=7 is no node count");
    test_report::Check(NodeCountOfComment("a, n=594048, b").value_or(0) == 594048, "comment: the count is read whole");
}

void TestAttributionAndForcing()
{
    std::ostringstream output;
    std::ostringstream error;
    const int          status = no_search_moves::Run({kGoodFixture}, output, error);
    test_report::Check(status == 0, "good: both games replayed to their ends, exit 0: " + error.str());

    std::ifstream file{kGoodFixture, std::ios::binary};
    test_report::Check(file.good(), "good: the fixture is found from the repository root");

    no_search_moves::ScanReport report;
    no_search_moves::ScanStream(file, report);
    test_report::Check(report.games == 2 && report.IsWhole(), "good: two whole games");

    const no_search_moves::EngineTally &alpha = report.engines["alpha"];
    test_report::Check(alpha.moves == 5, "good: alpha made 5 moves");
    test_report::Check(alpha.moves_without_search == 3, "good: 3 of them with no search");
    test_report::Check(alpha.only_legal_moves == 1, "good: Kh7 the only one forced, so the FEN's side was read");
    test_report::Check(alpha.UnforcedMoves() == 2, "good: Ra8+ and the book move not forced");
    test_report::Check(alpha.unforced_by_legal_move_count.size() == 2 &&
                           alpha.unforced_by_legal_move_count.at(20) == 1 &&
                           alpha.unforced_by_legal_move_count.at(2) == 1,
                       "good: with 20 and 2 legal moves on the board");

    const no_search_moves::EngineTally &beta = report.engines["beta"];
    test_report::Check(beta.moves == 5, "good: beta made 5 moves");
    test_report::Check(beta.moves_without_search == 3, "good: 3 of them with no search, the searched nps=0 not among");
    test_report::Check(beta.only_legal_moves == 3, "good: all three forced");
    test_report::Check(beta.UnforcedMoves() == 0, "good: none not forced");

    const std::string printed = output.str();
    test_report::Check(printed.find("games read: 2 in 1 file(s)") != std::string::npos, "good: the count is printed");
    test_report::Check(printed.find("alpha: 5 moves, 3 with no search: 1 the only legal move, 2 NOT forced") !=
                           std::string::npos,
                       "good: alpha's line is printed");
    test_report::Check(printed.find("2x1 20x1") != std::string::npos, "good: and what it had to choose from");
}

void TestGamesThatCannotBeUsed()
{
    std::ostringstream output;
    std::ostringstream error;
    const int          status = no_search_moves::Run({kBrokenFixture}, output, error);
    test_report::Check(status == 1, "broken: exit 1, so a script cannot miss it");

    std::ifstream               file{kBrokenFixture, std::ios::binary};
    no_search_moves::ScanReport report;
    no_search_moves::ScanStream(file, report);
    test_report::Check(report.games == 3, "broken: three games read");
    test_report::Check(report.games_unresolved == 1, "broken: Qd4, with no queen on the board, abandons its game");
    test_report::Check(report.games_nameless == 1, "broken: a game with no White tag is not attributed");
    test_report::Check(report.games_incomplete == 1, "broken: a file ending inside a game is counted");
    test_report::Check(report.engines["alpha"].moves == 1 && report.engines["beta"].moves == 1,
                       "broken: what preceded the bad move stands, and nothing after it was counted");

    const std::string printed = output.str();
    test_report::Check(printed.find("not legal where it stands: 1") != std::string::npos &&
                           printed.find("cut short: 1") != std::string::npos &&
                           printed.find("without White and Black tags: 1") != std::string::npos,
                       "broken: each is printed");
}

void TestUsage()
{
    std::ostringstream output;
    std::ostringstream error;
    test_report::Check(no_search_moves::Run({}, output, error) == 2, "usage: no files is exit 2");
    test_report::Check(no_search_moves::Run({"test/fixtures/no-such-file.pgn"}, output, error) == 2,
                       "usage: an unreadable file is exit 2, not an empty report");
}
} // namespace

int main()
{
    TestReadingAComment();
    TestAttributionAndForcing();
    TestGamesThatCannotBeUsed();
    TestUsage();
    return test_report::Summary("no_search_moves: every check above");
}

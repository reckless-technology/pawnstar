/// @file principal_variation_test.cpp Correctness gate for the published principal variation.
///
/// `Game::principal_variation_` is the main search thread's PV, published by `SearchState::IterativeDeepen`
/// alongside `ponder_move_`. `tools/gen_data.cpp` harvests the first few plies of it as extra NNUE training
/// positions, each labelled with the root score, so three things have to hold and none of them is obvious:
///
///  1. The line is **playable**: every move in it is legal in the position it is reached at. A PV can be
///     spliced from a transposition-table move (`CopyVariation`'s callers in search_state.h), and nothing
///     re-checks such a move against the position, so a consumer must. This test asserts it for the lines
///     the engine actually produces.
///  2. Replaying it **corrupts nothing**: the game history and the NNUE accumulators are untouched, because
///     the replay happens on copies of `Position` (which is immutable - `MakeMove` returns a new one).
///  3. **What is recorded is what is evaluated**: a harvested position is written out as a FEN, so parsing
///     that FEN back must give a position the net evaluates to exactly the same score. If `Position::ToString`
///     and `Position::FromString` disagreed anywhere that NNUE reads (placements, side to move), every
///     harvested record would carry a label belonging to a slightly different position.
///
/// It also pins the two engine contracts the generator's own loop depends on: a root that returns without
/// searching (a forced move) publishes no PV and reports no nodes, and a second search through the same
/// `Game` is a real search again once the caller clears the cancel flag the previous search left set.
///
/// And it pins a third root contract, the one 2.2.0 broke: a cancel landing inside the root's depth-3 ordering
/// pass never decides the move. The move played is the best of the root moves whose search completed, checked
/// against the pass replayed by hand at every doubling node limit from 2048 until the pass completes, or, when the
/// cancel beat every move, the root's table move if it is legal there and the first move generated otherwise.
///
/// With no net argument it is a no-op so `make check` stays green when no net is present.
///
/// Usage:  test_principal_variation [net.bin]

#include "chess_clock.h"
#include "game.h"
#include "move.h"
#include "nnue.h"
#include "position.h"
#include "search_state.h"
#include "test_report.h"

#include <cstdint>
#include <format>
#include <memory>
#include <string>
#include <vector>

namespace
{
/// @brief Positions to search. A quiet opening, a tactical middlegame, an endgame and a mate-threat
/// position, so the PV is built through the full range of search behaviour (TT splices included).
constexpr const char *kFens[] = {
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
    "r1bqkbnr/pppp1ppp/2n5/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R b KQkq - 0 1",
    "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
    "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
    "6k1/5ppp/8/8/8/8/5PPP/R5K1 w - - 0 1",
};

/// @brief How deep the test searches. Deep enough for a multi-move PV, shallow enough to stay quick.
constexpr int kSearchDepth = 8;

/// @brief Convert a side-to-move-relative score to White's perspective, as a training record carries it.
/// @param side_to_move_score Score from the mover's point of view. @param color_to_move Side to move.
/// @return The same score from White's point of view.
constexpr int WhiteRelativeScore(int side_to_move_score, Color color_to_move)
{
    return color_to_move == kWhite ? side_to_move_score : -side_to_move_score;
}

/// @brief Prepare the game for one search: clear the cancel flag the previous search left set and fix the
/// depth. Exactly what tools/gen_data.cpp's PrepareSearch does, which is what makes this a test of it.
/// @param game Game to prepare.
void PrepareSearch(Game &game)
{
    game.is_cancel_pending_.store(false, std::memory_order_relaxed);
    game.time_control_.clock_type_ = ChessClock::kFixedDepth;
    game.time_control_.depth_      = kSearchDepth;
    game.time_control_.max_nodes_  = 0;
}

/// @brief Walk a published principal variation the way tools/gen_data.cpp harvests it, checking each ply.
/// @param game The game that produced the line (its history must come out unchanged).
/// @param root The position that was searched.
/// @param white_score The root score, White's perspective: the label every harvested ply carries.
/// @return The number of plies walked.
int CheckHarvest(const Game &game, const Position &root, int white_score)
{
    const nnue::Network &network          = game.nnue_network_;
    const std::size_t    history_size     = game.positions_.size();
    const zobrist_t      history_tip_hash = game.CurrentPosition().hash_;
    Position             position         = root;
    int                  plies_walked     = 0;

    for (const Move &played : game.principal_variation_)
    {
        bool     is_legal = false;
        MoveList legal    = position.GenerateLegalMoves();
        for (const Move &candidate : legal)
        {
            if (candidate == played)
            {
                is_legal = true;
                break;
            }
        }
        if (!test_report::Check(is_legal, std::format("pv ply {} move {} is legal in {}", plies_walked + 1,
                                                      played.ToString(), position.ToString())))
        {
            return plies_walked; // the rest of the line is unreachable from here
        }
        position = position.MakeMove(played);
        ++plies_walked;

        // What a record carries is the FEN, so parsing it back must reproduce the position the net saw.
        const std::string fen      = position.ToString();
        const Position    reparsed = Position::FromString(fen);
        const int         direct   = network.Evaluate(position);
        const int         from_fen = network.Evaluate(reparsed);
        if (direct != from_fen || reparsed.hash_ != position.hash_)
        {
            test_report::Check(false, std::format("pv ply {} fen {} re-evaluates as {} not {} (hash {} vs {})",
                                                  plies_walked, fen, from_fen, direct, reparsed.hash_, position.hash_));
        }

        // The label is White-relative, so it is the same number at every ply of the line whoever is to
        // move - and converting it to this position's mover and back must return it unchanged.
        const int as_mover = WhiteRelativeScore(white_score, position.color_to_move_);
        if (WhiteRelativeScore(as_mover, position.color_to_move_) != white_score)
        {
            test_report::Check(false, std::format("pv ply {} score {} does not round-trip through {}'s"
                                                  " perspective",
                                                  plies_walked, white_score,
                                                  position.color_to_move_ == kWhite ? "white" : "black"));
        }
    }

    // The replay ran entirely on copies, so the game history must be exactly as the search left it.
    if (game.positions_.size() != history_size || game.CurrentPosition().hash_ != history_tip_hash)
    {
        test_report::Check(false, "harvesting the pv changed the game history");
    }
    return plies_walked;
}

/// @brief The position in which 2.2.0 played an unscored move out of a cancelled ordering pass. Black's
/// promotion searches are large, so a cancel lands inside the root's depth-3 pass at every small node limit; at
/// 8+0.08 the hard deadline did, and the engine played an underpromotion with no `info` line.
constexpr const char *kOrderingPassFen = "3k3b/3P1KP1/8/5B2/7p/7P/1p6/8 b - - 0 92";

/// @brief What the root's ordering pass ought to leave as the move to play, worked out independently of
/// SearchRootNode by running the same depth-3 searches in the same order and stopping where the cancel lands.
struct OrderingPassReplay
{
    Move expected_move;        ///< the first best-scoring completed move, or the first generated if none completed
    int  completed_move_count; ///< root moves whose depth-3 search finished before the cancel
    int  root_move_count;      ///< legal root moves
    bool was_cancelled;        ///< whether the cancel landed inside the pass at all
    Move interrupted_move;     ///< the move being searched when the cancel landed (Move::None if it did not)
};

/// @brief Set up a node-limited search of kOrderingPassFen from a cold table, as `go nodes` does.
/// @param game Game to prepare. @param node_limit The `go nodes` budget (0 = none).
void PrepareOrderingPassSearch(Game &game, uint64_t node_limit)
{
    game.SetPosition(kOrderingPassFen); // also clears the cancel flag and resets the clock
    game.transposition_table_.Clear();
    game.time_control_.clock_type_ = ChessClock::kInfinite;
    game.time_control_.max_nodes_  = node_limit;
}

/// @brief Run the root's ordering pass by hand: each legal move searched to kStartDepth on one fresh
/// SearchState, in generation order, with the node limit armed exactly as SearchRootNode arms it.
/// @param game Game to search. @param node_limit The node budget.
/// @return Where the cancel landed and which move should be played as a result.
OrderingPassReplay ReplayOrderingPass(Game &game, uint64_t node_limit)
{
    PrepareOrderingPassSearch(game, node_limit);
    game.time_control_.StartSearch(ChessClock::Duration::zero(), ChessClock::Duration::zero());
    game.transposition_table_.Age();
    const MoveList     moves = game.CurrentPosition().GenerateLegalMoves();
    OrderingPassReplay replay{moves[0], 0, (int)moves.size(), false, Move::None()};
    SearchState        state{game};
    Variation          pv{};
    int                best_score = 0;
    for (int i = 0; i < (int)moves.size(); ++i)
    {
        const int score = state.SearchSingleMove(kStartDepth, 0, kAlpha, kBeta, moves[i], pv, i).score();
        if (state.IsCancelled())
        {
            replay.was_cancelled    = true;
            replay.interrupted_move = moves[i];
            break;
        }
        // Strictly greater, so the first of equal scores wins, as the stable sort leaves it.
        if (replay.completed_move_count == 0 || score > best_score)
        {
            best_score           = score;
            replay.expected_move = moves[i];
        }
        ++replay.completed_move_count;
    }
    return replay;
}

/// @brief A cancel that lands inside the root's ordering pass must not decide the move. The move it
/// interrupts carries the negated cancel sentinel, which the 23-bit score field truncates to a huge positive
/// score; 2.2.0 sorted that move first and played it. The move played must be the best of the moves whose
/// search completed or, when none did, the root's table move if it is legal here, else the first generated.
/// @param game Game to search (one thread).
void CheckCancelledOrderingPass(Game &game)
{
    // A node limit lands where Search polls it, every 2048 nodes, so double from there until the pass completes.
    // With the shipped net the first move's depth-3 search is under 2048 nodes, so no node limit lands before a
    // move has completed (the synthetic net's first limit does); that case is pinned below by a cancel set
    // before the search starts, which reaches it with any net.
    int landed_after_a_completed_move = 0; // limits that cancel the pass part-way, with at least one move scored
    int interrupted_move_would_play   = 0; // of those, the ones where the interrupted move is not the answer
    for (uint64_t node_limit = 2048; node_limit <= (uint64_t{1} << 24); node_limit *= 2)
    {
        const OrderingPassReplay replay = ReplayOrderingPass(game, node_limit);
        if (!replay.was_cancelled)
        {
            break; // the pass completed within this budget; iterative deepening decides from here on
        }
        PrepareOrderingPassSearch(game, node_limit);
        const Move played = game.SearchRootNode();
        test_report::Check(
            played == replay.expected_move,
            std::format("nodes {}: cancelled in the ordering pass after {} of {} moves, interrupting {}; plays {}, "
                        "expected {}",
                        node_limit, replay.completed_move_count, replay.root_move_count,
                        replay.interrupted_move.ToString(), played.ToString(), replay.expected_move.ToString()));
        if (replay.completed_move_count != 0)
        {
            ++landed_after_a_completed_move;
            interrupted_move_would_play += replay.interrupted_move != replay.expected_move ? 1 : 0;
        }
    }
    // Without these the loop above could pass having tested nothing: a limit must land part-way through the
    // pass, and at least one must be a case the old code (which played the interrupted move) got wrong.
    test_report::Check(landed_after_a_completed_move != 0,
                       "a node limit cancels the ordering pass after a move has completed");
    test_report::Check(interrupted_move_would_play != 0,
                       "a node limit interrupts a move other than the one the completed moves choose");

    // Cancelled before the search begins (a `stop` that beats the worker to the pass): no move is scored, so
    // the move is the root's table move when it is one of the legal moves, and the first generated otherwise.
    const MoveList  root_moves = Position::FromString(kOrderingPassFen).GenerateLegalMoves();
    const Move      last_move  = root_moves[root_moves.size() - 1];
    const zobrist_t root_hash  = Position::FromString(kOrderingPassFen).hash_;

    PrepareOrderingPassSearch(game, 0);
    game.is_cancel_pending_.store(true, std::memory_order_relaxed);
    Move played = game.SearchRootNode();
    test_report::Check(played == root_moves[0], std::format("cancelled before the pass with a cold table: plays the "
                                                            "first generated move {} ({})",
                                                            root_moves[0].ToString(), played.ToString()));

    PrepareOrderingPassSearch(game, 0);
    game.transposition_table_.RecordTransposition(
        Transposition{root_hash, last_move, 0, kStartDepth, Transposition::NodeType::kPv});
    game.is_cancel_pending_.store(true, std::memory_order_relaxed);
    played = game.SearchRootNode();
    test_report::Check(played == last_move,
                       std::format("cancelled before the pass with a table move: plays the table move {} ({})",
                                   last_move.ToString(), played.ToString()));

    // A table move that is not legal here (as a hash collision would leave) is ignored: a White move from the
    // start position.
    const Move foreign_move =
        Position::FromString("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1").GenerateLegalMoves()[0];
    PrepareOrderingPassSearch(game, 0);
    game.transposition_table_.RecordTransposition(
        Transposition{root_hash, foreign_move, 0, kStartDepth, Transposition::NodeType::kPv});
    game.is_cancel_pending_.store(true, std::memory_order_relaxed);
    played = game.SearchRootNode();
    test_report::Check(played == root_moves[0],
                       std::format("cancelled before the pass with an illegal table move {}: plays the first generated "
                                   "move {} ({})",
                                   foreign_move.ToString(), root_moves[0].ToString(), played.ToString()));

    // With helper threads, which are launched after the pass: they must still be wound up and joined.
    const int thread_count = game.thread_count_;
    game.thread_count_     = 4;
    PrepareOrderingPassSearch(game, 0);
    game.is_cancel_pending_.store(true, std::memory_order_relaxed);
    played             = game.SearchRootNode();
    game.thread_count_ = thread_count;
    test_report::Check(played == root_moves[0],
                       std::format("cancelled before the pass with four threads: the helpers are joined and the first "
                                   "generated move {} is played ({})",
                                   root_moves[0].ToString(), played.ToString()));
}
} // namespace

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        return test_report::Summary("principal variation: skipped (no net supplied)");
    }
    // The Game is heap-allocated: at eight king buckets its inline feature-weight array is ~12.6 MB and
    // would overflow the default stack as a local.
    auto  game_ptr = std::make_unique<Game>();
    Game &game     = *game_ptr;
    if (!game.nnue_network_.Load(argv[1]) || !game.nnue_network_.loaded_)
    {
        test_report::Check(false, std::format("principal variation: failed to load net '{}'", argv[1]));
        return test_report::ExitCode();
    }
    game.use_own_book_ = false; // no book is loaded here, but make the engine's own judgement explicit
    game.thread_count_ = 1;     // the PV is published by the main thread; one thread makes it deterministic

    int total_plies = 0;
    for (const char *fen : kFens)
    {
        game.SetPosition(fen);
        const Position root = game.CurrentPosition();
        PrepareSearch(game);
        const Move best = game.SearchRootNode();

        test_report::Check(best != Move::None(), std::format("search found a move in {}", fen));
        test_report::Check(game.last_search_node_count_ != 0, std::format("search reported nodes in {}", fen));
        if (!test_report::Check(!game.principal_variation_.empty(),
                                std::format("search published a principal variation in {}", fen)))
        {
            continue;
        }
        test_report::Check(game.principal_variation_[0] == best,
                           std::format("pv starts with the best move {} in {}", best.ToString(), fen));
        if (game.principal_variation_.size() >= 2)
        {
            test_report::Check(game.ponder_move_ == game.principal_variation_[1],
                               std::format("ponder move is pv[1] in {}", fen));
        }
        total_plies += CheckHarvest(game, root, WhiteRelativeScore(best.score(), root.color_to_move_));

        // A second search through the same Game, with only the cancel flag cleared and no SetPosition in
        // between: the generator plays a whole game that way, and if this came back cancelled it would
        // produce one labelled position per game instead of a hundred (which is what it used to do).
        // Only that it really searched is asserted, not what it found: the table is warm from the first
        // search, so the ordering differs and an equally good alternative is a legitimate answer.
        PrepareSearch(game);
        const Move again = game.SearchRootNode();
        test_report::Check(game.last_search_node_count_ != 0,
                           std::format("a second search through the same game really searches, in {}", fen));
        test_report::Check(again != Move::None() && !game.principal_variation_.empty(),
                           std::format("the second search publishes a move and a pv in {}", fen));
        total_plies += CheckHarvest(game, root, WhiteRelativeScore(again.score(), root.color_to_move_));
    }

    // A root that short-cuts without searching (here a single legal move: White must capture the checking
    // queen) must publish no PV and report no nodes. That node count is how tools/gen_data.cpp tells a real
    // label from a move the search never scored.
    game.SetPosition("k7/8/8/8/8/8/1q6/K7 w - - 0 1");
    test_report::Check(game.CurrentPosition().GenerateLegalMoves().size() == 1, "the forced-move fixture is forced");
    PrepareSearch(game);
    const Move forced = game.SearchRootNode();
    test_report::Check(forced != Move::None(), "the forced move is returned");
    test_report::Check(game.last_search_node_count_ == 0, "a forced move reports no nodes");
    test_report::Check(game.principal_variation_.empty(), "a forced move publishes no principal variation");

    CheckCancelledOrderingPass(game);

    return test_report::Summary(
        std::format("PRINCIPAL VARIATION: {} published pv plies replayed, evaluated and signed", total_plies));
}

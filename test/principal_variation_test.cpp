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

    return test_report::Summary(
        std::format("PRINCIPAL VARIATION: {} published pv plies replayed, evaluated and signed", total_plies));
}

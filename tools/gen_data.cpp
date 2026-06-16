/// @file gen_data.cpp Self-play training-data generator for the experimental NNUE evaluation.
///
/// Plays games against itself with the loaded NNUE net (PAWNSTAR_EVALFILE, default nnue/pawnstar-v6.bin)
/// at a fixed search depth and writes one record per quiet position:
///
///     <fen> | <white_relative_eval_cp> | <wdl>
///
/// where wdl is the eventual game result from White's perspective (1.0 win, 0.5 draw, 0.0 loss). These
/// records are converted to the bullet trainer's binary format by tools/convert_data.py (see nnue/README.md).
///
/// Usage:  gen_data <out_file> <num_games> <seed> [depth] [random_plies]
///
/// The search runs single-threaded (this driver forces PAWNSTAR_THREADS=1) so games are deterministic for a
/// given seed and cheap per process; run several processes in parallel across cores to scale throughput.

#include "chess_clock.h"
#include "constants.h"
#include "game.h"
#include "move.h"
#include "position.h"
#include "search.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <vector>

namespace
{
/// @brief A streambuf that discards everything, used to silence the engine's UCI info output.
class NullBuffer : public std::streambuf
{
  public:
    int overflow(int c) override
    {
        return c;
    }
};

/// @brief Positions above this |eval| (cp) are skipped as training targets (near-mate, unstable).
constexpr int kEvalCap = 3000;
/// @brief Adjudicate a win once one side's eval exceeds this (cp) for several consecutive plies.
constexpr int kResignThreshold = 1500;
/// @brief Consecutive decisive plies required to adjudicate.
constexpr int kResignPlies = 4;
/// @brief Hard cap on game length (plies) before adjudicating a draw, so self-play games terminate promptly
/// (the Game position history grows dynamically now, so this is a data-quality bound, not an overflow guard).
constexpr int kMaxPlies = 200;

/// @brief One recorded training sample (game result is filled in once the game ends).
struct Sample
{
    std::string fen;         ///< Position in FEN.
    int         white_score; ///< Search eval in centipawns, White's perspective.
};

/// @brief Whether the current position is terminal; if so set the White-relative result.
bool IsTerminal(Game &game, double &white_result)
{
    const Position &p = game.CurrentPosition();
    if (p.IsCheckmate())
    {
        // The side to move is mated, so the other side won.
        white_result = (p.ColorToMove() == kWhite) ? 0.0 : 1.0;
        return true;
    }
    if (p.IsStalemate() || p.IsDrawByMaterial() || game.IsDrawByFiftyMoves() || game.IsDrawByRepetition())
    {
        white_result = 0.5;
        return true;
    }
    return false;
}

/// @brief Play one self-play game, appending its quiet positions to out (with the final result baked in).
/// @param game A Game with an NNUE net already loaded; reset to the start position here and reused across games.
void PlayGame(Game &game, std::ofstream &out, std::mt19937_64 &rng, int depth, int random_plies)
{
    game.NewGame();
    game.time_control.clock_type = ChessClock::kFixedDepth;
    game.time_control.depth      = depth;

    // Random opening plies for diversity (not recorded).
    for (int i = 0; i < random_plies; ++i)
    {
        MoveList moves = game.CurrentPosition().GenerateLegalMoves();
        if (moves.size() == 0)
        {
            return; // dead opening line; abandon this game
        }
        game.PlayMove(moves[rng() % moves.size()]);
    }

    std::vector<Sample> samples;
    double              white_result   = 0.5;
    int                 decisive_plies = 0;
    int                 decisive_sign  = 0;
    int                 ply            = 0;

    for (;; ++ply)
    {
        if (IsTerminal(game, white_result))
        {
            break;
        }
        if (ply >= kMaxPlies)
        {
            white_result = 0.5;
            break;
        }
        const Position &p             = game.CurrentPosition();
        const bool      white_to_move = p.ColorToMove() == kWhite;

        const Move best        = SearchRootNode(game);
        const int  stm_score   = best.score();
        const int  white_score = white_to_move ? stm_score : -stm_score;

        // Record only quiet, in-bounds, not-in-check positions (clean eval targets for NNUE).
        if (!p.IsInCheck() && best.IsQuiet() && std::abs(white_score) < kEvalCap)
        {
            samples.push_back({p.ToString(), white_score});
        }

        // Resign-style adjudication to end decided games early.
        const int sign = stm_score > kResignThreshold ? (white_to_move ? 1 : -1)
                         : stm_score < -kResignThreshold ? (white_to_move ? -1 : 1)
                                                         : 0;
        if (sign != 0 && sign == decisive_sign)
        {
            if (++decisive_plies >= kResignPlies)
            {
                white_result = sign > 0 ? 1.0 : 0.0;
                break;
            }
        }
        else
        {
            decisive_sign  = sign;
            decisive_plies = sign != 0 ? 1 : 0;
        }

        game.PlayMove(best);
    }

    for (const Sample &s : samples)
    {
        out << s.fen << " | " << s.white_score << " | " << white_result << '\n';
    }
}
} // namespace

int main(int argc, char **argv)
{
    if (argc < 4)
    {
        std::cerr << "usage: " << argv[0] << " <out_file> <num_games> <seed> [depth] [random_plies]\n";
        return 1;
    }
    const std::string out_file     = argv[1];
    const int         num_games    = std::atoi(argv[2]);
    const uint64_t    seed         = std::strtoull(argv[3], nullptr, 10);
    const int         depth        = argc > 4 ? std::atoi(argv[4]) : 8;
    const int         random_plies = argc > 5 ? std::atoi(argv[5]) : 8;

    // Force a deterministic single-threaded search and silence UCI info output.
    setenv("PAWNSTAR_THREADS", "1", 1);

    // NNUE is the only evaluator, so self-play needs a loaded net. Load once and reuse across games.
    const char       *eval_file = std::getenv("PAWNSTAR_EVALFILE");
    const std::string net_path  = eval_file ? eval_file : "nnue/pawnstar-v6.bin";
    Game              game;
    if (!game.NnueNetwork().Load(net_path))
    {
        std::cerr << "gen_data: FATAL: could not load NNUE net '" << net_path
                  << "' (set PAWNSTAR_EVALFILE). NNUE is the only evaluator.\n";
        return 1;
    }

    NullBuffer    null_buffer;
    std::streambuf *old_cout = std::cout.rdbuf(&null_buffer);

    std::ofstream  out(out_file);
    std::mt19937_64 rng(seed);
    for (int g = 0; g < num_games; ++g)
    {
        PlayGame(game, out, rng, depth, random_plies);
        if (((g + 1) % 50) == 0)
        {
            std::cerr << "info string gen_data seed=" << seed << " game " << (g + 1) << '/' << num_games << '\n';
        }
    }
    out.flush();
    std::cout.rdbuf(old_cout);
    return 0;
}

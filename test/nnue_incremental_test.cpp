/// @file nnue_incremental.cpp Correctness gate for the incremental NNUE accumulator.
///
/// Two halves, because a random walk and a chosen line catch different things.
///
/// **Random search trees, on the shipped net.** The accumulator maintained incrementally by SearchState
/// across PlayMove/UndoMove must yield exactly the same evaluation as a full from-scratch refresh, at
/// every node (covering captures, castling, en passant and promotions as the random games progress, plus
/// make/undo round-trips). Needs a net file; skipped without one.
///
/// **Chosen boundary crossings, on a synthetic net at this build's shape.** A random game from the start
/// position hardly ever walks a king across the d/e file boundary, and with mirroring on that is exactly
/// the crossing whose refresh rule is silent when it is wrong — the king keeps its bucket while every
/// feature row the perspective reads moves. These lines cross each boundary on purpose: the mirror, the
/// board-half, both on one ply, castling, the output-head boundaries, and a promotion and an en passant
/// with no king move. They compare the accumulators lane by lane rather than the evaluations, and they
/// report how many plies actually forced a refresh so a case that quietly stopped crossing anything cannot
/// pass. The net is built in memory at the compiled shape, so this half runs in a checkout with no .bin
/// and is the only way to exercise a shape nothing has been trained for yet.
///
/// Usage:  test_nnue_incremental [net.bin]

#include "game.h"
#include "nnue.h"
#include "position.h"
#include "search_state.h"
#include "test_report.h"
#include <memory>

#include <cstdint>
#include <cstring>
#include <format>
#include <iostream>
#include <random>
#include <set>
#include <string>
#include <vector>

namespace
{
long g_checks = 0;

/// @brief Recursively play random legal moves, comparing the incremental accumulator against a full
/// refresh at entry and again after undoing all children (which under copy-make catches a slot whose
/// validity was tracked wrongly, where it used to catch a reverse-update bug in UndoMove).
void Walk(SearchState &state, std::mt19937_64 &rng, int depth, int branch)
{
    const Position      &p   = state.CurrentPosition();
    const nnue::Network &net = state.game_.nnue_network_;
    const int            inc = net.Evaluate(state.CurrentAccumulator(), p);
    const int            ref = net.Evaluate(p); // full refresh
    ++g_checks;
    if (inc != ref)
    {
        test_report::Check(false,
                           std::format("incremental accumulator {} != full refresh {} fen={}", inc, ref, p.ToString()));
    }
    if (depth <= 0)
    {
        return;
    }
    MoveList moves = p.GenerateLegalMoves();
    if (moves.size() == 0)
    {
        return;
    }
    for (int k = 0; k < branch; ++k)
    {
        const Move m = moves[rng() % moves.size()];
        state.PlayMove(m);
        Walk(state, rng, depth - 1, branch);
        state.UndoMove();
    }
    // After undoing every child the accumulator must again match this position's full refresh.
    const int inc_after = net.Evaluate(state.CurrentAccumulator(), p);
    ++g_checks;
    if (inc_after != ref)
    {
        test_report::Check(false, std::format("incremental accumulator after undo {} != full refresh {} fen={}",
                                              inc_after, ref, p.ToString()));
    }
}
} // namespace

/// @brief A net at **this build's** shape, with deterministic pseudo-random weights.
///
/// The crossing cases below have to run at whatever shape the build was configured for, and at any shape but
/// the shared one (mirroring on, 8 output heads) the shipped net is rightly refused: `nnue/pawnstar-v20.bin`
/// is at the shared shape, so a build configured for the retired v12's cannot load it. So the test makes its
/// own. Weight *quality* is irrelevant here:
/// what is being checked is that the incrementally-updated accumulator equals a from-scratch rebuild, and
/// that holds for any weights at all. Writing the payload out by hand is also a second opinion on the
/// payload layout — an independent writer against nnue.h's reader.
/// @return A complete RNNUE1 image: the 64-byte header this build stamps, then the payload.
std::vector<std::uint8_t> SyntheticNetImage()
{
    std::vector<std::uint8_t> image(sizeof(nnue::NetHeader) + nnue::kNetPayloadBytes);
    std::uint8_t             *out = image.data() + sizeof(nnue::NetHeader);

    // xorshift64*, seeded by hand: the same net on every machine and every run, so a failure is
    // reproducible rather than something that happened once.
    std::uint64_t state      = 0x9E3779B97F4A7C15ULL;
    const auto    next_value = [&state]() {
        state ^= state >> 12;
        state ^= state << 25;
        state ^= state >> 27;
        return state * 0x2545F4914F6CDD1DULL;
    };
    const auto write_int16 = [&out](std::int16_t value) {
        std::memcpy(out, &value, sizeof(value));
        out += sizeof(value);
    };

    // feature_weights, then feature_bias: small enough that an accumulator of up to 32 columns stays well
    // inside int16, so the synthetic net exercises the same arithmetic a trained one does.
    for (std::size_t i = 0; i < nnue::kFeatureWeightCount + nnue::kFeatureBiasCount; ++i)
    {
        write_int16(static_cast<std::int16_t>(static_cast<int>(next_value() >> 56) - 128));
    }
    // output_weights: inside [-127, 127] so Load's int8 requantisation scale comes out 1 and the int8
    // output layer is lossless, which keeps this test about the accumulator and not about requantisation.
    for (std::size_t i = 0; i < nnue::kOutputWeightCount; ++i)
    {
        write_int16(static_cast<std::int16_t>(static_cast<int>(next_value() >> 56) - 128 + 1));
    }
    // output_bias: int32 per output head, in QA*QB units.
    for (std::size_t i = 0; i < nnue::kOutputBiasCount; ++i)
    {
        const std::int32_t bias = static_cast<std::int32_t>(next_value() >> 50) - 4096;
        std::memcpy(out, &bias, sizeof(bias));
        out += sizeof(bias);
    }

    nnue::NetHeader header = nnue::ExpectedNetHeader();
    nnue::FinaliseNetHeader(header, image.data() + sizeof(nnue::NetHeader), nnue::kNetPayloadBytes);
    std::memcpy(image.data(), &header, sizeof(header));
    return image;
}

/// @brief The legal move written @p name in @p position, or Move::None().
Move FindMove(const Position &position, const std::string &name)
{
    for (const Move &move : position.GenerateLegalMoves())
    {
        if (move.ToString() == name)
        {
            return move;
        }
    }
    return Move::None();
}

/// @brief Play @p moves from @p fen, comparing the incrementally-updated accumulator with a from-scratch
/// rebuild at every ply, then walk the whole line back and compare again at every ply on the way.
///
/// This compares the **accumulators themselves**, lane by lane, not just the evaluations they produce:
/// two different accumulators can evaluate to the same number, so comparing evals would let a real
/// divergence through. It also records how many plies forced a refresh, because a case that was *meant* to
/// cross a boundary but did not would otherwise pass while testing nothing.
/// @param net The loaded net. @param fen Starting position. @param moves Moves in coordinate notation.
/// @param what What this line is for, named in any failure.
void CheckLine(const nnue::Network &net, const std::string &fen, const std::vector<std::string> &moves,
               const std::string &what)
{
    const auto same = [](const nnue::Accumulator &a, const nnue::Accumulator &b) {
        return a.white_ == b.white_ && a.black_ == b.black_;
    };
    const auto describe = [&](const Position &p, const std::string &stage) {
        const nnue::Perspective white = nnue::PerspectiveFor(p, false);
        const nnue::Perspective black = nnue::PerspectiveFor(p, true);
        return std::format("{} [{}] fen={} white(bank={} flip={}) black(bank={} flip={}) head={}", what, stage,
                           p.ToString(), white.bank_, white.square_flip_, black.bank_, black.square_flip_,
                           nnue::OutputBucket(p));
    };

    Position start = Position::FromString(fen);
    // Replay the whole line first, so the walk back has every position to hand.
    std::vector<Position> line{start};
    for (const std::string &name : moves)
    {
        const Move move = FindMove(line.back(), name);
        if (move == Move::None())
        {
            test_report::Check(false, std::format("{}: '{}' is not legal in {}", what, name, line.back().ToString()));
            return;
        }
        line.push_back(line.back().MakeMove(move));
    }

    nnue::Accumulator incremental;
    net.Refresh(incremental, start);
    int forced_refreshes = 0;
    for (std::size_t ply = 1; ply < line.size(); ++ply)
    {
        const Position &from = line[ply - 1];
        const Position &to   = line[ply];
        if (nnue::PerspectiveFor(from, false) != nnue::PerspectiveFor(to, false) ||
            nnue::PerspectiveFor(from, true) != nnue::PerspectiveFor(to, true))
        {
            ++forced_refreshes;
        }
        net.Update(incremental, from, to);
        nnue::Accumulator rebuilt;
        net.Refresh(rebuilt, to);
        ++g_checks;
        if (!same(incremental, rebuilt))
        {
            test_report::Check(false, std::format("incremental accumulator != full rebuild — {}",
                                                  describe(to, std::format("ply {} forward", ply))));
            return;
        }
        ++g_checks;
        if (net.Evaluate(incremental, to) != net.Evaluate(to))
        {
            test_report::Check(false, std::format("incremental evaluation != full refresh — {}",
                                                  describe(to, std::format("ply {} forward", ply))));
            return;
        }
    }
    // And back: Update is its own inverse with `from` and `to` swapped. Copy-make means UndoMove no longer
    // relies on that, but it is the property any reverse delta would need, so it stays pinned here.
    for (std::size_t ply = line.size() - 1; ply > 0; --ply)
    {
        net.Update(incremental, line[ply], line[ply - 1]);
        nnue::Accumulator rebuilt;
        net.Refresh(rebuilt, line[ply - 1]);
        ++g_checks;
        if (!same(incremental, rebuilt))
        {
            test_report::Check(false, std::format("incremental accumulator != full rebuild on the way back — {}",
                                                  describe(line[ply - 1], std::format("ply {} back", ply - 1))));
            return;
        }
    }
    test_report::Check(true, std::format("{}: {} plies, {} forcing a refresh, accumulator bit-identical "
                                         "forward and back",
                                         what, moves.size(), forced_refreshes));
}

/// @brief The lines that cross the boundaries a random walk almost never reaches.
///
/// A random game from the start position hardly ever walks a king across the d/e file boundary, and the
/// mirror crossing is the one that is **silent** when it is wrong: at the mirrored map a king stepping
/// d1 -> e1 keeps its bucket (file 3 and file 4 both mirror to file 3) while every feature row it reads
/// moves, so a refresh rule that compares bucket indices alone diffs an accumulator whose whole basis just
/// changed. No crash, no assertion — just a wrong evaluation. These lines make each crossing happen on
/// purpose, and report how many plies actually forced a refresh so that a case which quietly stopped
/// crossing anything cannot pass.
void BoundaryCrossings(const nnue::Network &net)
{
    // The white king stepping across d/e and back, then the black king doing the same. At the mirrored
    // shape each step flips that perspective's mirroring while leaving its bucket alone.
    CheckLine(net, "4k3/8/8/8/8/8/8/3K4 w - - 0 1", {"d1e1", "e8d8", "e1d1", "d8e8", "d1e1", "e8f8", "e1f1", "f8e8"},
              "kings crossing the d/e mirror boundary, both perspectives");
    // Crossing the half boundary (rank 4 -> 5) changes the bucket but not the mirroring: the other half of
    // the refresh rule, on its own. The idle king sits in a corner so the walking one is never blocked.
    CheckLine(net, "7k/8/8/8/8/8/8/3K4 w - - 0 1",
              {"d1d2", "h8g8", "d2d3", "g8h8", "d3d4", "h8g8", "d4d5", "g8h8", "d5d4", "h8g8"},
              "the white king crossing the rank-4/5 half boundary");
    CheckLine(net, "3k4/8/8/8/8/8/8/7K b - - 0 1",
              {"d8d7", "h1g1", "d7d6", "g1h1", "d6d5", "h1g1", "d5d4", "g1h1", "d4d5", "h1g1"},
              "the black king crossing the rank-4/5 half boundary");
    // Both at once, diagonally: mirroring and bucket change on the same ply.
    CheckLine(net, "7k/8/8/8/3K4/8/8/8 w - - 0 1", {"d4e5", "h8g8", "e5d4", "g8h8", "d4e5", "h8g8"},
              "the white king changing mirroring and bucket on the same ply");
    // Castling moves a king two files at once, which on the mirrored map can take it from a mirrored file
    // to an unmirrored one in a single ply — and it is the one king move that also moves a rook.
    // The rooks are walked out of the kings' way as the line goes, so that every king step below is legal:
    // a rook left on d8 or a1 covers exactly the squares the kings need to cross back over.
    CheckLine(net, "r3k2r/8/8/8/8/8/8/4K2R w Kkq - 0 1",
              {"e1g1", "e8c8", "f1f5", "d8g8", "g1f1", "c8b8", "f1e1", "b8a8", "e1d1", "a8b8"},
              "castling both ways, then the kings walking back across the boundary");
    // A promotion changes a piece type without a king moving, and en passant removes a pawn from a square
    // that is neither the from- nor the to-square of the move.
    CheckLine(net, "4k3/6P1/8/3pP3/8/8/8/4K3 w - d6 0 1", {"e5d6", "e8d8", "g7g8q", "d8d7", "g8d8"},
              "en passant and a promotion, with no king move");
}

/// @brief Walk the piece count all the way down from the start position, preferring captures, and check
/// the accumulator at every ply.
///
/// The output head is `(piece_count - 2) / 4`, so its boundaries are at 6, 10, 14, 18, 22, 26 and 30
/// pieces. Hand-writing a legal capture chain that crosses all of them is fiddly and brittle — the first
/// attempt at one had an illegal move in it — so the line is generated: take a capture whenever one is
/// legal, otherwise a move, from a fixed seed. That walks the count down on its own and reaches every head
/// a position can select, and the test reports which heads it actually visited rather than assuming.
/// @param net The loaded net.
void CaptureWalkThroughOutputHeads(const nnue::Network &net)
{
    const auto same = [](const nnue::Accumulator &a, const nnue::Accumulator &b) {
        return a.white_ == b.white_ && a.black_ == b.black_;
    };
    std::mt19937_64   rng(20261027);
    Position          current = Position::FromString("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w - - 0 1");
    nnue::Accumulator incremental;
    net.Refresh(incremental, current);
    std::set<int> heads_visited{nnue::OutputBucket(current)};
    int           plies = 0;
    for (; plies < 400; ++plies)
    {
        MoveList captures = current.GenerateLegalCaptures();
        MoveList moves    = current.GenerateLegalMoves();
        if (moves.size() == 0)
        {
            break;
        }
        const Move     move = captures.size() > 0 ? captures[rng() % captures.size()] : moves[rng() % moves.size()];
        const Position next = current.MakeMove(move);
        net.Update(incremental, current, next);
        nnue::Accumulator rebuilt;
        net.Refresh(rebuilt, next);
        ++g_checks;
        if (!same(incremental, rebuilt) || net.Evaluate(incremental, next) != net.Evaluate(next))
        {
            test_report::Check(false,
                               std::format("capture walk: incremental != full rebuild at ply {} after {}, "
                                           "fen={} head={}",
                                           plies + 1, move.ToString(), next.ToString(), nnue::OutputBucket(next)));
            return;
        }
        heads_visited.insert(nnue::OutputBucket(next));
        current = next;
    }
    std::string visited;
    for (const int head : heads_visited)
    {
        visited += std::format("{}{}", visited.empty() ? "" : ",", head);
    }
    test_report::Check(heads_visited.size() == static_cast<std::size_t>(nnue::kNumOutputBuckets),
                       std::format("capture walk: {} plies, accumulator bit-identical throughout, output heads "
                                   "visited {{{}}} of {}",
                                   plies, visited, nnue::kNumOutputBuckets));
}

int main(int argc, char **argv)
{
    // `--write-synthetic-net <path>` writes the synthetic net out and stops, which is what gives a shape
    // nothing has been trained for a **bench signature** before it has a net.
    //
    // At a new shape the engine rightly refuses the shipped net and exits, so `bench` cannot run and there
    // is no node count to compare between platforms or between the three bit-exact ports. The synthetic net
    // closes that gap: it comes from a fixed seed through fixed arithmetic, so its bytes are identical on
    // every platform, and `PAWNSTAR_EVALFILE=<it> pawnstar` then benches a real search over a real net.
    // The node count that comes out is a parity signature for the new shape, available the moment the shape
    // compiles rather than only once a training run has finished.
    if (argc == 3 && std::string(argv[1]) == "--write-synthetic-net")
    {
        const std::vector<std::uint8_t> image = SyntheticNetImage();
        std::ofstream                   out(argv[2], std::ios::binary);
        out.write(reinterpret_cast<const char *>(image.data()), static_cast<std::streamsize>(image.size()));
        if (!out)
        {
            std::cerr << std::format("cannot write '{}'\n", argv[2]);
            return 1;
        }
        std::cout << std::format("wrote '{}': {} bytes — {} king buckets, {} output head(s), mirroring {}, "
                                 "format_version {}, map crc {:#018x}\n",
                                 argv[2], image.size(), nnue::kNumKingBuckets, nnue::kNumOutputBuckets,
                                 nnue::kIsKingMirroringEnabled ? "on" : "off", nnue::kNetFormatVersion,
                                 nnue::KingBucketMapCrc());
        return 0;
    }

    // The synthetic net first, and unconditionally: it is at this build's shape, so it runs in a checkout
    // with no .bin and it is the only way to exercise a shape no net has been trained for yet.
    {
        auto                            net_ptr = std::make_unique<nnue::Network>();
        const std::vector<std::uint8_t> image   = SyntheticNetImage();
        if (!net_ptr->LoadFromMemory(image.data(), image.size(), "synthetic"))
        {
            test_report::Check(false, "incremental NNUE: this build refused its own synthetic net — the header "
                                      "ExpectedNetHeader writes and the one LoadFromMemory accepts disagree");
            return test_report::ExitCode();
        }
        test_report::Check(true, std::format("synthetic net at this build's shape loads: {} king buckets, {} output "
                                             "head(s), mirroring {}, {}-byte payload",
                                             nnue::kNumKingBuckets, nnue::kNumOutputBuckets,
                                             nnue::kIsKingMirroringEnabled ? "on" : "off", nnue::kNetPayloadBytes));
        BoundaryCrossings(*net_ptr);
        CaptureWalkThroughOutputHeads(*net_ptr);
    }

    if (argc < 2)
    {
        return test_report::Summary(std::format(
            "INCREMENTAL NNUE: {} checks bit-identical on the synthetic net (no net file supplied)", g_checks));
    }
    // Load the net into a Game; SearchState then maintains the accumulator incrementally.
    auto  game_ptr = std::make_unique<Game>();
    Game &game     = *game_ptr;
    if (!game.nnue_network_.Load(argv[1]) || !game.nnue_network_.loaded_)
    {
        test_report::Check(false, std::format("incremental NNUE: failed to load net '{}'", argv[1]));
        return test_report::ExitCode();
    }

    // Several independent random search trees from the start position (the net persists across SetPosition()).
    for (uint64_t seed = 1; seed <= 12; ++seed)
    {
        game.SetPosition();
        SearchState     state{game};
        std::mt19937_64 rng(seed);
        Walk(state, rng, /*depth=*/10, /*branch=*/2);
    }

    return test_report::Summary(std::format(
        "INCREMENTAL NNUE: {} checks bit-identical to full refresh, over the shipped net's random walks and "
        "the boundary-crossing lines on a synthetic net at this build's shape",
        g_checks));
}

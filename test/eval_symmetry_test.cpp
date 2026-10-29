/// @file eval_symmetry_test.cpp The one net-independent check on what the engine *does* with a net's weights.
///
/// **Why this test exists.** Everything else in the chain that validates a freshly trained net checks
/// transport or shape, not use. `stamp_net` writes the header from compile-time constants and the loader
/// verifies the magic, the header CRC, every architecture field, the king-bucket map's CRC, the payload size
/// and the payload CRC — so truncation, corruption, a wrong shape and trainer/engine map drift are all
/// caught. The SPRT harness's depth-4 probe proves each engine loaded its own net. Held-out loss catches
/// garbage weights. **Nothing checks symmetry.** This architecture mirrors: a perspective whose own king
/// stands on files e–h is reflected across the d/e boundary (`square ^ kFileFlip`), the king square selects
/// one of eight input banks, and the black perspective flips both colour and rank (`square ^ kRankFlip`).
/// A defect in any of that — the file mirror, the bank selection, the perspective swap, the colour feature
/// mapping — leaves the *training* loss looking perfect whenever the trainer shares the defect, and the
/// engine's evaluation silently wrong. `test/nnue_reference.txt` cannot stand in: its 250 expected scores
/// are v12's own, so a new net legitimately disagrees with every one of them, which is exactly why that
/// suite has never gated a new net.
///
/// **The invariant.** For a perspective network a position and its colour-flipped vertical mirror are the
/// same position seen from the other chair. `flip` reflects every square vertically (`sq ^ kRankFlip`),
/// swaps every piece's colour, swaps the side to move, mirrors the castling rights and mirrors any
/// en-passant square. `Network::Evaluate` returns a **side-to-move-relative** score, so here the invariant
/// reads
///
///     Evaluate(P) == Evaluate(flip(P))
///
/// which is the same statement as the White-relative `eval(P) == -eval(flip(P))`: flipping swaps the side
/// to move, so the sign the White-relative form carries is already in the convention. The evaluation is
/// integer and quantised and the computation is symmetric by construction — the flipped position's black
/// perspective reads byte-identical feature rows to the original's white perspective, and the piece count,
/// hence the output head, is unchanged — so this asserts **exact equality, not a tolerance.** Any non-zero
/// difference is a defect. Both kernels are asserted: the shipped int8 `Evaluate` and the int16 reference
/// `EvaluateExact`.
///
/// **It is net-independent on purpose.** Every assertion relates two evaluations of the *same* net and no
/// expected score is pinned anywhere in this file, so it gates v12, the net training now, and every future
/// net without being rewritten. It therefore takes whichever net the build's shape-independent suites take
/// (the shipped net at its own shape, the synthetic net at any other) and runs at both shapes CI builds. At
/// v12's shape mirroring is off, so what it checks there is the perspective and colour mapping; at the
/// shared shape (`KING_MIRRORING=1`) the file mirror is live and checked too.
///
/// **The flip itself is tested, not trusted.** A wrong flip would make the whole suite either vacuous or
/// falsely red, so `FlipFen` is checked against hand-computed literals, for involution
/// (`flip(flip(P)) == P`) on every case, and for the legal-move count — which a colour-flipped vertical
/// mirror must preserve exactly, castling and en passant included.
///
/// **The position set is accounted for, not assumed.** The suite tallies which input bank each perspective
/// selected and which output head each position chose, and fails if the set stopped covering all of them —
/// so a case list that quietly drifted into exercising half the map cannot pass.
///
/// What is deliberately *not* here: the incremental-versus-from-scratch accumulator check, the other
/// classic net-independent NNUE defect. `test_nnue_incremental` already does it, both over random search
/// trees and over lines chosen to cross every bucket and mirror boundary.
///
/// Usage:  test_eval_symmetry [net.bin]

#include "constants.h"
#include "nnue.h"
#include "position.h"

#include "test_report.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdlib>
#include <format>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace
{

/// @brief One test position and the reason it is in the set.
struct Case
{
    std::string_view fen; ///< The position, as a FEN.
    std::string_view why; ///< Why this one is here — what it covers that the others do not.
};

/// @brief The positions, chosen so that between them (and their flips, which this suite also evaluates)
/// every input bank is selected by both colours' perspectives, every output head is chosen, the file mirror
/// is both inactive and active, the two kings fall on the same and on opposite sides of the d/e boundary,
/// and castling rights and an en-passant square are each present — those last two because the flip has to
/// mirror both for the flipped position to be the same position at all. The seventeen taken from
/// `test/nnue_reference.txt` are already in the repository and came from real games, so they are known
/// legal; the seven written by hand reach the ends of the material range and make a sign error loud.
constexpr std::array<Case, 24> kCases = {{
    // --- From test/nnue_reference.txt.
    {"4r1k1/6p1/1pP1p2r/p1p2pNp/2Pb1P2/3P4/1P2R2P/R6K w - a6 0 31",
     "an en-passant square on the a-file, the edge the file mirror moves furthest; both kings on e-h"},
    {"r1b3k1/p1p2ppr/p2qpn2/3p4/P2P3p/1P2P2P/2P1NPP1/RN1Q1RK1 b - a3 0 13",
     "the same a-file en passant with the other side to move, so the flip of the case above is also a case"},
    {"8/4k3/P7/1nN5/1P2BP2/4K3/8/8 b - f3 0 65",
     "an en-passant square in an eight-piece endgame rather than an opening, on a low output head"},
    {"r3k2r/1p1b1ppp/nqn1p3/p2pP3/2Pp1BP1/N4N2/PP3P1P/1R1Q1RK1 b kq c3 0 12",
     "castling rights and an en-passant square together, kings either side of the boundary (g1 vs e8)"},
    {"r2q1rk1/2p1bp2/b1p4n/p2pP1pp/3PP3/P1N5/1P2N1PP/R1BQ1RK1 w - g6 0 14",
     "an en-passant square on the g-file, so the square itself is one the file mirror moves"},
    {"rnbqkbn1/1ppp1ppr/p6p/4p3/2P5/4PP2/PP1P1KPP/RNBQ1BNR w q e6 0 5",
     "a White king already off e1 (f2) with only Black's queenside right left"},
    {"rnbqkbnr/1pp1pp1p/6p1/p2p4/3P4/4P2N/PPP2PPP/RNBQKB1R w KQk d6 0 6",
     "lop-sided castling rights (KQk), which the flip must mirror to Kkq rather than carry across"},
    {"r1bqk1nr/ppp1bppp/2npp3/8/2PP4/1P2P3/P3KPPP/RNBQ1BNR b kq d3 0 5",
     "the White king on e2 and an en-passant square on d3: the two sides of the boundary, one file apart"},
    {"r1b1k2r/1pp3bp/6p1/p2p4/5P2/1P6/P2Q1NBP/qNK4R b kq - 1 18",
     "a White king on c1 (unmirrored) against a Black king on e8 (mirrored) - each side mirrors alone"},
    {"rnb1k2r/p2pbpp1/1pp4p/4p3/PP2P3/N5P1/2PP2BP/1RBK2NR w kq - 1 11",
     "a White king on d1, the file immediately on the a-d side of the boundary"},
    {"rnbq1r1k/ppp1bp1p/7p/4P2Q/4P3/2pB1N2/P4PPP/RN2K2R w KQ - 0 13",
     "White-only castling rights, and a Black king on h8 - the far edge of the mirrored half"},
    {"3n1bnr/3k2p1/5p1p/r1p1p2B/2P1P3/4B3/PP3PPP/R3K1NR w KQ - 1 19",
     "a Black king already off its back rank (d7) while White still holds both rights"},
    {"8/3R4/8/5K2/2k5/8/4r3/8 b - - 22 84",
     "four pieces, the lowest output head, and both kings in their own advanced half"},
    {"1B6/8/2p1p3/3p4/1P1P1p2/2P3k1/4K3/8 b - - 11 53",
     "a Black king on g3, deep in White's half, which puts the two perspectives in different board halves"},
    {"8/6P1/4Nn2/p2p4/k2P4/3K4/8/8 b - - 0 43", "a Black king on a4, the a-file edge, with a pawn about to promote"},
    {"8/8/8/1kp1K3/8/8/3r4/5R2 b - - 66 102",
     "five pieces with the kings on b5 and e5 - adjacent files, opposite sides of the boundary, one rank"},
    {"5r1k/2p4p/1pRb3N/p7/5P2/1bN3PP/1P6/7K w - - 9 36",
     "both kings on the h-file: the case where both perspectives mirror at once"},
    // --- Written by hand, for the ends of the material range and for the obvious sign errors.
    {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
     "the start position: 32 pieces (the top output head), all four rights, and its own flip but for the "
     "side to move - so any asymmetry here is a pure perspective defect with nothing else in the way"},
    {"rnbqkbnr/ppppppp1/8/7p/8/8/PPPPPPPP/RNBQKBNR w KQkq h6 0 2",
     "an en-passant square on the h-file, the other edge, which no reference position carries"},
    {"8/8/8/4k3/8/8/8/K6Q w - - 0 1",
     "KQ v K: three pieces, head 0, and a score lop-sided enough that a sign error reads as White lost"},
    {"7k/8/8/8/8/8/8/R6K b - - 0 1", "KR v K with Black to move: the same asymmetry seen from the other chair"},
    {"6k1/5ppp/8/8/8/8/8/R5K1 w - - 0 1", "a rook against three pawns - asymmetric material at six pieces"},
    {"8/8/8/8/8/8/P7/K1k5 w - - 0 1",
     "a bare pawn endgame with both kings on a-d and in their own back half, the corner of the map the "
     "middlegame positions never reach"},
    {"4k3/8/8/8/8/8/8/4K2R w K - 0 1",
     "one castling right and nothing else on the board, so the right's mirroring is tested in isolation"},
}};

/// @brief Split a FEN into its whitespace-separated fields.
/// @param fen The FEN. @return Its fields, in order.
std::vector<std::string> Fields(std::string_view fen)
{
    std::vector<std::string> fields;
    std::istringstream       stream{std::string{fen}};
    for (std::string field; stream >> field;)
    {
        fields.push_back(field);
    }
    return fields;
}

/// @brief Swap a letter's case, leaving anything else alone (so '-' and the digits pass through).
/// @param c The character. @return The same letter in the other case.
char SwapCase(char c)
{
    if (c >= 'a' && c <= 'z')
    {
        return static_cast<char>(c - 'a' + 'A');
    }
    if (c >= 'A' && c <= 'Z')
    {
        return static_cast<char>(c - 'A' + 'a');
    }
    return c;
}

/// @brief The colour-flipped vertical mirror of a FEN: every square reflected across the 4/5 rank boundary,
/// every piece's colour swapped, the side to move swapped, the castling rights mirrored (and re-emitted in
/// the canonical KQkq order) and any en-passant square reflected with the rest. The clocks are unchanged,
/// being properties of the game rather than the board. A missing clock field is filled in, so a four-field
/// FEN flips too.
/// @param fen The position. @return Its flip, as a FEN.
std::string FlipFen(std::string_view fen)
{
    std::vector<std::string> fields = Fields(fen);
    while (fields.size() < 6)
    {
        fields.push_back(fields.size() == 4 ? "0" : "1");
    }

    // The board: the ranks in reverse order, each piece in the other colour.
    std::vector<std::string> ranks;
    {
        std::istringstream stream{fields[0]};
        for (std::string rank; std::getline(stream, rank, '/');)
        {
            ranks.push_back(rank);
        }
    }
    std::string board;
    for (std::size_t i = ranks.size(); i-- > 0;)
    {
        if (!board.empty())
        {
            board += '/';
        }
        for (const char c : ranks[i])
        {
            board += SwapCase(c);
        }
    }

    const std::string side_to_move = fields[1] == "w" ? "b" : "w";

    std::string swapped_rights;
    for (const char c : fields[2])
    {
        swapped_rights += SwapCase(c);
    }
    std::string rights;
    for (const char c : std::string_view{"KQkq"})
    {
        if (swapped_rights.find(c) != std::string::npos)
        {
            rights += c;
        }
    }
    if (rights.empty())
    {
        rights = "-";
    }

    std::string en_passant = fields[3];
    if (en_passant != "-" && en_passant.size() == 2)
    {
        en_passant[1] = static_cast<char>('0' + 9 - (en_passant[1] - '0'));
    }

    return std::format("{} {} {} {} {} {}", board, side_to_move, rights, en_passant, fields[4], fields[5]);
}

/// @brief The input bank a perspective selects, as a bucket index rather than a weight offset.
/// @param position The position. @param black true for the black perspective. @return Its bank index.
int BankOf(const Position &position, bool black)
{
    return nnue::PerspectiveFor(position, black).bank_ / nnue::kInputSize;
}

/// @brief Whether a perspective mirrors its squares across the d/e boundary.
/// @param position The position. @param black true for the black perspective. @return true if mirrored.
bool IsMirrored(const Position &position, bool black)
{
    return (nnue::PerspectiveFor(position, black).square_flip_ & nnue::kFileFlip) != 0;
}

/// @brief Assert one per-position condition, printing a line only when it fails. There are twenty-four
/// positions and seven conditions each, and a line per passing check would bury the suites either side of
/// this one in ctest's streamed output; the counts are reported in aggregate at the end instead. A failure
/// still prints in full, naming the position it failed on.
/// @param passed Whether the condition held. @param detail What failed, including the FEN.
/// @return @p passed, so a caller can skip the rest of a case whose premise did not hold.
bool Require(bool passed, std::string_view detail)
{
    if (!passed)
    {
        test_report::Check(false, detail);
    }
    return passed;
}

/// @brief Check FlipFen against literals worked out by hand, so the flip is pinned to something other than
/// its own code before any position is evaluated with it.
void FlipIsWhatItClaims()
{
    struct Pair
    {
        std::string_view before;
        std::string_view after;
    };

    // Each `after` was derived by hand from the `before` - ranks reversed, cases swapped, rights mirrored
    // into KQkq order, the en-passant rank reflected (6 <-> 3) - not by running the function.
    constexpr std::array<Pair, 5> kPairs = {{
        // The start position is its own mirror, so only the side to move may change. A flip that dropped a
        // rank, mis-ordered the ranks or lost the rights fails here with nothing else to hide behind.
        {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
         "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR b KQkq - 0 1"},
        // A middlegame position with an a-file en-passant square: every rank different, nothing symmetric.
        {"4r1k1/6p1/1pP1p2r/p1p2pNp/2Pb1P2/3P4/1P2R2P/R6K w - a6 0 31",
         "r6k/1p2r2p/3p4/2pB1p2/P1P2PnP/1Pp1P2R/6P1/4R1K1 b - a3 0 31"},
        // Black-only rights become White-only, and the lower-case queen on a1 becomes an upper-case one on a8.
        {"r1b1k2r/1pp3bp/6p1/p2p4/5P2/1P6/P2Q1NBP/qNK4R b kq - 1 18",
         "Qnk4r/p2q1nbp/1p6/5p2/P2P4/6P1/1PP3BP/R1B1K2R w KQ - 1 18"},
        // An h-file en-passant square, the edge: h6 must become h3 and stay on the h-file.
        {"rnbqkbnr/ppppppp1/8/7p/8/8/PPPPPPPP/RNBQKBNR w KQkq h6 0 2",
         "rnbqkbnr/pppppppp/8/8/7P/8/PPPPPPP1/RNBQKBNR b KQkq h3 0 2"},
        // A four-field FEN: the clocks are filled in rather than the flip losing a field.
        {"8/8/8/4k3/8/8/8/K6Q w - -", "k6q/8/8/8/4K3/8/8/8 b - - 0 1"},
    }};
    for (const Pair &pair : kPairs)
    {
        const std::string got = FlipFen(pair.before);
        Require(got == pair.after, std::format("flip({}) = '{}', expected '{}'", pair.before, got, pair.after));
    }
    test_report::Check(test_report::failures == 0,
                       std::format("the flip matches {} hand-computed FENs", kPairs.size()));
}

} // namespace

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        return test_report::Summary("EVAL SYMMETRY: skipped (no net supplied)");
    }
    auto           net_ptr = std::make_unique<nnue::Network>();
    nnue::Network &net     = *net_ptr;
    if (!net.Load(argv[1]))
    {
        test_report::Check(false, std::format("EVAL SYMMETRY: failed to load net '{}'", argv[1]));
        return test_report::ExitCode();
    }

    FlipIsWhatItClaims();

    // What the set covered, tallied as the suite goes so a case list that drifted cannot pass quietly.
    std::set<int> white_banks, black_banks, output_heads;
    bool          saw_mirrored = false, saw_unmirrored = false, saw_opposite_sides = false;
    bool          saw_castling_rights = false, saw_en_passant = false;
    int           worst_int8 = 0, worst_exact = 0;
    std::string   worst_fen;
    std::size_t   legal = 0, flips_sound = 0, symmetric = 0;

    for (const Case &test_case : kCases)
    {
        const Position position = Position::FromString(test_case.fen);
        const auto     fields   = Fields(test_case.fen);
        // FromString answers a position it cannot parse with the start position, so a typo in a FEN above
        // would otherwise test the start position twenty-four times over and pass. Hold the parse to the
        // board it was given.
        if (!Require(Fields(position.ToString())[0] == fields[0],
                     std::format("the FEN does not parse to the board it states: {}", test_case.fen)))
        {
            continue;
        }
        if (!Require(position.HasBothKings(), std::format("a king is missing: {}", test_case.fen)))
        {
            continue;
        }
        // A position in which the side NOT to move stands in check is unreachable, and its evaluation is
        // not a statement about anything. Checking it here keeps an illegal case out of the set.
        const Square enemy_king = (position.pieces_[kKing] & position.colors_[EnemyOf(position.color_to_move_)]).Lsb();
        if (!Require(!position.IsAttacked(enemy_king, position.color_to_move_),
                     std::format("illegal - the side not to move is in check: {}", test_case.fen)))
        {
            continue;
        }
        ++legal;

        // The flip, and the flip of the flip, through the engine's own FEN writer so the comparison is of
        // canonical text on both sides.
        const std::string canonical   = position.ToString();
        const std::string flipped_fen = FlipFen(canonical);
        const Position    flipped     = Position::FromString(flipped_fen);
        if (!Require(Fields(flipped.ToString())[0] == Fields(flipped_fen)[0],
                     std::format("the flip is not a board the engine can parse: {}", flipped_fen)))
        {
            continue;
        }
        const std::string round_trip = Position::FromString(FlipFen(flipped_fen)).ToString();
        bool              sound = Require(round_trip == canonical,
                                          std::format("flip(flip(P)) != P: '{}' became '{}'", canonical, round_trip));
        // A colour-flipped vertical mirror is an isomorphism of the game, so it preserves the legal move
        // count exactly - castling and en passant included. This is the assertion that would catch a flip
        // which dropped a right or put the en-passant square on the wrong rank, either of which would
        // otherwise leave the evaluations equal (the features carry neither) and the suite green.
        sound &= Require(flipped.GenerateLegalMoves().size() == position.GenerateLegalMoves().size(),
                         std::format("the flip has {} legal moves, not {}: {}", flipped.GenerateLegalMoves().size(),
                                     position.GenerateLegalMoves().size(), test_case.fen));
        sound &= Require(nnue::OutputBucket(flipped) == nnue::OutputBucket(position),
                         std::format("the flip selects output head {}, not {}: {}", nnue::OutputBucket(flipped),
                                     nnue::OutputBucket(position), test_case.fen));
        flips_sound += sound ? 1 : 0;

        // The invariant. Exact equality on both kernels: the shipped int8 Evaluate and the int16 reference.
        const int int8_difference  = net.Evaluate(position) - net.Evaluate(flipped);
        const int exact_difference = net.EvaluateExact(position) - net.EvaluateExact(flipped);
        if (int8_difference == 0 && exact_difference == 0)
        {
            ++symmetric;
        }
        else
        {
            test_report::Check(false,
                               std::format("ASYMMETRIC  {}\n         eval(P)={} eval(flip)={} diff={} | "
                                           "exact(P)={} exact(flip)={} diff={}\n         flip = {}\n"
                                           "         {}",
                                           test_case.fen, net.Evaluate(position), net.Evaluate(flipped),
                                           int8_difference, net.EvaluateExact(position), net.EvaluateExact(flipped),
                                           exact_difference, flipped_fen, test_case.why));
        }
        if (std::abs(int8_difference) > worst_int8 || std::abs(exact_difference) > worst_exact)
        {
            worst_fen = test_case.fen;
        }
        worst_int8  = std::max(worst_int8, std::abs(int8_difference));
        worst_exact = std::max(worst_exact, std::abs(exact_difference));

        // Coverage. Both positions count: the flip's white perspective reads the oriented king square of
        // the original's black perspective, so evaluating both is what gives each colour all eight banks.
        for (const Position &p : {position, flipped})
        {
            white_banks.insert(BankOf(p, false));
            black_banks.insert(BankOf(p, true));
            output_heads.insert(nnue::OutputBucket(p));
            for (const bool black : {false, true})
            {
                (IsMirrored(p, black) ? saw_mirrored : saw_unmirrored) = true;
            }
        }
        const int white_king_file = (position.pieces_[kKing] & position.colors_[kWhite]).Lsb() & nnue::kFileFlip;
        const int black_king_file = (position.pieces_[kKing] & position.colors_[kBlack]).Lsb() & nnue::kFileFlip;
        saw_opposite_sides |= (white_king_file >= 4) != (black_king_file >= 4);
        saw_castling_rights |= fields[2] != "-";
        saw_en_passant |= fields[3] != "-";
    }

    test_report::Check(legal == kCases.size(),
                       std::format("{} of {} positions parse to the board they state, with both kings on it and "
                                   "the side not to move out of check",
                                   legal, kCases.size()));
    test_report::Check(flips_sound == kCases.size(),
                       std::format("{} of {} flips satisfy flip(flip(P)) == P and preserve the legal move count "
                                   "and the output head",
                                   flips_sound, kCases.size()));
    test_report::Check(symmetric == kCases.size(),
                       std::format("eval(P) == eval(flip(P)) EXACTLY, on both the int8 and the int16 kernel, for "
                                   "{} of {} positions",
                                   symmetric, kCases.size()));

    // The set must still exercise what it was built to exercise. A failure here is not a defect in the
    // engine - it says the case list no longer covers the map, which is how this suite would rot.
    std::set<int> every_bank, every_head;
    for (int bank = 0; bank < nnue::kNumKingBuckets; ++bank)
    {
        every_bank.insert(bank);
    }
    for (int head = 0; head < nnue::kNumOutputBuckets; ++head)
    {
        every_head.insert(head);
    }
    test_report::Check(white_banks == every_bank,
                       std::format("the white perspective selected all {} input banks", nnue::kNumKingBuckets));
    test_report::Check(black_banks == every_bank,
                       std::format("the black perspective selected all {} input banks", nnue::kNumKingBuckets));
    test_report::Check(output_heads == every_head,
                       std::format("all {} output heads were chosen", nnue::kNumOutputBuckets));
    test_report::Check(saw_unmirrored, "at least one perspective indexed unmirrored (its king on files a-d)");
    if (nnue::kIsKingMirroringEnabled)
    {
        test_report::Check(saw_mirrored, "at least one perspective was mirrored (its king on files e-h)");
    }
    test_report::Check(saw_opposite_sides, "at least one position has its kings either side of the d/e boundary");
    test_report::Check(saw_castling_rights, "at least one position carries castling rights");
    test_report::Check(saw_en_passant, "at least one position carries an en-passant square");

    return test_report::Summary(std::format(
        "EVAL SYMMETRY: {} positions, eval(P) == eval(flip(P)) exactly on both kernels (worst |diff| int8 {}, "
        "exact {}{}); banks {}, heads {}; mirroring {}",
        kCases.size(), worst_int8, worst_exact, worst_fen.empty() ? std::string{} : std::format(" at {}", worst_fen),
        nnue::kNumKingBuckets, nnue::kNumOutputBuckets, nnue::kIsKingMirroringEnabled ? "on" : "off"));
}

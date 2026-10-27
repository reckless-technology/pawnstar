/// @file lichess_extract_test.cpp Unit tests of the Lichess PGN extraction rules (tools/lichess_extract.h).
///
/// The extractor reads a 29 GB stream it can never re-read, writes shards nothing downstream validates
/// semantically, and every one of its rules fails *quietly*: a label shifted by one ply, a sign flipped for
/// one side, a result token spelled the way PGN spells it rather than the way bulletformat reads it. Each of
/// those trains to something worse while every number in the yield report still looks exactly right. So the
/// rules are pinned here, in process, against PGN written by hand - the only place they can be checked
/// without a multi-gigabyte download.
///
/// What is pinned, and why each one is here rather than trusted:
///
///  * **The after-the-move rule.** In Lichess PGN the `[%eval]` comment *follows* the move and describes the
///    position that move reached. Pairing it with the position the move was played *from* shifts every label
///    by a ply. The FENs below are written out by hand so the pairing is asserted and not merely recomputed
///    by the same code that produced it.
///  * **The result mapping.** bulletformat's text parser accepts `1.0` / `0.5` / `0.0` and does *not* accept
///    PGN's own `1/2-1/2`, so the mapping is not the identity and a draw is the case that would slip.
///  * **The White-relative sign.** Lichess's eval is already White-relative and bulletformat flips both
///    columns itself for a black-to-move FEN, so the extractor must *not* flip. A spurious flip inverts the
///    label of every black-to-move position - half the data - and nothing downstream would complain.
///  * **The SAN resolver**, on the tokens a hand-rolled reader gets wrong: castling (which names no square),
///    promotion, an en-passant capture onto an empty square, and a disambiguated piece move. Plus the
///    refusal of an ambiguous token, which must stay a refusal rather than become a guess.
///  * **Mate handling**, which is a skip by default and a clamp when asked.
///
/// Usage:  test_lichess_extract   (no arguments, no resources)

#include "lichess_extract.h"

#include "test_report.h"

#include <string>
#include <unordered_set>
#include <vector>

namespace
{
void Check(bool condition, const std::string &message)
{
    test_report::Check(condition, message);
}

/// @brief One emitted record, as the shard would carry it.
struct Record
{
    std::string fen;
    int         white_score = 0;
    std::string wdl;
};

/// @brief Collects records in memory, deduplicating per "shard" exactly as the real writer does.
class CollectingSink final : public lichess::RecordSink
{
  public:
    bool IsNewToShard(zobrist_t hash) override
    {
        return seen_.insert(hash).second;
    }

    void Write(const std::string &fen, int white_score, const char *wdl_text) override
    {
        records.push_back(Record{fen, white_score, wdl_text});
    }

    void EndGame() override
    {
        ++games_ended;
    }

    std::vector<Record> records;
    int                 games_ended = 0;

  private:
    std::unordered_set<zobrist_t> seen_;
};

/// @brief Options that isolate one rule at a time: no skipping, no ply cap, no adjudication.
/// The defaults exist to mirror tools/gen_data.cpp on real data; a test that left them on would be
/// asserting several rules at once and would not say which of them broke.
lichess::Options BareOptions()
{
    lichess::Options options;
    options.skip_plies   = 0;
    options.max_plies    = 0;
    options.resign_cp    = 0;
    options.resign_plies = 0;
    return options;
}

/// @brief Wrap movetext in the tag block a Lichess export carries.
/// @param result The Result tag and the movetext's own result token. @param movetext The moves.
/// @param white_elo White's rating tag. @param black_elo Black's rating tag. @return The PGN text.
std::string MakeGame(const std::string &result, const std::string &movetext, const std::string &white_elo = "2000",
                     const std::string &black_elo = "2000")
{
    return "[Event \"Rated blitz game\"]\n"
           "[Result \"" +
           result + "\"]\n[WhiteElo \"" + white_elo + "\"]\n[BlackElo \"" + black_elo + "\"]\n\n" + movetext + " " +
           result + "\n\n";
}

// The first two plies of a game, with the evals Lichess really writes for them. Both FENs are written out by
// hand: these are the standard start position after one and two plies, with the en-passant square set
// because each move was a double step, and the halfmove clock reset to 0 because each was a pawn move.
constexpr const char *kAfterE4 = "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1";
constexpr const char *kAfterE5 = "rnbqkbnr/pppp1ppp/8/4p3/4P3/8/PPPP1PPP/RNBQKBNR w KQkq e6 0 2";

void AfterTheMoveRule()
{
    // 1. e4 {[%eval 0.18]} e5 {[%eval 0.22]} 2. Nf3 {[%eval 0.15]}
    // The eval after "e4" belongs to the position e4 REACHED, which is Black to move. Nf3's own record is
    // dropped under the default quiet mode only if the game ends there, so a fourth ply follows it.
    const std::string pgn = MakeGame("1-0", "1. e4 { [%eval 0.18] [%clk 0:03:00] } 1... e5 { [%eval 0.22] } "
                                            "2. Nf3 { [%eval 0.15] } 2... Nc6 { [%eval 0.20] }");

    CollectingSink      sink;
    lichess::Statistics statistics;
    lichess::Options    options = BareOptions();
    options.quiet_mode          = lichess::QuietMode::kPlayedMove; // judge the move in, so every ply counts

    lichess::ExtractText(pgn, options, sink, statistics);

    Check(sink.records.size() == 4, "every ply of a four-ply annotated game is emitted");

    if (sink.records.size() < 2)
    {
        return;
    }

    Check(sink.records[0].fen == kAfterE4,
          "the record for ply 1 is the position AFTER e4 (Black to move), not the one it was played from");
    Check(sink.records[0].white_score == 18, "0.18 pawns becomes 18 centipawns");
    Check(sink.records[1].fen == kAfterE5, "the record for ply 2 is the position AFTER e5 (White to move)");
    Check(sink.records[1].white_score == 22, "0.22 pawns becomes 22 centipawns");

    // The off-by-one this suite exists to catch: had the eval been paired with the position the move was
    // played from, ply 1's record would be the start position and ply 2's would be kAfterE4.
    Check(sink.records[0].fen != lichess::kStartPositionFen, "ply 1 is not labelled with the start position");
    Check(sink.records[1].fen != kAfterE4, "ply 2 is not labelled with ply 1's position");
}

void ResultMapping()
{
    Check(std::string{lichess::ResultToWdlToken("1-0")} == "1.0", "1-0 maps to the wdl token 1.0");
    Check(std::string{lichess::ResultToWdlToken("0-1")} == "0.0", "0-1 maps to the wdl token 0.0");
    // The one that is not the identity: bulletformat does not accept the PGN spelling "1/2-1/2".
    Check(std::string{lichess::ResultToWdlToken("1/2-1/2")} == "0.5", "1/2-1/2 maps to the wdl token 0.5");
    Check(lichess::ResultToWdlToken("*") == nullptr, "an unfinished game * has no wdl token");
    Check(lichess::ResultToWdlToken("") == nullptr, "a missing result has no wdl token");

    const std::string movetext = "1. e4 { [%eval 0.18] } 1... e5 { [%eval 0.22] }";

    for (const auto &pair :
         {std::pair<const char *, const char *>{"1-0", "1.0"}, std::pair<const char *, const char *>{"0-1", "0.0"},
          std::pair<const char *, const char *>{"1/2-1/2", "0.5"}})
    {
        CollectingSink      sink;
        lichess::Statistics statistics;
        lichess::ExtractText(MakeGame(pair.first, movetext), BareOptions(), sink, statistics);
        Check(!sink.records.empty() && sink.records[0].wdl == pair.second,
              std::string{"a "} + pair.first + " game labels every record " + pair.second);
    }

    // An unfinished game has no label at all, so it must contribute nothing rather than default to a draw.
    CollectingSink      sink;
    lichess::Statistics statistics;
    lichess::ExtractText(MakeGame("*", movetext), BareOptions(), sink, statistics);
    Check(sink.records.empty() && statistics.games_without_result == 1,
          "an unfinished * game emits no records and is counted as having no result");
}

void WhiteRelativeSign()
{
    // White's second move is a blunder, so the eval after it is negative. It must stay negative: Lichess's
    // eval is already White-relative and bulletformat flips the columns itself for a black-to-move FEN.
    const std::string pgn = MakeGame("0-1", "1. e4 { [%eval 0.18] } 1... e5 { [%eval 0.22] } "
                                            "2. Ke2 { [%eval -2.05] } 2... Nc6 { [%eval -2.10] }");

    CollectingSink      sink;
    lichess::Statistics statistics;
    lichess::Options    options = BareOptions();
    options.quiet_mode          = lichess::QuietMode::kPlayedMove;

    lichess::ExtractText(pgn, options, sink, statistics);

    Check(sink.records.size() == 4, "the blundering game emits all four plies");

    if (sink.records.size() < 3)
    {
        return;
    }

    Check(sink.records[0].white_score == 18 && sink.records[1].white_score == 22,
          "an opening good for White carries a positive White-relative score");
    Check(sink.records[2].white_score == -205,
          "a move bad for White carries a NEGATIVE score - the eval is White-relative and is not flipped");
    // The position after White's 2. Ke2 has Black to move, which is exactly the case a spurious
    // side-to-move flip would invert.
    Check(sink.records[2].fen.find(" b ") != std::string::npos,
          "that record is a black-to-move position, the case a side-to-move flip would corrupt");

    int white_score = 0;
    Check(lichess::ScoreFromLabel(lichess::ParseEvalComment("[%eval -3.87]"), BareOptions(), white_score) &&
              white_score == -387,
          "-3.87 pawns becomes -387 centipawns with no sign flip");
    Check(lichess::ScoreFromLabel(lichess::ParseEvalComment("[%eval 24.15]"), BareOptions(), white_score) &&
              white_score == 2415,
          "24.15 pawns becomes 2415 centipawns");
}

void EvalCommentParsing()
{
    const lichess::EvalLabel decimal = lichess::ParseEvalComment("[%eval 0.18] [%clk 0:02:00]");
    Check(decimal.has_eval && !decimal.is_mate, "an eval beside a clock comment is found");

    const lichess::EvalLabel clock_only = lichess::ParseEvalComment("[%clk 0:02:00]");
    Check(!clock_only.has_eval, "a comment with only a clock carries no eval");

    const lichess::EvalLabel human = lichess::ParseEvalComment("what a move!");
    Check(!human.has_eval, "a bare human comment carries no eval");

    const lichess::EvalLabel white_mates = lichess::ParseEvalComment("[%eval #5]");
    Check(white_mates.has_eval && white_mates.is_mate && white_mates.mate_sign == 1, "#5 is White mating");

    const lichess::EvalLabel black_mates = lichess::ParseEvalComment("[%eval #-3]");
    Check(black_mates.has_eval && black_mates.is_mate && black_mates.mate_sign == -1, "#-3 is Black mating");

    // A value that is not a number must make the ply unannotated rather than mislabel it as 0.00, which
    // would look exactly like a drawn position.
    Check(!lichess::ParseEvalComment("[%eval banana]").has_eval, "an unparseable eval leaves the ply unlabelled");
}

void MateHandling()
{
    lichess::Options options     = BareOptions();
    int              white_score = 0;

    Check(options.mate_score_cp == 0, "mate evals are skipped by default");
    Check(!lichess::ScoreFromLabel(lichess::ParseEvalComment("[%eval #5]"), options, white_score),
          "with --mate-score-cp 0 a mate eval yields no label");

    options.mate_score_cp = 2000;
    Check(lichess::ScoreFromLabel(lichess::ParseEvalComment("[%eval #5]"), options, white_score) && white_score == 2000,
          "with --mate-score-cp 2000 White mating clamps to +2000");
    Check(lichess::ScoreFromLabel(lichess::ParseEvalComment("[%eval #-3]"), options, white_score) &&
              white_score == -2000,
          "Black mating clamps to -2000");

    // The adjudication reads a score even for a ply whose record is skipped, so a mate must count as
    // decisive there - otherwise a game that ends in a forced mate is harvested past the point gen_data's
    // own pool would have stopped.
    Check(lichess::AdjudicationScore(lichess::ParseEvalComment("[%eval #5]")) > kMaxEvaluation - 1,
          "a mate eval is fully decisive for the adjudication even when its record is skipped");
    Check(lichess::AdjudicationScore(lichess::ParseEvalComment("[%eval #-3]")) < -(kMaxEvaluation - 1),
          "a mate for Black is decisive the other way");
    Check(lichess::AdjudicationScore(lichess::ParseEvalComment("[%clk 0:01:00]")) == 0,
          "an unannotated ply is neutral for the adjudication");

    // Skipping is visible in the statistics as its own reason, so a run can tell "Lichess did not analyse
    // this" from "we refused the label".
    CollectingSink      sink;
    lichess::Statistics statistics;
    lichess::ExtractText(MakeGame("1-0", "1. e4 { [%eval #5] } 1... e5 { [%eval 0.22] }"), BareOptions(), sink,
                         statistics);
    Check(statistics.dropped_mate_eval == 1, "a skipped mate eval is counted as dropped_mate_eval");
}

void SanCastling()
{
    const Position position = Position::FromString("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");

    const Move short_castle = lichess::ParseSanMove(position, "O-O");
    Check(short_castle.from() == Square{"e1"} && short_castle.to() == Square{"g1"},
          "O-O resolves to the king's two-square step to g1");

    const Move long_castle = lichess::ParseSanMove(position, "O-O-O");
    Check(long_castle.from() == Square{"e1"} && long_castle.to() == Square{"c1"}, "O-O-O resolves to c1");

    // Some writers use zeroes rather than letter O.
    Check(lichess::ParseSanMove(position, "0-0") == short_castle, "0-0 is accepted as O-O");
    Check(lichess::ParseSanMove(position, "0-0-0") == long_castle, "0-0-0 is accepted as O-O-O");

    // A king move written as a king move must never resolve to the castling, and vice versa.
    const Move king_step = lichess::ParseSanMove(position, "Kf1");
    Check(king_step.from() == Square{"e1"} && king_step.to() == Square{"f1"}, "Kf1 is the ordinary king step");
    Check(!(king_step == short_castle), "a king move and the castling are different moves");

    const Position no_rights = Position::FromString("r3k2r/8/8/8/8/8/8/R3K2R w - - 0 1");
    Check(lichess::ParseSanMove(no_rights, "O-O") == Move::None(),
          "O-O is refused when the position has no castling rights");
}

void SanPromotionAndEnPassant()
{
    // A pawn on the seventh with the whole back rank to promote onto, plus a capture-promotion.
    const Position promoting = Position::FromString("4k1n1/5P2/8/8/8/8/8/4K3 w - - 0 1");

    const Move to_queen = lichess::ParseSanMove(promoting, "f8=Q");
    Check(to_queen.to() == Square{"f8"} && to_queen.promoted() == kQueen, "f8=Q promotes to a queen");

    const Move to_knight = lichess::ParseSanMove(promoting, "f8=N");
    Check(to_knight.to() == Square{"f8"} && to_knight.promoted() == kKnight, "f8=N promotes to a knight");
    Check(!(to_queen == to_knight), "the promotion piece distinguishes two otherwise identical moves");

    const Move capture_promotion = lichess::ParseSanMove(promoting, "fxg8=Q");
    Check(capture_promotion.from() == Square{"f7"} && capture_promotion.to() == Square{"g8"} &&
              capture_promotion.promoted() == kQueen,
          "fxg8=Q is the capture-promotion onto the knight");

    Check(lichess::ParseSanMove(promoting, "f8=K") == Move::None(), "f8=K is not a move and is refused");

    // En passant: the destination square is EMPTY, which is the case a hand-rolled reader gets wrong. Here
    // Black has just played f7-f5, so White's e5 pawn may take on f6.
    const Position en_passant = Position::FromString("4k3/8/8/4Pp2/8/8/8/4K3 w - f6 0 1");
    const Move     capture    = lichess::ParseSanMove(en_passant, "exf6");
    Check(capture.from() == Square{"e5"} && capture.to() == Square{"f6"},
          "exf6 resolves the en-passant capture onto an empty square");

    // The same tokens driven through a whole game, so the movetext path is covered too and not just the
    // resolver: 1. e4 d5 2. e5 f5 3. exf6 is the shortest real en passant.
    CollectingSink      sink;
    lichess::Statistics statistics;
    lichess::ExtractText(MakeGame("1-0", "1. e4 { [%eval 0.2] } 1... d5 { [%eval 0.3] } 2. e5 { [%eval 0.2] } "
                                         "2... f5 { [%eval 1.1] } 3. exf6 { [%eval 1.2] } 3... Nf6 { [%eval 1.0] }"),
                         BareOptions(), sink, statistics);
    Check(statistics.plies_unparsed_san == 0 && statistics.games_with_unparsed_san == 0,
          "a game containing an en-passant capture replays with every SAN token resolved");
}

void SanDisambiguation()
{
    // Both rooks can reach c1 along the first rank, so the bare token is ambiguous and the file says which.
    const Position position = Position::FromString("4k3/8/8/8/8/8/8/R5RK w - - 0 1");

    const Move from_a_file = lichess::ParseSanMove(position, "Rac1");
    Check(from_a_file.from() == Square{"a1"} && from_a_file.to() == Square{"c1"}, "Rac1 is the a1 rook");

    const Move from_g_file = lichess::ParseSanMove(position, "Rgc1");
    Check(from_g_file.from() == Square{"g1"} && from_g_file.to() == Square{"c1"}, "Rgc1 is the g1 rook");

    // The refusal matters as much as the resolution: guessing one of two legal moves would silently corrupt
    // every position after it in the game.
    Check(lichess::ParseSanMove(position, "Rc1") == Move::None(), "an ambiguous Rc1 is refused rather than guessed at");

    // Rank disambiguation, for two rooks on one file.
    const Position stacked = Position::FromString("4k3/8/8/8/R7/8/R7/4K3 w - - 0 1");
    const Move     lower   = lichess::ParseSanMove(stacked, "R2a3");
    Check(lower.from() == Square{"a2"} && lower.to() == Square{"a3"}, "R2a3 names the rank to disambiguate");

    // A lowercase file letter is a pawn move, never a bishop: b4 and Bb4 must not resolve alike.
    const Position pawn_or_bishop = Position::FromString("4k3/8/8/8/8/8/1P6/2B1K3 w - - 0 1");
    const Move     pawn_push      = lichess::ParseSanMove(pawn_or_bishop, "b4");
    Check(pawn_push.from() == Square{"b2"} && pawn_push.to() == Square{"b4"} && pawn_push.piece() == kPawn,
          "a lowercase b4 is the pawn's double step, not a bishop move");
    const Move bishop_move = lichess::ParseSanMove(pawn_or_bishop, "Bd2");
    Check(bishop_move.from() == Square{"c1"} && bishop_move.to() == Square{"d2"} && bishop_move.piece() == kBishop,
          "an uppercase B is the bishop");
    // b2 holds White's own pawn, so the bishop cannot go there: the resolver matches against the engine's
    // legal moves rather than parsing geometry, so an illegal destination is refused and not invented.
    Check(lichess::ParseSanMove(pawn_or_bishop, "Bb2") == Move::None(),
          "a bishop move onto an own piece is refused - legality comes from the move generator");

    // Decorations carry no information and must not stop a token resolving.
    Check(lichess::ParseSanMove(pawn_or_bishop, "b4!?") == pawn_push, "annotation glyphs are stripped");
    const Position checking = Position::FromString("4k3/8/8/8/8/8/8/4K2R w - - 0 1");
    Check(lichess::ParseSanMove(checking, "Re1+") == lichess::ParseSanMove(checking, "Re1"),
          "a check mark is stripped");
}

void QuietModeIsGenDatas()
{
    // 1. e4 e5 2. f4 exf4: the position after f4 has a CAPTURE played from it, so gen_data's rule (judge the
    // move out of the position) drops it while judging the move in would keep it. This is the whole reason
    // the default is kNextMove - see QuietMode in the header.
    const std::string pgn = MakeGame("1-0", "1. e4 { [%eval 0.18] } 1... e5 { [%eval 0.22] } "
                                            "2. f4 { [%eval -0.34] } 2... exf4 { [%eval -0.39] } "
                                            "3. Nf3 { [%eval -0.39] } 3... Qe7 { [%eval 0.34] }");

    lichess::Options defaults;
    Check(defaults.quiet_mode == lichess::QuietMode::kNextMove,
          "the default quiet mode is kNextMove, which is tools/gen_data.cpp's own rule");

    lichess::Options next_mode = BareOptions();
    next_mode.quiet_mode       = lichess::QuietMode::kNextMove;
    CollectingSink      next_sink;
    lichess::Statistics next_statistics;
    lichess::ExtractText(pgn, next_mode, next_sink, next_statistics);

    lichess::Options played_mode = BareOptions();
    played_mode.quiet_mode       = lichess::QuietMode::kPlayedMove;
    CollectingSink      played_sink;
    lichess::Statistics played_statistics;
    lichess::ExtractText(pgn, played_mode, played_sink, played_statistics);

    Check(next_sink.records.size() < played_sink.records.size(),
          "kNextMove keeps strictly fewer positions than kPlayedMove on a game containing a capture");

    // Under kNextMove the position after 2. f4 is dropped (exf4 is played from it); under kPlayedMove it is
    // kept (f4 itself was quiet) and the position after exf4 is dropped instead.
    const std::string after_f4 = "rnbqkbnr/pppp1ppp/8/4p3/4PP2/8/PPPP2PP/RNBQKBNR b KQkq f3 0 2";
    const auto        holds    = [](const std::vector<Record> &records, const std::string &fen) {
        for (const Record &record : records)
        {
            if (record.fen == fen)
            {
                return true;
            }
        }
        return false;
    };

    Check(!holds(next_sink.records, after_f4),
          "kNextMove drops the position a capture was played from, as gen_data does");
    Check(holds(played_sink.records, after_f4), "kPlayedMove keeps it, which is the filter we do not use");

    // The end of a game has no move out of it, so kNextMove must refuse the final position rather than
    // treat "no next move" as quiet.
    lichess::Options    ending = BareOptions();
    CollectingSink      ending_sink;
    lichess::Statistics ending_statistics;
    lichess::ExtractText(MakeGame("1-0", "1. e4 { [%eval 0.18] } 1... e5 { [%eval 0.22] }"), ending, ending_sink,
                         ending_statistics);
    Check(ending_sink.records.size() == 1,
          "under kNextMove the last ply of a game is refused: there is no move out of it");
}

void FiltersAndCounters()
{
    const std::string movetext = "1. e4 { [%eval 0.18] } 1... e5 { [%eval 0.22] } 2. Nf3 { [%eval 0.15] } "
                                 "2... Nc6 { [%eval 0.20] } 3. Bb5 { [%eval 0.18] } 3... a6 { [%eval 0.25] }";

    // --skip-plies drops the shared opening, which every game repeats.
    lichess::Options skipping = BareOptions();
    skipping.skip_plies       = 4;
    skipping.quiet_mode       = lichess::QuietMode::kPlayedMove;
    CollectingSink      skip_sink;
    lichess::Statistics skip_statistics;
    lichess::ExtractText(MakeGame("1-0", movetext), skipping, skip_sink, skip_statistics);
    Check(skip_statistics.dropped_skip_plies == 4 && skip_sink.records.size() == 2,
          "--skip-plies 4 drops the first four plies and keeps the rest");

    // --min-elo reads both ratings, and a provisional "?" counts as 0 so any real threshold drops it.
    lichess::Options rated = BareOptions();
    rated.min_elo          = 2100;
    CollectingSink      rated_sink;
    lichess::Statistics rated_statistics;
    lichess::ExtractText(MakeGame("1-0", movetext, "2200", "2000"), rated, rated_sink, rated_statistics);
    Check(rated_sink.records.empty() && rated_statistics.games_passing_elo == 0,
          "--min-elo drops a game where only one player is rated high enough");

    CollectingSink      unrated_sink;
    lichess::Statistics unrated_statistics;
    lichess::ExtractText(MakeGame("1-0", movetext, "?", "?"), rated, unrated_sink, unrated_statistics);
    Check(unrated_sink.records.empty(), "a provisional ? rating counts as 0 and fails any real --min-elo");
    Check(lichess::ParseElo("?") == 0 && lichess::ParseElo("1873") == 1873, "ParseElo reads a rating or 0");

    // The eval cap, which exists because a near-mate score is an unstable training target.
    lichess::Options capped = BareOptions();
    capped.eval_cap         = 100;
    capped.quiet_mode       = lichess::QuietMode::kPlayedMove;
    CollectingSink      capped_sink;
    lichess::Statistics capped_statistics;
    lichess::ExtractText(MakeGame("1-0", "1. e4 { [%eval 0.18] } 1... e5 { [%eval 9.90] } 2. Nf3 { [%eval 0.15] } "
                                         "2... Nc6 { [%eval 0.20] }"),
                         capped, capped_sink, capped_statistics);
    Check(capped_statistics.dropped_eval_cap == 1, "a score at or above the eval cap is dropped");

    // A non-standard game carries a FEN or a Variant tag, and must not be replayed from the start position.
    CollectingSink      variant_sink;
    lichess::Statistics variant_statistics;
    lichess::ExtractText("[Result \"1-0\"]\n[Variant \"Chess960\"]\n[FEN \"bqnbnrkr/pppppppp/8/8/8/8/PPPPPPPP/"
                         "BQNBNRKR w KQkq - 0 1\"]\n\n1. e4 { [%eval 0.18] } 1-0\n\n",
                         BareOptions(), variant_sink, variant_statistics);
    Check(variant_sink.records.empty() && variant_statistics.games_non_standard == 1,
          "a non-standard game is refused rather than replayed from the standard start position");

    // The per-shard dedup: the same position reached twice in one shard is written once.
    CollectingSink      dedup_sink;
    lichess::Statistics dedup_statistics;
    const std::string   repeated      = MakeGame("1-0", "1. Nf3 { [%eval 0.1] } 1... Nf6 { [%eval 0.1] } "
                                                        "2. Ng1 { [%eval 0.0] } 2... Ng8 { [%eval 0.0] } "
                                                        "3. Nf3 { [%eval 0.1] } 3... Nf6 { [%eval 0.1] } "
                                                        "4. Nc3 { [%eval 0.1] } 4... Nc6 { [%eval 0.1] }");
    lichess::Options    dedup_options = BareOptions();
    dedup_options.quiet_mode          = lichess::QuietMode::kPlayedMove;
    lichess::ExtractText(repeated, dedup_options, dedup_sink, dedup_statistics);
    Check(dedup_statistics.dropped_duplicate > 0, "a position repeated inside one shard is deduplicated");
}

void ReaderAndProvenance()
{
    // A bounded prefix cuts the last game mid-movetext. It must be dropped - harvesting a game whose
    // movetext stops inside a SAN token would emit positions from a game that never happened - and the
    // provenance digest must not widen to cover it.
    const std::string complete  = MakeGame("1-0", "1. e4 { [%eval 0.18] } 1... e5 { [%eval 0.22] }");
    const std::string truncated = complete + "[Event \"Rated blitz game\"]\n[Result \"1-0\"]\n\n1. d4 { [%eval 0.2";

    CollectingSink      sink;
    lichess::Statistics statistics;
    lichess::ExtractText(truncated, BareOptions(), sink, statistics);
    Check(statistics.games_truncated == 1, "a game cut off mid-movetext is counted as truncated");
    Check(statistics.games_seen == 2 && statistics.games_used == 1, "only the complete game is walked");

    // The digest covers the prefix ending at the last COMPLETE game, so the truncated tail is excluded.
    std::istringstream whole{truncated};
    lichess::PgnReader reader{whole};
    lichess::PgnGame   game;

    while (reader.NextGame(game))
    {
    }

    Check(reader.attributable_bytes() == complete.size(),
          "attributable_bytes stops at the end of the last complete game, excluding the truncated tail");
    Check(reader.bytes_consumed() > reader.attributable_bytes(),
          "bytes_consumed includes the truncated tail, so the two are genuinely different numbers");

    // The digest must be of exactly those bytes - the same value an independent hash of the prefix gives.
    sha256::Hasher expected;
    expected.Update(complete.data(), complete.size());
    Check(reader.Digest() == expected.Digest(),
          "the provenance digest is the SHA-256 of exactly the attributable prefix, with no readahead");

    // A reader over a complete stream attributes all of it.
    std::istringstream only_complete{complete};
    lichess::PgnReader clean{only_complete};

    while (clean.NextGame(game))
    {
    }

    Check(clean.attributable_bytes() == complete.size() && clean.bytes_consumed() == complete.size(),
          "with no truncated tail the two byte counts agree");
}

void MovetextTolerance()
{
    // None of these appear in a Lichess export, but all appear in PGN in the wild and each would otherwise
    // be read as a move: a recursive variation, a numeric annotation glyph, and a move number glued on.
    CollectingSink      sink;
    lichess::Statistics statistics;
    lichess::Options    options = BareOptions();
    options.quiet_mode          = lichess::QuietMode::kPlayedMove;

    lichess::ExtractText(MakeGame("1-0", "1.e4 $1 { [%eval 0.18] } (1. d4 d5) 1... e5 { [%eval 0.22] } "
                                         "2. Nf3 { [%eval 0.15] } 2... Nc6 { [%eval 0.2] }"),
                         options, sink, statistics);

    Check(statistics.plies_seen == 4 && statistics.plies_unparsed_san == 0,
          "a glued move number, a glyph and a recursive variation are all ignored, leaving four plies");

    if (sink.records.empty())
    {
        return;
    }

    Check(sink.records[0].fen == kAfterE4, "the glued \"1.e4\" still resolves to the e4 record");
}
} // namespace

int main()
{
    AfterTheMoveRule();
    ResultMapping();
    WhiteRelativeSign();
    EvalCommentParsing();
    MateHandling();
    SanCastling();
    SanPromotionAndEnPassant();
    SanDisambiguation();
    QuietModeIsGenDatas();
    FiltersAndCounters();
    ReaderAndProvenance();
    MovetextTolerance();
    return test_report::Summary(
        "LICHESS EXTRACT: after-the-move labelling, result mapping, White-relative sign, SAN, filters, provenance");
}

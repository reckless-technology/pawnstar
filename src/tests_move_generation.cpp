/// @file Standard perft move generation tests.

#include <algorithm>
#include <chrono>
#include <cinttypes>
#include <cstring>
#include <format>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <string_view>

using namespace std::chrono;

using std::string;
using std::string_view;

#include "chess_clock.h"
#include "debug_hashtable.h"
#include "position.h"
#include "transposition_table.h"

/// @brief Structure to hold a basic perft test.
struct PerftTest
{
    string   position; ///< FEN string view of starting position
    int      depth;    ///< perft search depth
    uint64_t count;    ///< total number of leaf node moves
};

static constexpr MoveList GeneratePseudoLegalMoves(const Position &position);

/// @brief Run standard perft test - recursive search.
/// @param src_position position to search
/// @param depth search depth
/// @param num_moves total number of moves at terminal / leaf nodes
static void Perft(const Position &src_position, int depth, uint64_t &num_moves)
{
    MoveList move_list{src_position.GenerateLegalMoves()};
    if (depth > 1)
    {
        for (const auto &move : move_list)
        {
            Position position{src_position.MakeMove(move)};
            Perft(position, depth - 1, num_moves);
        }
        return;
    }
    num_moves += move_list.size();
}

/// @brief Run a comprehensive perft test and regression check every node against a pseudo legal move generator -
/// recursive. This is very slow compared with the standard perft test.
/// @param src_position Starting position.
/// @param depth search depth.
/// @param num_moves total number of moves at terminal / leaf nodes
static void PerftRegression(const Position &src_position, int depth, uint64_t &num_moves)
{
    MoveList       move_list{src_position.GenerateLegalMoves()}; // Legal moves only - main generator.
    std::set<Move> move_set{move_list.begin(), move_list.end()};
    MoveList       pseudo_legal_moves{GeneratePseudoLegalMoves(src_position)}; // Pseudo legal moves.
    const Color    color = src_position.ColorToMove();
    std::set<Move> legal_move_set;
    // Test pseudo legal moves for legality.
    for (auto move : pseudo_legal_moves)
    {
        Position dst_position{src_position.MakeMove(move)};
        if (!dst_position.IsAttacked(dst_position.KingLocation(color), EnemyOf(color)))
        {
            legal_move_set.insert(move);
        }
    }
    // Compare legal and validated move lists for equality
    for (auto move : move_set)
    {
        if (!legal_move_set.contains(move))
        {
            std::cout << std::format("Error on position {} missing move {}\n", src_position.ToString(),
                                     move.ToString());
        }
    }
    for (auto move : legal_move_set)
    {
        if (!move_set.contains(move))
        {
            std::cout << std::format("Error on position {} superfluous move {}\n", src_position.ToString(),
                                     move.ToString());
        }
    }
    if (depth > 1)
    {
        for (const auto &move : move_list)
        {
            Position position{src_position.MakeMove(move)};
            PerftRegression(position, depth - 1, num_moves);
        }
        return;
    }
    num_moves += move_set.size();
}

// Standard Perft test position set.
extern const std::array<const char *, 132> perft_results;

/// @brief Run the suite of perft tests.
void RunPerftTests(bool do_regression)
{
    std::vector<PerftTest> tests;
    for (const char *line : perft_results)
    {
        // Split into tokens separated by semicolons
        std::vector<string> tokens;
        string              test_str{line};
        for (std::size_t i = test_str.find(';'); i != string::npos; i = test_str.find(';'))
        {
            tokens.push_back(test_str.substr(0, i));
            test_str = test_str.substr(i + 1);
        }
        tokens.push_back(test_str);
        // First token is FEN position
        const auto fen = tokens[0];
        tokens.erase(tokens.begin());
        // Remaining tokens are depth and counts
        for (auto token : tokens)
        {
            // Format is "Dxx NNNN"
            int      depth;
            uint64_t count;
            if (std::sscanf(token.c_str(), " D%d %" PRIu64, &depth, &count) != 2)
            {
                std::cout << std::format("ERROR: unable to scan perft test {}\n", test_str);
            }
            tests.push_back(PerftTest{fen, depth, count});
        }
    }
    bool       is_good     = true;
    uint64_t   total_nodes = 0;
    const auto first_start = std::chrono::system_clock::now();
    for (const auto &test : tests)
    {
        Position position{Position::FromString(test.position)};
        string   pos_string{position.ToString()};
        uint64_t num_moves = 0;
        total_nodes += test.count;
        const auto start = std::chrono::system_clock::now();
        do_regression ? PerftRegression(position, test.depth, num_moves) : Perft(position, test.depth, num_moves);
        const auto stop       = std::chrono::system_clock::now();
        auto       elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(stop - start).count();
        if (elapsed_us == 0)
        {
            elapsed_us = 1;
        }
        std::cout << std::format("{:<75} depth:{:2} moves:{:10} Mnps:{:4}\n", pos_string, test.depth, num_moves,
                                 num_moves / elapsed_us);
        if (num_moves != test.count)
        {
            std::cout << std::format("ERROR: expected {} got {}\n", test.count, num_moves);
            is_good = false;
        }
    }
    const auto last_stop = std::chrono::system_clock::now();
    std::cout << std::format("{:<40}{:10}\n", "total elapsed milliseconds",
                             std::chrono::duration_cast<std::chrono::milliseconds>(last_stop - first_start).count());
    std::cout << std::format(
        "{:<40}{:10}\n", "mean positions per second",
        total_nodes * 1000 / std::chrono::duration_cast<std::chrono::milliseconds>(last_stop - first_start).count());
    if (is_good)
    {
        std::cout << "******************* PERFT PASS *******************\n";
    }
    else
    {
        std::cout << "******************* PERFT FAIL *******************\n";
    }
}

static constexpr std::array<uint8_t, 2> double_push_rank{3, 4};
static constexpr std::array<uint8_t, 2> promotion_rank{7, 0};

/// @brief Generate all pseudo legal moves for this position.
/// @param position Position for which to generate moves.
/// @return List of all pseudo legal moves (some may leave King in check).
/// This is only used for regression testing of the main move generator.
static constexpr MoveList GeneratePseudoLegalMoves(const Position &position)
{
    MoveList       moves;
    const Color    color            = position.ColorToMove();
    const Bitboard friendly_pieces  = position.PiecesOfColor(color);
    const Bitboard occupied_squares = position.OccupiedSquares();
    const Bitboard vacant_squares   = ~occupied_squares;
    const Bitboard enemy_pieces     = occupied_squares ^ friendly_pieces;

    // Generate pawn moves.
    Bitboard pawns = position.Pawns() & friendly_pieces;
    for (Square from : pawns)
    {
        // Single pawn push.
        Square to = color == kWhite ? from + 8 : from - 8;
        if ((Bitboard{to} & occupied_squares).IsEmpty())
        {
            if (to.Rank() == promotion_rank[color])
            {
                // Push promotion.
                moves.push_back(Move::Promotion(from, to, kQueen));
                moves.push_back(Move::Promotion(from, to, kRook));
                moves.push_back(Move::Promotion(from, to, kBishop));
                moves.push_back(Move::Promotion(from, to, kKnight));
            }
            else
            {
                // Regular single push.
                moves.push_back(Move::NonCapture(from, to, kPawn));
                // Double pawn push.
                to = color == kWhite ? from + 16 : from - 16;
                if (to.Rank() == double_push_rank[color])
                {
                    if ((Bitboard{to} & occupied_squares).IsEmpty())
                    {
                        moves.push_back(Move::DoublePush(from, to));
                    }
                }
            }
        }
        // Pawn captures
        const Bitboard pawn_attacks = color == kWhite ? kPawnAttacksWhite[from] : kPawnAttacksBlack[from];
        const Bitboard captures     = pawn_attacks & enemy_pieces;
        for (Square to : captures)
        {
            if (to.Rank() == promotion_rank[color])
            {
                // Capture promotion.
                moves.push_back(Move::Promotion(from, to, kQueen, position.PieceAt(to)));
                moves.push_back(Move::Promotion(from, to, kRook, position.PieceAt(to)));
                moves.push_back(Move::Promotion(from, to, kBishop, position.PieceAt(to)));
                moves.push_back(Move::Promotion(from, to, kKnight, position.PieceAt(to)));
            }
            else
            {
                // Regular capture.
                moves.push_back(Move::Capture(from, to, kPawn, position.PieceAt(to)));
            }
        }
        // En passant capture.
        if (position.EnPassantIndex() != 0)
        {
            if ((pawn_attacks & Bitboard{position.EnPassantIndex()}).IsNotEmpty())
            {
                moves.push_back(Move::EpCapture(from, position.EnPassantIndex()));
            }
        }
    }

    for (auto &[piece, fn] : piece_attackers)
    {
        const Bitboard b = position.PiecesOfType(piece) & friendly_pieces;
        for (Square from : b)
        {
            const Bitboard attacks  = fn(occupied_squares, from);
            const Bitboard captures = attacks & enemy_pieces;
            for (Square to : captures)
            {
                moves.push_back(Move::Capture(from, to, piece, position.PieceAt(to)));
            }
            const Bitboard non_captures = attacks & vacant_squares;
            for (Square to : non_captures)
            {
                moves.push_back(Move::NonCapture(from, to, piece));
            }
        }
    }

    // Generate castling moves
    if (!position.IsInCheck())
    {
        if (color == kWhite)
        {
            if (position.MayWhiteCastleKingside() && (occupied_squares & (Bitboard("F1") | Bitboard("G1"))).IsEmpty() &&
                !position.IsAttacked("F1", kBlack) && !position.IsAttacked("G1", kBlack))
            {
                moves.push_back(Move::Castling("E1", "G1"));
            }
            if (position.MayWhiteCastleQueenside() &&
                (occupied_squares & (Bitboard("B1") | Bitboard("C1") | Bitboard("D1"))).IsEmpty() &&
                !position.IsAttacked("D1", kBlack) && !position.IsAttacked("C1", kBlack))
            {
                moves.push_back(Move::Castling("E1", "C1"));
            }
        }
        else
        {
            if (position.MayBlackCastleKingside() && (occupied_squares & (Bitboard("F8") | Bitboard("G8"))).IsEmpty() &&
                !position.IsAttacked("F8", kWhite) && !position.IsAttacked("G8", kWhite))
            {
                moves.push_back(Move::Castling("E8", "G8"));
            }
            if (position.MayBlackCastleQueenside() &&
                (occupied_squares & (Bitboard("B8") | Bitboard("C8") | Bitboard("D8"))).IsEmpty() &&
                !position.IsAttacked("D8", kWhite) && !position.IsAttacked("C8", kWhite))
            {
                moves.push_back(Move::Castling("E8", "C8"));
            }
        }
    }

    return moves;
}
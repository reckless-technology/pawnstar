#include <cctype>
#include <chrono>
#include <format>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "debug_hashtable.h"
#include "game.h"
#include "opening_book.h"
#include "position.h"
#include "transposition_table.h"

using std::string;

/// @brief Constructor. Seeds the PRNG.
OpeningBook::OpeningBook() : prng_{static_cast<uint32_t>(std::chrono::system_clock::now().time_since_epoch().count())}
{
}

/// @brief Parse the opening book file and create the opening book from it.
/// @param filename Filename of book file.
/// @return true on success.
bool OpeningBook::Initialize(std::string_view filename)
{
    std::ifstream file{string(filename)};
    if (!file)
    {
        return false;
    }
    const bool is_ok = InitializeFromStream(file);
    file.close();
    return is_ok;
}

/// @brief Pseudo randomly select from available book moves
/// @param hash the position hash
/// @return Move selected from book, or Move::None if no move found
Move OpeningBook::GetMove(zobrist_t hash)
{
    if (book_.count(hash))
    {
        const auto &moves = book_[hash];
        return moves[prng_() % moves.size()];
    }
    return Move::None();
}

/// @brief Free the opening book
void OpeningBook::Free()
{
    book_.clear();
}

/// @brief Print available book moves for a position
/// @param position Position to consider for book moves
void OpeningBook::DisplayAvailableMoves(const Position &position)
{
    if (!book_.count(position.Hash()))
    {
        return;
    }
    // Create an associative container, indexed by move, of how many times that move appears for this position.
    std::unordered_map<Move, int, Move> move_counts;
    for (const auto &move : book_[position.Hash()])
    {
        ++move_counts[move];
    }
    std::cout << "MOVE   COUNT\n";
    for (const auto &[move, freq] : move_counts)
    {
        std::cout << std::format("{:<8} {:3}\n", move.ToString(), freq);
    }
}

/// @brief Parse a single line of play and add it to the book. '#' denotes a comment and the rest of the line
/// will be ignored. Move numbers are ignored.
/// @param line The line of play
/// @return true on success
bool OpeningBook::ParseLineOfPlay(std::string_view line)
{
    Position          position{Position::FromString("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1")};
    std::stringstream ss;
    ss << line;
    while (ss)
    {
        string move_string;
        ss >> move_string;
        if (move_string.length() == 0 || isdigit(move_string[0]))
        {
            continue; // Ignore move numbers or blanks.
        }
        if (move_string[0] == '#')
        {
            return true; // Done with this line.
        }
        const zobrist_t hash  = position.Hash();
        auto            moves = position.GenerateLegalMoves();
        auto            i =
            std::ranges::find_if(moves, [&move_string](const Move &move) { return move.ToString() == move_string; });
        if (i != moves.end())
        {
            book_[hash].push_back(*i);
            position = position.MakeMove(*i);
        }
        else
        {
            std::cout << std::format("Error found in book line {} move {}.\n", line, move_string);
            return false;
        }
    }
    return true;
}

/// @brief Resolve a SAN move string to a Move given a position.
/// @param pos Position in which to resolve the move.
/// @param san_view SAN string, e.g. "e4", "Nf3", "O-O", "exd8=Q".
/// @return The matched legal move, or Move::None() if not found.
static Move ParseSan(const Position &pos, std::string_view san_view)
{
    std::string san{san_view};

    // Strip trailing check, checkmate, and annotation symbols.
    while (!san.empty() &&
           (san.back() == '+' || san.back() == '#' || san.back() == '!' || san.back() == '?'))
    {
        san.pop_back();
    }

    if (san.empty())
        return Move::None();

    const MoveList moves = pos.GenerateLegalMoves();

    // Castling — O-O-O must be tested before O-O.
    if (san == "O-O-O" || san == "0-0-0")
    {
        const auto it = std::ranges::find_if(
            moves, [](const Move &m) { return m.type() == Move::CASTLING && m.to().File() == 2; });
        return it != moves.end() ? *it : Move::None();
    }
    if (san == "O-O" || san == "0-0")
    {
        const auto it = std::ranges::find_if(
            moves, [](const Move &m) { return m.type() == Move::CASTLING && m.to().File() == 6; });
        return it != moves.end() ? *it : Move::None();
    }

    // An uppercase letter prefix denotes a non-pawn piece.
    Piece  moving_piece = PAWN;
    size_t i            = 0;
    if (isupper(static_cast<unsigned char>(san[0])))
    {
        switch (san[0])
        {
        case 'N': moving_piece = KNIGHT; break;
        case 'B': moving_piece = BISHOP; break;
        case 'R': moving_piece = ROOK;   break;
        case 'Q': moving_piece = QUEEN;  break;
        case 'K': moving_piece = KING;   break;
        default:  return Move::None();
        }
        i = 1;
    }

    // Detect pawn promotion piece ("e8=Q" style).
    Piece      promo_piece = Piece::NONE;
    const auto eq_pos      = san.find('=');
    if (eq_pos != std::string::npos)
    {
        if (eq_pos + 1 < san.size())
        {
            switch (san[eq_pos + 1])
            {
            case 'Q': promo_piece = QUEEN;  break;
            case 'R': promo_piece = ROOK;   break;
            case 'B': promo_piece = BISHOP; break;
            case 'N': promo_piece = KNIGHT; break;
            default:  return Move::None();
            }
        }
        san.erase(eq_pos); // Drop "=X" suffix.
    }

    // The destination square is always the last two characters.
    if (san.size() < i + 2)
        return Move::None();

    const char dest_file_char = san[san.size() - 2];
    const char dest_rank_char = san[san.size() - 1];
    if (dest_file_char < 'a' || dest_file_char > 'h' || dest_rank_char < '1' || dest_rank_char > '8')
        return Move::None();

    const Square to_sq(dest_file_char - 'a', dest_rank_char - '1');

    // Parse any disambiguation characters between the piece prefix and the destination.
    // 'x' (capture indicator) is intentionally ignored.
    int disambig_file = -1;
    int disambig_rank = -1;
    for (size_t j = i; j + 2 < san.size(); ++j)
    {
        const char c = san[j];
        if      (c >= 'a' && c <= 'h') disambig_file = c - 'a';
        else if (c >= '1' && c <= '8') disambig_rank = c - '1';
    }

    // Search legal moves for a match.
    for (const Move &m : moves)
    {
        // clang-format off
        if (m.piece()     != moving_piece)                                      continue;
        if (m.to()        != to_sq)                                             continue;
        if (disambig_file >= 0 && m.from().File() != (uint8_t)disambig_file)   continue;
        if (disambig_rank >= 0 && m.from().Rank() != (uint8_t)disambig_rank)   continue;
        if (promo_piece   != Piece::NONE && m.promoted() != promo_piece)        continue;
        if (promo_piece   == Piece::NONE && m.promoted() != Piece::NONE)        continue;
        // clang-format on
        return m;
    }
    return Move::None();
}

/// @brief Parse a PGN-format stream and add each game's mainline moves to the opening book.
/// Comments, variations, and annotation glyphs are skipped. A result token (1-0, 0-1,
/// 1/2-1/2, *) resets the position for the next game.
/// @param is Input stream containing one or more PGN games.
/// @return true (individual game parse errors do not abort the whole file).
bool OpeningBook::ParsePgn(std::istream &is)
{
    const string start_fen{"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"};
    Position     position  = Position::FromString(start_fen);
    int          var_depth = 0;
    bool         in_error  = false;
    std::string  token;

    const auto skip_through = [&](char end) {
        char c;
        while (is.get(c) && c != end) {}
    };

    const auto skip_to_eol = [&]() {
        char c;
        while (is.get(c) && c != '\n') {}
    };

    const auto process_token = [&]() {
        if (token.empty())
            return;

        // Result tokens end the current game and reset the position.
        if (token == "1-0" || token == "0-1" || token == "1/2-1/2" || token == "*")
        {
            position = Position::FromString(start_fen);
            in_error = false;
            token.clear();
            return;
        }

        if (in_error)
        {
            token.clear();
            return;
        }

        // Strip any leading move-number prefix, e.g. "1." from "1.e4", or "3..." from "3...Nf6".
        std::string_view san{token};
        size_t           j = 0;
        while (j < san.size() && (isdigit(static_cast<unsigned char>(san[j])) || san[j] == '.'))
            ++j;
        san = san.substr(j);

        // Skip move numbers, ellipsis, NAG glyphs ($n), and other non-move tokens.
        if (san.empty() || san[0] == '$')
        {
            token.clear();
            return;
        }
        const char first        = san[0];
        const bool is_san_token = (first >= 'a' && first <= 'h') ||
                                  first == 'N' || first == 'B' || first == 'R' ||
                                  first == 'Q' || first == 'K' || first == 'O';
        if (!is_san_token)
        {
            token.clear();
            return;
        }

        const Move move = ParseSan(position, san);
        if (move == Move::None())
        {
            std::cout << std::format("PGN parse error: unrecognised move '{}'\n", san);
            in_error = true;
            token.clear();
            return;
        }

        book_[position.Hash()].push_back(move);
        position = position.MakeMove(move);
        token.clear();
    };

    char c;
    while (is.get(c))
    {
        // clang-format off
        if (c == '{')                                { process_token(); skip_through('}'); continue; }
        if (c == ';')                                { process_token(); skip_to_eol();    continue; }
        if (c == '(')                                { process_token(); ++var_depth;      continue; }
        if (c == ')')                                { if (var_depth > 0) --var_depth;    continue; }
        if (var_depth > 0)                           continue; // Inside a variation: skip.
        if (c == '[')                                { process_token(); skip_through(']'); continue; }
        if (isspace(static_cast<unsigned char>(c)))  { process_token();                   continue; }
        // clang-format on
        token += c;
    }
    process_token(); // Handle any token not followed by whitespace.
    return true;
}

/// @brief Parse a multiline stringstream containing open book lines of play and create the opening book from it.
/// @param iss stringstream to be parsed, with one line of play per new line.
/// @return true on success
bool OpeningBook::InitializeFromStream(std::istream &ss)
{
    // Skip leading whitespace to detect the file format.
    while (ss.peek() != EOF && isspace(static_cast<unsigned char>(ss.peek())))
        ss.get();

    // A '[' at the start of content indicates PGN format; otherwise use the line-of-play format.
    if (ss.peek() == '[')
        return ParsePgn(ss);

    while (ss)
    {
        string line_of_play;
        std::getline(ss, line_of_play);
        if (!ParseLineOfPlay(line_of_play))
        {
            return false;
        }
    }
    return true;
}
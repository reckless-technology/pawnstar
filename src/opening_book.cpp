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
#include "random.h"
#include "transposition_table.h"

using std::ifstream;
using std::istream;
using std::string;
using std::string_view;
using std::stringstream;
using std::unordered_map;
using std::vector;

/**
 * @brief The opening book is stored as a map, indexed by the Zobrist
 * hash of the position, containing a vector of Moves which are
 * available from that position. The same move may be in the
 * moves vector multiple times if it appears in several lines of
 * play, i.e. the frequency of occurence of a move in the list
 * is proportional to the liklihood that we want to play it.
 */
static unordered_map<uint64_t, vector<Move>> book;

static bool InitializeOpeningBookFromStream(istream &ss);

/**
 * @brief Parse the opening book file and create the opening book from it.
 * @param filename Filename of book file.
 * @return true on success.
 */
bool InitializeOpeningBookFromFile(string_view filename)
{
    ifstream file{string(filename)};
    if (!file)
    {
        return false;
    }
    const bool is_ok = InitializeOpeningBookFromStream(file);
    file.close();
    return is_ok;
}

/**
 * @brief Pseudo randomly select from available book moves
 * @param hash the position hash
 * @return Move selected from book, or 0 if no move found
 */
Move GetBookMove(uint64_t hash)
{
    if (book.count(hash))
    {
        const auto &moves = book[hash];
        return moves[NextRandom() % moves.size()];
    }
    return Move::None();
}

/**
 * @brief Free the opening book
 */
void FreeOpeningBook()
{
    book.clear();
}

/**
 * @brief Print available book moves for a position
 * @param position Position to consider for book moves
 */
void DisplayAvailableBookMoves(const Position &position)
{
    if (!book.count(position.Hash()))
    {
        return;
    }
    unordered_map<Move, int, Move> move_counts;
    for (const auto &move : book[position.Hash()])
    {
        ++move_counts[move];
    }
    printf("MOVE   COUNT\n");
    MoveList move_list = position.GenerateLegalMoves();
    for (const auto &[move, freq] : move_counts)
    {
        string move_string{position.MoveToString(move, &move_list)};
        printf("%-8s %3d\n", move_string.c_str(), freq);
    }
}

/**
 * @brief Parse a single line of play (PGN line) and add it to the book.
 *        Moves ending with a '?' are bad moves and will not be played by pawnstar.
 *        '#' denotes a comment and the rest of the line will be ignored
 *        Move numbers are ignored (the line is in PGN format).
 * @param line The line of play
 * @return true on success
 */
static bool ParseLineOfPlay(std::string_view line)
{
    Game         game{};
    stringstream ss;
    ss << line;
    while (ss)
    {
        string move_string;
        ss >> move_string;
        if (move_string.length() == 0 || isdigit(move_string[0]))
        {
            continue; /* Ignore move numbers or blanks. */
        }
        if (move_string[0] == '#')
        {
            return true; /* Done with this line. */
        }
        const uint64_t hash = game.CurrentPosition().Hash();
        const Move     move = game.PlayMove(move_string);
        if (!move)
        {
            std::cout << "ERROR found in book line " << line << " move " << move_string << std::endl;
            return false;
        }
        if (move_string.find('?') == string::npos) /* Ignore bad moves */
        {
            book[hash].push_back(move);
        }
    }
    return true;
}

/**
 * @brief Parse a multiline stringstream containing open book lines of
 *        play and create the opening book from it.
 * @param iss stringstream to be parsed, with one line of play per new line.
 * @return true on success
 */
static bool InitializeOpeningBookFromStream(istream &ss)
{
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
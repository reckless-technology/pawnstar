#include <string>
#include <vector>
#include <unordered_map>
#include <sstream>
#include <fstream>

#include "pawnstar.h"

using std::string;
using std::vector;
using std::unordered_map;
using std::stringstream;
using std::ifstream;

/**
 * @brief The opening book is stored as a map, indexed by the Zobrist 
 * hash of the position, containing a vector of Moves which are
 * available from that position. The same move may be in the 
 * moves vector multiple times if it appears in several lines of
 * play, i.e. the frequency of occurence of a move in the list
 * is proportional to the liklihood that we want to play it.
 */
static unordered_map<uint64_t, vector<Move>> book;

static bool InitializeOpeningBookFromStringStream(stringstream& ss);

/**
 * @brief Parse the opening book file and create the opening book from it.
 * @param filename Filename of book file.
 * @return true on success.
 */
bool InitializeOpeningBookFromFile(const char* filename)
{
    ifstream file { filename };
    if (!file.is_open())
    {
        return false;
    }
    stringstream ss;
    ss << file.rdbuf();
    file.close();
    return InitializeOpeningBookFromStringStream(ss);
}

/**
 * @brief Use the internal opening book if a book file is 
 *        not available
 * @return true on success
 */
bool InitializeDefaultOpeningBook()
{
    extern const char* OPENING_BOOK_MOVES;
    stringstream ss { OPENING_BOOK_MOVES };
    return InitializeOpeningBookFromStringStream(ss);
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
        const auto& moves = book[hash];
        return moves[NextRandom() % moves.size()];
    }
    return 0;
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
void DisplayAvailableBookMoves(const Position& position)
{
    if (!book.count(position.hash_))
    {
        return;
    }
    unordered_map<Move, int> move_counts;
    for (const auto& move : book[position.hash_])
    {
        ++move_counts[move];
    }
    printf("MOVE   COUNT\n");
    for (const auto& [move, freq] : move_counts)
    {
        char buffer[16];
        MoveToString(position, move, buffer);
        printf("%-8s %3d\n", buffer, freq);
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
static bool ParseLineOfPlay(const string& line)
{
    Game game;
    InitializeGame(game);
    stringstream ss { line };
    while (ss)
    {
        string token;
        ss >> token;
        if (token.length() == 0 || isdigit(token[0]))
        {
            continue; /* Ignore move numbers or blanks. */
        }
        if (token[0] == '#')
        {
            return true; /* Done with this line. */
        }
        char move_string_mutable[16];
        strncpy(move_string_mutable, token.c_str(), sizeof(move_string_mutable) - 1);
        const uint64_t hash = game.position->hash_;
        const Move move = PlayMoveString(game, move_string_mutable);
        if (move == 0)
        {
            printf("ERROR found in book line '%s' move '%s'\n", line.c_str(), token.c_str());
            return false;
        }
        if (token.find('?') == string::npos) /* Ignore bad moves */
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
static bool InitializeOpeningBookFromStringStream(stringstream& ss)
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
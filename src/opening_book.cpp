#include "pawnstar.h"
#include <ctype.h>
#include <string.h>

#if _MSC_VER
#define strtok_r strtok_s
#endif

/**
 * @file Functions to use an opening book.
 * Opening book text files are 1 line per line of play, with moves in SAN format.
 * The book itself is stored as a singly linked list. 
 * Book moves are indexed according to the Zobrist hash of the position.
*/

typedef struct BookPosition BookPosition;
/**
 * @brief Structure containing a node of the opening book linked list.
 */
struct BookPosition
{
    uint64_t        hash;       /**< Hash of the position for this entry    */
    int             capacity;   /**< Current capacity of moves array        */
    int             count;      /**< Current number of moves in array       */
    Move*           moves;      /**< The moves available for this position  */
    BookPosition*   next;       /**< Next entry in the linked list          */
};

static BookPosition* opening_book = NULL;   /**< Head of opening book linked list   */

/**
 * @brief Create a new book entry and return a pointer to it.
 * @param hash Zobrist hash for this position.
 * @return Pointer to new position entry.
*/
static BookPosition* NewBookPosition(uint64_t hash)
{
    BookPosition* bp = (BookPosition*)malloc(sizeof(BookPosition));
    bp->hash = hash;
    bp->capacity = 1;
    bp->count = 0;
    bp->moves = (Move*)malloc(sizeof(Move));
    bp->next = NULL;
    return bp;
}

/**
 * @brief Find an entry in the book
 * @param hash Zobrist hash to search for
 * @return Pointer to entry if found, else NULL
 */
static BookPosition* FindBookPosition(uint64_t hash)
{
    BookPosition* bp;
    for (bp = opening_book; bp; bp = bp->next)
    {
        if (bp->hash == hash)
        {
            break;
        }
    }
    return bp;
}

/**
 * @brief Add a new book move
 * @param hash Zobrist hash of the position
 * @param move Move to add
 */
static void AddMove(uint64_t hash, Move move)
{
    BookPosition* bp = FindBookPosition(hash);
    if (!bp)
    {
        bp = NewBookPosition(hash);
        bp->next = opening_book;
        opening_book = bp;
    }
    if (bp->count == bp->capacity)
    {
        bp->capacity <<= 1;
        bp->moves = (Move*)realloc(bp->moves, bp->capacity * sizeof(Move));
    }
    bp->moves[bp->count++] = move;
}

/**
 * @brief Pseudo randomly select from available book moves
 * @param hash the position hash
 * @return Move selected from book, or 0 if no move found
*/
Move GetBookMove(uint64_t hash)
{
    const BookPosition* bp = FindBookPosition(hash);
    return bp ? bp->moves[(unsigned)NextRandom() % bp->count] : 0;
}

/**
 * @brief Free the opening book
*/
void FreeOpeningBook()
{
    BookPosition* bp = opening_book;
    while (bp)
    {
        BookPosition* next = bp->next;
        free(bp->moves);
        free(bp);
        bp = next;
    }
    opening_book = NULL;
}

/**
 * @brief Find a move in an array of moves.
 * @param moves Array to search
 * @param move move to search for
 * @return Pointer to move found or NULL if not found
*/
constexpr const Move* 
FindMove(const Move* moves, Move move)
{
    while (*moves)
    {
        if (*moves == move)
        {
            return moves;
        }
        ++moves;
    }
    return NULL;
}

/**
 * @brief Print available book moves for a position
 * @param position Position to consider for book moves
*/
void DisplayAvailableBookMoves(const Position* position)
{
    Move book_moves[MAX_MOVES_PER_POSITION] = { 0 };
    int move_counts[MAX_MOVES_PER_POSITION] = { 0 };
    int num_discrete_moves = 0;
    const BookPosition* bp = FindBookPosition(position->hash);
    if (!bp)
    {
        return;
    }
    for (int i = 0; i != bp->count; ++i)
    {
        const Move* move = FindMove(book_moves, bp->moves[i]);
        if (!move)
        {
            /* This is the first time we have seen this book move */
            book_moves[num_discrete_moves] = bp->moves[i];
            move_counts[num_discrete_moves] = 1;
            ++num_discrete_moves;
        }
        else
        {
            ++move_counts[move - book_moves]; /* increment the number of times we saw it */
        }
    }
    printf("move   count\n");
    for (int i = 0; i != num_discrete_moves; ++i)
    {
        char buffer[16];
        MoveToString(position, book_moves[i], buffer);
        printf("%-8s %3u\n", buffer, move_counts[i]);
    }
}

/**
 * @brief Parse a multiline string containing open book lines of play
 *        and create the opening book from it.
 * @param book_string String to be parsed, newline per line of play.
 * @return true on success
 */
bool InitializeOpeningBookFromString(const char* book_string)
{
    bool result = true;
    char* const book = strdup(book_string);
    int line_number = 0;
    char* line_ctx = NULL;
    char* move_ctx = NULL;
    FreeOpeningBook();
    // Split the string into lines
    for (char* line = strtok_r(book, "\n", &line_ctx);
        line;
        line = strtok_r(NULL, "\n", &line_ctx))
    {
        Game game[1];
        ++line_number;
        InitializeGame(game);
        // Split the line into tokens separated by spaces
        for (char* move_str = strtok_r(line, " ", &move_ctx);
            move_str;
            move_str = strtok_r(NULL, " ", &move_ctx))
        {
            const uint64_t hash = game->position->hash;
            if (move_str[0] == '#')
            {
                break; // comment - ignore rest of line
            }
            if (isdigit(move_str[0]))
            {
                continue; // move number only
            }
            char* const c = strchr(move_str, '?');
            const bool is_bad_move = !!c;
            if (c)
            {
                *c = '\0';
            }
            const int move = PlayMoveString(game, move_str);
            if (!move)
            {
                printf("ERROR: illegal move '%s' found in line %u of opening book\n", move_str, line_number);
                result = false;
                break;
            }
            if (!is_bad_move)
            {
                AddMove(hash, move);
            }
        }
    }
    free(book);
    return result;
}

/**
 * @brief Parse the opening book file and create the opening book from it.
 * @param filename Filename of book file.
 * @return true on success.
 */
bool InitializeOpeningBookFromFile(const char filename[])
{
    char* buffer;
    long len;
    bool result;
    FILE* file = fopen(filename, "r");
    if (!file)
    {
        printf("NOTE: unable to open book file '%s'\n", filename);
        return false;
    }
    fseek(file, 0, SEEK_END);
    len = ftell(file);
    fseek(file, 0, SEEK_SET);
    buffer = (char*)malloc(len + 1);
    if (fread(buffer, 1, len, file) == 0)
    {
        printf("NOTE: unable to read from book file '%s'\n", filename);
    }
    fclose(file);
    buffer[len] = '\0';
    result = InitializeOpeningBookFromString(buffer);
    free(buffer);
    return result;
}

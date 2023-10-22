#include "pawnstar.h"
#include <ctype.h>
#include <string.h>

#if _MSC_VER
#define strtok_r strtok_s
#endif

/*
Functions to use an opening book.
Opening book text files are 1 line per line of play, with moves in SAN format.

The book itself is stored as a singly linked list. Book moves are indexed
according to the Zobrist hash of the position.

Structure containing a node of the opening book linked list.
*/
typedef struct BookPosition BookPosition;
struct BookPosition
{
    uint64_t        hash;
    int             capacity;
    int             count;
    int* moves;
    BookPosition* next;
};
/*
Head pointer
*/
static BookPosition* opening_book = NULL;
/*
Create a new BookPosition and return a pointer to it
*/
static BookPosition* NewBookPosition(uint64_t hash)
{
    BookPosition* bp = malloc(sizeof(BookPosition));
    bp->hash = hash;
    bp->capacity = 1;
    bp->count = 0;
    bp->moves = malloc(sizeof(int));
    bp->next = NULL;
    return bp;
}
/*
Find a BookPosition from a position hash (NULL if not found)
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
/*
Add a new move to the opening book
*/
static void AddMove(uint64_t hash, int move)
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
        bp->moves = realloc(bp->moves, bp->capacity * sizeof(int));
    }
    bp->moves[bp->count++] = move;
}
/*
Select one from the possible book moves for the current position
Returns 0 if there is no book move for this position.
*/
int GetBookMove(uint64_t hash)
{
    const BookPosition* bp = FindBookPosition(hash);
    return bp ? bp->moves[(unsigned)NextRandom() % bp->count] : 0;
}
/*
Free the heap memory used by the opening book
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
/*
Show all possible book moves for this position
*/
void DisplayAvailableBookMoves(const Position* position)
{
    int book_moves[MAX_MOVES_PER_POSITION] = { 0 };
    int move_counts[MAX_MOVES_PER_POSITION] = { 0 };
    int next_move = 0;
    const BookPosition* bp = FindBookPosition(position->hash);
    if (!bp)
    {
        return;
    }
    for (int i = 0; i != bp->count; ++i)
    {
        const int* move = FindMove(book_moves, bp->moves[i]);
        if (!move)
        {
            /* This is the first time we have seen this book move */
            book_moves[next_move] = bp->moves[i];
            move_counts[next_move] = 1;
            ++next_move;
        }
        else
        {
            ++move_counts[move - book_moves]; /* increment the number of times we saw it */
        }
    }
    printf("move   count\n");
    for (int i = 0; i != next_move; ++i)
    {
        char buffer[16];
        MoveToString(position, book_moves[i], buffer);
        printf("%-8s %3u\n", buffer, move_counts[i]);
    }
}
/*
Initialize an opening book from a string, with each line containing a book
line of play.
Returns true on success.
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

/*
Parse the book text file and build the opening book from it
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
    buffer = malloc(len + 1);
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

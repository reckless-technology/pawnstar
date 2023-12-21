#include <vector>

#include "position.h"
#include "debug_hashtable.h"
#include "transposition_table.h"
#include "function_prototypes.h"

static bool IsPrime(size_t x);

static std::vector<Transposition> transposition_table;

/**
 * @brief Delete the transposition table.
 */
void 
FreeTranspositionTable()
{
    transposition_table.clear();
}

/**
 * @brief Create the transposition table.
 * @param megabytes Approx max size of the table in megabytes. May be slightly smaller (table is prime size).
 * @return true on success
 */
void 
InitializeTranspositionTable(size_t megabytes)
{
    size_t num_entries = (megabytes * MEGABYTE) / sizeof(Transposition);
    /* Find the next smallest prime number and make the table that size */
    if ((num_entries & 1) == 0)
    {
        --num_entries;
    }
    while (!IsPrime(num_entries))
    {
        num_entries -= 2;
    }
    transposition_table.clear();
    transposition_table.resize(num_entries);
}

/**
 * @brief Find an entry in the TT if one exists for this position.
 * @param hash Zobrist hash of position
 * @param transposition Reference to transposition to assign if found
 * @return true on success
 */
bool 
FindTransposition(uint64_t       hash, 
                  Transposition& transposition)
{
    const Transposition& t = transposition_table[hash % transposition_table.size()];
    if (t.hash == hash)
    {
        transposition = t;
        return true;
    }
    return false;
}

/**
 * @brief Insert a new entry into the table.
 * @param hash Zobrist hash of position.
 * @param depth Search depth.
 * @param score Score for this position.
 * @param move Best move found.
 * @param node_type Node type.
 */
void 
RecordTransposition(uint64_t hash, 
                    int      depth, 
                    int      score, 
                    Move     move, 
                    int      node_type)
{   
    Transposition& t = transposition_table[hash % transposition_table.size()];
    if (t.hash == 0 || t.hash == hash || t.is_previous || t.depth < depth)
    {
        t.hash          = hash;
        t.move          = move;
        t.score         = score;
        t.depth         = depth;
        t.node_type     = node_type;
        t.is_previous   = false;
    }
}

void
ShowTableUsage()
{
    size_t count = 0;
    for (size_t i = 0; i != transposition_table.size(); ++i)
    {
        if (transposition_table[i].hash != 0)
        {
            ++count;
        }
    }
    printf("Transposition table used %zu of %zu entries (%zu%% full)\n", 
        count, 
        transposition_table.size(),
        (count * 100) / transposition_table.size());
}

void
AgeTranspositionTable()
{
    for (auto& t : transposition_table)
    {
        if (t.hash == 0)
        {
            t.is_previous = true;
        }
    }
}

/**
 * @brief determine if a number is a prime number.
 * We get marginally better hash dispersion when the hashtable size is a prime number
 * @param x candidate
 * @return true if x is prime
*/
static bool IsPrime(size_t x)
{
    if (x < 3)
    {
        return false;
    }
    for (size_t i = 2; i * i <= x; ++i)
    {
        if (x % i == 0)
        {
            return false;
        }
    }
    return true;
}
#include <algorithm>
#include <vector>

#include "debug_hashtable.h"
#include "position.h"
#include "transposition_table.h"

static bool IsPrime(std::size_t x);

/**
 * Transposition table is just a vector of Transpositions, which
 * we index by using the Zobrist hash of the position.
 */
static std::vector<Transposition> table;

/**
 * @brief Delete the transposition table.
 */
void FreeTranspositionTable()
{
    table.clear();
}

/**
 * @brief Create the transposition table.
 * @param megabytes Approx max size of the table in megabytes. May be slightly smaller (table is prime size).
 * @return true on success
 */
void InitializeTranspositionTable(std::size_t megabytes)
{
    std::size_t num_entries = (megabytes * MEGABYTE) / sizeof(Transposition);
    /* Find the next smallest prime number and make the table that size */
    if ((num_entries & 1) == 0)
    {
        --num_entries;
    }
    while (!IsPrime(num_entries))
    {
        num_entries -= 2;
    }
    table.clear();
    table.resize(num_entries);
}

/**
 * @brief Find an entry in the TT if one exists for this position.
 * @param hash Zobrist hash of position
 * @param transposition Reference to transposition to assign if found
 * @return true on success
 */
bool FindTransposition(uint64_t hash, Transposition &transposition)
{
    const Transposition &t = table[hash % table.size()];
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
void RecordTransposition(uint64_t hash, int depth, int score, Move move, NodeType node_type)
{
    Transposition &t = table[hash % table.size()];
    if (t.hash == 0 || t.is_old || t.depth < depth)
    {
        t.hash      = hash;
        t.move      = move;
        t.score     = score;
        t.depth     = depth;
        t.node_type = node_type;
        t.is_old    = false;
    }
}

std::pair<int, int> TranspositionTableUsage()
{
    const std::size_t count = std::ranges::count_if(table, [](const Transposition &t) { return t.hash != 0; });
    return std::pair<int, int>{count, (count * 100) / table.size()};
}

void AgeTranspositionTable()
{
    std::ranges::for_each(table, [](Transposition &t) { t.is_old = true; });
}

/**
 * @brief determine if a number is a prime number.
 * We get marginally better hash dispersion when the hashtable size is a prime number
 * @param x candidate
 * @return true if x is prime
 */
static bool IsPrime(std::size_t x)
{
    if (x < 3)
    {
        return false;
    }
    for (std::size_t i = 2; i * i <= x; ++i)
    {
        if (x % i == 0)
        {
            return false;
        }
    }
    return true;
}
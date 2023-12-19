#include "position.h"
#include "debug_hashtable.h"
#include "transposition_table.h"
#include "function_prototypes.h"

static bool IsPrime(int x);

constexpr int num_entries_per_bucket = 4;

struct HashBucket
{
    Transposition entries[num_entries_per_bucket];
};

static HashBucket*  transposition_table;
static int          table_num_entries;
/*
Delete the transposition table
*/
void 
FreeTranspositionTable(void)
{
    if (transposition_table)
    {
        free(transposition_table);
        transposition_table = NULL;
        table_num_entries   = 0;
    } 
}
/*
Create the transposition table of a specified maximum size
*/
bool 
InitializeTranspositionTable(int megabytes)
{
    FreeTranspositionTable();
    table_num_entries = (megabytes * MEGABYTE) / sizeof(HashBucket);
    /* Find the next smallest prime number and make the table that size */
    if ((table_num_entries & 1) == 0)
    {
        --table_num_entries;
    }
    while (!IsPrime(table_num_entries))
    {
        table_num_entries -= 2;
    }
    transposition_table = (HashBucket*)calloc(table_num_entries, sizeof(HashBucket));
    if (!transposition_table)
    {
        printf("ERROR: unable to create transposition transposition_table of %u megabytes\n", megabytes);
        table_num_entries = 0;
        return false;
    }
    return true;
}
/*
Find a transposition entry for this position if one exists
*/
bool 
FindTransposition(uint64_t hash, 
                  Transposition& transposition)
{
    const HashBucket& bucket = transposition_table[hash % table_num_entries]; 
    for (int i = num_entries_per_bucket - 1; i >= 0; --i)
    {
        const Transposition& t = bucket.entries[i];
        if (t.hash == hash)
        {
            transposition = t;
            return true;
        }
    }
    return false;
}

/**
 * @brief Insert a new entry into the table.
 * Hash replacement policy:
 * - Replace entry with same hash
 * - Replace empty entry
 * - Replace entry with lowest depth
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
    HashBucket& bucket = transposition_table[hash % table_num_entries];
    Transposition* candidate = &bucket.entries[0];
    for (int i = num_entries_per_bucket - 1; i >= 0; --i)
    {
        if (bucket.entries[i].hash == hash)
        {
            candidate = &bucket.entries[i];
            break;
        }
        if (bucket.entries[i].hash == 0)
        {
            candidate = &bucket.entries[i];
            continue;
        }
        if (bucket.entries[i].depth < candidate->depth)
        {
            candidate = &bucket.entries[i];
        }
    }
    candidate->hash = hash;
    candidate->move = move;
    candidate->score = score;
    candidate->depth = depth;
    candidate->node_type = node_type;
    
}

/*
We get marginally better dispersion when the hashtable size is a prime number
Determine if a candidate size is prime (excluding 1 and 2)
*/
static bool IsPrime(int x)
{
    int i;
    if (x < 3)
    {
        return false;
    }
    for (i = 2; i * i <= x; ++i)
    {
        if (x % i == 0)
        {
            return false;
        }
    }
    return true;
}
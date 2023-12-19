#include "position.h"
#include "debug_hashtable.h"
#include "transposition_table.h"
#include "function_prototypes.h"

static bool IsPrime(int x);

struct TableEntry
{
    Transposition entries[2];
};

static TableEntry*  transposition_table;
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
    table_num_entries = (megabytes * MEGABYTE) / sizeof(TableEntry);
    /* Find the next smallest prime number and make the table that size */
    if ((table_num_entries & 1) == 0)
    {
        --table_num_entries;
    }
    while (!IsPrime(table_num_entries))
    {
        table_num_entries -= 2;
    }
    transposition_table = (TableEntry*)calloc(table_num_entries, sizeof(TableEntry));
    if (!transposition_table)
    {
        printf("ERROR: unable to create transposition transposition_table of %u megabytes\n", megabytes);
        table_num_entries = 0;
        return false;
    }
    return true;
}

/**
 * @brief Find an entry in the TT if one exists for this position.
 * @param hash Zobrist hash of position
 * @param transposition Reference to transposition to assign if found
 * @return true on success
 */
bool 
FindTransposition(uint64_t hash, 
                  Transposition& transposition)
{
    const TableEntry& bucket = transposition_table[hash % table_num_entries]; 
    if (bucket.entries[0].hash == hash)
    {
        transposition = bucket.entries[0];
        return true;
    }
    if (bucket.entries[1].hash == hash)
    {
        transposition = bucket.entries[1];
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
    TableEntry& bucket = transposition_table[hash % table_num_entries];
    if (bucket.entries[0].hash == 0 || bucket.entries[0].hash == hash)
    {
        bucket.entries[0].hash      = hash;
        bucket.entries[0].move      = move;
        bucket.entries[0].score     = score;
        bucket.entries[0].depth     = depth;
        bucket.entries[0].node_type = node_type;
        return;
    }
    if (bucket.entries[1].hash != 0)
    {
        bucket.entries[0] = bucket.entries[1];
    }
    bucket.entries[1].hash      = hash;
    bucket.entries[1].move      = move;
    bucket.entries[1].score     = score;
    bucket.entries[1].depth     = depth;
    bucket.entries[1].node_type = node_type;
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
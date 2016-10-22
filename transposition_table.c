#include "pawnstar.h"

#define TRANSPOSITIONS_PER_BUCKET 4

static bool IsPrime(int x);

typedef struct HashBucket
{
    Transposition   transpositions[TRANSPOSITIONS_PER_BUCKET];
} HashBucket;

static HashBucket*  transposition_table;
static int          table_bucket_count;
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
        table_bucket_count   = 0;
    } 
}
/*
Create the transposition table of a specified maximum size
*/
bool 
InitializeTranspositionTable(int megabytes)
{
    FreeTranspositionTable();
    table_bucket_count = (megabytes * MEGABYTE) / sizeof(HashBucket);
    /* Find the next smallest prime number and make the table that size */
    if ((table_bucket_count & 1) == 0)
    {
        --table_bucket_count;
    }
    while (!IsPrime(table_bucket_count))
    {
        table_bucket_count -= 2;
    }
    transposition_table = calloc(table_bucket_count, sizeof(HashBucket));
    if (!transposition_table)
    {
        printf("ERROR: unable to create transposition transposition_table of %u megabytes\n", megabytes);
        table_bucket_count = 0;
        return false;
    }
    return true;
}
/*
Find a transposition entry for this position if one exists
*/
bool 
FindTransposition(uint64 hash, 
                  Transposition* transposition)
{
    int i;
    const HashBucket* const bucket = &transposition_table[hash % table_bucket_count]; 
    const Transposition* t = bucket->transpositions;
    for (i = TRANSPOSITIONS_PER_BUCKET; i != 0; --i, ++t)
    {
        if (t->hash == hash)
        {
            *transposition = *t;
            return true;
        }
    }
    return false;
}
/*
Insert a new entry into the transposition table.
Hash bucket replacement policy (in priority order):
    # replace an entry with the same hash
    # replace an empty slot
    # replace the entry with the smallest depth
*/
void 
RecordTransposition(uint64 hash, 
                    int    depth, 
                    int    score, 
                    int    move, 
                    int    node_type)
{   
    HashBucket* const bucket = &transposition_table[hash % table_bucket_count]; 
    int best_score           = 0;
    Transposition* candidate = bucket->transpositions;
    Transposition* t         = bucket->transpositions;
    for (int i = TRANSPOSITIONS_PER_BUCKET; i != 0; --i, ++t)
    {
        const int s = 
            4 * (t->hash == hash) +
            2 * (t->hash == 0)    +
                (t->depth < candidate->depth);
        if (s > best_score)
        {
            best_score = s;
            candidate  = t;
            if (s >= 4)
            {
                break;
            }
        }
    }
    candidate->hash      = hash;
    candidate->move      = move;
    candidate->score     = (short)score;
    candidate->depth     = (int8)depth;
    candidate->node_type = (uint8)node_type;
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
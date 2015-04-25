#include "pawnstar.h"

#define MEGABYTE                    0x100000
#define TRANSPOSITIONS_PER_BUCKET   13
#define SMALL_HASTABLE_SIZE         4999

static bool IsPrime(int x);

typedef struct HashBucket
{
    Transposition   transpositions[TRANSPOSITIONS_PER_BUCKET];
} HashBucket;

static HashBucket*      transposition_table;
static int              table_bucket_count;
static Transposition    small_transposition_table[SMALL_HASTABLE_SIZE]; /* fits in L1 cache */
/******************************************************************************
Delete the transposition table
NB: NOT thread safe
*******************************************************************************/
void FreeTranspositionTable(void)
{
    if (transposition_table)
    {
        free(transposition_table);
        transposition_table = NULL;
        table_bucket_count   = 0;
    } 
}
/******************************************************************************
Create the transposition table of a specified maximum size
NB: NOT thread safe
*******************************************************************************/
bool InitializeTranspositionTable(int megabytes)
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
/******************************************************************************
Find a transposition entry for this position if one exists
*******************************************************************************/
bool FindTransposition(uint64 hash, Transposition* transposition)
{
    Transposition s = small_transposition_table[hash % SMALL_HASTABLE_SIZE];
    if ((s.hash ^ s.payload) == hash)
    {
        INCREMENT("table hit small table");
        *transposition = s;
        return true;
    }
    HashBucket* const bucket = &transposition_table[hash % table_bucket_count]; 
    for (int i = TRANSPOSITIONS_PER_BUCKET - 1; i >= 0; --i)
    {
        if ((bucket->transpositions[i].hash ^ bucket->transpositions[i].payload) == hash)
        {
            *transposition = bucket->transpositions[i];
            if ((transposition->hash ^ transposition->payload) == hash) /* check another thread didn't pre-empt */
            {
                return true;
            }            
        }
    }
    return false;
}
/******************************************************************************
Insert a new entry into the transposition table.
Hash bucket replacement policy (in priority order):
# replace an entry with the same hash
# replace an empty slot
# replace a non PV node
# replace the entry with the smallest depth
*******************************************************************************/
void RecordTransposition(uint64 hash, int depth, int score, int move, int node_type)
{  
    Transposition t = 
    {
        .depth     = (short)depth,
        .score     = (short)score,
        .move      = move,
        .node_type = node_type
    };
    t.hash = hash ^ t.payload;
    int replace = 0;  
    int best_score = 0;
    HashBucket* const bucket = &transposition_table[hash % table_bucket_count];  
    for (int i = TRANSPOSITIONS_PER_BUCKET - 1; i >= 0; --i)
    {
        const int score = 
            8 * ((bucket->transpositions[i].hash ^ bucket->transpositions[i].payload) == hash) +
            4 *  (bucket->transpositions[i].hash == 0)                                         +
            2 *  (bucket->transpositions[i].node_type != NODE_PV)                              +
                 (bucket->transpositions[i].depth < bucket->transpositions[replace].depth);

        if (score > best_score)
        {
            best_score = score;
            replace = i;
            if (score >= 8)
            {
                break;
            }
        }
    }
    bucket->transpositions[replace] = t;
    small_transposition_table[hash % SMALL_HASTABLE_SIZE] = t;
}
/******************************************************************************
We get marginally better dispersion when the hashtable size is a prime number
Determine if a candidate size is prime (excluding 1 and 2)
*******************************************************************************/
static bool IsPrime(int x)
{
    if (x < 3)
    {
        return false;
    }
    for (int i = 2; i * i <= x; ++i)
    {
        if (x % i == 0)
        {
            return false;
        }
    }
    return true;
}
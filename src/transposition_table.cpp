#include "pawnstar.h"

static bool IsPrime(int x);

static Transposition*   transposition_table;
static int              table_num_entries;
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
    table_num_entries = (megabytes * MEGABYTE) / sizeof(Transposition);
    /* Find the next smallest prime number and make the table that size */
    if ((table_num_entries & 1) == 0)
    {
        --table_num_entries;
    }
    while (!IsPrime(table_num_entries))
    {
        table_num_entries -= 2;
    }
    transposition_table = (Transposition*)calloc(table_num_entries, sizeof(Transposition));
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
                  Transposition* transposition)
{
    const Transposition* const t = &transposition_table[hash % table_num_entries]; 
    if (t->hash == hash)
    {
        *transposition = *t;
        return true;
    }
    return false;
}
/*
Insert a new entry into the transposition table.

*/
void 
RecordTransposition(uint64_t hash, 
                    int      depth, 
                    int      score, 
                    Move     move, 
                    int      node_type)
{   
    Transposition* const t  = &transposition_table[hash % table_num_entries];
    t->hash                 = hash;
    t->move                 = move;
    t->score                = (int16_t)score;
    t->depth                = (int8_t)depth;
    t->node_type            = (uint8_t)node_type;
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
#include "pawnstar.h"

static bool IsPrime(int x);

static Transposition*   transposition_table;
static int              table_num_entries;

static Transposition*   quiescent_table;
static int              quiescent_num_entries;

void 
FreeTranspositionTable(void)
{
    if (transposition_table)
    {
        free(transposition_table);
        transposition_table = NULL;
        table_num_entries = 0;
    } 
}


bool 
InitializeTranspositionTable(int megabytes, int quiescent_megabytes)
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
    transposition_table = calloc(table_num_entries, sizeof(Transposition));
    if (!transposition_table)
    {
        printf("ERROR: unable to create transposition transposition_table of %u megabytes\n", megabytes);
        table_num_entries = 0;
        return false;
    }
    quiescent_num_entries = (quiescent_megabytes * MEGABYTE) / sizeof(Transposition);
    if ((quiescent_num_entries & 1) == 0)
    {
        --quiescent_num_entries;
    }
    while (!IsPrime(quiescent_num_entries))
    {
        quiescent_num_entries -= 2;
    }
    quiescent_table = calloc(quiescent_num_entries, sizeof(Transposition));
    if (!quiescent_table)
    {
        printf("ERROR: unable to create quiescent transposition transposition_table of %u megabytes\n", quiescent_megabytes);
        quiescent_num_entries = 0;
        return false;
    }
    return true;
}


bool 
FindTransposition(uint64_t         hash, 
                  Transposition*   transposition,
                  bool             is_quiescent)
{
    const Transposition* const t = is_quiescent ? 
        &quiescent_table[hash & quiescent_num_entries] : 
        &transposition_table[hash % table_num_entries];
    if (t->hash == hash)
    {
        *transposition = *t;
        return true;
    }
    return false;
}


void 
RecordTransposition(uint64_t hash, 
                    int      depth, 
                    int      score, 
                    int      move, 
                    int      node_type,
                    bool     is_quiescent)
{   
    Transposition* const t = is_quiescent ? 
        &quiescent_table[hash & quiescent_num_entries] : 
        &transposition_table[hash % table_num_entries];
    INCREMENT_IF(t->hash && t->hash != hash, "hash table collisions");
    t->hash         = hash;
    t->move         = move;
    t->score        = (int16_t)score;
    t->depth        = (int8_t)depth;
    t->node_type    = (uint8_t)node_type;
}


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
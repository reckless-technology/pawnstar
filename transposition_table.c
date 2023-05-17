#include "pawnstar.h"

typedef struct
{
    Transposition*  entries;
    int             num_entries;
} TranspositionTable;

static bool IsPrime(int x);
static void FreeTT(TranspositionTable* table);
static void InitTT(TranspositionTable* table, int megabytes);

static TranspositionTable tables[2]; /**< primary and quiescent tables */

void 
InitializeTranspositionTable(int megabytes, int quiescent_megabytes)
{
    InitTT(&tables[TT_MAIN     ], megabytes);
    InitTT(&tables[TT_QUIESCENT], quiescent_megabytes);
}


bool 
FindTransposition(uint64_t         hash, 
                  Transposition*   transposition,
                  int              which_table)
{
    const TranspositionTable* table = &tables[which_table];
    const Transposition* t = &table->entries[hash % table->num_entries];
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
                    int      which_table)
{   
    const TranspositionTable* table = &tables[which_table];
    Transposition* t = &table->entries[hash % table->num_entries];
    INCREMENT_IF(t->hash && t->hash != hash, "hash table collisions");
    t->hash         = hash;
    t->move         = move;
    t->score        = (int16_t)score;
    t->depth        = (int8_t)depth;
    t->node_type    = (uint8_t)node_type;
}

static void
FreeTT(TranspositionTable* table)
{
    if (table->entries)
    {
        free(table->entries);
        table->entries = NULL;
    }
    table->num_entries = 0;
}

static void
InitTT(TranspositionTable* table, int megabytes)
{
    FreeTT(table);
    int num_entries = (megabytes * MEGABYTE) / sizeof(Transposition);
    if ((num_entries & 1) == 0)
    {
        --num_entries;
    }
    while (!IsPrime(num_entries))
    {
        num_entries -= 2;
    }
    table->entries = calloc(num_entries, sizeof(Transposition));
    if (table->entries == NULL)
    {
        printf("ERROR: unable to create transposition transposition_table of %u megabytes\n", megabytes);
        return;
    }
    table->num_entries = num_entries;
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
#include "pawnstar.h"

static bool IsPrime(size_t x);

static Transposition*   main_table;
static size_t           main_table_size;
static Transposition    small_transposition_table[SMALL_HASTABLE_SIZE];
static Transposition    pv_table[SMALL_HASTABLE_SIZE];
/******************************************************************************
Delete the transposition table
NB: NOT thread safe
*******************************************************************************/
void FreeTranspositionTable(void)
{
    if (main_table)
    {
        free(main_table);
        main_table = NULL;
        main_table_size   = 0;
    } 
}
/******************************************************************************
Create the transposition table of a specified maximum size
NB: NOT thread safe
*******************************************************************************/
bool InitializeTranspositionTable(int megabytes)
{
    FreeTranspositionTable();
    main_table_size = (megabytes * MEGABYTE) / sizeof(Transposition);
    if ((main_table_size & 1) == 0)
    {
        --main_table_size;
    }
    while (!IsPrime(main_table_size))
    {
        main_table_size -= 2;
    }
    main_table = calloc(main_table_size, sizeof(Transposition));
    if (!main_table)
    {
        printf("ERROR: unable to create transposition main_table of %u megabytes\n", megabytes);
        main_table_size = 0;
        return false;
    }
    return true;
}
/******************************************************************************
Find a transposition entry for this position if one exists
*******************************************************************************/
bool FindTransposition(uint64 hash, Transposition* transposition)
{
    const int idx = hash % SMALL_HASTABLE_SIZE;
    Transposition t = small_transposition_table[idx];
    if ((t.hash ^ t.payload) == hash)
    {
        INCREMENT("table hit small table");
        *transposition = t;
        return true;
    }
    t = pv_table[idx];
    if ((t.hash ^ t.payload) == hash)
    {
        INCREMENT("table hit pv table");
        *transposition = t;
        return true;
    }    
    t = main_table[hash % main_table_size];
    if ((t.hash ^ t.payload) == hash)
    {
        *transposition = t;
        return true;
    }
    return false;
}
/******************************************************************************
Insert a new entry into the transposition table.
*******************************************************************************/
void RecordTransposition(uint64 hash, int depth, int score, int move, int node_type)
{  
    Transposition t = 
    {
        .depth     = (char)depth,
        .score     = (short)score,
        .move      = move,
        .node_type = (uchar)node_type
    };
    t.hash = hash ^ t.payload;
    const int idx = hash % SMALL_HASTABLE_SIZE;
    small_transposition_table[idx] = t;
    if (node_type == NODE_PV)
    {
        pv_table[idx] = t;
    }
    main_table[hash % main_table_size] = t; 
}
/******************************************************************************
We get marginally better dispersion when the hashtable size is a prime number
Determine if a candidate size is prime
*******************************************************************************/
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
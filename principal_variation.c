#include "pawnstar.h"
#include <Windows.h>
/******************************************************************************
Functions to record and subsequently retrieve the principal variation (PV)
*******************************************************************************/
typedef struct
{
    uint64          hash;
    int             move;
    volatile long   lock;
} PvMove;

static PvMove pv_table[PV_TABLE_SIZE];

/******************************************************************************
Initialize the PV hashtable, deleting any existing entries
*******************************************************************************/
void InitializePrincipalVariationTable(void)
{
    memset(pv_table, 0, sizeof(pv_table)); 
}
/******************************************************************************
Record a new PV move (thread safe)
*******************************************************************************/
void RecordPrincipalVariationMove(uint64 hash, int move)
{ 
    INCREMENT("pv moves recorded");
    PvMove* const p = &pv_table[hash % PV_TABLE_SIZE];
    while (_InterlockedCompareExchange(&p->lock, 1, 0) != 0)
    {
        INCREMENT("lock contention pv record");
    }
    p->hash = hash;
    p->move = move;
    _InterlockedExchange(&p->lock, 0);
}
/******************************************************************************
Retrieve the PV move for the given hash (if any) 
*******************************************************************************/
int GetPrincipalVariationMove(uint64 hash)
{
    PvMove* const p = &pv_table[hash % PV_TABLE_SIZE];
    while (_InterlockedCompareExchange(&p->lock, 1, 0) != 0)
    {
        INCREMENT("lock contention pv retrieve");
    }
    if (p->hash == hash)
    {
        const int result = p->move;
        _InterlockedExchange(&p->lock, 0);
        return result;
    }
    _InterlockedExchange(&p->lock, 0);
    return 0;
}
/******************************************************************************
Get the PV from the PV hashtable, given the root position Zobrist hash
*******************************************************************************/
void FindPrincipalVariation(const Position* root_position, int principal_variation[])
{
    const int* const initial_ptr = principal_variation;
    int move;
    Position src_position[1];
    Position dst_position[1];
    *src_position = *root_position;   
    *principal_variation = 0;
    while ((move = GetPrincipalVariationMove(src_position->hash)) != 0)
    {
        if (FindMove(initial_ptr, move))
        {
            break; // don't want loops in PV
        }
        *principal_variation++ = move;
        *principal_variation   = 0;
        MakeMove(dst_position, src_position, move);
        *src_position = *dst_position;
        if (principal_variation - initial_ptr == MAX_PLY - 1)
        {
            break;
        }
    }
}
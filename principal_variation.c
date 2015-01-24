#include "pawnstar.h"
#include <Windows.h>
/******************************************************************************
Functions to record and subsequently retrieve the principal variation (PV)
Uses Robert Hyatt's XOR trick to do lockless thread safe retrieval
*******************************************************************************/
typedef struct
{
    uint64  hash;   
    uint64  payload; /* (move | (random << 32)) */
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
        PvMove entry;
        entry.payload = (uint64)move | ((uint64)NextRandom() << 32);
        entry.hash    = hash ^ entry.payload;
        pv_table[hash % PV_TABLE_SIZE] = entry;
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
/******************************************************************************
Retrieve the PV move for the given hash (if any) 
*******************************************************************************/
int GetPrincipalVariationMove(uint64 hash)
{
    const PvMove entry = pv_table[hash % PV_TABLE_SIZE];
    if ((entry.hash ^ entry.payload) == hash)
    {
        return (int)entry.payload;
    }
    return 0;
}

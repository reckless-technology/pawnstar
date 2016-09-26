/*
Debugging counts dictionary implemented as a hash table (so that the increment 
function, which might be called tens of millions of times per move, does not 
slow down the program too much). This is useful for counting nodes, cutoffs, 
table hits etc when debugging. The hash function is null, i.e. just use the 
address of the string literal. There is no thread locking per se, and no 
checking of hash collisions; this is intentional and for fastest speed, which 
is the main goal. It seems to work OK in practice, provided you have a 
hashtable array which is prime sized and has lots of free space.

Only included in Debug and ReleaseX configurations.
*/
#include "pawnstar.h"
#if DEBUGX

#define STRING_LEN 0x80

typedef struct
{
    const char* key;
    int        count;
} DebugEntry;

static DebugEntry debug_dict[DEBUG_DICT_SIZE];
/*
Initialize or reset the dictionary
*/
void DebugXClear()
{
    memset(debug_dict, 0, sizeof(debug_dict));
}
/*
Increment the count associated with the string literal in key
*/
void DebugXIncrement(const char key[])
{
    /*
    String literals are typically aligned on 4 byte boundaries, so throw away
    the bottom 2 bits of the address before finding the index into the table
    */
    DebugEntry* const entry = &debug_dict[((uint64)key >> 2) % DEBUG_DICT_SIZE];
    entry->key = key;                       /* don't care about hash collisions */
    _InterlockedIncrement(&entry->count);   /* alleviates miscounts due to races from worker threads */
}

void DebugXIncrementIf(bool condition, const char key[])
{
    if (condition)
    {
        DebugXIncrement(key);
    }
}
/*
Write out the various debugging counts to file in alphabetical order
*/
void DebugXWrite(FILE* file)
{
    char* const strings = (char*)calloc(DEBUG_DICT_SIZE, STRING_LEN);
    char* s = strings;
    const DebugEntry* entry;
    for (entry = debug_dict; entry != debug_dict + DEBUG_DICT_SIZE; ++entry)
    {
        if (entry->key)
        {
            sprintf(s, "%-50s%10u\n", entry->key, entry->count);
            s += STRING_LEN;
        }
    }
    qsort(strings, (s - strings) / STRING_LEN, STRING_LEN, (CompareFn)strcmp);
    s = strings;
    fprintf(file, "************************** DEBUGX **************************\n");
    while (*s)
    {
        fprintf(file, s);
        s += STRING_LEN;
    }
    free(strings);
}
#endif
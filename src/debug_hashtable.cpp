/** @file Debug hash table.
* Debugging counts dictionary implemented as a hash table (so that the increment 
* function, which might be called tens of millions of times per move, does not 
* slow down the program too much). This is useful for counting nodes, cutoffs, 
* table hits etc when debugging. The hash function is null, i.e. just use the 
* address of the string literal. It seems to work OK in practice, provided you 
* have a hashtable array which is prime sized and has lots of free space.
*
* Only included when CPP flag DEBUGX is nonzero.
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

/**
* @brief Initialize or reset the dictionary
*/
void DebugXClear()
{
    memset(debug_dict, 0, sizeof(debug_dict));
}

/**
* @brief Increment the count associated with the string literal in key
*/
void DebugXIncrement(const char* key)
{
    /*
    String literals are typically aligned on 4 byte boundaries, so throw away
    the bottom 2 bits of the address before finding the index into the table
    */
    DebugEntry* const entry = &debug_dict[((uint64_t)key >> 2) % DEBUG_DICT_SIZE];
    if (entry->key == NULL)
    {
        entry->key = key;
    }
    else if (entry->key != key)
    {
        printf("ERROR: debug hash collision '%s'\n", key);
    }
    ++entry->count;
}

/**
* @brief Conditionally increment the count associated with the string literal in key
*/
void DebugXIncrementIf(bool condition, const char* key)
{
    if (condition)
    {
        DebugXIncrement(key);
    }
}

int CompareEntries(const DebugEntry** left, const DebugEntry** right)
{
    return strcmp((*left)->key, (*right)->key);
}

/**
 * @brief Write the debug dictionary out in alphabetic order.
 * @param file File to write to (typically stdout)
*/
void DebugXWrite(FILE* file)
{
    DebugEntry* entries[DEBUG_DICT_SIZE];
    int num_entries = 0;
    for (int i = 0; i != DEBUG_DICT_SIZE; ++i)
    {
        if (debug_dict[i].key != NULL)
        {
            entries[num_entries++] = &debug_dict[i];
        }
    }
    qsort(entries, num_entries, sizeof(DebugEntry*), (int(*)(const void*, const void*))CompareEntries);
    fputs("************************** DEBUGX **************************\n", file);
    for (int i = 0; i != num_entries; ++i)
    {
        fprintf(file, "%-50s%10u\n", entries[i]->key, entries[i]->count);
    }
    fputs("************************************************************\n", file);
}

#endif
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
#include <cstdio>
#include <map>
#include <string>
#include <cstring>
#include <algorithm>

using std::map;
using std::string;

#include "debug_hashtable.h"
#include "options.h"
#include "function_prototypes.h"

#if DEBUGX

DebugEntry debug_dict[DEBUG_DICT_SIZE];

/**
* @brief Initialize or reset the dictionary
*/
void DebugXClear()
{
    std::fill(&debug_dict[0], &debug_dict[DEBUG_DICT_SIZE], DebugEntry{0,0});
}

/**
 * @brief Write the debug dictionary out in alphabetic order.
*/
void DebugXWrite()
{
    map<string, int> entries;
    for (int i = 0; i != DEBUG_DICT_SIZE; ++i)
    {
        if (debug_dict[i].key != NULL)
        {
            entries[debug_dict[i].key] = debug_dict[i].count;
        }
    }
    printf("************************** DEBUGX **************************\n");
    for (const auto& i : entries)
    {
        printf("%-50s%10u\n", i.first.c_str(), i.second);
    }
    printf("************************************************************\n");
}

#endif
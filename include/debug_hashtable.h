#pragma once
#include <cstdint>

#include "options.h"

#if DEBUGX
#define DEBUG_STATEMENT(x)  x
#define INCREMENT(x)        DebugXIncrement(x)
#define INCREMENT_IF(b,x)   DebugXIncrementIf(b,x)
#else
#define DEBUG_STATEMENT(x)
#define INCREMENT(x)
#define INCREMENT_IF(b,x)
#endif

#if EVAL_DEBUGX
#define EVAL_INCREMENT(x)   DebugXIncrement(x)
#else
#define EVAL_INCREMENT(x)
#endif

#if DEBUGX
/**
 * @brief Simple debug hash table dictionary fo diagnostic counts.
 */
struct DebugEntry
{
    const char* key;
    int        count;
};

extern DebugEntry debug_dict[DEBUG_DICT_SIZE];

/**
* @brief Increment the count associated with the string literal in key
*/
constexpr void DebugXIncrement(const char* key)
{
    /*
    String literals are typically aligned on 4 byte boundaries, so throw away
    the bottom 2 bits of the address before finding the index into the table
    */
    DebugEntry* const entry = &debug_dict[((uint64_t)key >> 2) % DEBUG_DICT_SIZE];
    entry->key = key;
    ++entry->count;
}

/**
* @brief Conditionally increment the count associated with the string literal in key
*/
constexpr void DebugXIncrementIf(bool condition, const char* key)
{
    if (condition)
    {
        DebugXIncrement(key);
    }
}

void DebugXClear(void);
void DebugXWrite();
#endif /* DEBUGX */
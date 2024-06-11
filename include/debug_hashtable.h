#pragma once
#include <string_view>
#include <unordered_map>

#include "options.h"

#if DEBUGX

typedef std::unordered_map<std::string_view, uint32_t> DebugTable;
extern DebugTable                                      debug_dictionary;

void DebugXClear();
void DebugXWrite();

#define DEBUG_STATEMENT(x) x
#define INCREMENT(x)       ++debug_dictionary[x];
#define INCREMENT_IF(b, x)     \
    if (b)                     \
    {                          \
        ++debug_dictionary[x]; \
    }

#else

#define DEBUG_STATEMENT(x)
#define INCREMENT(x)
#define INCREMENT_IF(b, x)
#define DebugXClear(x)
#define DebugXWrite(x)

#endif
/// @file Debugging counts dictionary.

#pragma once
#include <cstdint>
#include <string_view>
#include <unordered_map>

#if DEBUGX

/// @brief Maps a literal string to a count value.
typedef std::unordered_map<std::string_view, int64_t> DebugTable;
extern DebugTable                                     debug_dictionary;

void DebugXClear();
void DebugXWrite();

#define INCREMENT(x)   ++debug_dictionary[x]
#define ASSIGN(x, val) debug_dictionary[x] = val
#define INCREMENT_IF(b, x)     \
    if (b)                     \
    {                          \
        ++debug_dictionary[x]; \
    }

#else

#define INCREMENT(x)
#define ASSIGN(x, val)
#define INCREMENT_IF(b, x)
#define DebugXClear(x)
#define DebugXWrite(x)

#endif
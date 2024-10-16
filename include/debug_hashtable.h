/// @file Debugging counts dictionary.

#pragma once
#include <cstdint>
#include <string_view>
#include <unordered_map>

#if DEBUGX

/// @brief FNV-1a 64 hash. Simple, fast, and constexpr.
struct string_hasher
{
    constexpr std::size_t operator()(std::string_view str) const
    {
        std::size_t result = 14695981039346656037ull;
        for (char c : str)
        {
            result ^= c;
            result *= 1099511628211ull;
        }
        return result;
    }
};

/// @brief Maps a literal string to a count value.
typedef std::unordered_map<std::string_view, int64_t, string_hasher> DebugTable;
extern DebugTable                                                    debug_dictionary;

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
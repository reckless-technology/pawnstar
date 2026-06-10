/// @file debug_hashtable.h Debugging counts dictionary.

#pragma once
#include <cstdint>
#include <string_view>
#include <unordered_map>

#if DEBUGX

/// @brief FNV-1a 64 hash. Simple, fast, and constexpr.
struct string_hasher
{
    /// @brief Hash a string view.
    /// @param str String to hash.
    /// @return 64-bit FNV-1a hash.
    constexpr std::size_t operator()(std::string_view str) const
    {
        std::size_t result = 14695981039346656037ull;
        for (auto c : str)
        {
            result ^= c;
            result *= 1099511628211ull;
        }
        return result;
    }
};

/// @brief Maps a literal string to a count value.
using DebugTable = std::unordered_map<std::string_view, int64_t, string_hasher>;
extern DebugTable debug_dictionary; ///< Global dictionary of named diagnostic counters.

/// @brief Reset all diagnostic counters to zero.
void DebugXClear();
/// @brief Print all diagnostic counters in alphabetical order.
void DebugXWrite();

#define INCREMENT(x)   ++debug_dictionary[x]     ///< Increment the named counter.
#define ASSIGN(x, val) debug_dictionary[x] = val ///< Set the named counter to a value.
/// @brief Increment the named counter when the condition is true.
#define INCREMENT_IF(b, x)     \
    if (b)                     \
    {                          \
        ++debug_dictionary[x]; \
    }

#else

#define INCREMENT(x)       ///< No-op when diagnostics are disabled.
#define ASSIGN(x, val)     ///< No-op when diagnostics are disabled.
#define INCREMENT_IF(b, x) ///< No-op when diagnostics are disabled.
#define DebugXClear(x)     ///< No-op when diagnostics are disabled.
#define DebugXWrite(x)     ///< No-op when diagnostics are disabled.

#endif
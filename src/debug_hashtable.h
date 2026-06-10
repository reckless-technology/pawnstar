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

/// @brief Find-or-create the counter slot for a name and return a stable pointer to it.
/// std::unordered_map nodes are stable across insert/rehash and DebugXClear zeroes values rather than
/// erasing nodes, so the returned pointer is valid for the program's lifetime. Each INCREMENT call site
/// caches this pointer in a function-local static, so after the first hit a counter update is a single
/// pointer increment — no string hashing and no map lookup on the hot path.
int64_t *DebugSlot(std::string_view name);

/// @brief Reset all diagnostic counters to zero.
void DebugXClear();
/// @brief Print all diagnostic counters in alphabetical order.
void DebugXWrite();

/// @brief Increment the named counter (slot pointer cached per call site).
#define INCREMENT(x)                              \
    do                                            \
    {                                             \
        static int64_t *_dbg_slot = DebugSlot(x); \
        ++*_dbg_slot;                             \
    } while (0)
/// @brief Set the named counter to a value (slot pointer cached per call site).
#define ASSIGN(x, val)                            \
    do                                            \
    {                                             \
        static int64_t *_dbg_slot = DebugSlot(x); \
        *_dbg_slot                = (val);        \
    } while (0)
/// @brief Increment the named counter when the condition is true (slot pointer cached per call site).
#define INCREMENT_IF(b, x)                        \
    do                                            \
    {                                             \
        static int64_t *_dbg_slot = DebugSlot(x); \
        if (b)                                    \
            ++*_dbg_slot;                         \
    } while (0)

#else

#define INCREMENT(x)       ///< No-op when diagnostics are disabled.
#define ASSIGN(x, val)     ///< No-op when diagnostics are disabled.
#define INCREMENT_IF(b, x) ///< No-op when diagnostics are disabled.
#define DebugXClear(x)     ///< No-op when diagnostics are disabled.
#define DebugXWrite(x)     ///< No-op when diagnostics are disabled.

#endif
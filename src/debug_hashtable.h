/// @file debug_hashtable.h Debugging counts dictionary (header-only).

#pragma once
#include <cstdint>
#include <string_view>
#include <unordered_map>

#if DEBUGX
#include <format>
#include <iostream>
#include <map>
#include <mutex>

/// @brief FNV-1a 64 hash. Simple, fast, and constexpr.
struct string_hasher
{
    constexpr std::size_t operator()(std::string_view str) const;
};

/// @brief Hash a string view.
/// @param str String to hash.
/// @return 64-bit FNV-1a hash.
constexpr std::size_t string_hasher::operator()(std::string_view str) const
{
    std::size_t result = 14695981039346656037ull;
    for (auto c : str)
    {
        result ^= c;
        result *= 1099511628211ull;
    }
    return result;
}

/// @brief Maps a literal string to a count value.
using DebugTable = std::unordered_map<std::string_view, int64_t, string_hasher>;

/// @brief Global dictionary of named diagnostic counters.
inline DebugTable debug_dictionary;

/// @brief Find-or-create the counter slot for a name and return a stable pointer to it.
/// std::unordered_map nodes are stable across insert/rehash and DebugXClear zeroes values rather than
/// erasing nodes, so the returned pointer is valid for the program's lifetime. Each INCREMENT call site
/// caches this pointer in a function-local static, so after the first hit a counter update is a single
/// pointer increment — no string hashing and no map lookup on the hot path.
/// Creation is serialised so concurrent first-time creations from worker threads don't race on the map.
inline int64_t *DebugSlot(std::string_view name)
{
    static std::mutex           slot_mutex;
    std::lock_guard<std::mutex> lock(slot_mutex);
    return &debug_dictionary[name];
}

/// @brief Reset all diagnostic counters to zero. Entries are kept (not erased) so the slot pointers
/// cached at each INCREMENT call site remain valid.
inline void DebugXClear()
{
    for (auto &entry : debug_dictionary)
    {
        entry.second = 0;
    }
}

/// @brief Print the non-zero debug counters in alphabetic order.
inline void DebugXWrite()
{
    std::map<std::string_view, int64_t> sorted_entries(debug_dictionary.begin(), debug_dictionary.end());
    std::cout << "********************* DEBUGX *********************\n";
    for (const auto &[title, count] : sorted_entries)
    {
        if (count != 0)
        {
            std::cout << std::format("{:<40}{:10}\n", title, count);
        }
    }
    std::cout << "**************************************************\n";
}

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

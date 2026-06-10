
#include <format>
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <string_view>

#include "constants.h"
#include "debug_hashtable.h"

#if DEBUGX

DebugTable debug_dictionary;

/// @brief Find-or-create the counter slot for a name and return a stable pointer to it.
/// Creation is serialised so concurrent first-time creations from worker threads don't race on the map.
int64_t *DebugSlot(std::string_view name)
{
    static std::mutex           slot_mutex;
    std::lock_guard<std::mutex> lock(slot_mutex);
    return &debug_dictionary[name];
}

/// @brief Reset all diagnostic counters to zero. Entries are kept (not erased) so the slot pointers
/// cached at each INCREMENT call site remain valid.
void DebugXClear()
{
    for (auto &entry : debug_dictionary)
    {
        entry.second = 0;
    }
}

/// @brief Print the non-zero debug counters in alphabetic order.
void DebugXWrite()
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

#endif
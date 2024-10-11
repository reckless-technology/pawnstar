
#include <format>
#include <iomanip>
#include <iostream>
#include <map>
#include <string_view>

#include "constants.h"
#include "debug_hashtable.h"

#if DEBUGX

DebugTable debug_dictionary;

///
/// @brief Initialize or reset the dictionary
///
void DebugXClear()
{
    debug_dictionary.clear();
}

/// @brief Print the debug dictionary in alphabetic order.
void DebugXWrite()
{
    std::map<std::string_view, int64_t> sorted_entries(debug_dictionary.begin(), debug_dictionary.end());
    std::cout << "********************* DEBUGX *********************" << std::endl;
    for (const auto &[title, count] : sorted_entries)
    {
        std::cout << std::format("{:<40}{:10}\n", title, count);
    }
    std::cout << "**************************************************" << std::endl;
}

#endif
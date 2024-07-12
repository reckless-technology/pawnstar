
#include <iomanip>
#include <iostream>
#include <map>
#include <string_view>

using std::map;
using std::string_view;

#include "constants.h"
#include "debug_hashtable.h"
#include "function_prototypes.h"

#if DEBUGX

DebugTable debug_dictionary;

/**
 * @brief Initialize or reset the dictionary
 */
void DebugXClear()
{
    debug_dictionary.clear();
}

/**
 * @brief Write the debug dictionary out in alphabetic order.
 */
void DebugXWrite()
{
    map<string_view, uint32_t> sorted_entries(debug_dictionary.begin(), debug_dictionary.end());
    std::cout << "************************** DEBUGX **************************" << std::endl;
    for (const auto &[title, count] : sorted_entries)
    {
        std::cout << std::setw(50) << std::left << title << std::setw(10) << std::right << count << std::endl;
    }
    std::cout << "************************************************************" << std::endl;
}

#endif
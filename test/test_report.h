#pragma once
/// @file test_report.h Shared, consistent result reporting for the test executables.
///
/// Every test result line is printed as "[PASS] <summary>" or "[FAIL] <summary>" so that all test
/// programs report uniformly. Each suite reports its individual checks (or notable failures) with
/// Check(...) and ends with one Check(...) summary line for the whole suite; ExitCode() gives the
/// process exit status (0 = all passed).

#include <iostream>
#include <string_view>

namespace test_report
{
inline int failures = 0; ///< Number of failed Check() calls so far.

/// @brief Report one result line: "[PASS] <summary>" or "[FAIL] <summary>"; tally failures.
/// @param passed Whether the check passed. @param summary Brief description of what was checked.
/// @return @p passed (so the call can be used in a condition).
inline bool Check(bool passed, std::string_view summary)
{
    std::cout << (passed ? "[PASS] " : "[FAIL] ") << summary << '\n';
    if (!passed)
    {
        ++failures;
    }
    return passed;
}

/// @brief Process exit code for a test: 0 if every Check() passed, 1 otherwise.
inline int ExitCode()
{
    return failures == 0 ? 0 : 1;
}

/// @brief Print the final whole-suite line — "[PASS] <detail>" if every Check() passed, else
/// "[FAIL] <detail>" — without itself counting as a check, and return the process exit code.
/// @param detail Brief suite summary (e.g. "SEE: 24/24 cases passed").
inline int Summary(std::string_view detail)
{
    std::cout << (failures == 0 ? "[PASS] " : "[FAIL] ") << detail << '\n';
    return ExitCode();
}
} // namespace test_report

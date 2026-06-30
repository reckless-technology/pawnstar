#pragma once
/// @file version.h Engine version constants and the "major.minor.build" version string.

#include <format>
#include <string>

// Engine version, presented to the UCI host as "id name Pawnstar <major>.<minor>.<build>".
//
// The major and minor numbers are edited by hand below (the human-meaningful version). The build number is
// supplied at compile time via the PAWNSTAR_BUILD_NUMBER macro, which the build system derives from the git
// commit count (`git rev-list --count HEAD`) — a globally-reproducible number, identical for every clone at
// a given commit. It defaults to 0 so that the test/tool translation units (which pull the engine in through
// its headers but are not the engine build) and editor tooling still compile cleanly without the define.
inline constexpr int kVersionMajor = 0;
inline constexpr int kVersionMinor = 13;

#ifndef PAWNSTAR_BUILD_NUMBER
#define PAWNSTAR_BUILD_NUMBER 0
#endif
inline constexpr int kBuildNumber = PAWNSTAR_BUILD_NUMBER;

/// @brief The engine version as a "major.minor.build" string (e.g. "0.12.5").
inline std::string VersionString()
{
    return std::format("{}.{}.{}", kVersionMajor, kVersionMinor, kBuildNumber);
}

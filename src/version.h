#pragma once
/// @file version.h The engine's "major.minor.build" version string.

#include <string>

// The engine's version, MAJOR.MINOR.BUILD, presented to the UCI host as "id name Pawnstar <version>". It comes from git
// alone (cmake/version.cmake): MAJOR.MINOR is the nearest annotated tag vMAJOR.MINOR, BUILD the number of commits since
// it. The build force-includes a generated header defining PAWNSTAR_VERSION_STRING into the engine's translation unit;
// the 0.0.0 default lets the test and tool translation units (which pull the engine in through its headers but are not
// the engine build) and editor tooling compile without it. Nothing here is edited to release: tag main and push the
// tag.
#ifndef PAWNSTAR_VERSION_STRING
#define PAWNSTAR_VERSION_STRING "0.0.0"
#endif

/// @brief The engine version as a "major.minor.build" string (e.g. "1.0.5").
/// @return the version
inline std::string VersionString()
{
    return PAWNSTAR_VERSION_STRING;
}

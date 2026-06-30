# Compute the globally-reproducible build number (the git commit count) and write a header defining
# PAWNSTAR_BUILD_NUMBER, but only when the number changes (so the engine TU recompiles only on a real
# change, not on every build). Mirrors the Makefile's VERSION_HEADER mechanism so both build systems report
# the same number for a given commit.
#
# Invoked per build as a script: `cmake -DSOURCE_DIR=<repo> -DHEADER_FILE=<path> -P write_build_number.cmake`.
# Falls back to 0 outside a git checkout (e.g. a source tarball). NOTE: a shallow clone undercounts.
execute_process(
  COMMAND git -C "${SOURCE_DIR}" rev-list --count HEAD
  OUTPUT_VARIABLE _n
  OUTPUT_STRIP_TRAILING_WHITESPACE
  RESULT_VARIABLE _rc
  ERROR_QUIET)
if(NOT _rc EQUAL 0 OR NOT _n MATCHES "^[0-9]+$")
  set(_n 0)
endif()

set(_content "#pragma once\n#define PAWNSTAR_BUILD_NUMBER ${_n}\n")
set(_old "")
if(EXISTS "${HEADER_FILE}")
  file(READ "${HEADER_FILE}" _old)
endif()
if(NOT _old STREQUAL _content)
  file(WRITE "${HEADER_FILE}" "${_content}")
endif()
message(STATUS "Pawnstar build number ${_n}")

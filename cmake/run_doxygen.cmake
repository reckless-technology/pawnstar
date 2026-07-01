# Run doxygen with PROJECT_NUMBER injected from the version, so the API docs are stamped with the current
# major.minor.build without hardcoding a (stale) number in the Doxyfile. Invoked via `cmake -P` by the `doc`
# custom target (mirrors the Makefile's piped PROJECT_NUMBER). Args: SOURCE_DIR, BIN_DIR, VERSION.
find_program(DOXYGEN_EXE doxygen)
if(NOT DOXYGEN_EXE)
  message(FATAL_ERROR "doxygen not found (required by the `doc` target / `make doc`)")
endif()

# Append PROJECT_NUMBER to a copy of the Doxyfile — doxygen takes the last assignment of a key.
file(READ "${SOURCE_DIR}/Doxyfile" _cfg)
set(_generated "${BIN_DIR}/Doxyfile.versioned")
file(WRITE "${_generated}" "${_cfg}\nPROJECT_NUMBER = ${VERSION}\n")

# Doxyfile paths (INPUT, OUTPUT_DIRECTORY=doc) are relative to the repo root, so run from there.
execute_process(
  COMMAND "${DOXYGEN_EXE}" "${_generated}"
  WORKING_DIRECTORY "${SOURCE_DIR}"
  RESULT_VARIABLE _rc)
if(NOT _rc EQUAL 0)
  message(FATAL_ERROR "doxygen failed with exit code ${_rc}")
endif()

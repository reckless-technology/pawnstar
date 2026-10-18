# Run every ctest on an instrumented build (PAWNSTAR_COVERAGE) and report the engine's coverage. Invoked via
# `cmake -P` by the `coverage` custom target in CMakeLists.txt. Args: SOURCE_DIR, BIN_DIR, CTEST, LLVM_PROFDATA,
# LLVM_COV, OBJECTS (the instrumented executables, '|'-separated, the engine first).
#
#   1. <build>/prof is emptied and ctest runs with LLVM_PROFILE_FILE=<build>/prof/%p-%m.profraw, so every process -
#      each test binary, and the engine the UCI test drives - writes a raw profile of its own (%p the process id, %m
#      the binary's signature, so concurrent writers never share a file).
#   2. llvm-profdata merges them into <build>/coverage/pawnstar.profdata.
#   3. llvm-cov reports src/'s coverage over all the objects (the header-only engine is compiled into each, and
#      llvm-cov merges a header's counts across them), exports it as lcov (<build>/coverage/coverage.lcov) and
#      writes an HTML report (<build>/coverage/html/index.html).
#
# A failing test fails the target, after the report: the numbers are still worth reading, the run is not green.

string(REPLACE "|" ";" OBJECTS "${OBJECTS}")
list(POP_FRONT OBJECTS _first_object)
set(_object_args "")
foreach(_object IN LISTS OBJECTS)
  list(APPEND _object_args -object "${_object}")
endforeach()

set(_prof_dir "${BIN_DIR}/prof")
set(_out_dir "${BIN_DIR}/coverage")
file(REMOVE_RECURSE "${_prof_dir}" "${_out_dir}")
file(MAKE_DIRECTORY "${_prof_dir}" "${_out_dir}")

set(ENV{LLVM_PROFILE_FILE} "${_prof_dir}/%p-%m.profraw")
execute_process(COMMAND "${CTEST}" --test-dir "${BIN_DIR}" --output-on-failure RESULT_VARIABLE _ctest_rc)

file(GLOB _raw_profiles "${_prof_dir}/*.profraw")
if(NOT _raw_profiles)
  message(FATAL_ERROR "coverage: no raw profiles in ${_prof_dir} (is the build configured with PAWNSTAR_COVERAGE?)")
endif()
set(_profdata "${_out_dir}/pawnstar.profdata")
execute_process(COMMAND "${LLVM_PROFDATA}" merge -sparse ${_raw_profiles} -o "${_profdata}"
                RESULT_VARIABLE _rc)
if(NOT _rc EQUAL 0)
  message(FATAL_ERROR "coverage: llvm-profdata merge failed with exit code ${_rc}")
endif()

# The engine's own sources alone: src/ (every header and main.cpp), not the tests, the tools or the system headers.
set(_cov_common "${_first_object}" ${_object_args} "-instr-profile=${_profdata}" "${SOURCE_DIR}/src")
execute_process(COMMAND "${LLVM_COV}" report ${_cov_common} RESULT_VARIABLE _rc)
if(NOT _rc EQUAL 0)
  message(FATAL_ERROR "coverage: llvm-cov report failed with exit code ${_rc}")
endif()
execute_process(COMMAND "${LLVM_COV}" export -format=lcov ${_cov_common}
                OUTPUT_FILE "${_out_dir}/coverage.lcov" RESULT_VARIABLE _rc)
if(NOT _rc EQUAL 0)
  message(FATAL_ERROR "coverage: llvm-cov export failed with exit code ${_rc}")
endif()
execute_process(COMMAND "${LLVM_COV}" show -format=html "-output-dir=${_out_dir}/html" ${_cov_common}
                RESULT_VARIABLE _rc)
if(NOT _rc EQUAL 0)
  message(FATAL_ERROR "coverage: llvm-cov show failed with exit code ${_rc}")
endif()
message(STATUS "coverage: ${_out_dir}/coverage.lcov and ${_out_dir}/html/index.html")

if(NOT _ctest_rc EQUAL 0)
  message(FATAL_ERROR "coverage: ctest failed (exit code ${_ctest_rc}); the report above is of a failing run")
endif()

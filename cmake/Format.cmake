cmake_minimum_required(VERSION 3.24)

get_filename_component(WORKFLOW_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

if (DEFINED WORKFLOW_CLANG_FORMAT_EXECUTABLE AND
    NOT WORKFLOW_CLANG_FORMAT_EXECUTABLE STREQUAL "")
    set(CLANG_FORMAT_EXECUTABLE "${WORKFLOW_CLANG_FORMAT_EXECUTABLE}")
elseif (DEFINED ENV{CLANG_FORMAT} AND NOT "$ENV{CLANG_FORMAT}" STREQUAL "")
    set(CLANG_FORMAT_EXECUTABLE "$ENV{CLANG_FORMAT}")
else()
    find_program(CLANG_FORMAT_EXECUTABLE NAMES clang-format-22 clang-format)
endif()

if (NOT CLANG_FORMAT_EXECUTABLE OR NOT EXISTS "${CLANG_FORMAT_EXECUTABLE}")
    message(FATAL_ERROR
            "clang-format 22 or newer was not found. Set CLANG_FORMAT to its executable path.")
endif()

execute_process(
        COMMAND "${CLANG_FORMAT_EXECUTABLE}" --version
        RESULT_VARIABLE CLANG_FORMAT_VERSION_RESULT
        OUTPUT_VARIABLE CLANG_FORMAT_VERSION_OUTPUT
        ERROR_VARIABLE CLANG_FORMAT_VERSION_ERROR
        OUTPUT_STRIP_TRAILING_WHITESPACE)
if (NOT CLANG_FORMAT_VERSION_RESULT EQUAL 0)
    message(FATAL_ERROR
            "Unable to run ${CLANG_FORMAT_EXECUTABLE}: ${CLANG_FORMAT_VERSION_ERROR}")
endif()

string(REGEX MATCH "version ([0-9]+)" CLANG_FORMAT_VERSION_MATCH
       "${CLANG_FORMAT_VERSION_OUTPUT}")
if (NOT CLANG_FORMAT_VERSION_MATCH OR CMAKE_MATCH_1 LESS 22)
    message(FATAL_ERROR
            "clang-format 22 or newer is required; found: ${CLANG_FORMAT_VERSION_OUTPUT}")
endif()

file(GLOB_RECURSE WORKFLOW_FORMAT_FILES LIST_DIRECTORIES false
        "${WORKFLOW_ROOT}/src/*.c"
        "${WORKFLOW_ROOT}/src/*.cc"
        "${WORKFLOW_ROOT}/src/*.cpp"
        "${WORKFLOW_ROOT}/src/*.cxx"
        "${WORKFLOW_ROOT}/src/*.h"
        "${WORKFLOW_ROOT}/src/*.hpp"
        "${WORKFLOW_ROOT}/example/*.c"
        "${WORKFLOW_ROOT}/example/*.cc"
        "${WORKFLOW_ROOT}/example/*.cpp"
        "${WORKFLOW_ROOT}/example/*.cxx"
        "${WORKFLOW_ROOT}/example/*.h"
        "${WORKFLOW_ROOT}/example/*.hpp"
        "${WORKFLOW_ROOT}/test/*.c"
        "${WORKFLOW_ROOT}/test/*.cc"
        "${WORKFLOW_ROOT}/test/*.cpp"
        "${WORKFLOW_ROOT}/test/*.cxx"
        "${WORKFLOW_ROOT}/test/*.h"
        "${WORKFLOW_ROOT}/test/*.hpp")
list(FILTER WORKFLOW_FORMAT_FILES EXCLUDE REGEX "[/\\]src[/\\]third_party[/\\]")
list(SORT WORKFLOW_FORMAT_FILES)

if (NOT WORKFLOW_FORMAT_FILES)
    message(FATAL_ERROR "No first-party C/C++ files were found")
endif()

if (WORKFLOW_FORMAT_MODE STREQUAL "fix")
    set(WORKFLOW_FORMAT_ARGUMENTS -i --style=file --fallback-style=none)
elseif (WORKFLOW_FORMAT_MODE STREQUAL "check")
    set(WORKFLOW_FORMAT_ARGUMENTS --dry-run --Werror --style=file --fallback-style=none)
else()
    message(FATAL_ERROR "WORKFLOW_FORMAT_MODE must be 'fix' or 'check'")
endif()

execute_process(
        COMMAND "${CLANG_FORMAT_EXECUTABLE}"
                ${WORKFLOW_FORMAT_ARGUMENTS}
                ${WORKFLOW_FORMAT_FILES}
        WORKING_DIRECTORY "${WORKFLOW_ROOT}"
        RESULT_VARIABLE WORKFLOW_FORMAT_RESULT
        OUTPUT_VARIABLE WORKFLOW_FORMAT_OUTPUT
        ERROR_VARIABLE WORKFLOW_FORMAT_ERROR)

if (NOT WORKFLOW_FORMAT_RESULT EQUAL 0)
    message(FATAL_ERROR
            "clang-format ${WORKFLOW_FORMAT_MODE} failed:\n"
            "${WORKFLOW_FORMAT_OUTPUT}${WORKFLOW_FORMAT_ERROR}")
endif()

list(LENGTH WORKFLOW_FORMAT_FILES WORKFLOW_FORMAT_FILE_COUNT)
message(STATUS
        "clang-format ${WORKFLOW_FORMAT_MODE} passed for ${WORKFLOW_FORMAT_FILE_COUNT} files")

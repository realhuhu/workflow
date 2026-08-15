cmake_minimum_required(VERSION 3.24)

if (NOT DEFINED WORKFLOW_TEST_PAGE_ROOT)
    message(FATAL_ERROR "WORKFLOW_TEST_PAGE_ROOT is required")
endif()

get_filename_component(
        _test_app_root
        "${WORKFLOW_TEST_PAGE_ROOT}/.."
        ABSOLUTE)
file(READ "${_test_app_root}/cases.cpp" _test_cases_source)

string(REGEX MATCHALL
        "std::make_unique<(Image|Text)Clicker>"
        _test_chain_starts
        "${_test_cases_source}")
string(REGEX MATCHALL
        "->end[(][)]"
        _test_chain_ends
        "${_test_cases_source}")
list(LENGTH _test_chain_starts _test_chain_start_count)
list(LENGTH _test_chain_ends _test_chain_end_count)
if (NOT _test_chain_start_count EQUAL 20 OR
    NOT _test_chain_end_count EQUAL 20)
    message(FATAL_ERROR
            "Every test case must be one strict Clicker chain. "
            "Found ${_test_chain_start_count} starts and "
            "${_test_chain_end_count} ends; expected 20 of each.")
endif()

foreach (_forbidden_pattern
        "auto[ \t]+next"
        "std::move"
        "(Image|Text)Clicker[ \t]+clicker"
        "(Image|Text)RunConfig[ \t]+[A-Za-z_][A-Za-z0-9_]*[ \t]*[=;]")
    if (_test_cases_source MATCHES "${_forbidden_pattern}")
        message(FATAL_ERROR
                "Test cases must use chained calls and inline aggregate "
                "configuration; forbidden pattern: ${_forbidden_pattern}")
    endif()
endforeach()

set(_expected_pages
        01-static-image.html
        02-image-selector.html
        03-any-image.html
        04-delayed-image.html
        05-disappearing-image.html
        06-stable-image.html
        07-image-region.html
        08-image-threshold.html
        09-static-text.html
        10-any-text.html
        11-delayed-text.html
        12-disappearing-text.html
        13-stable-text.html
        14-text-region.html
        15-previous-relation.html
        16-repeat-click.html
        17-drag.html
        18-scroll.html
        19-hidden-layer.html
        20-mixed-workflow.html)

file(GLOB _actual_pages
        RELATIVE "${WORKFLOW_TEST_PAGE_ROOT}/cases"
        "${WORKFLOW_TEST_PAGE_ROOT}/cases/*.html")
list(SORT _actual_pages)
if (NOT _actual_pages STREQUAL _expected_pages)
    message(FATAL_ERROR
            "Test pages differ from the required 20-page contract.\n"
            "Expected: ${_expected_pages}\nActual: ${_actual_pages}")
endif()

foreach (_page IN LISTS _expected_pages)
    file(READ "${WORKFLOW_TEST_PAGE_ROOT}/cases/${_page}" _html)
    string(REGEX REPLACE "^[0-9]+-|\\.html$" "" _slug "${_page}")
    string(FIND "${_html}" "data-case=\"${_slug}\"" _case_marker)
    string(FIND "${_html}" "../assets/case.js" _runtime_marker)
    if (_case_marker EQUAL -1 OR _runtime_marker EQUAL -1)
        message(FATAL_ERROR "Invalid test page: ${_page}")
    endif()
endforeach()

foreach (_required_file
        index.html
        assets/catalog.js
        assets/case.js
        assets/index.js
        assets/site.css)
    if (NOT EXISTS "${WORKFLOW_TEST_PAGE_ROOT}/${_required_file}")
        message(FATAL_ERROR "Missing test page asset: ${_required_file}")
    endif()
endforeach()

file(READ "${WORKFLOW_TEST_PAGE_ROOT}/assets/catalog.js" _catalog)
string(REGEX MATCHALL "id: [0-9]+" _catalog_ids "${_catalog}")
list(LENGTH _catalog_ids _catalog_count)
if (NOT _catalog_count EQUAL 20)
    message(FATAL_ERROR "The test catalog must contain exactly 20 cases; found ${_catalog_count}")
endif()

file(READ "${WORKFLOW_TEST_PAGE_ROOT}/assets/case.js" _runtime)
foreach (_page IN LISTS _expected_pages)
    string(REGEX REPLACE "^[0-9]+-|\\.html$" "" _slug "${_page}")
    string(FIND "${_runtime}" "\"${_slug}\":" _renderer)
    if (_renderer EQUAL -1)
        message(FATAL_ERROR "No renderer exists for test page: ${_slug}")
    endif()
endforeach()

message(STATUS "Validated 20 deterministic test pages")

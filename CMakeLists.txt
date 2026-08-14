cmake_minimum_required(VERSION 3.24)

project(workflow VERSION 0.1.0 LANGUAGES CXX)

if (NOT WIN32)
    message(FATAL_ERROR "workflow currently supports Windows only")
endif()
if (NOT MSVC)
    message(FATAL_ERROR "workflow release packages require the MSVC toolchain")
endif()

include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

if (PROJECT_IS_TOP_LEVEL)
    set(_workflow_developer_default ON)
else()
    set(_workflow_developer_default OFF)
endif()

option(WORKFLOW_BUILD_EXAMPLES "Build workflow examples" ${_workflow_developer_default})
option(WORKFLOW_BUILD_TESTS "Build workflow tests" ${_workflow_developer_default})
option(WORKFLOW_INSTALL "Generate install and CMake package rules" ${PROJECT_IS_TOP_LEVEL})
set(WORKFLOW_CLANG_FORMAT_EXECUTABLE "$ENV{CLANG_FORMAT}" CACHE FILEPATH
        "clang-format 22 or newer executable")

if (PROJECT_IS_TOP_LEVEL)
    add_custom_target(workflow_format
            COMMAND ${CMAKE_COMMAND}
                    -DWORKFLOW_FORMAT_MODE=fix
                    "-DWORKFLOW_CLANG_FORMAT_EXECUTABLE=${WORKFLOW_CLANG_FORMAT_EXECUTABLE}"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Format.cmake"
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            COMMENT "Formatting first-party C/C++ sources"
            VERBATIM)
    add_custom_target(workflow_lint
            COMMAND ${CMAKE_COMMAND}
                    -DWORKFLOW_FORMAT_MODE=check
                    "-DWORKFLOW_CLANG_FORMAT_EXECUTABLE=${WORKFLOW_CLANG_FORMAT_EXECUTABLE}"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Format.cmake"
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            COMMENT "Checking first-party C/C++ formatting"
            VERBATIM)
endif()

set(WORKFLOW_QT_ROOT "$ENV{WORKFLOW_QT_ROOT}" CACHE PATH
        "Qt 5.15.2 MSVC 2019 x64 root")
if (WORKFLOW_QT_ROOT)
    list(PREPEND CMAKE_PREFIX_PATH "${WORKFLOW_QT_ROOT}")
endif()
find_package(Qt5 5.15.2 EXACT REQUIRED COMPONENTS Core)

set(WORKFLOW_OPENCV_DIR "$ENV{WORKFLOW_OPENCV_DIR}" CACHE PATH
        "OpenCV 4.10 install directory containing OpenCVConfig.cmake")
if (WORKFLOW_OPENCV_DIR)
    set(OpenCV_DIR "${WORKFLOW_OPENCV_DIR}")
endif()
# RapidOCR and the public API are built against OpenCV 4.10. Mixing OpenCV
# ABIs in one process is unsafe, so downstream packages use the exact version.
find_package(OpenCV 4.10 EXACT REQUIRED COMPONENTS core imgproc imgcodecs)

set(WORKFLOW_ONNXRUNTIME_VERSION "1.13.1")
set(WORKFLOW_ONNXRUNTIME_ROOT "$ENV{WORKFLOW_ONNXRUNTIME_ROOT}" CACHE PATH
        "ONNX Runtime ${WORKFLOW_ONNXRUNTIME_VERSION} Windows x64 package root")
set(WORKFLOW_ONNXRUNTIME_LIBRARY
        "${WORKFLOW_ONNXRUNTIME_ROOT}/lib/onnxruntime.lib" CACHE FILEPATH
        "ONNX Runtime import library")
set(WORKFLOW_ONNXRUNTIME_RUNTIME
        "${WORKFLOW_ONNXRUNTIME_ROOT}/lib/onnxruntime.dll" CACHE FILEPATH
        "ONNX Runtime runtime DLL")
set(_workflow_onnxruntime_header
        "${WORKFLOW_ONNXRUNTIME_ROOT}/include/onnxruntime_c_api.h")
if (NOT EXISTS "${_workflow_onnxruntime_header}" OR
    NOT EXISTS "${WORKFLOW_ONNXRUNTIME_ROOT}/VERSION_NUMBER" OR
    NOT EXISTS "${WORKFLOW_ONNXRUNTIME_LIBRARY}" OR
    NOT EXISTS "${WORKFLOW_ONNXRUNTIME_RUNTIME}")
    message(FATAL_ERROR
            "WORKFLOW_ONNXRUNTIME_ROOT is missing headers, import library, or runtime DLL")
endif()
file(READ "${WORKFLOW_ONNXRUNTIME_ROOT}/VERSION_NUMBER" _workflow_ort_version)
string(STRIP "${_workflow_ort_version}" _workflow_ort_version)
if (NOT _workflow_ort_version STREQUAL "${WORKFLOW_ONNXRUNTIME_VERSION}")
    message(FATAL_ERROR
            "Windows 7 packages require ONNX Runtime ${WORKFLOW_ONNXRUNTIME_VERSION}; found: ${_workflow_ort_version}")
endif()
file(STRINGS "${_workflow_onnxruntime_header}" _workflow_ort_api_line
        REGEX "^#define ORT_API_VERSION [0-9]+$")
if (NOT _workflow_ort_api_line STREQUAL "#define ORT_API_VERSION 13")
    message(FATAL_ERROR
            "Windows 7 packages require ONNX Runtime 1.13.x (ORT API 13); found: ${_workflow_ort_api_line}")
endif()

set(WORKFLOW_MODELS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/data/rapidocr/models" CACHE PATH
        "Directory containing det.onnx, cls.onnx, rec.onnx, and keys.txt")
foreach (_model det.onnx cls.onnx rec.onnx keys.txt)
    if (NOT EXISTS "${WORKFLOW_MODELS_DIR}/${_model}")
        message(FATAL_ERROR "OCR model is missing: ${WORKFLOW_MODELS_DIR}/${_model}")
    endif()
endforeach()

add_library(workflow_onnxruntime INTERFACE)
add_library(workflow::onnxruntime ALIAS workflow_onnxruntime)
set_target_properties(workflow_onnxruntime PROPERTIES EXPORT_NAME onnxruntime)
target_include_directories(workflow_onnxruntime INTERFACE
        "$<BUILD_INTERFACE:${WORKFLOW_ONNXRUNTIME_ROOT}/include>")
target_link_libraries(workflow_onnxruntime INTERFACE
        "$<BUILD_INTERFACE:${WORKFLOW_ONNXRUNTIME_LIBRARY}>")

add_subdirectory(src/third_party/rapidocr)

set(_workflow_win7_shim_targets
        workflow_win7_libraryloader
        workflow_win7_processtopology
        workflow_win7_eventing)
add_library(workflow_win7_libraryloader SHARED
        src/platform/win7/forwarder.cpp
        src/platform/win7/libraryloader.def)
add_library(workflow_win7_processtopology SHARED
        src/platform/win7/forwarder.cpp
        src/platform/win7/processtopology.def)
add_library(workflow_win7_eventing SHARED
        src/platform/win7/forwarder.cpp
        src/platform/win7/eventing.def)
set_target_properties(workflow_win7_libraryloader PROPERTIES
        AUTOMOC OFF
        PREFIX ""
        OUTPUT_NAME "api-ms-win-core-libraryloader-l1-2-0")
set_target_properties(workflow_win7_processtopology PROPERTIES
        AUTOMOC OFF
        PREFIX ""
        OUTPUT_NAME "api-ms-win-core-processtopology-obsolete-l1-1-0")
set_target_properties(workflow_win7_eventing PROPERTIES
        AUTOMOC OFF
        PREFIX ""
        OUTPUT_NAME "api-ms-win-eventing-provider-l1-1-0")
foreach (_shim_target IN LISTS _workflow_win7_shim_targets)
    target_link_options(${_shim_target} PRIVATE
            /NOENTRY
            /NODEFAULTLIB
            /SUBSYSTEM:WINDOWS,6.01)
endforeach()

set(WORKFLOW_SOURCES
        src/workflow.h
        src/core/environment.cpp
        src/core/environment.h
        src/core/segment.cpp
        src/core/segment.h
        src/core/types.cpp
        src/core/types.h
        src/matching/image.cpp
        src/matching/image.h
        src/matching/selector.cpp
        src/matching/selector.h
        src/matching/text.cpp
        src/matching/text.h
        src/untils/base.cpp
        src/untils/base.h
        src/untils/image.cpp
        src/untils/image.h
        src/untils/text.cpp
        src/untils/text.h
        src/clickers/base.cpp
        src/clickers/base.h
        src/clickers/image.cpp
        src/clickers/image.h
        src/clickers/text.cpp
        src/clickers/text.h
        src/platform/mouse.h
        src/platform/platform.h
        src/platform/win32_mouse.cpp
        src/platform/win32_platform.cpp
        src/support/emitter.cpp
        src/support/emitter.h
        src/support/logging.cpp
        src/support/logging.h
        src/support/ocr.cpp
        src/support/ocr.h
        src/support/resources.cpp
        src/support/resources.h
        src/support/timing.cpp
        src/support/timing.h)

add_library(workflow STATIC ${WORKFLOW_SOURCES})
add_library(workflow::workflow ALIAS workflow)
set_target_properties(workflow PROPERTIES
        DEBUG_POSTFIX d
        EXPORT_NAME workflow)
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src" PREFIX "Source" FILES ${WORKFLOW_SOURCES})
target_compile_definitions(workflow PUBLIC
        UNICODE
        _UNICODE
        NOMINMAX
        WINVER=0x0601
        _WIN32_WINNT=0x0601
        NTDDI_VERSION=0x06010000)
target_compile_definitions(workflow PRIVATE OCR_STATIC)
target_compile_options(workflow
        PUBLIC "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
        PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/permissive->")
target_compile_features(workflow PUBLIC cxx_std_20)
target_include_directories(workflow PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>"
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")
target_include_directories(workflow PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src/third_party/rapidocr/include")
target_link_libraries(workflow PUBLIC
        workflow_rapidocr
        Qt5::Core
        ${OpenCV_LIBS}
        user32
        gdi32)

set(WORKFLOW_OPENCV_BIN_DIR "$ENV{WORKFLOW_OPENCV_BIN_DIR}" CACHE PATH
        "Directory containing the OpenCV 4.10 runtime DLLs")
if (NOT WORKFLOW_OPENCV_BIN_DIR AND OpenCV_DIR AND OpenCV_ARCH AND OpenCV_RUNTIME)
    set(WORKFLOW_OPENCV_BIN_DIR
            "${OpenCV_DIR}/${OpenCV_ARCH}/${OpenCV_RUNTIME}/bin")
endif()

set(_workflow_opencv_runtime_available OFF)
if (EXISTS "${WORKFLOW_OPENCV_BIN_DIR}/opencv_world4100.dll")
    set(WORKFLOW_OPENCV_WORLD ON)
    set(_workflow_opencv_runtime_available ON)
elseif (EXISTS "${WORKFLOW_OPENCV_BIN_DIR}/opencv_core4100.dll" AND
       EXISTS "${WORKFLOW_OPENCV_BIN_DIR}/opencv_imgproc4100.dll" AND
       EXISTS "${WORKFLOW_OPENCV_BIN_DIR}/opencv_imgcodecs4100.dll")
    set(WORKFLOW_OPENCV_WORLD OFF)
    set(_workflow_opencv_runtime_available ON)
endif()

set(WORKFLOW_WINDOWS_MANIFEST
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/workflow-windows.manifest")

function(workflow_enable_windows7 target)
    if (NOT TARGET ${target})
        message(FATAL_ERROR "workflow_enable_windows7 target does not exist: ${target}")
    endif()
    if (MSVC)
        get_target_property(_workflow_win32_executable ${target} WIN32_EXECUTABLE)
        if (_workflow_win32_executable)
            set(_workflow_subsystem WINDOWS)
        else()
            set(_workflow_subsystem CONSOLE)
        endif()
        target_link_options(${target} PRIVATE
                "/SUBSYSTEM:${_workflow_subsystem},6.01"
                "/MANIFESTINPUT:${WORKFLOW_WINDOWS_MANIFEST}")
    endif()
endfunction()

function(workflow_stage_runtime target)
    if (NOT TARGET ${target})
        message(FATAL_ERROR "workflow_stage_runtime target does not exist: ${target}")
    endif()
    if (NOT _workflow_opencv_runtime_available)
        message(FATAL_ERROR
                "OpenCV 4.10 runtime DLLs were not found in ${WORKFLOW_OPENCV_BIN_DIR}")
    endif()

    add_dependencies(${target} ${_workflow_win7_shim_targets})

    set(_runtime_dir "$<TARGET_FILE_DIR:${target}>")
    set(_workflow_source_dir "${CMAKE_CURRENT_FUNCTION_LIST_DIR}")
    add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${WORKFLOW_ONNXRUNTIME_RUNTIME}" "${_runtime_dir}"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "$<TARGET_FILE:Qt5::Core>" "${_runtime_dir}"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "$<TARGET_FILE:workflow_win7_libraryloader>" "${_runtime_dir}"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "$<TARGET_FILE:workflow_win7_processtopology>" "${_runtime_dir}"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "$<TARGET_FILE:workflow_win7_eventing>" "${_runtime_dir}"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${WORKFLOW_MODELS_DIR}" "${_runtime_dir}/models"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${_runtime_dir}/licenses"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${_workflow_source_dir}/LICENSE"
                    "${_runtime_dir}/licenses/workflow-MIT.txt"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${_workflow_source_dir}/THIRD_PARTY_NOTICES.md"
                    "${_runtime_dir}/licenses/THIRD_PARTY_NOTICES.md"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${_workflow_source_dir}/src/third_party/rapidocr/LICENSE"
                    "${_runtime_dir}/licenses/Apache-2.0.txt"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${_workflow_source_dir}/licenses/BOOST-1.0.txt"
                    "${_runtime_dir}/licenses/Boost-1.0.txt"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${WORKFLOW_ONNXRUNTIME_ROOT}/LICENSE"
                    "${_runtime_dir}/licenses/ONNXRuntime-MIT.txt"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${WORKFLOW_ONNXRUNTIME_ROOT}/ThirdPartyNotices.txt"
                    "${_runtime_dir}/licenses/ONNXRuntime-ThirdPartyNotices.txt"
            VERBATIM)
    if (WORKFLOW_OPENCV_WORLD)
        add_custom_command(TARGET ${target} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${WORKFLOW_OPENCV_BIN_DIR}/opencv_world4100$<$<CONFIG:Debug>:d>.dll"
                        "${_runtime_dir}"
                VERBATIM)
    else()
        foreach (_component core imgproc imgcodecs)
            add_custom_command(TARGET ${target} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                            "${WORKFLOW_OPENCV_BIN_DIR}/opencv_${_component}4100$<$<CONFIG:Debug>:d>.dll"
                            "${_runtime_dir}"
                    VERBATIM)
        endforeach()
    endif()
endfunction()

if (WORKFLOW_BUILD_EXAMPLES)
    add_executable(mixed_workflow_example examples/mixed_workflow.cpp)
    target_link_libraries(mixed_workflow_example PRIVATE workflow::workflow)
    workflow_enable_windows7(mixed_workflow_example)
    workflow_stage_runtime(mixed_workflow_example)

    if (PROJECT_IS_TOP_LEVEL)
        find_program(WORKFLOW_POWERSHELL_EXECUTABLE NAMES pwsh powershell REQUIRED)
        if (WORKFLOW_OPENCV_WORLD)
            set(_workflow_opencv_audit_dll
                    "${WORKFLOW_OPENCV_BIN_DIR}/opencv_world4100$<$<CONFIG:Debug>:d>.dll")
        else()
            set(_workflow_opencv_audit_dll
                    "${WORKFLOW_OPENCV_BIN_DIR}/opencv_core4100$<$<CONFIG:Debug>:d>.dll")
        endif()
        add_custom_target(workflow_win7_audit
                COMMAND "${WORKFLOW_POWERSHELL_EXECUTABLE}"
                        -NoProfile
                        -ExecutionPolicy Bypass
                        -File "${CMAKE_CURRENT_SOURCE_DIR}/scripts/Test-Windows7Compatibility.ps1"
                        "$<TARGET_FILE:mixed_workflow_example>"
                        "${WORKFLOW_ONNXRUNTIME_RUNTIME}"
                        "$<TARGET_FILE:Qt5::Core>"
                        "${_workflow_opencv_audit_dll}"
                        "$<TARGET_FILE:workflow_win7_libraryloader>"
                        "$<TARGET_FILE:workflow_win7_processtopology>"
                        "$<TARGET_FILE:workflow_win7_eventing>"
                DEPENDS mixed_workflow_example
                COMMENT "Auditing PE subsystem versions and post-Windows-7 imports"
                VERBATIM)
    endif()
endif()

if (WORKFLOW_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if (WORKFLOW_INSTALL)
    set(_workflow_package_cmake_dir "${CMAKE_INSTALL_LIBDIR}/cmake/workflow")
    set(_workflow_package_data_dir "${CMAKE_INSTALL_DATADIR}/workflow")

    install(TARGETS workflow workflow_rapidocr workflow_onnxruntime
            EXPORT workflowTargets
            ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
            LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
            RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
    install(TARGETS ${_workflow_win7_shim_targets}
            RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/src/"
            DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
            FILES_MATCHING
            PATTERN "*.h"
            PATTERN "win7" EXCLUDE
            PATTERN "third_party" EXCLUDE)
    install(FILES "${WORKFLOW_ONNXRUNTIME_LIBRARY}"
            DESTINATION "${CMAKE_INSTALL_LIBDIR}")
    install(FILES "${WORKFLOW_ONNXRUNTIME_RUNTIME}"
            DESTINATION "${CMAKE_INSTALL_BINDIR}")
    install(DIRECTORY "${WORKFLOW_MODELS_DIR}/"
            DESTINATION "${_workflow_package_data_dir}/models")
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE"
            DESTINATION "${_workflow_package_data_dir}/licenses"
            RENAME workflow-MIT.txt)
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/THIRD_PARTY_NOTICES.md"
            DESTINATION "${_workflow_package_data_dir}/licenses")
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/third_party/rapidocr/LICENSE"
            DESTINATION "${_workflow_package_data_dir}/licenses"
            RENAME Apache-2.0.txt)
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/licenses/BOOST-1.0.txt"
            DESTINATION "${_workflow_package_data_dir}/licenses"
            RENAME Boost-1.0.txt)
    install(FILES "${WORKFLOW_ONNXRUNTIME_ROOT}/LICENSE"
            DESTINATION "${_workflow_package_data_dir}/licenses"
            RENAME ONNXRuntime-MIT.txt)
    install(FILES "${WORKFLOW_ONNXRUNTIME_ROOT}/ThirdPartyNotices.txt"
            DESTINATION "${_workflow_package_data_dir}/licenses"
            RENAME ONNXRuntime-ThirdPartyNotices.txt)
    install(FILES "${WORKFLOW_WINDOWS_MANIFEST}"
            DESTINATION "${_workflow_package_data_dir}/windows")
    install(FILES
            "${CMAKE_CURRENT_SOURCE_DIR}/README.md"
            "${CMAKE_CURRENT_SOURCE_DIR}/CONTRIBUTING.md"
            "${CMAKE_CURRENT_SOURCE_DIR}/CHANGELOG.md"
            "${CMAKE_CURRENT_SOURCE_DIR}/SECURITY.md"
            DESTINATION "${_workflow_package_data_dir}/docs")

    configure_package_config_file(
            "${CMAKE_CURRENT_SOURCE_DIR}/cmake/workflowConfig.cmake.in"
            "${CMAKE_CURRENT_BINARY_DIR}/workflowConfig.cmake"
            INSTALL_DESTINATION "${_workflow_package_cmake_dir}"
            PATH_VARS CMAKE_INSTALL_BINDIR CMAKE_INSTALL_LIBDIR
                      CMAKE_INSTALL_INCLUDEDIR CMAKE_INSTALL_DATADIR)
    write_basic_package_version_file(
            "${CMAKE_CURRENT_BINARY_DIR}/workflowConfigVersion.cmake"
            VERSION "${PROJECT_VERSION}"
            COMPATIBILITY SameMajorVersion)
    configure_file(
            "${CMAKE_CURRENT_SOURCE_DIR}/cmake/WorkflowRuntime.cmake.in"
            "${CMAKE_CURRENT_BINARY_DIR}/WorkflowRuntime.cmake"
            @ONLY)

    install(FILES
            "${CMAKE_CURRENT_BINARY_DIR}/workflowConfig.cmake"
            "${CMAKE_CURRENT_BINARY_DIR}/workflowConfigVersion.cmake"
            "${CMAKE_CURRENT_BINARY_DIR}/WorkflowRuntime.cmake"
            DESTINATION "${_workflow_package_cmake_dir}")
    install(EXPORT workflowTargets
            FILE workflowTargets.cmake
            NAMESPACE workflow::
            DESTINATION "${_workflow_package_cmake_dir}")

    set(CPACK_GENERATOR ZIP)
    set(CPACK_PACKAGE_NAME workflow)
    set(CPACK_PACKAGE_VENDOR "workflow contributors")
    set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
    set(CPACK_PACKAGE_FILE_NAME
            "workflow-${PROJECT_VERSION}-windows-x64-win7sp1")
    set(CPACK_PACKAGE_CHECKSUM SHA256)
    include(CPack)
endif()

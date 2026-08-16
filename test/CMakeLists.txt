add_executable(workflow_unit workflow.cpp)
target_link_libraries(workflow_unit PRIVATE workflow::workflow)
target_compile_definitions(workflow_unit PRIVATE
        "WORKFLOW_TEST_WORKFLOW_ROOT=\"${CMAKE_CURRENT_SOURCE_DIR}/app/page/workflows\"")

workflow_enable_windows7(workflow_unit)
workflow_stage_runtime(workflow_unit)

if (PROJECT_IS_TOP_LEVEL)
    find_program(WORKFLOW_UNIT_POWERSHELL_EXECUTABLE
            NAMES pwsh powershell
            REQUIRED)
    add_custom_target(workflow_win7_audit
            COMMAND "${WORKFLOW_UNIT_POWERSHELL_EXECUTABLE}"
                    -NoProfile
                    -ExecutionPolicy Bypass
                    -File "${PROJECT_SOURCE_DIR}/tool/audit.ps1"
                    "$<TARGET_FILE_DIR:workflow_unit>"
            DEPENDS workflow_unit
            COMMENT "Auditing the SDK test runtime for Windows 7 compatibility"
            VERBATIM)
endif()

add_test(NAME workflow.unit COMMAND $<TARGET_FILE:workflow_unit>)
set_tests_properties(workflow.unit PROPERTIES
        WORKING_DIRECTORY "$<TARGET_FILE_DIR:workflow_unit>")

add_executable(workflow_windows windows.cpp)
target_link_libraries(workflow_windows PRIVATE workflow::workflow)
workflow_enable_windows7(workflow_windows)
add_test(NAME workflow.windows COMMAND $<TARGET_FILE:workflow_windows>)

add_test(NAME workflow.pages
        COMMAND ${CMAKE_COMMAND}
                "-DWORKFLOW_TEST_PAGE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}/app/page"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/app/validate.cmake")

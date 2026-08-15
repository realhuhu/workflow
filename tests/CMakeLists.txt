add_executable(workflow_tests workflow_tests.cpp)
target_link_libraries(workflow_tests PRIVATE workflow::workflow)

workflow_enable_windows7(workflow_tests)
workflow_stage_runtime(workflow_tests)

add_test(NAME workflow.contracts COMMAND $<TARGET_FILE:workflow_tests>)
set_tests_properties(workflow.contracts PROPERTIES
        WORKING_DIRECTORY "$<TARGET_FILE_DIR:workflow_tests>")

add_executable(workflow_windows_target_contract windows_target_contract.cpp)
target_link_libraries(workflow_windows_target_contract PRIVATE workflow::workflow)
workflow_enable_windows7(workflow_windows_target_contract)
add_test(NAME workflow.windows-target
        COMMAND $<TARGET_FILE:workflow_windows_target_contract>)

add_test(NAME workflow.browser-fixtures
        COMMAND ${CMAKE_COMMAND}
                "-DWORKFLOW_BROWSER_FIXTURE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}/browser/web"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/browser/ValidateFixtures.cmake")

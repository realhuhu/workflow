add_executable(workflow_unit workflow.cpp)
target_link_libraries(workflow_unit PRIVATE workflow::workflow)

workflow_enable_windows7(workflow_unit)
workflow_stage_runtime(workflow_unit)

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

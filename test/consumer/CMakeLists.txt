cmake_minimum_required(VERSION 3.24)

project(workflow_package_consumer LANGUAGES CXX)

find_package(workflow CONFIG REQUIRED)

add_executable(workflow_package_consumer main.cpp)
target_link_libraries(workflow_package_consumer PRIVATE workflow::workflow)
workflow_enable_windows7(workflow_package_consumer)
workflow_stage_runtime(workflow_package_consumer)

enable_testing()
add_test(NAME workflow.package-consumer
        COMMAND $<TARGET_FILE:workflow_package_consumer>)

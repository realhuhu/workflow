set(WORKFLOW_TEST_APP_SOURCES
        main.cpp
        window.cpp
        window.h
        cases.cpp
        cases.h)

add_executable(workflow_test WIN32 ${WORKFLOW_TEST_APP_SOURCES})
target_link_libraries(workflow_test PRIVATE
        workflow::workflow
        Qt5::Gui
        Qt5::Widgets
        Qt5::WebEngineWidgets)
workflow_enable_windows7(workflow_test)
workflow_stage_runtime(workflow_test)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}"
        PREFIX "Test App"
        FILES ${WORKFLOW_TEST_APP_SOURCES})

add_custom_command(TARGET workflow_test POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_CURRENT_SOURCE_DIR}/page"
                "$<TARGET_FILE_DIR:workflow_test>/test-page"
        COMMENT "Staging the automation test pages"
        VERBATIM)

find_program(WORKFLOW_WINDEPLOYQT_EXECUTABLE
        NAMES windeployqt windeployqt.exe
        HINTS "${WORKFLOW_QT_ROOT}/bin"
        REQUIRED)
add_custom_command(TARGET workflow_test POST_BUILD
        COMMAND "${WORKFLOW_WINDEPLOYQT_EXECUTABLE}"
                --no-compiler-runtime
                --no-translations
                "$<TARGET_FILE:workflow_test>"
        COMMAND ${CMAKE_COMMAND} -E rm -f
                "$<TARGET_FILE_DIR:workflow_test>/position/qtposition_winrt.dll"
                "$<TARGET_FILE_DIR:workflow_test>/position/qtposition_winrtd.dll"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/README.md"
                "$<TARGET_FILE_DIR:workflow_test>/README.md"
        COMMENT "Deploying the Qt WebEngine test runtime"
        VERBATIM)

set_target_properties(workflow_test PROPERTIES OUTPUT_NAME "workflow-test")

if (PROJECT_IS_TOP_LEVEL)
    find_program(WORKFLOW_TEST_POWERSHELL_EXECUTABLE
            NAMES pwsh powershell
            REQUIRED)
    add_custom_target(workflow_test_win7_audit
            COMMAND "${WORKFLOW_TEST_POWERSHELL_EXECUTABLE}"
                    -NoProfile
                    -ExecutionPolicy Bypass
                    -File "${PROJECT_SOURCE_DIR}/tool/audit.ps1"
                    "$<TARGET_FILE_DIR:workflow_test>"
            DEPENDS workflow_test
            COMMENT "Auditing the test runtime for Windows 7 compatibility"
            VERBATIM)
endif()

if (WORKFLOW_INSTALL)
    install(DIRECTORY "$<TARGET_FILE_DIR:workflow_test>/"
            DESTINATION "."
            COMPONENT Test
            EXCLUDE_FROM_ALL
            USE_SOURCE_PERMISSIONS
            PATTERN "CMakeFiles" EXCLUDE
            PATTERN "*_autogen" EXCLUDE
            PATTERN "*.cmake" EXCLUDE
            PATTERN "*.ninja" EXCLUDE
            PATTERN "*.obj" EXCLUDE
            PATTERN "*.pdb" EXCLUDE
            PATTERN "*.ilk" EXCLUDE
            PATTERN "*.lib" EXCLUDE
            PATTERN "*.exp" EXCLUDE
            PATTERN "*.manifest" EXCLUDE
            PATTERN "*.rc" EXCLUDE
            PATTERN "*.res" EXCLUDE)
endif()

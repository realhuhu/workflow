set(WORKFLOW_BROWSER_HARNESS_SOURCES
        main.cpp
        browser.cpp
        browser.h
        cases.cpp
        cases.h)

add_executable(workflow_browser_harness WIN32 ${WORKFLOW_BROWSER_HARNESS_SOURCES})
target_link_libraries(workflow_browser_harness PRIVATE
        workflow::workflow
        Qt5::Gui
        Qt5::Widgets
        Qt5::WebEngineWidgets)
workflow_enable_windows7(workflow_browser_harness)
workflow_stage_runtime(workflow_browser_harness)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}"
        PREFIX "Browser Harness"
        FILES ${WORKFLOW_BROWSER_HARNESS_SOURCES})

add_custom_command(TARGET workflow_browser_harness POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_CURRENT_SOURCE_DIR}/web"
                "$<TARGET_FILE_DIR:workflow_browser_harness>/browser-test"
        COMMENT "Staging the browser automation fixtures"
        VERBATIM)

find_program(WORKFLOW_WINDEPLOYQT_EXECUTABLE
        NAMES windeployqt windeployqt.exe
        HINTS "${WORKFLOW_QT_ROOT}/bin"
        REQUIRED)
add_custom_command(TARGET workflow_browser_harness POST_BUILD
        COMMAND "${WORKFLOW_WINDEPLOYQT_EXECUTABLE}"
                --no-compiler-runtime
                --no-translations
                "$<TARGET_FILE:workflow_browser_harness>"
        COMMAND ${CMAKE_COMMAND} -E rm -f
                "$<TARGET_FILE_DIR:workflow_browser_harness>/position/qtposition_winrt.dll"
                "$<TARGET_FILE_DIR:workflow_browser_harness>/position/qtposition_winrtd.dll"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/README.md"
                "$<TARGET_FILE_DIR:workflow_browser_harness>/README.md"
        COMMENT "Deploying the Qt WebEngine test runtime"
        VERBATIM)

set_target_properties(workflow_browser_harness PROPERTIES
        OUTPUT_NAME "workflow-browser-harness")

if (PROJECT_IS_TOP_LEVEL)
    find_program(WORKFLOW_BROWSER_POWERSHELL_EXECUTABLE
            NAMES pwsh powershell
            REQUIRED)
    add_custom_target(workflow_browser_win7_audit
            COMMAND "${WORKFLOW_BROWSER_POWERSHELL_EXECUTABLE}"
                    -NoProfile
                    -ExecutionPolicy Bypass
                    -File "${PROJECT_SOURCE_DIR}/scripts/Test-Windows7Compatibility.ps1"
                    "$<TARGET_FILE_DIR:workflow_browser_harness>"
            DEPENDS workflow_browser_harness
            COMMENT "Auditing the Browser UI runtime for Windows 7 compatibility"
            VERBATIM)
endif()

if (WORKFLOW_INSTALL)
    install(DIRECTORY "$<TARGET_FILE_DIR:workflow_browser_harness>/"
            DESTINATION "."
            COMPONENT Browser
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

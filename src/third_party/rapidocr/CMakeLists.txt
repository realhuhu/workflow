add_library(workflow_rapidocr STATIC
        src/AngleNet.cpp
        src/CrnnNet.cpp
        src/DbNet.cpp
        src/OcrUtils.cpp
        src/OcrLite.cpp
        src/clipper.cpp)

set_target_properties(workflow_rapidocr PROPERTIES AUTOMOC OFF)
set_target_properties(workflow_rapidocr PROPERTIES
        DEBUG_POSTFIX d
        EXPORT_NAME rapidocr)

target_compile_definitions(workflow_rapidocr PRIVATE
        OCR_STATIC
        UNICODE
        _UNICODE
        NOMINMAX
        WINVER=0x0601
        _WIN32_WINNT=0x0601
        NTDDI_VERSION=0x06010000)
target_compile_options(workflow_rapidocr PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
        "$<$<CXX_COMPILER_ID:MSVC>:/permissive->")
target_compile_features(workflow_rapidocr PRIVATE cxx_std_20)

target_include_directories(workflow_rapidocr PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include")

target_link_libraries(workflow_rapidocr PUBLIC
        workflow_onnxruntime
        ${OpenCV_LIBS})

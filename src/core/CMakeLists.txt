set(CORE_SOURCES
    application.cpp
    event_manager.cpp
    thread_pool.cpp
    screen_capture_manager.cpp
    capture_utils.cpp
    ocr_manager.cpp
    ocr_utils.cpp
    paddle_ocr_engine.cpp
    minicpm_v_engine.cpp
    minicpm_v_model.cpp
    tesseract_engine.cpp
    command_line_parser.cpp
    simple_daemon.cpp
    performance_monitor.cpp
)

set(CORE_HEADERS
    ${CMAKE_SOURCE_DIR}/include/application.h
    ${CMAKE_SOURCE_DIR}/include/event_manager.h
    ${CMAKE_SOURCE_DIR}/include/thread_pool.h
    ${CMAKE_SOURCE_DIR}/include/screen_capture.h
    ${CMAKE_SOURCE_DIR}/include/ocr_engine.h
    ${CMAKE_SOURCE_DIR}/include/paddle_ocr_engine.h
    ${CMAKE_SOURCE_DIR}/include/minicpm_v_engine.h
    ${CMAKE_SOURCE_DIR}/include/command_line_parser.h
)

add_library(core_lib STATIC
    ${CORE_SOURCES}
    ${CORE_HEADERS}
)

target_include_directories(core_lib PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(core_lib
    ${CMAKE_THREAD_LIBS_INIT}
    paddle_inference
    opencv_inference
    llama_inference
)

# Add PaddleInference if found
if(PaddleInference_FOUND)
    target_compile_definitions(core_lib PUBLIC PADDLE_INFERENCE_FOUND=1)
    target_link_libraries(core_lib PaddleInference::paddle_inference)
else()
    target_compile_definitions(core_lib PUBLIC PADDLE_INFERENCE_FOUND=0)
endif()

# Add OpenCV if found
if(TARGET opencv_core)
    target_compile_definitions(core_lib PUBLIC OPENCV_FOUND=1)
    target_link_libraries(core_lib opencv_core opencv_imgproc opencv_imgcodecs)
else()
    target_compile_definitions(core_lib PUBLIC OPENCV_FOUND=0)
endif()

# Add llama.cpp - force enable since we know it exists
target_include_directories(core_lib PRIVATE
    ${CMAKE_BINARY_DIR}/_deps/llama_cpp-src/include
    ${CMAKE_BINARY_DIR}/_deps/llama_cpp-src/common
    ${CMAKE_BINARY_DIR}/_deps/llama_cpp-src/ggml/include
)
target_link_libraries(core_lib llama ggml)
target_compile_definitions(core_lib PUBLIC LLAMA_CPP_AVAILABLE=1)
target_compile_options(core_lib PRIVATE -fopenmp)
target_link_options(core_lib PRIVATE -fopenmp)

# Also link common library
if(TARGET common)
    target_link_libraries(core_lib common)
endif()
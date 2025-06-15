cmake_minimum_required(VERSION 3.14)
project(WorkStudyAssistant VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Set compile flags
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -Wall -Wextra")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

# Add module path
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

# Add Drogon build path for package finding
list(APPEND CMAKE_PREFIX_PATH "${CMAKE_SOURCE_DIR}/third_party/drogon/build")

# Find packages
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(PaddleInference)
find_package(screen_capture_lite)
# find_package(Drogon) # Re-enabled later after yaml-cpp fixes

# Platform-specific settings
if(WIN32)
    set(PLATFORM_SOURCES src/platform/windows)
    set(PLATFORM_LIBS user32 gdi32 ole32 oleaut32)
elseif(UNIX AND NOT APPLE)
    set(PLATFORM_SOURCES src/platform/linux)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(X11 REQUIRED x11)
    pkg_check_modules(ATSPI REQUIRED atspi-2)
    set(PLATFORM_LIBS ${X11_LIBRARIES} ${ATSPI_LIBRARIES})
endif()

# Include directories
include_directories(include)
include_directories(third_party/include)

# Add subdirectories
add_subdirectory(src/core)
add_subdirectory(src/platform)
add_subdirectory(src/web)
add_subdirectory(src/ai)
add_subdirectory(src/storage)
add_subdirectory(third_party)

# Main executable
add_executable(work_study_assistant
    src/main.cpp
)

# Benchmark executable
add_executable(test_benchmark
    test_benchmark.cpp
)

# Real LLaMA test executable
add_executable(test_real_llama
    test_real_llama.cpp
)

target_link_libraries(work_study_assistant
    core_lib
    platform_lib
    web_lib
    ai_lib
    storage_lib
    ${CMAKE_THREAD_LIBS_INIT}
    OpenSSL::SSL
    OpenSSL::Crypto
    SQLite::SQLite3
    ${PLATFORM_LIBS}
    paddle_inference
    opencv_inference
    llama_inference
)

# Link llama.cpp libraries if available
if(TARGET llama)
    target_link_libraries(work_study_assistant llama)
    if(TARGET common)
        target_link_libraries(work_study_assistant common)
    endif()
endif()

target_link_libraries(test_benchmark
    core_lib
    ${CMAKE_THREAD_LIBS_INIT}
)

target_link_libraries(test_real_llama
    core_lib
    ai_lib
    storage_lib
    ${CMAKE_THREAD_LIBS_INIT}
    OpenSSL::SSL
    OpenSSL::Crypto
    SQLite::SQLite3
)

# Link llama.cpp libraries for real test
if(TARGET llama)
    target_link_libraries(test_real_llama llama)
    if(TARGET common)
        target_link_libraries(test_real_llama common)
    endif()
endif()

# Install targets
install(TARGETS work_study_assistant
    RUNTIME DESTINATION bin
)

# Tests
enable_testing()
add_subdirectory(tests)

# Documentation
find_package(Doxygen)
if(DOXYGEN_FOUND)
    set(DOXYGEN_IN ${CMAKE_CURRENT_SOURCE_DIR}/docs/Doxyfile.in)
    set(DOXYGEN_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)
    
    configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)
    
    add_custom_target(doc_doxygen ALL
        COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Generating API documentation with Doxygen"
        VERBATIM)
endif()
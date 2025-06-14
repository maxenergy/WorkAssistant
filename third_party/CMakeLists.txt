# Third party libraries
include(FetchContent)
include(ExternalProject)

# Set third party build directory
set(THIRD_PARTY_BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR}/third_party)
set(THIRD_PARTY_INSTALL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/install)

# Create directories
file(MAKE_DIRECTORY ${THIRD_PARTY_BUILD_DIR})
file(MAKE_DIRECTORY ${THIRD_PARTY_INSTALL_DIR})

# ============================================
# PaddlePaddle C++ Inference Library
# ============================================

set(PADDLE_VERSION "2.5.2")
set(PADDLE_ROOT ${THIRD_PARTY_INSTALL_DIR}/paddle_inference)

# Determine platform and architecture
if(WIN32)
    set(PADDLE_PLATFORM "win")
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(PADDLE_ARCH "amd64")
    else()
        set(PADDLE_ARCH "win32")
    endif()
    set(PADDLE_LIB_SUFFIX ".lib")
    set(PADDLE_DLL_SUFFIX ".dll")
elseif(APPLE)
    set(PADDLE_PLATFORM "mac")
    set(PADDLE_ARCH "x86_64")
    set(PADDLE_LIB_SUFFIX ".dylib")
    set(PADDLE_DLL_SUFFIX ".dylib")
else()
    set(PADDLE_PLATFORM "linux")
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(PADDLE_ARCH "x86_64")
    else()
        set(PADDLE_ARCH "x86")
    endif()
    set(PADDLE_LIB_SUFFIX ".so")
    set(PADDLE_DLL_SUFFIX ".so")
endif()

# Check for CUDA support
find_package(CUDA QUIET)
if(CUDA_FOUND)
    set(PADDLE_WITH_CUDA "ON")
    set(PADDLE_CUDA_TAG "cuda${CUDA_VERSION_MAJOR}.${CUDA_VERSION_MINOR}")
    message(STATUS "Found CUDA ${CUDA_VERSION}, enabling GPU support for PaddlePaddle")
else()
    set(PADDLE_WITH_CUDA "OFF")
    set(PADDLE_CUDA_TAG "cpu")
    message(STATUS "CUDA not found, using CPU-only PaddlePaddle")
endif()

# Construct download URL
set(PADDLE_URL_BASE "https://paddle-inference-lib.bj.bcebos.com")
set(PADDLE_FILENAME "paddle_inference-${PADDLE_PLATFORM}-${PADDLE_ARCH}-${PADDLE_CUDA_TAG}-${PADDLE_VERSION}.tgz")
set(PADDLE_URL "${PADDLE_URL_BASE}/${PADDLE_VERSION}-${PADDLE_CUDA_TAG}/${PADDLE_FILENAME}")

message(STATUS "PaddlePaddle URL: ${PADDLE_URL}")

# Check if PaddlePaddle is manually installed
if(EXISTS ${PADDLE_ROOT}/paddle/lib/libpaddle_inference${PADDLE_LIB_SUFFIX})
    message(STATUS "Found manually installed PaddlePaddle at ${PADDLE_ROOT}")
    set(PADDLE_MANUALLY_INSTALLED TRUE)
else()
    message(WARNING "PaddlePaddle not found at ${PADDLE_ROOT}")
    message(STATUS "Please manually download PaddlePaddle C++ inference library:")
    message(STATUS "1. Visit: https://www.paddlepaddle.org.cn/inference/master/guides/install/download_lib.html")
    message(STATUS "2. Download the Linux CPU version")
    message(STATUS "3. Extract to: ${PADDLE_ROOT}")
    message(STATUS "4. Re-run cmake")
    set(PADDLE_MANUALLY_INSTALLED FALSE)
    
    # Create a dummy target to avoid build errors
    add_custom_target(paddle_inference_download
        COMMAND ${CMAKE_COMMAND} -E echo "PaddlePaddle library needs manual installation"
        COMMAND ${CMAKE_COMMAND} -E echo "Download from: https://www.paddlepaddle.org.cn/inference/master/guides/install/download_lib.html"
        COMMAND ${CMAKE_COMMAND} -E echo "Extract to: ${PADDLE_ROOT}"
    )
endif()

# Set PaddlePaddle paths
set(PADDLE_INC_DIR ${PADDLE_ROOT}/paddle/include)
set(PADDLE_LIB_DIR ${PADDLE_ROOT}/paddle/lib)
set(PADDLE_BIN_DIR ${PADDLE_ROOT}/paddle/bin)

# Define PaddlePaddle libraries
set(PADDLE_LIBRARIES
    ${PADDLE_LIB_DIR}/libpaddle_inference${PADDLE_LIB_SUFFIX}
)

if(WIN32)
    list(APPEND PADDLE_LIBRARIES
        ${PADDLE_LIB_DIR}/common${PADDLE_LIB_SUFFIX}
        ${PADDLE_LIB_DIR}/paddle_fluid${PADDLE_LIB_SUFFIX}
    )
endif()

# Create imported target for PaddlePaddle
add_library(paddle_inference INTERFACE)

if(PADDLE_MANUALLY_INSTALLED)
    target_include_directories(paddle_inference INTERFACE ${PADDLE_INC_DIR})
    target_link_libraries(paddle_inference INTERFACE ${PADDLE_LIBRARIES})

    # Add compile definitions
    target_compile_definitions(paddle_inference INTERFACE
        PADDLE_WITH_MKLDNN
        PADDLE_WITH_MKLML
    )

    if(PADDLE_WITH_CUDA STREQUAL "ON")
        target_compile_definitions(paddle_inference INTERFACE PADDLE_WITH_CUDA)
    endif()
    
    message(STATUS "PaddlePaddle inference library configured successfully")
else()
    # Create stub implementation for build compatibility
    target_compile_definitions(paddle_inference INTERFACE PADDLE_NOT_AVAILABLE)
    message(STATUS "Building with PaddlePaddle stub - library not available")
endif()

add_dependencies(paddle_inference paddle_inference_download)

# ============================================
# OpenCV (required for PaddleOCR)
# ============================================

find_package(OpenCV QUIET)
if(NOT OpenCV_FOUND)
    message(STATUS "OpenCV not found, downloading...")
    
    FetchContent_Declare(
        opencv
        GIT_REPOSITORY https://github.com/opencv/opencv.git
        GIT_TAG 4.8.0
        GIT_SHALLOW TRUE
    )
    
    # OpenCV build options
    set(BUILD_LIST core,imgproc,imgcodecs,highgui CACHE STRING "OpenCV modules to build")
    set(BUILD_EXAMPLES OFF CACHE BOOL "Build OpenCV examples")
    set(BUILD_TESTS OFF CACHE BOOL "Build OpenCV tests")
    set(BUILD_PERF_TESTS OFF CACHE BOOL "Build OpenCV perf tests")
    set(WITH_IPP OFF CACHE BOOL "Include Intel IPP support")
    set(WITH_TBB OFF CACHE BOOL "Include Intel TBB support")
    
    FetchContent_MakeAvailable(opencv)
    
    # Create alias for consistent naming
    add_library(opencv_inference INTERFACE)
    target_link_libraries(opencv_inference INTERFACE opencv_core opencv_imgproc opencv_imgcodecs opencv_highgui)
else()
    message(STATUS "Found OpenCV ${OpenCV_VERSION}")
    add_library(opencv_inference INTERFACE)
    target_link_libraries(opencv_inference INTERFACE ${OpenCV_LIBS})
    target_include_directories(opencv_inference INTERFACE ${OpenCV_INCLUDE_DIRS})
endif()

# ============================================
# Exports for parent CMakeLists.txt
# ============================================

# Export variables to parent scope
set(PADDLE_INC_DIR ${PADDLE_INC_DIR} PARENT_SCOPE)
set(PADDLE_LIB_DIR ${PADDLE_LIB_DIR} PARENT_SCOPE)
set(PADDLE_LIBRARIES ${PADDLE_LIBRARIES} PARENT_SCOPE)
set(PADDLE_WITH_CUDA ${PADDLE_WITH_CUDA} PARENT_SCOPE)

message(STATUS "PaddlePaddle will be installed to: ${PADDLE_ROOT}")
message(STATUS "PaddlePaddle include dir: ${PADDLE_INC_DIR}")
message(STATUS "PaddlePaddle library dir: ${PADDLE_LIB_DIR}")

# Future third party integrations:
# - screen_capture_lite
# - llama.cpp  
# - tesseract
# - libsodium
# - drogon
# - nlohmann/json
set(AI_SOURCES
    llama_engine.cpp
    ai_content_analyzer.cpp
)

set(AI_HEADERS
    ${CMAKE_SOURCE_DIR}/include/ai_engine.h
)

add_library(ai_lib STATIC
    ${AI_SOURCES}
    ${AI_HEADERS}
)

target_include_directories(ai_lib PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

# Add llama.cpp includes and libraries - force enable since we know it exists
target_include_directories(ai_lib PRIVATE
    ${CMAKE_BINARY_DIR}/_deps/llama_cpp-src/include
    ${CMAKE_BINARY_DIR}/_deps/llama_cpp-src/common
    ${CMAKE_BINARY_DIR}/_deps/llama_cpp-src/ggml/include
)
target_link_libraries(ai_lib llama ggml)
target_compile_definitions(ai_lib PRIVATE LLAMA_CPP_AVAILABLE=1)
target_compile_options(ai_lib PRIVATE -fopenmp)
target_link_options(ai_lib PRIVATE -fopenmp)

# Also link common library
if(TARGET common)
    target_link_libraries(ai_lib common)
endif()

target_link_libraries(ai_lib
    core_lib
    ${CMAKE_THREAD_LIBS_INIT}
)
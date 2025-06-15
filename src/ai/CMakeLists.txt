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

# Add llama.cpp includes and libraries if available
if(TARGET llama)
    target_include_directories(ai_lib PRIVATE
        ${CMAKE_BINARY_DIR}/_deps/llama_cpp-src/include
        ${CMAKE_BINARY_DIR}/_deps/llama_cpp-src/common
        ${CMAKE_BINARY_DIR}/_deps/llama_cpp-src/ggml/include
    )
    target_link_libraries(ai_lib llama)
    
    # Also link common library if available
    if(TARGET common)
        target_link_libraries(ai_lib common)
    endif()
endif()

target_link_libraries(ai_lib
    core_lib
    ${CMAKE_THREAD_LIBS_INIT}
)
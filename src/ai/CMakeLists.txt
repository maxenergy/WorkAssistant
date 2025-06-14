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

target_link_libraries(ai_lib
    core_lib
    ${CMAKE_THREAD_LIBS_INIT}
)
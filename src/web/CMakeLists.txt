set(WEB_SOURCES
    web_server.cpp
    api_handlers.cpp
    websocket_handler.cpp
    websocket_server_factory.cpp
    placeholder.cpp
)

# Add Drogon implementation if available
if(Drogon_FOUND)
    list(APPEND WEB_SOURCES drogon_web_server.cpp)
endif()

set(WEB_HEADERS
    ${CMAKE_SOURCE_DIR}/include/web_server.h
)

add_library(web_lib STATIC
    ${WEB_SOURCES}
    ${WEB_HEADERS}
)

target_include_directories(web_lib PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

# Drogon is found by the parent CMakeLists.txt
if(Drogon_FOUND)
    message(STATUS "Drogon found, enabling enhanced web interface")
    add_definitions(-DDROGON_ENABLED)
    set(DROGON_LIBRARIES Drogon::Drogon)
else()
    message(STATUS "Drogon not found, using simple HTTP server")
    set(DROGON_LIBRARIES "")
endif()

target_link_libraries(web_lib
    core_lib
    storage_lib
    ${CMAKE_THREAD_LIBS_INIT}
    ${DROGON_LIBRARIES}
)
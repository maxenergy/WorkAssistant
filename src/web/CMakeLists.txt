set(WEB_SOURCES
    web_server.cpp
    api_handlers.cpp
    websocket_handler.cpp
    placeholder.cpp
)

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

# Find Drogon framework
find_package(PkgConfig REQUIRED)
pkg_check_modules(DROGON QUIET drogon)

if(DROGON_FOUND)
    message(STATUS "Drogon found, enabling web interface")
    add_definitions(-DWEB_ENABLED)
else()
    # If Drogon is not found via pkg-config, try find_package
    find_package(Drogon QUIET)
    if(Drogon_FOUND)
        message(STATUS "Drogon found via find_package, enabling web interface")
        set(DROGON_LIBRARIES Drogon::Drogon)
        set(DROGON_INCLUDE_DIRS "")
        add_definitions(-DWEB_ENABLED)
    else()
        message(STATUS "Drogon not found, web interface will be disabled")
        # Create a dummy implementation
        set(DROGON_LIBRARIES "")
        set(DROGON_INCLUDE_DIRS "")
        add_definitions(-DWEB_DISABLED)
    endif()
endif()

target_link_libraries(web_lib
    core_lib
    storage_lib
    ${CMAKE_THREAD_LIBS_INIT}
    ${DROGON_LIBRARIES}
)

# Include directories for Drogon if found via pkg-config
if(DROGON_INCLUDE_DIRS)
    target_include_directories(web_lib PRIVATE ${DROGON_INCLUDE_DIRS})
endif()
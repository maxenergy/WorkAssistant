set(STORAGE_SOURCES
    sqlite_storage_engine.cpp
    encrypted_storage_manager.cpp
    storage_utils.cpp
)

set(STORAGE_HEADERS
    ${CMAKE_SOURCE_DIR}/include/storage_engine.h
)

add_library(storage_lib STATIC
    ${STORAGE_SOURCES}
    ${STORAGE_HEADERS}
)

target_include_directories(storage_lib PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

# Find required libraries
find_package(PkgConfig REQUIRED)
find_package(OpenSSL REQUIRED)

# Try to find SQLite3
find_package(SQLite3 QUIET)
if(NOT SQLite3_FOUND)
    pkg_check_modules(SQLITE3 REQUIRED sqlite3)
    set(SQLite3_LIBRARIES ${SQLITE3_LIBRARIES})
    set(SQLite3_INCLUDE_DIRS ${SQLITE3_INCLUDE_DIRS})
endif()

target_link_libraries(storage_lib
    core_lib
    ai_lib
    ${CMAKE_THREAD_LIBS_INIT}
    OpenSSL::SSL
    OpenSSL::Crypto
    ${SQLite3_LIBRARIES}
)

# Include directories for SQLite3 if found via pkg-config
if(SQLITE3_INCLUDE_DIRS)
    target_include_directories(storage_lib PRIVATE ${SQLITE3_INCLUDE_DIRS})
endif()
# Test framework setup
cmake_minimum_required(VERSION 3.14)

# Enable testing
enable_testing()

# Basic storage functionality test
add_executable(test_storage
    test_storage.cpp
)

target_include_directories(test_storage PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(test_storage
    storage_lib
    ${CMAKE_THREAD_LIBS_INIT}
)

add_test(NAME StorageTest COMMAND test_storage)

# Basic AI test
add_executable(test_ai
    test_ai.cpp
)

target_include_directories(test_ai PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(test_ai
    ai_lib
    core_lib
    ${CMAKE_THREAD_LIBS_INIT}
)

add_test(NAME AITest COMMAND test_ai)

# Integration test (basic functionality)
add_executable(test_integration
    test_integration.cpp
)

target_include_directories(test_integration PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(test_integration
    core_lib
    ai_lib
    storage_lib
    web_lib
    ${CMAKE_THREAD_LIBS_INIT}
)

add_test(NAME IntegrationTest COMMAND test_integration)

# Custom test target for running all tests
add_custom_target(run_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --verbose
    DEPENDS test_storage test_ai test_integration
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
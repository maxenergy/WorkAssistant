set(CORE_SOURCES
    application.cpp
    event_manager.cpp
    thread_pool.cpp
    screen_capture_manager.cpp
    ocr_manager.cpp
    tesseract_engine.cpp
)

set(CORE_HEADERS
    ${CMAKE_SOURCE_DIR}/include/application.h
    ${CMAKE_SOURCE_DIR}/include/event_manager.h
    ${CMAKE_SOURCE_DIR}/include/thread_pool.h
    ${CMAKE_SOURCE_DIR}/include/screen_capture.h
    ${CMAKE_SOURCE_DIR}/include/ocr_engine.h
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
)
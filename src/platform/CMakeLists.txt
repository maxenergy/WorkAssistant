if(WIN32)
    set(PLATFORM_SOURCES
        windows/win32_window_monitor.cpp
        windows/win32_screen_capture.cpp
        windows/win32_daemon.cpp
    )
    set(PLATFORM_HEADERS
        windows/win32_window_monitor.h
        windows/win32_screen_capture.h  
        windows/win32_daemon.h
    )
elseif(UNIX AND NOT APPLE)
    set(PLATFORM_SOURCES
        linux/x11_window_monitor_minimal.cpp
        linux/linux_screen_capture.cpp
        linux/linux_daemon.cpp
        linux/atspi_client.cpp
    )
    set(PLATFORM_HEADERS
        linux/x11_window_monitor.h
        linux/linux_screen_capture.h
        linux/linux_daemon.h
        linux/atspi_client.h
    )
endif()

add_library(platform_lib STATIC
    ${PLATFORM_SOURCES}
    ${PLATFORM_HEADERS}
    window_monitor_factory.cpp
)

target_include_directories(platform_lib PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(platform_lib
    core_lib
    ${PLATFORM_LIBS}
)
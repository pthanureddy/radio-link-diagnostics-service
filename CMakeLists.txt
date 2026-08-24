cmake_minimum_required(VERSION 3.20)
project(radio_link_diagnostics_service VERSION 1.0.0 LANGUAGES CXX)

include(GNUInstallDirs)

option(RADIO_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)
option(RADIO_ENABLE_SANITIZERS "Enable address and undefined behavior sanitizers" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(radio_link
    src/crc32.cpp
    src/frame.cpp
    src/link_analyzer.cpp
    src/udp_socket.cpp
)
target_include_directories(radio_link PUBLIC include)

if(NOT MSVC)
    if(RADIO_ENABLE_SANITIZERS)
        target_compile_options(radio_link PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(radio_link PUBLIC -fsanitize=address,undefined)
    endif()
endif()

if(WIN32)
    target_link_libraries(radio_link PUBLIC ws2_32)
endif()

add_executable(radio_link_monitor app/monitor_main.cpp)
target_link_libraries(radio_link_monitor PRIVATE radio_link)

add_executable(radio_link_sender app/sender_main.cpp)
target_link_libraries(radio_link_sender PRIVATE radio_link)

enable_testing()
add_executable(radio_link_tests tests/radio_link_tests.cpp)
target_link_libraries(radio_link_tests PRIVATE radio_link)
add_test(NAME radio_link_unit_and_integration_tests COMMAND radio_link_tests)

foreach(target radio_link radio_link_monitor radio_link_sender radio_link_tests)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 $<$<BOOL:${RADIO_WARNINGS_AS_ERRORS}>:/WX>)
    else()
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Wpedantic
            $<$<BOOL:${RADIO_WARNINGS_AS_ERRORS}>:-Werror>)
    endif()
endforeach()

install(TARGETS radio_link_monitor radio_link_sender
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

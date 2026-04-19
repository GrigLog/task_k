cmake_minimum_required(VERSION 3.22)
project(malware_scanner VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_TESTS "Build tests" ON)

set(JSON_INCLUDE ${CMAKE_SOURCE_DIR}/third_party)

set(COMMON_SOURCES
    src/common/message.cpp
    src/common/message.h
    src/common/scanner.cpp
    src/common/scanner.h
)

set(SERVER_SOURCES
    src/server/main.cpp
    src/server/stats_store.cpp
    src/server/stats_store.h
    src/server/listen_socket.h
    src/server/worker_child.cpp
    src/server/worker_child.h
)

set(CLIENT_SOURCES
    src/client/client_main.cpp
    src/client/client_connect.cpp
    src/client/client_connect.h
)

set(STATS_SOURCES
    src/stats/stats_main.cpp
    src/stats/stats_request.cpp
    src/stats/stats_request.h
)

add_executable(malware_scanner_client ${CLIENT_SOURCES} ${COMMON_SOURCES})
add_executable(malware_scanner_server ${SERVER_SOURCES} ${COMMON_SOURCES})
add_executable(malware_scanner_stats ${STATS_SOURCES})

find_package(Threads REQUIRED)

target_include_directories(malware_scanner_client PRIVATE ${CMAKE_SOURCE_DIR}/src ${JSON_INCLUDE})
target_include_directories(malware_scanner_server PRIVATE ${CMAKE_SOURCE_DIR}/src ${JSON_INCLUDE})
target_include_directories(malware_scanner_stats PRIVATE ${CMAKE_SOURCE_DIR}/src ${JSON_INCLUDE})

target_link_libraries(malware_scanner_server PRIVATE Threads::Threads)

target_compile_features(malware_scanner_client PUBLIC cxx_std_17)
target_compile_features(malware_scanner_server PUBLIC cxx_std_17)
target_compile_features(malware_scanner_stats PUBLIC cxx_std_17)

if(BUILD_TESTS)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
    )
    FetchContent_MakeAvailable(googletest)
    enable_testing()
    add_executable(malware_scanner_tests
        src/test/test_scanner.cpp
        src/test/test_message.cpp
        src/test/test_stats_store.cpp
        src/test/test_integration.cpp
        ${COMMON_SOURCES}
        src/server/stats_store.cpp
        src/stats/stats_request.cpp
    )
    target_include_directories(malware_scanner_tests PRIVATE ${CMAKE_SOURCE_DIR}/src ${JSON_INCLUDE})
    target_link_libraries(malware_scanner_tests PRIVATE GTest::gtest_main Threads::Threads)
    target_compile_features(malware_scanner_tests PUBLIC cxx_std_17)
    target_compile_definitions(malware_scanner_tests PRIVATE
        "MALWARE_SCANNER_SERVER=\"$<TARGET_FILE:malware_scanner_server>\""
        "MALWARE_SCANNER_CLIENT=\"$<TARGET_FILE:malware_scanner_client>\""
    )
    add_dependencies(malware_scanner_tests malware_scanner_server malware_scanner_client)
    include(GoogleTest)
    gtest_discover_tests(malware_scanner_tests)
endif()

find_program(CLANG_FORMAT "clang-format")
if(CLANG_FORMAT)
    file(GLOB_RECURSE ALL_SOURCE_FILES
        "${CMAKE_SOURCE_DIR}/src/*.cpp"
        "${CMAKE_SOURCE_DIR}/src/*.h"
    )
    add_custom_target(format-check
        COMMAND ${CLANG_FORMAT} --dry-run
            --style=file:${CMAKE_CURRENT_SOURCE_DIR}/.clang-format-yandex
            --Werror ${ALL_SOURCE_FILES}
        COMMENT "Checking code style with clang-format"
        VERBATIM
    )
    add_custom_target(format-fix
        COMMAND ${CLANG_FORMAT} -i
            --style=file:${CMAKE_CURRENT_SOURCE_DIR}/.clang-format-yandex
            ${ALL_SOURCE_FILES}
        VERBATIM
    )
endif()

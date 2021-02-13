cmake_minimum_required(VERSION 3.10)
project(eidos)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -Wall -Wextra")

include(CTest)

# Google Test
include(cmake/DownloadProject/DownloadProject.cmake)
download_project(
        PROJ googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG release-1.10.0
        UPDATE_DISCONNECTED 1
)

add_subdirectory(${googletest_SOURCE_DIR} ${googletest_BINARY_DIR})

# Boost
find_package(Boost REQUIRED COMPONENTS date_time system log program_options)

# eidos
add_executable(eidos src/main.cc src/storage/storage_base.hpp include/eidos/types.hpp include/eidos/result.hpp src/storage/memstore.hpp src/server.cc src/server.hpp src/request.hpp src/context.hpp src/tcp.hpp include/eidos/version.hpp)
target_include_directories(eidos
        PRIVATE
        "${Boost_INCLUDE_DIRS}"
        "${PROJECT_SOURCE_DIR}/include")

target_link_libraries(eidos
        Boost::date_time Boost::system Boost::log Boost::program_options)

# test
add_executable(e-test test/result.cc src/storage/dirstore.hpp)
target_link_libraries(e-test gtest gmock_main)
target_include_directories(e-test PRIVATE "${PROJECT_SOURCE_DIR}/include" "${gtest_SOURCE_DIR}/include" "${gmock_SOURCE_DIR}/include")
add_test(NAME eidos-test COMMAND e-test)

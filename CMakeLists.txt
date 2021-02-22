# Copyright 2021 SiLeader and Cerussite.
#
# Licensed under the Apache License, Version 2.0 (the “License”);
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an “AS IS” BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

cmake_minimum_required(VERSION 3.10)
project(eidos)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)

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

# OpenSSL
find_package(OpenSSL REQUIRED)

# NuRaft (submodule)
set(BOOST_INCLUDE_PATH "${Boost_INCLUDE_DIRS}/boost" ${Boost_INCLUDE_DIRS})
set(BOOST_LIBRARY_PATH ${Boost_LIBRARY_DIRS})
add_subdirectory(third_party/NuRaft)

# eidos
add_executable(eidos
        src/main.cc
        src/storage/storage_base.hpp
        include/eidos/types.hpp
        include/eidos/result.hpp
        src/storage/memstore.hpp
        src/server.cc src/server.hpp
        src/request.hpp
        src/context.hpp
        src/tcp.hpp
        include/eidos/version.hpp
        src/storage/raft.hpp)

target_compile_options(eidos PRIVATE
        -pthread
        -Wall -Wextra -Weffc++
        -Wformat=2 -Wcast-qual -Wcast-align -Wstrict-aliasing=2 -Wbad-function-cast -Wwrite-strings -Wconversion
        -Wfloat-equal -Wpointer-arith -Winit-self -Wredundant-decls -Wdeclaration-after-statement)

if (${ADDSAN})
    target_compile_options(eidos PRIVATE -fsanitize=address,undefined -g)
    target_link_options(eidos PRIVATE -fsanitize=address,undefined -g)
elseif (${MEMSAN})
    target_compile_options(eidos PRIVATE -fsanitize=memory,undefined -g)
    target_link_options(eidos PRIVATE -fsanitize=memory,undefined -g)
endif ()

target_include_directories(eidos
        PRIVATE
        "${Boost_INCLUDE_DIRS}"
        "${PROJECT_SOURCE_DIR}/include"
        "${PROJECT_SOURCE_DIR}/third_party/NuRaft/include")

target_link_libraries(eidos
        Boost::date_time Boost::system  # ASIO
        Boost::log Boost::program_options
        ${OPENSSL_LIBRARIES}
        static_lib)

# test
add_executable(e-test test/result.cc src/storage/raft.hpp)
target_link_libraries(e-test gtest gmock_main)
target_include_directories(e-test PRIVATE "${PROJECT_SOURCE_DIR}/include" "${gtest_SOURCE_DIR}/include" "${gmock_SOURCE_DIR}/include")
add_test(NAME eidos-test COMMAND e-test)

# SPDX-License-Identifier: BSD-3-Clause
cmake_minimum_required(VERSION 3.24)

project (exrkdu)

set(EXECUTABLE_OUTPUT_PATH "${CMAKE_BINARY_DIR}/bin")

add_subdirectory(ext/openexr)
add_executable(exrkdu src/main/cpp/main.cpp)
target_include_directories(exrkdu PRIVATE ext/cxxopts)
target_link_libraries(exrkdu OpenEXRCore)

if(WIN32 AND (BUILD_SHARED_LIBS OR OPENEXR_BUILD_BOTH_STATIC_SHARED))
  target_compile_definitions(exrkdu PRIVATE OPENEXR_DLL)
endif()
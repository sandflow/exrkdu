cmake_minimum_required(VERSION 3.24)

project (exrkdu)

set(EXECUTABLE_OUTPUT_PATH "${CMAKE_BINARY_DIR}/bin")

# find KDU

# Path to the Kakadu SDK include directory, e.g., managed/all_includes
find_path(KDU_INCLUDE_DIR kdu_args.h PATH_SUFFIXES kakadu kdu)
# Path to the Kakadu SDK library, e.g., libkdu_v86R.so
find_library(KDU_LIBRARY NAMES kdu_v86R PATH_SUFFIXES kakadu kdu)
# Path to the Kakadu SDK auxiliary library, e.g., libkdu_a86R.so
find_library(KDU_AUX_LIBRARY NAMES kdu_a86R PATH_SUFFIXES kakadu kdu)

# add OpenEXR

add_subdirectory(ext/openexr)

# build application

add_executable(exrkdu src/main/cpp/main.cpp src/main/cpp/kdu.cpp src/main/cpp/ht_common.cpp)
target_include_directories(exrkdu PRIVATE ${KDU_INCLUDE_DIR})
target_include_directories(exrkdu PRIVATE ext/cxxopts)
target_link_libraries(exrkdu OpenEXRCore ${KDU_LIBRARY} ${KDU_AUX_LIBRARY})

if(WIN32 AND (BUILD_SHARED_LIBS OR OPENEXR_BUILD_BOTH_STATIC_SHARED))
  target_compile_definitions(exrkdu PRIVATE OPENEXR_DLL)
endif()
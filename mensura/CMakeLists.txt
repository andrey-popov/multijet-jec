cmake_minimum_required(VERSION 2.8)
project(multijet)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/lib")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -Wall -Wextra -O2")


include_directories("include" "$ENV{MENSURA_INSTALL}/include")

set(orig_cxx_flags "${CMAKE_CXX_FLAGS}")
find_package(ROOT 6 REQUIRED)
include(${ROOT_USE_FILE})
set(CMAKE_CXX_FLAGS "${orig_cxx_flags}")  # Restore value overwritten by ROOT

find_package(Boost 1.63 COMPONENTS filesystem program_options REQUIRED)
include_directories(${Boost_INCLUDE_DIR})

set(MENSURA_LIBRARIES "$ENV{MENSURA_INSTALL}/lib/libmensura.so"
    "$ENV{MENSURA_INSTALL}/lib/libPECReader.so")

add_subdirectory(src)
add_subdirectory(prog)

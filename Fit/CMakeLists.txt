cmake_minimum_required(VERSION 2.8.12)
project (jecfit)

# Basic configuration
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

include_directories("${PROJECT_SOURCE_DIR}/include")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -Wall -Wextra -O2")


# Sources of the main library
set(LIBRARY_SOURCES "")

foreach(src FitBase.cpp Multijet.cpp Rebin.cpp)
    list(APPEND LIBRARY_SOURCES "${PROJECT_SOURCE_DIR}/src/${src}")
endforeach(src)


# External packages
find_package(ROOT REQUIRED COMPONENTS Minuit2)
include_directories(${ROOT_INCLUDE_DIRS})


# Main library
add_library(jecfit SHARED ${LIBRARY_SOURCES})
target_link_libraries(jecfit ${ROOT_LIBRARIES})

# Main program
add_executable(fit "prog/fit.cpp")
target_link_libraries(fit jecfit ${ROOT_LIBRARIES})

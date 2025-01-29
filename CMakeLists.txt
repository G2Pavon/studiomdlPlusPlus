cmake_minimum_required(VERSION 3.14)
project(StudioMDL++)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS_DEBUG "-g")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wshadow -Wpedantic -Wconversion")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

include_directories(src)
include_directories(src/monsters)

set(SOURCES
    src/utils/cmdlib.cpp
    src/utils/mathlib.cpp
    src/utils/stripification.cpp
    src/format/image/bmpread.cpp
    src/format/qc.cpp
    src/studiomdl.cpp
    src/writemdl.cpp
)

add_executable(${PROJECT_NAME} ${SOURCES})
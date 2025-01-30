cmake_minimum_required(VERSION 3.14)
project(studiomdl++)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_BUILD_TYPE Release)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

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

target_include_directories(${PROJECT_NAME} PRIVATE src src/utils src/format /src/format/image src/monsters)
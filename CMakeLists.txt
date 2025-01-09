cmake_minimum_required(VERSION 3.10)
project(StudioMDL)

# Enable C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Default build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# Enable debugging symbols
set(CMAKE_C_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_DEBUG "-g")

# Add compiler warnings
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wshadow -Wpedantic -Wconversion")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wshadow -Wpedantic -Wconversion")

# Enable sanitizers for Debug builds
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address -fsanitize=undefined")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fsanitize=undefined")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address -fsanitize=undefined")
endif()

# Output Directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Include Directories
include_directories(src)
include_directories(src/monsters)

# Source Files
set(SOURCES
    src/bmpread.cpp
    src/cmdlib.cpp
    src/mathlib.cpp
    src/scriplib.cpp
    src/studiomdl.cpp
    src/tristrip.cpp
    src/write.cpp
)

# Create the Executable Target
add_executable(${PROJECT_NAME} ${SOURCES})

# Define debug mode macro
add_definitions(-DDEBUG_MODE)

# Optionally link to other libs
# target_link_libraries(${PROJECT_NAME} <your_libs>)

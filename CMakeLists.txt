cmake_minimum_required(VERSION 3.14)
project(studiomdlplusplus)

set(CMAKE_CXX_STANDARD 17)
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

set(MDLTOOL_SOURCES
    src/utils/cmdlib.cpp
    src/format/image/bmpread.cpp
    src/mdl/mdl_reader.cpp
    src/mdl/mdl_validator.cpp
    src/mdl/mdl_writer.cpp
    src/mdl/texture_importer.cpp
    src/mdl/skin_family_editor.cpp
    src/tools/mdltool_main.cpp
)

add_executable(mdltool++ ${MDLTOOL_SOURCES})
target_include_directories(mdltool++ PRIVATE src src/utils src/format src/format/image)

enable_testing()

add_executable(mdltool_tests
    src/utils/cmdlib.cpp
    src/format/image/bmpread.cpp
    src/mdl/mdl_reader.cpp
    src/mdl/mdl_validator.cpp
    src/mdl/mdl_writer.cpp
    src/mdl/texture_importer.cpp
    src/mdl/skin_family_editor.cpp
    tests/mdltool_tests.cpp
)
target_include_directories(mdltool_tests PRIVATE src src/utils src/format src/format/image)
add_test(NAME mdltool_tests COMMAND mdltool_tests)

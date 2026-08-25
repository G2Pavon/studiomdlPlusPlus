cmake_minimum_required(VERSION 3.14)
project(studiomdlplusplus LANGUAGES C CXX)

include(FetchContent)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(STUDIOMDLPP_BUILD_CLI "Build mdltool++ CLI" ON)
option(STUDIOMDLPP_BUILD_GUI "Build mdltool_gui" ON)
option(STUDIOMDLPP_BUILD_TESTS "Build tests" ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set(MDL_CORE_SOURCES
    src/utils/cmdlib.cpp
    src/format/image/bmpread.cpp
    src/mdl/mdl_reader.cpp
    src/mdl/mdl_validator.cpp
    src/mdl/mdl_writer.cpp
    src/mdl/texture_importer.cpp
    src/mdl/skin_family_editor.cpp
)

add_library(mdl_core STATIC ${MDL_CORE_SOURCES})
target_include_directories(mdl_core PUBLIC src src/utils src/format src/format/image)

set(STUDIOMDL_SOURCES
    src/utils/cmdlib.cpp
    src/utils/mathlib.cpp
    src/utils/stripification.cpp
    src/format/image/bmpread.cpp
    src/format/qc.cpp
    src/studiomdl.cpp
    src/writemdl.cpp
)

add_executable(studiomdl++ ${STUDIOMDL_SOURCES})
target_include_directories(studiomdl++ PRIVATE src src/utils src/format src/format/image src/monsters)

if(STUDIOMDLPP_BUILD_CLI)
    add_executable(mdltool++ src/tools/mdltool_main.cpp)
    target_link_libraries(mdltool++ PRIVATE mdl_core)
endif()

if(STUDIOMDLPP_BUILD_GUI)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

    set(NFD_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(NFD_INSTALL OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(
        imgui
        URL https://github.com/ocornut/imgui/archive/fd13a1e8923a0a7077b404fc36fd063b25a0c0b5.zip
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_Declare(
        glfw
        URL https://github.com/glfw/glfw/archive/e5ca45dddfb081afd23ec77fa62b2d01c41fe56b.zip
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_Declare(
        nfd
        URL https://github.com/btzy/nativefiledialog-extended/archive/86f742bab39f1b253ad111e8ce776b46dd1ccdbe.zip
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(imgui glfw nfd)

    find_package(OpenGL REQUIRED)

    add_library(imgui_docking STATIC
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
    )
    target_include_directories(imgui_docking PUBLIC
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
    )
    target_link_libraries(imgui_docking PUBLIC glfw OpenGL::GL)

    add_executable(mdltool_gui
        src/gui/main.cpp
        src/gui/application.cpp
        src/gui/editor_state.cpp
        src/gui/gui_theme.cpp
        src/gui/texture_preview.cpp
        src/gui/windows/model_tree_window.cpp
        src/gui/windows/skins_window.cpp
        src/gui/windows/textures_window.cpp
        src/gui/windows/inspector_window.cpp
        src/gui/windows/log_window.cpp
        src/gui/dialogs/add_skin_dialog.cpp
        src/gui/dialogs/replace_texture_dialog.cpp
    )
    target_include_directories(mdltool_gui PRIVATE
        src
        src/gui
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
        ${nfd_SOURCE_DIR}/src/include
    )
    target_link_libraries(mdltool_gui PRIVATE mdl_core imgui_docking glfw nfd OpenGL::GL)
endif()

if(STUDIOMDLPP_BUILD_TESTS)
    enable_testing()
    add_executable(mdltool_tests
        src/utils/cmdlib.cpp
        src/format/image/bmpread.cpp
        src/mdl/mdl_reader.cpp
        src/mdl/mdl_validator.cpp
        src/mdl/mdl_writer.cpp
        src/mdl/texture_importer.cpp
        src/mdl/skin_family_editor.cpp
        src/gui/editor_state.cpp
        tests/mdltool_tests.cpp
    )
    target_include_directories(mdltool_tests PRIVATE src src/utils src/format src/format/image src/gui)
    add_test(NAME mdltool_tests COMMAND mdltool_tests)
endif()

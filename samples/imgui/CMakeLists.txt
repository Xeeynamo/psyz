cmake_minimum_required(VERSION 3.10..3.31)

project(imgui)

set(CMAKE_C_STANDARD 11)

set (IMGUI_SOURCES
    ../../external/cimgui/imgui/backends/imgui_impl_opengl3.cpp
    ../../external/cimgui/imgui/backends/imgui_impl_sdl3.cpp
)
add_compile_definitions(CIMGUI_USE_OPENGL3 CIMGUI_USE_SDL3)
include_directories(../../external/cimgui/imgui/backends)
include_directories(../../external/cimgui/imgui)

add_subdirectory(../../psyz build)
add_subdirectory(../../external/cimgui cimgui EXCLUDE_FROM_ALL)
add_executable(${PROJECT_NAME}
        main.cpp
        hello.c
        ${IMGUI_SOURCES})
target_link_libraries(${PROJECT_NAME} PRIVATE psyz cimgui)

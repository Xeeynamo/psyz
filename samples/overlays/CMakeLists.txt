cmake_minimum_required(VERSION 3.19..3.31)

project(overlays)

set(CMAKE_C_STANDARD 11)

add_subdirectory(../../psyz build)
target_include_directories(psyz PRIVATE ../../psyz/include)
target_compile_definitions(psyz PRIVATE __psyz)

add_executable(host main.c)
target_link_libraries(host PRIVATE psyz)
psyz_exports(host)
psyz_title(host "Sample: overlays")

psyz_add_module(my_ovl HOST host SOURCES my_ovl.c)

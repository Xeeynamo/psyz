cmake_minimum_required(VERSION 3.10..3.31)
project(rgb24 C)
set(CMAKE_C_STANDARD 11)
add_subdirectory(../../psyz build)
add_executable(rgb24 rgb24.c)
target_link_libraries(rgb24 PRIVATE psyz)
psyz_title(rgb24 "Sample: RGB24 XOR")
psyz_web_configure_target(rgb24)
psyz_ios_configure_target(rgb24
    BUNDLE_IDENTIFIER "org.psyz.sample.gpu-rgb24"
    DISPLAY_NAME "PSY-Z RGB24")

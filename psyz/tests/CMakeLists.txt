cmake_minimum_required(VERSION 3.10..3.31)
project(psyz_tests)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/release-1.12.1.zip
)

FetchContent_MakeAvailable(googletest)
enable_testing()
add_executable(psyz_tests
    test_main.cpp
    test_libapi.cpp
    test_libgte.cpp
    test_sdlgl.cpp
)
add_subdirectory(../ build)
target_link_libraries(psyz_tests PRIVATE gtest_main psyz)
include(GoogleTest)
gtest_discover_tests(psyz_tests)

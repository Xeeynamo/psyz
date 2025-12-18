cmake_minimum_required(VERSION 3.10..3.31)
project(psyz_tests)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
# Force GoogleTest to use dynamic runtime library
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

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

if(CMAKE_CROSSCOMPILING)
    add_test(NAME psyz_tests COMMAND psyz_tests)
else()
    gtest_discover_tests(psyz_tests DISCOVERY_MODE PRE_TEST)
endif()

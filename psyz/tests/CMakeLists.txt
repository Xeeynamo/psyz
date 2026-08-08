cmake_minimum_required(VERSION 3.10..3.31)
project(psyz_tests)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(PSYZ_TEST_SRC
    test_main.cpp
    test_libapi.cpp
    test_libgte.cpp
    test_libgpu.cpp
    test_libcd.cpp
    test_libspu.cpp
)

if(PLATFORM_PSP)
    set(PSYZ_TEST_USE_COMPAT ON)
    list(APPEND PSYZ_TEST_SRC target/target_psp.cpp)
endif()

if(PSYZ_TEST_USE_COMPAT)
    add_executable(${PROJECT_NAME} ${PSYZ_TEST_SRC})
    target_include_directories(${PROJECT_NAME} PRIVATE target)
    if(IS_PPSSPP_EMU)
        target_compile_definitions(${PROJECT_NAME} PRIVATE IS_PPSSPP_EMU=1)
    endif()
    add_subdirectory(../ build)
    target_link_libraries(${PROJECT_NAME} PRIVATE psyz)

    psyz_title(${PROJECT_NAME} "PsyZ Tests")

    set_target_properties(${PROJECT_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_NAME psyz_tests_psp)
    return()
endif()

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
add_executable(${PROJECT_NAME} ${PSYZ_TEST_SRC})
add_subdirectory(../ build)
target_link_libraries(${PROJECT_NAME} PRIVATE gtest_main psyz)
psyz_title(${PROJECT_NAME} "PsyZ Tests")
include(GoogleTest)

if(CMAKE_CROSSCOMPILING)
    add_test(NAME psyz_tests COMMAND psyz_tests)
else()
    gtest_discover_tests(${PROJECT_NAME} DISCOVERY_MODE PRE_TEST)
endif()

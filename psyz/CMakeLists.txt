cmake_minimum_required(VERSION 3.10..3.31)

project(psyz)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(
        ${CMAKE_CURRENT_SOURCE_DIR}/../external/SDL
        ${CMAKE_CURRENT_BINARY_DIR}/sdl
        EXCLUDE_FROM_ALL)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)

option(GTE_USE_HW_SQRT "Use hardware-accelerated sqrt (C math library)" OFF)

set(PSYZ_SOURCES
    src/platform/log.c
    src/platform/common.c
    src/psyz/libapi.c
    src/psyz/libetc.c
    src/psyz/libgpu.c
    ../decomp/src/libcard/card.c
    ../decomp/src/libcard/init.c
    ../decomp/src/libgpu/sys.c
    ../decomp/src/libgpu/ext.c
    ../decomp/src/libgpu/font.c
    ../decomp/src/libgpu/otag.c
    ../decomp/src/libgpu/prim.c
    ../decomp/src/libgpu/tmd.c
    ../decomp/src/libgte/sincos.c
    ../decomp/src/libgte/sqrtbl.c
    ../decomp/src/libgte/geo_00.c
    ../decomp/src/libgte/geo_01.c
    ../decomp/src/libgte/ratan.c
    src/psyz/libgte.c
    src/psyz/libgs.c
    src/psyz/libcd.c
    src/psyz/libcard.c
    src/psyz/libspu.c
    src/psyz/libsnd.c
    src/psyz/libsn.c
    src/psyz/libpress.c
)

# Choose sqrt implementation
if(GTE_USE_HW_SQRT)
    list(APPEND PSYZ_SOURCES src/psyz/libgte_hw_sqrt.c)
    message(STATUS "Using hardware-accelerated sqrt (C math library)")
else()
    list(APPEND PSYZ_SOURCES src/psyz/libgte_sw_sqrt.c)
    message(STATUS "Using software sqrt (lookup table)")
endif()
set(PSYZ_LIBS)
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    list(APPEND PSYZ_SOURCES src/platform/plat_win.c)
    list(APPEND PSYZ_SOURCES src/platform/sdl2_gl.c)
    list(APPEND PSYZ_SOURCES src/platform/glad.c)
    list(APPEND PSYZ_LIBS -lopengl32)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    list(APPEND PSYZ_SOURCES src/platform/plat_unix.c)
    list(APPEND PSYZ_SOURCES src/platform/sdl2_gl.c)
    list(APPEND PSYZ_LIBS -framework OpenGL)
else()
    list(APPEND PSYZ_SOURCES src/platform/plat_unix.c)
    list(APPEND PSYZ_SOURCES src/platform/sdl2_gl.c)
    list(APPEND PSYZ_LIBS -lGL)
endif()

add_library(psyz STATIC ${PSYZ_SOURCES})
target_compile_definitions(psyz PUBLIC __psyz)
target_include_directories(psyz PUBLIC include)
target_link_libraries(psyz PUBLIC SDL3::SDL3 ${PSYZ_LIBS})

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
    src/platform/psyz.c
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
    ../decomp/src/libcd/sys.c
    src/psyz/libcard.c
    ../decomp/src/libspu/s_cb.c
    ../decomp/src/libspu/s_crwa.c
    ../decomp/src/libspu/s_dcb.c
    ../decomp/src/libspu/s_gva.c
    ../decomp/src/libspu/s_i.c
    ../decomp/src/libspu/s_ini.c
    ../decomp/src/libspu/s_it.c
    ../decomp/src/libspu/s_itc.c
    ../decomp/src/libspu/s_m_f.c
    ../decomp/src/libspu/s_m_init.c
    ../decomp/src/libspu/s_q.c
    ../decomp/src/libspu/s_r.c
    ../decomp/src/libspu/s_rmp.c
    ../decomp/src/libspu/s_sr.c
    ../decomp/src/libspu/spu.c
    ../decomp/src/libspu/sr_gaks.c
    src/psyz/libspu.c
    ../decomp/src/libsnd/ssclose.c
    ../decomp/src/libsnd/ssinit.c
    ../decomp/src/libsnd/ssinit_c.c
    ../decomp/src/libsnd/ssplay.c
    ../decomp/src/libsnd/ssquit.c
    ../decomp/src/libsnd/sssattr.c
    ../decomp/src/libsnd/sssv.c
    ../decomp/src/libsnd/sstable.c
    ../decomp/src/libsnd/sstick.c
    ../decomp/src/libsnd/ut_rdel.c
    ../decomp/src/libsnd/ut_rdep.c
    ../decomp/src/libsnd/ut_rev.c
    ../decomp/src/libsnd/ut_roff.c
    ../decomp/src/libsnd/ut_ron.c
    ../decomp/src/libsnd/vs_mono.c
    ../decomp/src/libsnd/vs_vh.c
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
set(PSYZ_INC ${CMAKE_CURRENT_SOURCE_DIR}/include)
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    list(APPEND PSYZ_SOURCES src/platform/plat_win.c)
    list(APPEND PSYZ_SOURCES src/platform/sdl3_gl.c)
    list(APPEND PSYZ_SOURCES src/platform/glad.c)
    list(APPEND PSYZ_SOURCES src/platform/sdl3_audio.c)
    if(MSVC)
        list(APPEND PSYZ_LIBS opengl32)
    else()
        list(APPEND PSYZ_LIBS -lopengl32)
    endif()
    list(APPEND PSYZ_INC ${CMAKE_CURRENT_SOURCE_DIR}/../external/SDL/src/video/khronos)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    list(APPEND PSYZ_SOURCES src/platform/plat_unix.c)
    list(APPEND PSYZ_SOURCES src/platform/sdl3_gl.c)
    list(APPEND PSYZ_SOURCES src/platform/glad.c)
    list(APPEND PSYZ_SOURCES src/platform/sdl3_audio.c)
    find_library(OPENGL_LIBRARY OpenGL REQUIRED)
    list(APPEND PSYZ_LIBS ${OPENGL_LIBRARY})
    list(APPEND PSYZ_INC ${CMAKE_CURRENT_SOURCE_DIR}/../external/SDL/src/video/khronos)
else()
    list(APPEND PSYZ_SOURCES src/platform/plat_unix.c)
    list(APPEND PSYZ_SOURCES src/platform/sdl3_gl.c)
    list(APPEND PSYZ_SOURCES src/platform/sdl3_audio.c)
    list(APPEND PSYZ_LIBS -lGL)
endif()

add_library(psyz STATIC ${PSYZ_SOURCES})
target_compile_definitions(psyz PUBLIC __psyz)
target_include_directories(psyz PUBLIC ${PSYZ_INC})
target_link_libraries(psyz PUBLIC SDL3::SDL3 ${PSYZ_LIBS})

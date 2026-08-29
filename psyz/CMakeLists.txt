cmake_minimum_required(VERSION 3.10..3.31)

project(psyz)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/strip_file_prefix.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/title.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/ios.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/web.cmake)

if(NOT PSP)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST OFF CACHE BOOL "" FORCE)
    add_subdirectory(
            ${CMAKE_CURRENT_SOURCE_DIR}/../external/SDL
            ${CMAKE_CURRENT_BINARY_DIR}/sdl
            EXCLUDE_FROM_ALL)
endif()

option(GTE_USE_HW_SQRT "Use hardware-accelerated sqrt (C math library)" OFF)

set(PSYZ_SOURCES
    src/platform/psyz.c
    src/psyz/dma.c
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
    ../decomp/src/libgte/sqrtbl.c
    ../decomp/src/libgte/ratan.c
    src/psyz/libgte.c
    src/psyz/libgs.c
    src/psyz/libcd.c
    ../decomp/src/libcd/iso9660.c
    ../decomp/src/libcd/sys.c
    ../decomp/src/libcd/toc.c
    src/psyz/libcard.c
    ../decomp/src/libspu/s_cb.c
    ../decomp/src/libspu/s_crwa.c
    ../decomp/src/libspu/s_dcb.c
    ../decomp/src/libspu/s_gtm.c
    ../decomp/src/libspu/s_gva.c
    ../decomp/src/libspu/s_gvv.c
    ../decomp/src/libspu/s_i.c
    ../decomp/src/libspu/s_ih.c
    ../decomp/src/libspu/s_ini.c
    ../decomp/src/libspu/s_it.c
    ../decomp/src/libspu/s_itc.c
    ../decomp/src/libspu/s_m.c
    ../decomp/src/libspu/s_m_f.c
    ../decomp/src/libspu/s_m_init.c
    ../decomp/src/libspu/s_m_int.c
    ../decomp/src/libspu/s_m_m.c
    ../decomp/src/libspu/s_m_util.c
    ../decomp/src/libspu/s_m_wsa.c
    ../decomp/src/libspu/s_n2p.c
    ../decomp/src/libspu/s_q.c
    ../decomp/src/libspu/s_r.c
    ../decomp/src/libspu/s_rmp.c
    ../decomp/src/libspu/s_sca.c
    ../decomp/src/libspu/s_sic.c
    ../decomp/src/libspu/s_sk.c
    ../decomp/src/libspu/s_snv.c
    ../decomp/src/libspu/s_sr.c
    ../decomp/src/libspu/s_stm.c
    ../decomp/src/libspu/s_stsa.c
    ../decomp/src/libspu/s_w.c
    ../decomp/src/libspu/s_wp.c
    ../decomp/src/libspu/spu.c
    ../decomp/src/libspu/sr_gaks.c
    src/psyz/libspu.c
    ../decomp/src/libsnd/cres.c
    ../decomp/src/libsnd/decres.c
    ../decomp/src/libsnd/pause.c
    ../decomp/src/libsnd/play.c
    ../decomp/src/libsnd/playmode.c
    ../decomp/src/libsnd/replay.c
    ../decomp/src/libsnd/sscall.c
    ../decomp/src/libsnd/ssclose.c
    ../decomp/src/libsnd/sscres.c
    ../decomp/src/libsnd/ssend.c
    ../decomp/src/libsnd/ssinit.c
    ../decomp/src/libsnd/ssinit_c.c
    ../decomp/src/libsnd/ssinit_h.c
    ../decomp/src/libsnd/ssloop.c
    ../decomp/src/libsnd/ssopenp.c
    ../decomp/src/libsnd/ssplay.c
    ../decomp/src/libsnd/ssquit.c
    ../decomp/src/libsnd/sssattr.c
    ../decomp/src/libsnd/sssmv.c
    ../decomp/src/libsnd/ssstart.c
    ../decomp/src/libsnd/ssstop.c
    ../decomp/src/libsnd/sssv.c
    ../decomp/src/libsnd/ssvol.c
    ../decomp/src/libsnd/sstable.c
    ../decomp/src/libsnd/sstick.c
    ../decomp/src/libsnd/sstickcb.c
    ../decomp/src/libsnd/ut_ako.c
    ../decomp/src/libsnd/ut_autop.c
    ../decomp/src/libsnd/ut_cadsr.c
    ../decomp/src/libsnd/ut_key.c
    ../decomp/src/libsnd/ut_keyv.c
    ../decomp/src/libsnd/ut_rdel.c
    ../decomp/src/libsnd/ut_rdep.c
    ../decomp/src/libsnd/ut_rev.c
    ../decomp/src/libsnd/ut_roff.c
    ../decomp/src/libsnd/ut_ron.c
    ../decomp/src/libsnd/ut_vvol.c
    ../decomp/src/libsnd/vol.c
    ../decomp/src/libsnd/vm_aloc2.c
    ../decomp/src/libsnd/vm_autop.c
    ../decomp/src/libsnd/vm_doff.c
    ../decomp/src/libsnd/vm_f.c
    ../decomp/src/libsnd/vm_g.c
    ../decomp/src/libsnd/vm_init.c
    ../decomp/src/libsnd/vm_n2p.c
    ../decomp/src/libsnd/vm_noff.c
    ../decomp/src/libsnd/vm_nowof.c
    ../decomp/src/libsnd/vm_nowon.c
    ../decomp/src/libsnd/vm_vsu.c
    ../decomp/src/libsnd/vs_mono.c
    ../decomp/src/libsnd/vs_srv.c
    ../decomp/src/libsnd/vs_vab.c
    ../decomp/src/libsnd/vs_vh.c
    ../decomp/src/libsnd/vs_vtb.c
    ../decomp/src/libsnd/vs_vtbp.c
    ../decomp/src/libsnd/vs_vtc.c
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

set(PSYZ_RENDERER_DEFAULT "sdl3_gpu")
set(PSYZ_RENDERER "${PSYZ_RENDERER_DEFAULT}" CACHE STRING "Rendering backend: sdl3_gpu or sdl3_gl")
set_property(CACHE PSYZ_RENDERER PROPERTY STRINGS sdl3_gl sdl3_gpu)
if(NOT PSYZ_RENDERER STREQUAL "sdl3_gpu" AND NOT PSYZ_RENDERER STREQUAL "sdl3_gl")
    message(FATAL_ERROR "invalid PSYZ_RENDERER '${PSYZ_RENDERER}', expected sdl3_gpu or sdl3_gl")
endif()
message(STATUS "Using the ${PSYZ_RENDERER} rendering backend")

set(PSYZ_LIBS)
set(PSYZ_INC ${CMAKE_CURRENT_SOURCE_DIR}/include)
if(EMSCRIPTEN)
    list(APPEND PSYZ_SOURCES src/platform/plat_unix.c)
    list(APPEND PSYZ_SOURCES src/platform/sdl3_log.c)
    list(APPEND PSYZ_SOURCES src/platform/sdl3_audio.c)
    list(APPEND PSYZ_SOURCES src/platform/sdl3_gl.c)
    list(APPEND PSYZ_SOURCES src/psyz/psyz_spu.c)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    list(APPEND PSYZ_SOURCES src/platform/plat_win.c)
    list(APPEND PSYZ_SOURCES src/platform/sdl3_log.c)
    list(APPEND PSYZ_SOURCES src/platform/sdl3_audio.c)
    list(APPEND PSYZ_SOURCES src/psyz/psyz_spu.c)
    list(APPEND PSYZ_SOURCES src/dbgserver/dbgserver.c)
    list(APPEND PSYZ_LIBS ws2_32)
    if(PSYZ_RENDERER STREQUAL "sdl3_gl")
        list(APPEND PSYZ_SOURCES src/platform/sdl3_gl.c)
        list(APPEND PSYZ_SOURCES src/platform/glad.c)
        if(MSVC)
            list(APPEND PSYZ_LIBS sdl3_gl32)
        else()
            list(APPEND PSYZ_LIBS -lsdl3_gl32)
        endif()
    else()
    list(APPEND PSYZ_SOURCES src/platform/sdl3_gpu.c)
        list(APPEND PSYZ_INC ${CMAKE_CURRENT_SOURCE_DIR}/../external/SDL/src/video/khronos)
    endif()
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    list(APPEND PSYZ_SOURCES src/platform/plat_unix.c)
    list(APPEND PSYZ_SOURCES src/platform/sdl3_log.c)
    list(APPEND PSYZ_SOURCES src/platform/sdl3_audio.c)
    list(APPEND PSYZ_SOURCES src/psyz/psyz_spu.c)
    list(APPEND PSYZ_SOURCES src/dbgserver/dbgserver.c)
    if(PSYZ_RENDERER STREQUAL "sdl3_gl")
        message(FATAL_ERROR "sdl3_gl renderer is not supported on macOS; use sdl3_gpu")
    else()
        list(APPEND PSYZ_SOURCES src/platform/sdl3_gpu.c)
    endif()
elseif(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    enable_language(OBJC)
    set(PSYZ_IS_IOS TRUE)
    if(PSYZ_RENDERER STREQUAL "sdl3_gl")
        message(FATAL_ERROR "sdl3_gl renderer is not supported on iOS; use sdl3_gpu")
    endif()
    list(APPEND PSYZ_SOURCES src/platform/plat_unix.c)
    list(APPEND PSYZ_SOURCES src/platform/sdl3_log.c)
    list(APPEND PSYZ_SOURCES src/platform/sdl3_audio.c)
    list(APPEND PSYZ_SOURCES src/platform/sdl3_gpu.c)
    list(APPEND PSYZ_SOURCES src/ios/ios_platform.m)
    list(APPEND PSYZ_SOURCES src/psyz/psyz_spu.c)
    set_source_files_properties(
        src/ios/ios_platform.m PROPERTIES COMPILE_FLAGS "-fobjc-arc")
    find_library(PSYZ_UIKIT_FRAMEWORK UIKit)
    if(NOT PSYZ_UIKIT_FRAMEWORK)
        message(FATAL_ERROR "UIKit framework not found for iOS target")
    endif()
    list(APPEND PSYZ_LIBS ${PSYZ_UIKIT_FRAMEWORK})
elseif(PSP)
    # The pspdev toolchain provides the SDK include/link paths; the GE, the
    # display, the controller and the audio channel are driven directly, so
    # there is no SDL. File I/O talks to the Sony I/O manager natively rather
    # than through newlib (see src/psp/psp_file.c), so plat_unix.c is not used.
    # The software SPU stays in: the PSP has 24MB of user RAM and its native
    # output rate matches the 44100Hz the mixer produces.
    list(APPEND PSYZ_SOURCES src/psp/psp_file.c)
    list(APPEND PSYZ_SOURCES src/psp/psp_log.c)
    list(APPEND PSYZ_SOURCES src/psp/psp_gpu.c)
    list(APPEND PSYZ_SOURCES src/psp/psp_input.c)
    list(APPEND PSYZ_SOURCES src/psp/psp_misc.c)
    list(APPEND PSYZ_SOURCES src/psp/psp_audio.c)
    list(APPEND PSYZ_SOURCES src/psyz/psyz_spu.c)
    # pspdebug first: it depends on pspdisplay and pspge
    list(APPEND PSYZ_LIBS pspdebug pspgu pspdisplay pspge pspctrl pspaudio
         psppower)
    if(PSYZ_PSP_PROFILE)
        # gprof_start/gprof_stop (pspprof.h), used by -pg profiling builds;
        # see psyz/src/psp/README.md's "Profiling" section. -pg is scoped to
        # just this target (the engine/renderer) rather than the whole
        # program: instrumenting sotn-decomp's game logic too multiplies the
        # mcount call-site count (worse under -O3 inlining) and made a
        # 120-frame run take 10+ minutes on real hardware instead of seconds.
        list(APPEND PSYZ_LIBS pspprof)
        set(PSYZ_PSP_PROFILE_CFLAGS -pg)
    endif()
else()
    list(APPEND PSYZ_SOURCES src/platform/plat_unix.c)
    list(APPEND PSYZ_SOURCES src/platform/sdl3_log.c)
    list(APPEND PSYZ_SOURCES src/platform/sdl3_audio.c)
    list(APPEND PSYZ_SOURCES src/psyz/psyz_spu.c)
    list(APPEND PSYZ_SOURCES src/dbgserver/dbgserver.c)
    if(PSYZ_RENDERER STREQUAL "sdl3_gl")
        list(APPEND PSYZ_SOURCES src/platform/sdl3_gl.c)
        list(APPEND PSYZ_LIBS -lGL)
    else()
        list(APPEND PSYZ_SOURCES src/platform/sdl3_gpu.c)
    endif()
endif()

add_library(psyz STATIC ${PSYZ_SOURCES})
target_compile_definitions(psyz PUBLIC __psyz)
if(EMSCRIPTEN)
    target_compile_definitions(psyz PUBLIC PLATFORM_WEB=1)
endif()
if(PSYZ_IS_IOS)
    target_compile_definitions(psyz PUBLIC PLATFORM_IOS=1)
endif()
target_include_directories(psyz PUBLIC ${PSYZ_INC})
if(PSP)
    target_link_libraries(psyz PUBLIC ${PSYZ_LIBS})
else()
    target_link_libraries(psyz PUBLIC SDL3::SDL3 ${PSYZ_LIBS})
endif()
psyz_strip_file_prefix(psyz BASE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/..)

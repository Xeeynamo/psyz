cmake_minimum_required(VERSION 3.16..3.31)

project(psyz_web_samples C CXX)

if(NOT EMSCRIPTEN)
    message(FATAL_ERROR
        "The web gallery must be configured through Emscripten (use emcmake cmake).")
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(PSYZ_RENDERER "sdl3_gl" CACHE STRING "" FORCE)
add_subdirectory(../psyz psyz-build)

set(PSYZ_WEB_DIST_DIR "${CMAKE_BINARY_DIR}/dist" CACHE PATH
    "Output directory for the static web gallery")
file(MAKE_DIRECTORY "${PSYZ_WEB_DIST_DIR}")
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/site/"
     DESTINATION "${PSYZ_WEB_DIST_DIR}")

file(GLOB PSYZ_WEB_SITE_SOURCES CONFIGURE_DEPENDS
     RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}/site"
     "${CMAKE_CURRENT_SOURCE_DIR}/site/*"
     "${CMAKE_CURRENT_SOURCE_DIR}/site/.[!.]*")
set(PSYZ_WEB_SITE_FILES)
foreach(name IN LISTS PSYZ_WEB_SITE_SOURCES)
    add_custom_command(
        OUTPUT "${PSYZ_WEB_DIST_DIR}/${name}"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/site/${name}"
            "${PSYZ_WEB_DIST_DIR}/${name}"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/site/${name}"
        COMMENT "Updating gallery file ${name}"
        VERBATIM)
    list(APPEND PSYZ_WEB_SITE_FILES "${PSYZ_WEB_DIST_DIR}/${name}")
endforeach()
add_custom_target(psyz_web_site ALL DEPENDS ${PSYZ_WEB_SITE_FILES})

function(psyz_add_web_sample id source)
    set(target "psyz_web_${id}")
    add_executable(${target} "${source}")
    target_link_libraries(${target} PRIVATE psyz)
    psyz_web_configure_target(${target}
        OUTPUT_DIRECTORY "${PSYZ_WEB_DIST_DIR}/samples/${id}")
endfunction()

psyz_add_web_sample(gpu-hello ../samples/gpu_hello/hello.c)
psyz_add_web_sample(gpu-shapes ../samples/gpu_shapes/shapes.c)
psyz_add_web_sample(gpu-balls ../samples/gpu_balls/balls.c)
psyz_add_web_sample(gpu-transparency ../samples/gpu_transp/transp.c)
psyz_add_web_sample(gpu-horizontal-grid ../samples/gpu_horizontal_grid/horizontal_grid.c)
psyz_add_web_sample(gs-cube ../samples/gs_cube/cube.c)
psyz_add_web_sample(gte-diffuse ../samples/gte_diffuse/diffuse.c)
psyz_add_web_sample(etc-pads ../samples/etc_pads/pads.c)

option(PSYZ_GENERATE_WEB_PREVIEWS
    "Generate static gallery previews with a headless Chromium browser" ON)
set(PSYZ_CHROME_EXECUTABLE "" CACHE FILEPATH
    "Path to a Chrome or Chromium executable used for preview generation")

if(PSYZ_GENERATE_WEB_PREVIEWS)
    if(DEFINED ENV{EMSDK_NODE} AND EXISTS "$ENV{EMSDK_NODE}")
        set(PSYZ_NODE_EXECUTABLE "$ENV{EMSDK_NODE}" CACHE FILEPATH
            "Node.js executable used for preview generation" FORCE)
    else()
        find_program(PSYZ_NODE_EXECUTABLE NAMES node nodejs)
    endif()
    if(NOT PSYZ_NODE_EXECUTABLE)
        message(FATAL_ERROR "Node.js is required to generate gallery previews.")
    endif()
    if(NOT PSYZ_CHROME_EXECUTABLE)
        if(CMAKE_HOST_APPLE AND EXISTS
            "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome")
            set(PSYZ_CHROME_EXECUTABLE
                "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome"
                CACHE FILEPATH
                "Path to a Chrome or Chromium executable used for preview generation"
                FORCE)
        else()
            find_program(_PSYZ_CHROME_EXECUTABLE
                NAMES google-chrome-stable google-chrome chromium chromium-browser)
            if(_PSYZ_CHROME_EXECUTABLE)
                set(PSYZ_CHROME_EXECUTABLE "${_PSYZ_CHROME_EXECUTABLE}"
                    CACHE FILEPATH
                    "Path to a Chrome or Chromium executable used for preview generation"
                    FORCE)
            endif()
        endif()
    endif()
    if(NOT PSYZ_CHROME_EXECUTABLE)
        message(FATAL_ERROR
            "Chrome/Chromium is required to generate gallery previews. "
            "Set PSYZ_CHROME_EXECUTABLE or configure with "
            "-DPSYZ_GENERATE_WEB_PREVIEWS=OFF.")
    endif()

    set(PSYZ_WEB_SAMPLE_IDS
        gpu-hello
        gpu-shapes
        gpu-balls
        gpu-transparency
        gpu-horizontal-grid
        gs-cube
        gte-diffuse
        etc-pads)
    set(PSYZ_WEB_SAMPLE_TARGETS
        psyz_web_gpu-hello
        psyz_web_gpu-shapes
        psyz_web_gpu-balls
        psyz_web_gpu-transparency
        psyz_web_gpu-horizontal-grid
        psyz_web_gs-cube
        psyz_web_gte-diffuse
        psyz_web_etc-pads)
    set(PSYZ_WEB_PREVIEW_FILES)
    foreach(id IN LISTS PSYZ_WEB_SAMPLE_IDS)
        list(APPEND PSYZ_WEB_PREVIEW_FILES
            "${PSYZ_WEB_DIST_DIR}/previews/${id}.png")
    endforeach()

    add_custom_command(
        OUTPUT ${PSYZ_WEB_PREVIEW_FILES}
        COMMAND "${PSYZ_NODE_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/generate_previews.mjs"
            --dist "${PSYZ_WEB_DIST_DIR}"
            --chrome "${PSYZ_CHROME_EXECUTABLE}"
        DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/generate_previews.mjs"
            "${CMAKE_CURRENT_SOURCE_DIR}/site/samples.js"
            ${PSYZ_WEB_SITE_FILES}
            ${PSYZ_WEB_SAMPLE_TARGETS}
        COMMENT "Capturing static WebAssembly gallery previews"
        VERBATIM
        USES_TERMINAL)
    add_custom_target(psyz_web_previews ALL
        DEPENDS ${PSYZ_WEB_PREVIEW_FILES})
endif()

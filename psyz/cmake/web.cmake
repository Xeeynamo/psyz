set_property(GLOBAL PROPERTY PSYZ_WEB_CMAKE_DIR "${CMAKE_CURRENT_LIST_DIR}")

function(psyz_web_configure_target target)
    if(NOT EMSCRIPTEN)
        return()
    endif()

    if(NOT TARGET ${target})
        message(FATAL_ERROR "psyz_web_configure_target: unknown target '${target}'")
    endif()

    cmake_parse_arguments(PSYZ_WEB "" "OUTPUT_DIRECTORY" "" ${ARGN})
    if(NOT PSYZ_WEB_OUTPUT_DIRECTORY)
        set(PSYZ_WEB_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
    endif()

    get_property(psyz_web_cmake_dir GLOBAL PROPERTY PSYZ_WEB_CMAKE_DIR)
    set(psyz_web_shell "${psyz_web_cmake_dir}/web/shell.html")

    target_sources(${target} PRIVATE
        "${psyz_web_cmake_dir}/../src/web/web_timing.c")

    set_target_properties(${target} PROPERTIES
        OUTPUT_NAME "index"
        SUFFIX ".html"
        RUNTIME_OUTPUT_DIRECTORY "${PSYZ_WEB_OUTPUT_DIRECTORY}")

    target_link_options(${target} PRIVATE
        "-sASYNCIFY"
        "-sASYNCIFY_STACK_SIZE=65536"
        "-sALLOW_MEMORY_GROWTH=1"
        "-sSTACK_SIZE=1048576"
        "-sMIN_WEBGL_VERSION=2"
        "-sMAX_WEBGL_VERSION=2"
        "-sENVIRONMENT=web"
        "--shell-file=${psyz_web_shell}")
endfunction()

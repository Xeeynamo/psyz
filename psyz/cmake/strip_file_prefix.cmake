# psyz_strip_file_prefix(<target> [BASE_DIR <dir>])
#
# Makes __FILE__ relative for the given target.
# Useful to not include the full source path in each printed log.
function(psyz_strip_file_prefix target)
    cmake_parse_arguments(ARG "" "BASE_DIR" "" ${ARGN})
    if(NOT ARG_BASE_DIR)
        set(ARG_BASE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
    endif()
    get_filename_component(ARG_BASE_DIR "${ARG_BASE_DIR}" ABSOLUTE)
    if(MSVC)
        target_compile_options(${target} PRIVATE "/d1trimfile:${ARG_BASE_DIR}\\")
    elseif(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE "-fmacro-prefix-map=${ARG_BASE_DIR}/=")
    endif()
endfunction()

# psyz_exports(<target>)
# psyz_add_module(<target> SOURCES <src...> [HOST <target>])

set(PSYZ_MODULE_GLUE_DESKTOP
    "${CMAKE_CURRENT_LIST_DIR}/../src/platform/module_glue.c"
    CACHE INTERNAL "PsyZ module glue source")
set(PSYZ_MODULE_INCLUDE_DIR "${CMAKE_CURRENT_LIST_DIR}/../include"
    CACHE INTERNAL "PsyZ public include directory")

# psyz_exports(<target>)
#
# Declares that <target> is a host for all modules using that will be loaded
# from it. Modules must use psyz_add_module(HOST <target>)
function(psyz_exports target)
    if(NOT TARGET ${target})
        message(FATAL_ERROR "psyz_exports: '${target}' is not a target")
    endif()

    if(PSP)
        psyz_psp_host_exports(${target})
    else()
        set_target_properties(${target} PROPERTIES
            ENABLE_EXPORTS ON
            WINDOWS_EXPORT_ALL_SYMBOLS ON)
    endif()
endfunction()

# psyz_add_module(<target> SOURCES <src...> [HOST <target>])
#
# Builds target as a dynamic module (dll, dylib, prx), and link symbols to the
# specified HOST target. The host must use psyz_exports(<host_target>).
function(psyz_add_module target)
    set(oneValueArgs HOST)
    set(multiValueArgs SOURCES)
    cmake_parse_arguments(ARG "" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    if(NOT ARG_SOURCES)
        message(FATAL_ERROR "psyz_add_module: SOURCES is required")
    endif()
    if(ARG_HOST AND NOT TARGET ${ARG_HOST})
        message(FATAL_ERROR "psyz_add_module: HOST '${ARG_HOST}' is not a target")
    endif()

    if(PSP)
        if(ARG_HOST)
            set_property(TARGET ${ARG_HOST} APPEND PROPERTY
                PSYZ_PSP_MODULES ${target})
            psyz_psp_module(${target} SOURCES ${ARG_SOURCES}
                LIBRARY ${ARG_HOST} HOST ${ARG_HOST})
        else()
            psyz_psp_module(${target} SOURCES ${ARG_SOURCES})
        endif()
        target_compile_definitions(${target}_objs PRIVATE
            PSYZ_MODULE_NAME="${target}")
    else()
        add_library(${target} MODULE ${ARG_SOURCES} ${PSYZ_MODULE_GLUE_DESKTOP})
        target_include_directories(${target} PRIVATE ${PSYZ_MODULE_INCLUDE_DIR})
        target_compile_definitions(${target} PRIVATE PSYZ_MODULE_NAME="${target}")
        set_target_properties(${target} PROPERTIES PREFIX "")

        if(NOT ARG_HOST)
            return()
        endif()

        set_target_properties(${target} PROPERTIES
            LIBRARY_OUTPUT_DIRECTORY $<TARGET_FILE_DIR:${ARG_HOST}>
            RUNTIME_OUTPUT_DIRECTORY $<TARGET_FILE_DIR:${ARG_HOST}>)

        if(WIN32)
            target_link_libraries(${target} PRIVATE ${ARG_HOST})
        else()
            add_dependencies(${ARG_HOST} ${target})
        endif()
    endif()
endfunction()

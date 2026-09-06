set(PSYZ_WIN_COFF_TOOL "${CMAKE_CURRENT_LIST_DIR}/../tools/win/psyz_win_coff.py"
    CACHE INTERNAL "PsyZ Windows COFF helper")

# _psyz_win_export_objects(<out var> <target>)
#
# Collects the object files whose symbols the host should export.
function(_psyz_win_export_objects out target)
    set(_objs $<TARGET_OBJECTS:${target}>)
    get_target_property(_libs ${target} LINK_LIBRARIES)
    set(_seen "")
    while(_libs)
        list(POP_FRONT _libs _lib)
        if(_lib MATCHES "^\\$<(LINK_ONLY|BUILD_INTERFACE):([^<>]+)>$")
            set(_lib "${CMAKE_MATCH_2}")
        endif()
        if(_lib IN_LIST _seen OR NOT TARGET ${_lib})
            continue()
        endif()
        list(APPEND _seen ${_lib})
        get_target_property(_export ${_lib} PSYZ_EXPORT_TO_MODULES)
        get_target_property(_type ${_lib} TYPE)
        if(_export AND _type MATCHES "^(STATIC_LIBRARY|OBJECT_LIBRARY)$")
            list(APPEND _objs $<TARGET_OBJECTS:${_lib}>)
        endif()
        get_target_property(_transitive ${_lib} INTERFACE_LINK_LIBRARIES)
        if(_transitive)
            list(APPEND _libs ${_transitive})
        endif()
    endwhile()
    set(${out} "${_objs}" PARENT_SCOPE)
endfunction()

# psyz_win_export_closure(<host target>)
#
# Gives <host> an export table, so modules can reach <host> symbols
function(psyz_win_export_closure target)
    _psyz_win_export_objects(_objs ${target})
    set(_dir "${CMAKE_CURRENT_BINARY_DIR}/psyz_exports/${target}")
    set(_list "${_dir}/$<CONFIG>/objects.txt")
    set(_def "${_dir}/$<CONFIG>/${target}.def")

    file(GENERATE OUTPUT ${_list} CONTENT "$<JOIN:${_objs},\n>\n")

    set(_nm_arg "")
    if(NOT MSVC AND CMAKE_NM)
        set(_nm_arg --nm=${CMAKE_NM})
    endif()
    add_custom_command(TARGET ${target} PRE_LINK
        COMMAND ${CMAKE_COMMAND} -E __create_def ${_def} ${_list} ${_nm_arg}
        COMMENT "Collecting ${target} exports for psyz modules"
        VERBATIM)

    set_target_properties(${target} PROPERTIES
        ENABLE_EXPORTS ON
        WINDOWS_EXPORT_ALL_SYMBOLS OFF)
    if(MSVC)
        target_link_options(${target} PRIVATE "/DEF:${_def}")
    else()
        target_link_options(${target} PRIVATE "${_def}")
    endif()
    set_property(TARGET ${target} APPEND PROPERTY LINK_DEPENDS ${_def})
endfunction()

# psyz_win_module_autoimport(<target> <host> <source...>)
#
# Makes <target> able to read and write <host> globals.
function(psyz_win_module_autoimport target host)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)

    get_target_property(_exports ${host} ENABLE_EXPORTS)
    if(NOT _exports)
        message(FATAL_ERROR "psyz_add_module: host '${host}' does not export "
            "anything; call psyz_exports(${host}) first")
    endif()

    set(_dir "${CMAKE_CURRENT_BINARY_DIR}/psyz_autoimport/${target}/$<CONFIG>")
    set(_header "${_dir}/${target}_autoimport.h")
    set(_objs "${_dir}/scan_objects.txt")

    add_library(${target}_psyz_scan OBJECT EXCLUDE_FROM_ALL ${ARGN})
    target_include_directories(${target}_psyz_scan PRIVATE
        $<TARGET_PROPERTY:${target},INCLUDE_DIRECTORIES>)
    target_compile_definitions(${target}_psyz_scan PRIVATE
        $<TARGET_PROPERTY:${target},COMPILE_DEFINITIONS>)
    target_compile_options(${target}_psyz_scan PRIVATE
        $<FILTER:$<TARGET_PROPERTY:${target},COMPILE_OPTIONS>,EXCLUDE,psyz_autoimport>)
    foreach(_prop C_STANDARD C_STANDARD_REQUIRED C_EXTENSIONS
        CXX_STANDARD CXX_STANDARD_REQUIRED CXX_EXTENSIONS
        MSVC_RUNTIME_LIBRARY MSVC_DEBUG_INFORMATION_FORMAT)
        get_target_property(_value ${target} ${_prop})
        if(NOT _value STREQUAL "_value-NOTFOUND")
            set_property(TARGET ${target}_psyz_scan PROPERTY ${_prop} "${_value}")
        endif()
    endforeach()

    file(GENERATE OUTPUT ${_objs}
        CONTENT "$<JOIN:$<TARGET_OBJECTS:${target}_psyz_scan>,\n>\n")

    add_custom_command(
        OUTPUT ${_header}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${_dir}
        COMMAND ${Python3_EXECUTABLE} ${PSYZ_WIN_COFF_TOOL} gen
        --module-name ${target}
        --module-objs-file ${_objs}
        --import-lib $<TARGET_LINKER_FILE:${host}>
        --output ${_header}
        DEPENDS $<TARGET_OBJECTS:${target}_psyz_scan>
        $<TARGET_LINKER_FILE:${host}> ${_objs} ${PSYZ_WIN_COFF_TOOL}
        COMMENT "Resolving ${target} host imports from ${host}"
        VERBATIM COMMAND_EXPAND_LISTS)

    add_custom_target(${target}_psyz_autoimport DEPENDS ${_header})
    add_dependencies(${target}_psyz_autoimport ${target}_psyz_scan ${host})
    add_dependencies(${target} ${target}_psyz_autoimport)
    target_compile_options(${target} PRIVATE "/FI${_header}")
endfunction()

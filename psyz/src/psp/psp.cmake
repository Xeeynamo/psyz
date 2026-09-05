# Toolchain shim for the Sony PSP target, built on top of the pspdev SDK.
# Usage: cmake -DCMAKE_TOOLCHAIN_FILE=src/psp/psp.cmake ...

if(NOT DEFINED ENV{PSPDEV})
    if(EXISTS "$ENV{HOME}/pspdev")
        set(ENV{PSPDEV} "$ENV{HOME}/pspdev")
    else()
        set(ENV{PSPDEV} "/usr/local/pspdev")
    endif()
endif()

include("$ENV{PSPDEV}/psp/share/pspdev.cmake")

string(APPEND CMAKE_C_FLAGS_INIT " -G0")
string(APPEND CMAKE_CXX_FLAGS_INIT " -G0")

find_package(Python3 COMPONENTS Interpreter REQUIRED)

# Each PSP title must define itself as a module and attach system callbacks.
set(PSYZ_PSP_STARTUP "${CMAKE_CURRENT_LIST_DIR}/psp_startup.c")

# Required for PSP PRX modules to be used as PsyZ modules.
set(PSYZ_PSP_MODULE_GLUE "${CMAKE_CURRENT_LIST_DIR}/../psp/psp_module_glue.c")

# Avoid colliding prxexports.o files
function(psyz_psp_drop_prxexports target)
    get_target_property(_drop ${target} PSYZ_PSP_DROP_PRXEXPORTS)
    if(NOT _drop)
        return()
    endif()
    foreach(_prop LINK_OPTIONS INTERFACE_LINK_OPTIONS)
        get_target_property(_opts ${target} ${_prop})
        if(_opts)
            list(FILTER _opts EXCLUDE REGEX "prxexports\\.o$")
            set_target_properties(${target} PROPERTIES ${_prop} "${_opts}")
        endif()
    endforeach()
endfunction()

function(psyz_psp_title target title)
    target_sources(${target} PRIVATE ${PSYZ_PSP_STARTUP})
    create_pbp_file(TARGET ${target} TITLE ${title})
    psyz_psp_drop_prxexports(${target})
endfunction()

# ---------------------------------------------------------------------------
# Loadable-module (PRX) support

set(PSYZ_PSP_TOOLS_DIR "${CMAKE_CURRENT_LIST_DIR}/../../tools/psp")
set(PSYZ_PSP_INCLUDE_DIR "${CMAKE_CURRENT_LIST_DIR}/../../include")
set(PSYZ_PSP_MODULE_ABI "${PSYZ_PSP_TOOLS_DIR}/psp_module_abi.py")
set(PSYZ_PSP_LINK_MODULE "${PSYZ_PSP_TOOLS_DIR}/psp_link_module.py")

# psyz_psp_emit_abi(<host target>)
#
# Emits the rule that derives <host>'s ABI from the objects of every module
# bound to it. Runs after every in the psyz_add_module() registered directory.
function(psyz_psp_emit_abi target)
    get_target_property(_abi_dir ${target} PSYZ_PSP_ABI_DIR)
    get_target_property(_modules ${target} PSYZ_PSP_MODULES)
    if(NOT _modules)
        return()
    endif()

    set(_module_objs "")
    set(_imports "")
    set(_rsp_content "")
    foreach(_mod ${_modules})
        list(APPEND _module_objs $<TARGET_OBJECTS:${_mod}_objs>)
        list(APPEND _imports ${_abi_dir}/${_mod}_imports.S)
        list(APPEND _imports ${_abi_dir}/${_mod}_nids.json)
        string(APPEND _rsp_content
            "$<JOIN:$<TARGET_OBJECTS:${_mod}_objs>,\n>\n\n")
    endforeach()

    set(_stamp ${_abi_dir}/${target}.abi.stamp)
    set(_rsp ${_abi_dir}/${target}.objs.rsp)
    file(GENERATE OUTPUT ${_rsp} CONTENT "${_rsp_content}")
    get_target_property(_scan_srcs ${target} SOURCES)
    add_library(${target}_scan OBJECT ${_scan_srcs})
    target_include_directories(${target}_scan PRIVATE
        $<TARGET_PROPERTY:${target},INCLUDE_DIRECTORIES>)
    target_compile_definitions(${target}_scan PRIVATE
        $<TARGET_PROPERTY:${target},COMPILE_DEFINITIONS>)
    target_compile_options(${target}_scan PRIVATE
        $<TARGET_PROPERTY:${target},COMPILE_OPTIONS> -G0)
    target_compile_features(${target}_scan PRIVATE
        $<TARGET_PROPERTY:${target},COMPILE_FEATURES>)
    foreach(_prop C_STANDARD C_STANDARD_REQUIRED C_EXTENSIONS
        CXX_STANDARD CXX_STANDARD_REQUIRED CXX_EXTENSIONS POSITION_INDEPENDENT_CODE)
        get_target_property(_value ${target} ${_prop})
        if(NOT _value STREQUAL "_value-NOTFOUND")
            set_property(TARGET ${target}_scan PROPERTY ${_prop} "${_value}")
        endif()
    endforeach()

    set(_host_objs $<TARGET_OBJECTS:${target}_scan>)
    get_target_property(_host_libs ${target} LINK_LIBRARIES)
    if(_host_libs)
        target_link_libraries(${target}_scan PRIVATE ${_host_libs})
        set(_seen "")
        while(_host_libs)
            list(POP_FRONT _host_libs _lib)
            if(_lib MATCHES "^\\$<(LINK_ONLY|BUILD_INTERFACE):([^<>]+)>$")
                set(_lib "${CMAKE_MATCH_2}")
            endif()
            if(_lib IN_LIST _seen)
                continue()
            endif()
            list(APPEND _seen ${_lib})
            if(TARGET ${_lib})
                get_target_property(_type ${_lib} TYPE)
                if(_type STREQUAL "STATIC_LIBRARY")
                    list(APPEND _host_objs $<TARGET_FILE:${_lib}>)
                elseif(_type STREQUAL "OBJECT_LIBRARY")
                    list(APPEND _host_objs $<TARGET_OBJECTS:${_lib}>)
                endif()
                get_target_property(_transitive ${_lib} INTERFACE_LINK_LIBRARIES)
                if(_transitive)
                    list(APPEND _host_libs ${_transitive})
                endif()
            elseif(EXISTS "${_lib}")
                list(APPEND _host_objs ${_lib})
            endif()
        endwhile()
    endif()

    foreach(_sysdir $ENV{PSPDEV}/psp/lib $ENV{PSPDEV}/psp/sdk/lib)
        foreach(_syslib libc.a libg.a libm.a)
            if(EXISTS "${_sysdir}/${_syslib}")
                list(APPEND _host_objs ${_sysdir}/${_syslib})
            endif()
        endforeach()
    endforeach()
    execute_process(COMMAND ${CMAKE_C_COMPILER} -print-libgcc-file-name
        OUTPUT_VARIABLE _libgcc
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
    if(_libgcc AND EXISTS "${_libgcc}")
        list(APPEND _host_objs ${_libgcc})
    endif()
    foreach(_extra ${PSYZ_PSP_EXTRA_HOST_LIBS})
        list(APPEND _host_objs ${_extra})
    endforeach()

    # Support for BUILD_PRX=ON
    set(_syslib_arg "")
    if(BUILD_PRX)
        set(_syslib_arg --syslib)
    endif()

    set(_exports_c ${_abi_dir}/${target}_exports.c)
    set(_resolve_c ${_abi_dir}/${target}_resolve.c)
    set_source_files_properties(${_exports_c} ${_resolve_c} PROPERTIES GENERATED TRUE)
    target_sources(${target} PRIVATE ${_exports_c} ${_resolve_c})
    set_property(TARGET ${target} PROPERTY PSYZ_PSP_DROP_PRXEXPORTS 1)
    psyz_psp_drop_prxexports(${target})

    add_custom_command(
        OUTPUT ${_stamp}
        ${_abi_dir}/${target}.exp
        ${_imports}
        ${_exports_c}
        ${_abi_dir}/${target}_resolve.c
        COMMAND ${CMAKE_COMMAND} -E make_directory ${_abi_dir}
        COMMAND ${Python3_EXECUTABLE} ${PSYZ_PSP_MODULE_ABI} derive-exports
        --module-objs-file ${_rsp}
        --modules ${_modules}
        --host-objs ${_host_objs}
        --nm $ENV{PSPDEV}/bin/psp-nm
        --library ${target} --out-dir ${_abi_dir} ${_syslib_arg}
        COMMAND ${CMAKE_COMMAND} -E touch ${_stamp}
        DEPENDS ${_module_objs} ${_host_objs} ${_rsp} ${PSYZ_PSP_MODULE_ABI}
        COMMENT "Deriving ${target} module ABI from module objects"
        VERBATIM
        COMMAND_EXPAND_LISTS
    )

    add_custom_target(${target}_abi DEPENDS ${_stamp} ${_imports}
        ${_exports_c} ${_resolve_c} ${_abi_dir}/${target}.exp)
    add_dependencies(${target} ${target}_abi)
    add_dependencies(${target}_abi ${target}_scan)
    foreach(_mod ${_modules})
        add_dependencies(${target}_abi ${_mod}_objs)
    endforeach()
endfunction()

# psyz_psp_host_exports(<host target>)
#
# Marks <host target> as a module host.
function(psyz_psp_host_exports target)
    set(_abi_dir ${CMAKE_CURRENT_BINARY_DIR}/psyz_abi/${target})
    set_property(TARGET ${target} PROPERTY PSYZ_PSP_ABI_DIR ${_abi_dir})
    set_property(TARGET ${target} PROPERTY PSYZ_PSP_IS_HOST 1)
    cmake_language(EVAL CODE
        "cmake_language(DEFER CALL psyz_psp_emit_abi [[${target}]])")
endfunction()

# psyz_psp_module(<target> SOURCES <src...> [LIBRARY <name>] [HOST <target>])
# Builds a PRX with optional host imports and places it beside HOST.
function(psyz_psp_module target)
    set(oneValueArgs LIBRARY HOST)
    set(multiValueArgs SOURCES)
    cmake_parse_arguments(ARG "" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    if(NOT ARG_SOURCES)
        message(FATAL_ERROR "psyz_psp_module: SOURCES is required")
    endif()

    # Compile scan inputs separately from generated imports to avoid a dependency cycle.
    set(_sources ${ARG_SOURCES} ${PSYZ_PSP_MODULE_GLUE})

    add_library(${target}_objs OBJECT ${_sources})
    target_include_directories(${target}_objs PRIVATE
        $ENV{PSPDEV}/psp/sdk/include ${PSYZ_PSP_TOOLS_DIR} ${PSYZ_PSP_INCLUDE_DIR})
    # Avoid unavailable newlib imports and $gp-relative addressing in loaded PRXs.
    target_compile_options(${target}_objs PRIVATE -nostdlib -G0)
    if(ARG_LIBRARY)
        target_compile_definitions(${target}_objs PRIVATE
            PSYZ_MODULE_HAS_VAR_IMPORTS=1)
    endif()

    # Use the C driver because this toolchain lacks CMake ASM platform support.
    get_target_property(_objs_sources ${target}_objs SOURCES)
    foreach(_src ${_objs_sources})
        if(_src MATCHES "\\.[sS]$")
            set_source_files_properties(${_src} TARGET_DIRECTORY ${target}_objs
                PROPERTIES LANGUAGE C)
        endif()
    endforeach()

    set(_abi_objects "")
    if(ARG_LIBRARY)
        get_target_property(_abi_dir ${ARG_LIBRARY} PSYZ_PSP_ABI_DIR)
        if(NOT _abi_dir)
            message(FATAL_ERROR "psyz_psp_module: host '${ARG_LIBRARY}' is not a module host; call psyz_exports(${ARG_LIBRARY}) first")
        endif()
        set(_imports_s ${_abi_dir}/${target}_imports.S)
        # Make needs a file dependency on imports generated in the host's directory.
        set_source_files_properties(${_imports_s} PROPERTIES
            GENERATED TRUE LANGUAGE C
            OBJECT_DEPENDS ${_abi_dir}/${ARG_LIBRARY}.abi.stamp)
        add_library(${target}_abi_objs OBJECT
            ${_imports_s} ${PSYZ_PSP_TOOLS_DIR}/psyz_var_import_apply.c)
        target_include_directories(${target}_abi_objs PRIVATE
            $ENV{PSPDEV}/psp/sdk/include ${PSYZ_PSP_TOOLS_DIR}
            ${PSYZ_PSP_INCLUDE_DIR})
        target_compile_options(${target}_abi_objs PRIVATE -nostdlib -G0)
        add_dependencies(${target}_abi_objs ${ARG_LIBRARY}_abi)
        set(_abi_objects $<TARGET_OBJECTS:${target}_abi_objs>)
    endif()

    set(_outdir ${CMAKE_CURRENT_BINARY_DIR})
    if(ARG_HOST)
        get_target_property(_host_dir ${ARG_HOST} RUNTIME_OUTPUT_DIRECTORY)
        if(_host_dir)
            set(_outdir ${_host_dir})
        else()
            get_target_property(_outdir ${ARG_HOST} BINARY_DIR)
        endif()
    endif()

    set(_elf ${_outdir}/${target}.elf)
    set(_prx ${_outdir}/${target}.prx)

    if(ARG_LIBRARY)
        set(_nids_arg --nids ${_abi_dir}/${target}_nids.json --library ${ARG_LIBRARY})
        set(_abi_deps ${_abi_dir}/${target}_nids.json)
        set(_fixup_outputs ${_elf}.varfixup.S ${_elf}_varfixup.o)
    else()
        set(_nids_arg "")
    endif()

    add_custom_command(
        OUTPUT ${_elf} ${_prx}
        BYPRODUCTS ${_elf}.syslib.exp ${_elf}.syslib.c ${_elf}_syslib.o ${_fixup_outputs}
        COMMAND ${Python3_EXECUTABLE} ${PSYZ_PSP_LINK_MODULE}
        --cc ${CMAKE_C_COMPILER}
        --pspdev $ENV{PSPDEV}
        --out-elf ${_elf}
        --out-prx ${_prx}
        ${_nids_arg}
        -- $<TARGET_OBJECTS:${target}_objs> ${_abi_objects}
        DEPENDS ${target}_objs $<TARGET_OBJECTS:${target}_objs>
        ${_abi_objects} ${_abi_deps} ${PSYZ_PSP_LINK_MODULE} ${PSYZ_PSP_MODULE_ABI}
        $ENV{PSPDEV}/psp/sdk/lib/linkfile.prx $ENV{PSPDEV}/psp/sdk/lib/prxspecs
        $ENV{PSPDEV}/psp/sdk/lib/libpspmodinfo.a $ENV{PSPDEV}/psp/sdk/lib/libpspuser.a
        COMMENT "Linking PSP module ${target} (PRX)"
        VERBATIM
        COMMAND_EXPAND_LISTS
    )
    add_custom_target(${target} ALL DEPENDS ${_elf} ${_prx})
    # Object-file dependencies also need explicit target ordering.
    add_dependencies(${target} ${target}_objs)
    if(TARGET ${target}_abi_objs)
        add_dependencies(${target} ${target}_abi_objs)
    endif()
    if(ARG_HOST)
        add_dependencies(${ARG_HOST} ${target})
    endif()
endfunction()

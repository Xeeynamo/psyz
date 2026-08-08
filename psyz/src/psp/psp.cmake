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

# Each PSP title must define itself as a module and attach system callbacks.
set(PSYZ_PSP_STARTUP "${CMAKE_CURRENT_LIST_DIR}/psp_startup.c")

function(psyz_psp_title target title)
    target_sources(${target} PRIVATE ${PSYZ_PSP_STARTUP})
    create_pbp_file(TARGET ${target} TITLE ${title})
endfunction()

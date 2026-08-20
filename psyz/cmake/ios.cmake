# psyz_ios_configure_target(<target> [BUNDLE_IDENTIFIER <id>]
#                           [DISPLAY_NAME <name>])
set_property(GLOBAL PROPERTY PSYZ_IOS_CMAKE_DIR "${CMAKE_CURRENT_LIST_DIR}")

function(psyz_ios_configure_target target)
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
        return()
    endif()

    if(NOT TARGET ${target})
        message(FATAL_ERROR "psyz_ios_configure_target: unknown target '${target}'")
    endif()

    cmake_parse_arguments(PSYZ_IOS "" "BUNDLE_IDENTIFIER;DISPLAY_NAME" "" ${ARGN})

    if(NOT PSYZ_IOS_BUNDLE_IDENTIFIER)
        string(TOLOWER "${target}" bundle_suffix)
        string(REPLACE "_" "-" bundle_suffix "${bundle_suffix}")
        set(PSYZ_IOS_BUNDLE_IDENTIFIER "org.psyz.${bundle_suffix}")
    endif()
    if(NOT PSYZ_IOS_DISPLAY_NAME)
        set(PSYZ_IOS_DISPLAY_NAME "${target}")
    endif()

    get_property(psyz_ios_cmake_dir GLOBAL PROPERTY PSYZ_IOS_CMAKE_DIR)
    set(info_plist "${CMAKE_CURRENT_BINARY_DIR}/${target}-Info.plist")
    configure_file(
        "${psyz_ios_cmake_dir}/ios/Info.plist.in"
        "${info_plist}"
        @ONLY)

    set_target_properties(${target} PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_INFO_PLIST "${info_plist}"
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "${PSYZ_IOS_BUNDLE_IDENTIFIER}"
        XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2")
endfunction()

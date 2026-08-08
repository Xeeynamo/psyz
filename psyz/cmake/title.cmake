# psyz_title(<target> <title>)
#
# Sets the user-visible name of a game, for every supported configuration:
#   - SDL3: the initial window title, instead of "PSY-Z" as placeholder.

function(psyz_title target title)
    set(max_len 255)
    string(LENGTH "${title}" len)
    if(len GREATER 255)
        message(FATAL_ERROR "psyz_title: cannot be more than 255 characters")
    endif()
    if(title MATCHES "[\"\\\\]")
        message(FATAL_ERROR "psyz_title: cannot contain quotes or backslashes")
    endif()

    if(TARGET psyz)
        target_compile_definitions(psyz PRIVATE "PSYZ_TITLE=\"${title}\"")
    endif()
endfunction()

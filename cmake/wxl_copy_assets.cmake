# /cmake/wxl_copy_assets.cmake
#
# wxl_copy_assets(<target> FROM <dir> [TO <subdir>])
#
# Puts every file under <dir> beside <target>'s executable, in <subdir>
# (default "Assets"), keeping the tree below <dir>. A relative image or font
# path in a wxl application is resolved next to the exe, so this is where
# they have to be.
#
# Each file is a build rule of its own -- an OUTPUT depending on its source
# and added to the target -- rather than a POST_BUILD step: a POST_BUILD step
# runs only when the target relinks, so an asset edited while the code stays
# the same would never reach the build tree. The list comes from the folder
# (CONFIGURE_DEPENDS), so a file dropped into it is picked up by the next
# build without touching any CMakeLists.

include_guard(GLOBAL)

function(wxl_copy_assets target)
    cmake_parse_arguments(PARSE_ARGV 1 _arg "" "FROM;TO" "")
    if(NOT _arg_FROM)
        message(FATAL_ERROR "wxl_copy_assets(${target}): FROM <dir> is required")
    endif()
    if(NOT DEFINED _arg_TO)
        set(_arg_TO "Assets")
    endif()

    # The folder the exe lands in when nothing overrides it: the binary dir,
    # with the configuration below it under a multi-config generator.
    # $<TARGET_FILE_DIR> cannot name an OUTPUT, so it is spelled out.
    get_property(_multi GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
    set(_exe_dir "${CMAKE_CURRENT_BINARY_DIR}")
    if(_multi)
        string(APPEND _exe_dir "/$<CONFIG>")
    endif()

    file(GLOB_RECURSE _files CONFIGURE_DEPENDS LIST_DIRECTORIES false
         RELATIVE "${_arg_FROM}" "${_arg_FROM}/*")
    set(_copied)
    foreach(_file IN LISTS _files)
        set(_from "${_arg_FROM}/${_file}")
        set(_to "${_exe_dir}/${_arg_TO}/${_file}")
        add_custom_command(OUTPUT "${_to}"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_from}" "${_to}"
            DEPENDS "${_from}"
            COMMENT "Copying ${_arg_TO}/${_file} beside ${target}")
        list(APPEND _copied "${_to}")
    endforeach()
    target_sources(${target} PRIVATE ${_copied})
endfunction()

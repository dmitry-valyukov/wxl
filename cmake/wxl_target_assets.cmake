# /cmake/wxl_target_assets.cmake

include_guard(GLOBAL)

# Puts a folder of files beside the executable, as outputs of the build:
#
#     wxl_target_assets(sample.quadratic Assets)
#
# The usual way to do this is a POST_BUILD step that copies the folder. It is
# one line, but it is a tail of the link step rather than an output: when only
# an asset changed there is nothing to relink, the step does not run, and the
# copy beside the executable stays the old one until something else forces a
# link.
#
# So each file is copied by a rule with an output and a dependency, and the
# outputs are handed to the target as sources. Then ninja rebuilds exactly the
# files whose originals changed, and does it before the target is linked. The
# files are not compiled -- a target's sources may be anything, and only the
# languages CMake recognises reach a compiler.
#
# The folder is read at configure time, with CONFIGURE_DEPENDS, so a file added
# or removed in it is noticed on the next build and the build files are
# regenerated. The destination keeps the folder's own name, which is what an
# application looks for beside its executable.
function(wxl_target_assets target folder)
    get_filename_component(_root "${folder}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

    if(NOT IS_DIRECTORY "${_root}")
        message(FATAL_ERROR "wxl_target_assets(${target}): no folder at ${_root}")
    endif()

    get_filename_component(_name "${_root}" NAME)

    file(GLOB_RECURSE _files CONFIGURE_DEPENDS RELATIVE "${_root}" "${_root}/*")

    set(_copies "")

    foreach(_file IN LISTS _files)
        # Not $<TARGET_FILE_DIR:...>: an OUTPUT is evaluated where no target is
        # visible yet, and naming one there fails outright. This is the same
        # place -- where a Ninja Multi-Config build puts the executable.
        set(_copy "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/${_name}/${_file}")

        add_custom_command(
            OUTPUT "${_copy}"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_root}/${_file}" "${_copy}"
            DEPENDS "${_root}/${_file}"
            COMMENT "Copying ${_name}/${_file} beside ${target}"
            VERBATIM)

        list(APPEND _copies "${_copy}")
    endforeach()

    target_sources(${target} PRIVATE ${_copies})
endfunction()

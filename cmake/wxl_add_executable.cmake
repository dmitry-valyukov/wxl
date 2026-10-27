# /cmake/wxl_add_executable.cmake

include_guard(GLOBAL)

# What wxl needs beside an executable, put there.
#
# Two kinds of file, and they arrive by different roads.
#
# The Windows App Runtime bootstrapper is linked: wxl.ui declares it as an
# imported shared library, so CMake knows the DLL behind the import library and
# $<TARGET_RUNTIME_DLLS> names it for whoever links wxl. Anything else of that
# kind would be named too, without a line added here. The list is known only
# when the build files are generated, so this half is a build step of the
# target rather than a rule with an output -- there is no file name to declare
# beforehand.
#
# Win2D is not linked at all: its DLL carries no import library and activates
# by name at run time. CMake cannot know it, so wxl names it in its own
# WXL_RUNTIME_FILES property, and each file of that list is copied by a rule
# with a dependency, the way wxl_target_assets() copies a folder.
#
# The destination is where a Ninja Multi-Config build puts the executable of
# this directory. A target that moves its own output elsewhere would need
# $<TARGET_FILE_DIR>, which an OUTPUT cannot hold; nothing in the tree does.
function(wxl_target_runtime target)
    if(NOT WIN32)
        return()
    endif()

    get_target_property(_files wxl::ui WXL_RUNTIME_FILES)

    if(_files)
        set(_copies "")

        foreach(_file IN LISTS _files)
            get_filename_component(_name "${_file}" NAME)
            set(_copy "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/${_name}")

            add_custom_command(
                OUTPUT "${_copy}"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_file}" "${_copy}"
                DEPENDS "${_file}"
                COMMENT "Copying ${_name} beside ${target}"
                VERBATIM)

            list(APPEND _copies "${_copy}")
        endforeach()

        target_sources(${target} PRIVATE ${_copies})
    endif()

    # A target that links no DLL at all would leave the list empty, and a copy
    # without files is an error -- so the whole command is under the condition
    # and disappears with it.
    set(_dlls "$<TARGET_RUNTIME_DLLS:${target}>")

    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND "$<$<BOOL:${_dlls}>:${CMAKE_COMMAND};-E;copy_if_different;${_dlls};$<TARGET_FILE_DIR:${target}>>"
        COMMAND_EXPAND_LISTS
        COMMENT "Copying the runtime libraries beside ${target}")
endfunction()

# The manifest every executable built on wxl carries -- an application, and a
# console tool of the library's own -- generated from wxl_app.manifest.in beside
# this file: what it declares is the library's requirement rather than the
# executable's business, and the one thing that differs between executables --
# the name in the assembly identity -- is the target's own name, which is known
# right here. One template for every kind of executable, so what the process is
# told is said in one place. wxl_add_executable() calls this for an application,
# and a tool or a benchmark declared with add_executable() calls it on its own
# target:
#
#     add_executable(winui-srcgen main.cpp)
#     wxl_target_manifest(winui-srcgen)
#
# A test does not: it checks the code without the manifest, since the code has
# to be right without it, and a probe of the platform that links nothing of wxl
# keeps the process the platform gives it.
#
# A target whose sources carry a manifest of their own by then keeps it and gets
# no second one: two of them with different identities do not merge, and the
# link fails.
function(wxl_target_manifest target)
    get_target_property(_sources ${target} SOURCES)

    if(_sources MATCHES "\\.manifest(;|$)")
        return()
    endif()

    set(WXL_APP_NAME "${target}")
    set(_manifest "${CMAKE_CURRENT_BINARY_DIR}/${target}.manifest")

    # configure_file(), not file(CONFIGURE): it puts the template into the
    # configure dependencies by itself, so editing the template rebuilds the
    # build files instead of being quietly ignored.
    configure_file("${CMAKE_CURRENT_FUNCTION_LIST_DIR}/wxl_app.manifest.in"
                   "${_manifest}" @ONLY)

    target_sources(${target} PRIVATE "${_manifest}")
endfunction()

# An application on wxl, declared the way every one of them is declared:
#
#     wxl_add_executable(sample.quadratic
#         main.cpp)
#
# It is a windowed executable -- wxl owns wWinMain and there is no other kind
# of application here -- it links wxl::ui and nothing else of wxl, it takes its
# place in the IDE tree, and what wxl needs at run time lands beside it.
#
# The manifest comes with it (wxl_target_manifest), and nobody writes or edits
# one for an application. An application that passes a manifest of its own
# keeps it and gets no second one.
#
# What belongs to one application and not to all of them is said after this
# call, on the target it made: another library, an icon (wxl_target_icon), a
# folder shipped beside the executable (wxl_target_assets).
function(wxl_add_executable target)
    add_executable(${target} WIN32 ${ARGN})

    target_link_libraries(${target} PRIVATE wxl::ui)

    wxl_target_manifest(${target})
    wxl_set_target_folders(${target})
    wxl_target_runtime(${target})
endfunction()

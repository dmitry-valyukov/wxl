# ---- The wxl documentation site ----
#
#   cmake --build --preset x64-debug --target docs
#   build/x64/docs/wxl/html/index.html
#
# One site for the whole tree: the prose articles in docs/ and in each module's
# doc/ next to the API pulled out of the sources, because that is the pairing
# that makes either half worth reading. Doxygen links them by itself -- a
# mention of wxl::async::future in a markdown article becomes a link to its
# page, dozens of them in a single article -- and that link is what a
# separate site generator for the prose would cost.
#
# The site says nothing about the applications built on wxl, deliberately: wxl
# is presented as a library that stands on its own. The traffic goes the other
# way, through the tag file an application's own site reads.
#
# This directory is also a project of its own, with no compiler behind it:
#
#   cmake -S docs -B build/docs
#   cmake --build build/docs --target docs
#   ctest --test-dir build/docs
#
# Doxygen reads the sources without building them, so this is how the site is
# built where the code cannot be: on a Linux runner that publishes it, or on a
# machine without the toolchain. The wxl tree's own configuration is not
# needed for it, and neither are the generated projection and wrappers.

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.28)
    project(wxl-docs LANGUAGES NONE)
    include("${CMAKE_CURRENT_SOURCE_DIR}/../cmake/wxl_doxygen.cmake")
    option(BUILD_TESTING "Register the site as a test." ON)
    if(BUILD_TESTING)
        enable_testing()
    endif()
endif()

cmake_path(SET _tree NORMALIZE "${CMAKE_CURRENT_SOURCE_DIR}/..")
wxl_doxygen_tree_inputs(_input _exclude _images "${_tree}")

wxl_add_doxygen_site(docs
    PROJECT_NAME "wxl"
    BRIEF        "Winui Xaml-Less: декларативный интерфейс на C++23"
    LANGUAGE     Russian
    STRIP_FROM   "${_tree}"
    MAINPAGE     "${_tree}/docs/index.md"
    OUTPUT       "${CMAKE_BINARY_DIR}/docs/wxl"
    TAGFILE      "${CMAKE_BINARY_DIR}/docs/wxl.tag"
    INPUT        ${_input}
    EXCLUDE      ${_exclude}
    IMAGES       ${_images}
)

if(BUILD_TESTING)
    wxl_add_doxygen_test(docs)
endif()

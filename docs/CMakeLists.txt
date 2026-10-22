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
# built where the code cannot be: on the Linux runner that checks a pull
# request, or on a machine without the toolchain. The wxl tree's own
# configuration is not needed for it, and neither are the generated projection
# and wrappers. The published site is built the same way on Windows, once the
# wrappers have been generated into the tree it reads.

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

# Doxygen's text is in the language the wrappers are documented in, which the
# generator's --doc-language chose (language.cmake); the articles stay as they
# are written. It is read here, and read again before every run of Doxygen:
# wrappers generated anew in another language after this configure stop the
# build instead of getting labels in one language over descriptions in
# another.
include("${CMAKE_CURRENT_SOURCE_DIR}/language.cmake")
cmake_path(APPEND _tree wxl.ui src wxl doc_language.txt OUTPUT_VARIABLE _language_file)
wxl_docs_language(_code _language "${_language_file}")
if(_code STREQUAL "ru")
    set(_brief "WinUI XAML-Less: декларативный интерфейс на C++23")
else()
    set(_brief "WinUI XAML-Less: declarative user interface in C++23")
endif()
message(STATUS "Documentation site language: ${_language}")

set(_language_check
    "${CMAKE_COMMAND}"
    "-DWXL_DOCS_LANGUAGE=${_code} ${_language}"
    "-DWXL_DOCS_LANGUAGE_FILE=${_language_file}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/language.cmake")
add_custom_target(docs.language COMMAND ${_language_check} VERBATIM)
set_property(TARGET docs.language PROPERTY FOLDER docs)

wxl_add_doxygen_site(docs
    PROJECT_NAME "wxl"
    BRIEF        "${_brief}"
    LANGUAGE     ${_language}
    STRIP_FROM   "${_tree}"
    MAINPAGE     "${_tree}/docs/index.md"
    OUTPUT       "${CMAKE_BINARY_DIR}/docs/wxl"
    TAGFILE      "${CMAKE_BINARY_DIR}/docs/wxl.tag"
    INPUT        ${_input}
    EXCLUDE      ${_exclude}
    IMAGES       ${_images}
    DEPENDS      docs.language
)

if(BUILD_TESTING)
    wxl_add_doxygen_test(docs)
    add_test(NAME docs.language COMMAND ${_language_check})
    set_tests_properties(docs.language PROPERTIES FIXTURES_SETUP docs.language)
    set_tests_properties(docs PROPERTIES FIXTURES_REQUIRED docs.language)
endif()

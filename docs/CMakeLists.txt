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
# and wrappers: the site documents the wrappers a build of the tree generated
# when WXL_DOCS_WRAPPERS_DIR names them, and none when it is empty.

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.28)
    project(wxl-docs LANGUAGES NONE)
    include("${CMAKE_CURRENT_SOURCE_DIR}/../cmake/wxl_doxygen.cmake")
    option(BUILD_TESTING "Register the site as a test." ON)
    if(BUILD_TESTING)
        enable_testing()
    endif()
endif()

# The generated wrappers the site documents, with the doc_language.txt the
# generator left beside them. In the tree, a generation of the site's own: the
# profile wxl.ui compiles, full, written documented in the language
# WXL_DOC_LANGUAGE names into <build>/docs/wrappers/wxl, by a step only the
# site needs (docs.wrappers, not part of ALL), so that neither the language nor
# the site touches what the library compiles (cmake/wxl_ui_profile.cmake). The
# site waits for it, so Doxygen never reads a directory the generator is
# writing. Standalone, the directory WXL_DOCS_WRAPPERS_DIR names -- the
# docs/wrappers/wxl of a build of the tree -- or none.
set(_wrappers_step "")
if(TARGET winui-srcgen)
    include("${CMAKE_CURRENT_SOURCE_DIR}/../cmake/wxl_ui_profile.cmake")
    set(_wrappers "${CMAKE_BINARY_DIR}/docs/wrappers/wxl")
    wxl_generate_wrappers(docs.wrappers
        DIR     "${_wrappers}"
        PROFILE "${CMAKE_CURRENT_SOURCE_DIR}/../wxl.gen/profiles/full.winxl-profile"
        PARTS   1
        DOCS    "${WXL_DOC_LANGUAGE}")
    set_property(TARGET docs.wrappers PROPERTY FOLDER docs)
    set(_wrappers_step docs.wrappers)
else()
    set(WXL_DOCS_WRAPPERS_DIR "" CACHE PATH
        "Wrappers the site documents (<build>/docs/wrappers/wxl of a wxl build); empty, none")
    set(_wrappers "${WXL_DOCS_WRAPPERS_DIR}")
    if(_wrappers AND NOT IS_DIRECTORY "${_wrappers}")
        message(FATAL_ERROR
            "WXL_DOCS_WRAPPERS_DIR names ${_wrappers}, which is not a directory: name the "
            "wrappers a build of the wxl tree generated for its site "
            "(<build>/docs/wrappers/wxl), or nothing.")
    endif()
endif()

# The include root of the wrappers is stripped from their paths on the site, as
# the tree's is from the sources': a page names wxl/Button.h, the way it is
# included, and not the build directory it was generated in.
set(_wrappers_root "")
if(_wrappers)
    cmake_path(GET _wrappers PARENT_PATH _wrappers_root)
endif()

cmake_path(SET _tree NORMALIZE "${CMAKE_CURRENT_SOURCE_DIR}/..")
wxl_doxygen_tree_inputs(_input _exclude _images "${_tree}" "${_wrappers}")

# Doxygen's text is in the language the wrappers are documented in, which the
# generator's --doc-language chose; the articles stay as they are written. The
# language is known only once the wrappers are generated, so it is not read
# here: docs.language, a step after the generation, reads doc_language.txt and
# writes the fragment the Doxyfile includes last, with the language and the
# brief (language.cmake). The generator's table stays the one place Doxygen's
# names of the languages are written.
set(_language_file "")
if(_wrappers)
    set(_language_file "${_wrappers}/doc_language.txt")
endif()
set(_language_fragment "${CMAKE_BINARY_DIR}/docs/language.doxyfile")
add_custom_target(docs.language
    COMMAND "${CMAKE_COMMAND}"
            "-DWXL_DOCS_LANGUAGE_FILE=${_language_file}"
            "-DWXL_DOCS_FRAGMENT=${_language_fragment}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/language.cmake"
    BYPRODUCTS "${_language_fragment}"
    VERBATIM)
set_property(TARGET docs.language PROPERTY FOLDER docs)
if(_wrappers_step)
    add_dependencies(docs.language ${_wrappers_step})
endif()

wxl_add_doxygen_site(docs
    PROJECT_NAME "wxl"
    STRIP_FROM   "${_tree}" ${_wrappers_root}
    MAINPAGE     "${_tree}/docs/index.md"
    OUTPUT       "${CMAKE_BINARY_DIR}/docs/wxl"
    TAGFILE      "${CMAKE_BINARY_DIR}/docs/wxl.tag"
    INPUT        ${_input}
    EXCLUDE      ${_exclude}
    IMAGES       ${_images}
    INCLUDE      "${_language_fragment}"
    DEPENDS      docs.language
)

# The test runs the site's Doxyfile as it is, so it builds first what that
# Doxyfile reads and includes: docs.language, after the generation of the
# wrappers in the tree.
if(BUILD_TESTING)
    wxl_add_doxygen_test(docs)
    add_test(NAME docs.language
        COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}" --config $<CONFIG>
                --target docs.language)
    # The build may link winui-srcgen anew, which Windows refuses while the
    # executable runs: the winui-srcgen.* tests of wxl.gen/tests take the same
    # lock.
    set_tests_properties(docs.language PROPERTIES
        FIXTURES_SETUP docs.language
        RESOURCE_LOCK winui-srcgen)
    set_tests_properties(docs PROPERTIES FIXTURES_REQUIRED docs.language)
endif()

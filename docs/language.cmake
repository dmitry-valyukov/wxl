# ---- The language of the documentation site ----
#
# One language for Doxygen's text on the site, chosen once: the one
# winui-srcgen --docs wrote the wrappers' descriptions in. The generator leaves
# it beside the wrappers, in doc_language.txt -- its banner, then one line with
# the code --doc-language named and Doxygen's name for it ("ru Russian"). Which
# codes there are, and Doxygen's names for them, is the generator's to know,
# and it refuses any other code before it writes anything, so the file is only
# checked for its form here. No file, or no wrappers at all -- the pull request
# check -- means English, the generator's own default.
#
# Run as a step of the build, after the generation of the wrappers and before
# Doxygen, by the site target and by the setup of its test: it writes the
# Doxyfile fragment the site's Doxyfile includes last (wxl_add_doxygen_site
# INCLUDE), with the language and the brief in it. Not read at configure: the
# file appears only when the wrappers are generated, and a configure that read
# it would describe the generation before last. The fragment is written only
# when its text changes.
#
#   cmake -DWXL_DOCS_LANGUAGE_FILE=<doc_language.txt, or empty>
#         -DWXL_DOCS_FRAGMENT=<file.doxyfile> -P language.cmake

# wxl_docs_language(<code-var> <doxygen-var> <file>): the code <file> names and
# Doxygen's name for the language, en and English when there is no file. A file
# whose line under the banner is not a code and a name stops the build.
function(wxl_docs_language code_var doxygen_var file)
    set(_code en)
    set(_doxygen English)
    if(NOT file STREQUAL "" AND EXISTS "${file}")
        # The lines under the banner. file(STRINGS) drops the carriage returns
        # of a file written on Windows.
        file(STRINGS "${file}" _lines REGEX "^[^/]")
        set(_form "^([a-z]+(-[a-z]+)?) ([A-Za-z]+(-[A-Za-z]+)*)$")
        list(LENGTH _lines _count)
        if(_count EQUAL 1 AND _lines MATCHES "${_form}")
            set(_code "${CMAKE_MATCH_1}")
            set(_doxygen "${CMAKE_MATCH_3}")
        else()
            message(FATAL_ERROR
                "${file} should hold one line under its banner, a language code and Doxygen's "
                "name for it (\"ru Russian\"), and holds: '${_lines}'.")
        endif()
    endif()
    set(${code_var} "${_code}" PARENT_SCOPE)
    set(${doxygen_var} "${_doxygen}" PARENT_SCOPE)
endfunction()

if(NOT CMAKE_SCRIPT_MODE_FILE OR NOT DEFINED WXL_DOCS_FRAGMENT)
    message(FATAL_ERROR
        "docs/language.cmake is a script: cmake -DWXL_DOCS_LANGUAGE_FILE=<file> "
        "-DWXL_DOCS_FRAGMENT=<file> -P docs/language.cmake")
endif()

wxl_docs_language(_code _doxygen "${WXL_DOCS_LANGUAGE_FILE}")
# The articles stay as they are written, in Russian; the brief of the project
# follows the language only where it is Russian.
if(_code STREQUAL "ru")
    set(_brief "WinUI XAML-Less: декларативный интерфейс на C++23")
else()
    set(_brief "WinUI XAML-Less: declarative user interface in C++23")
endif()

set(_from "${WXL_DOCS_LANGUAGE_FILE}")
if(_from STREQUAL "")
    set(_from "no wrappers")
endif()
string(CONCAT _text
    "# Written by docs/language.cmake: ${_from}.\n"
    "OUTPUT_LANGUAGE        = ${_doxygen}\n"
    "PROJECT_BRIEF          = \"${_brief}\"\n")
set(_written "")
if(EXISTS "${WXL_DOCS_FRAGMENT}")
    file(READ "${WXL_DOCS_FRAGMENT}" _written)
endif()
if(NOT _written STREQUAL _text)
    file(WRITE "${WXL_DOCS_FRAGMENT}" "${_text}")
endif()
message(STATUS "Documentation site language: ${_doxygen}")

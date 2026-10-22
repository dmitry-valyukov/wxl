# ---- The language of the documentation site ----
#
# One language for Doxygen's text on the site, chosen once: the one
# winui-srcgen --docs wrote the wrappers' descriptions in. The generator leaves
# it beside the wrappers, in doc_language.txt -- its banner, then one line with
# the code --doc-language named and Doxygen's name for it ("ru Russian"). Which
# codes there are is the generator's to know, and it refuses any other before
# it writes anything, so the file is only checked for its form here. No file
# means no documented wrappers (the pull request check, a tree generated
# without --docs) and English, the generator's own default.
#
# Included by docs/CMakeLists.txt, which reads the file when it configures. Also
# run as a script before Doxygen, by the site target and by its test, to stop a
# build whose wrappers were generated again, in another language, after the
# configure. The whole line is compared, the code and Doxygen's name:
#
#   cmake "-DWXL_DOCS_LANGUAGE=<code> <name>" -DWXL_DOCS_LANGUAGE_FILE=<file> -P language.cmake

# wxl_docs_language(<code-var> <doxygen-var> <file>): the code <file> names and
# Doxygen's name for the language, en and English when there is no file. A file
# whose line under the banner is not a code and a name stops the configure.
function(wxl_docs_language code_var doxygen_var file)
    set(_code en)
    set(_doxygen English)
    if(EXISTS "${file}")
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

if(CMAKE_SCRIPT_MODE_FILE AND DEFINED WXL_DOCS_LANGUAGE)
    wxl_docs_language(_now_code _now_doxygen "${WXL_DOCS_LANGUAGE_FILE}")
    set(_now "${_now_code} ${_now_doxygen}")
    if(NOT _now STREQUAL WXL_DOCS_LANGUAGE)
        message(FATAL_ERROR
            "The wrappers are documented in '${_now}' now (${WXL_DOCS_LANGUAGE_FILE}), and the "
            "site was configured for '${WXL_DOCS_LANGUAGE}': they were generated again since. "
            "Configure the site again.")
    endif()
endif()

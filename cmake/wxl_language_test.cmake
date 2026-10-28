# A test of wxl_check_language_options (cmake/wxl_language.cmake): the check the
# root of the wxl tree makes in a project that adds it, on directories set up
# the way such a project leaves them. Run as a test (ctest -R language-options),
# and by hand:
#
#     cmake -P wxl_language_test.cmake
#
# Each case runs in a cmake process of its own, since a stop ends the script it
# happens in: the compile options of the directory (WXL_TEST_OPTIONS, a comma
# between two) and the language variables as the case gives them, then the
# check. What is checked: a directory with everything passes, and one with more
# options than wxl's too; anything missing -- an option, a variable unset or
# with another value -- stops, and the stop names the lines to add, those and
# no others. WIN32 is set in the case, so the options are checked on any host.

# A script sets no policies of its own, and if(IN_LIST) in the check needs
# CMP0057; the cases run through this file as well.
cmake_minimum_required(VERSION 3.28)

include("${CMAKE_CURRENT_LIST_DIR}/wxl_language.cmake")

if(DEFINED WXL_TEST_OPTIONS)
    set(WIN32 1)
    string(REPLACE "," ";" options "${WXL_TEST_OPTIONS}")
    set_property(DIRECTORY PROPERTY COMPILE_OPTIONS ${options})
    wxl_check_language_options()
    return()
endif()

set(_wxl_test_all
    -DCMAKE_CXX_STANDARD=23
    -DCMAKE_CXX_STANDARD_REQUIRED=ON
    -DCMAKE_CXX_EXTENSIONS=OFF
    -DCMAKE_CXX_MODULE_STD=1
    -DCMAKE_CXX_SCAN_FOR_MODULES=ON)

set(_wxl_test_lines
    "set(CMAKE_CXX_STANDARD 23)"
    "set(CMAKE_CXX_STANDARD_REQUIRED ON)"
    "set(CMAKE_CXX_EXTENSIONS OFF)"
    "set(CMAKE_CXX_MODULE_STD 1)"
    "set(CMAKE_CXX_SCAN_FOR_MODULES ON)")

# _wxl_test_case(<name> OPTIONS <options> [SET <-Dvariable=value>...] [LINES <line>...])
#
# No LINES: the check passes. LINES: it stops, and what it says holds each of
# them and none of the other lines it can name.
function(_wxl_test_case name)
    cmake_parse_arguments(A "" "OPTIONS" "SET;LINES" ${ARGN})
    execute_process(
        COMMAND "${CMAKE_COMMAND}" "-DWXL_TEST_OPTIONS=${A_OPTIONS}" ${A_SET}
                -P "${CMAKE_CURRENT_FUNCTION_LIST_FILE}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE output)
    if(NOT A_LINES)
        if(NOT result EQUAL 0)
            message(FATAL_ERROR "language-options: ${name} stopped:\n${output}")
        endif()
        return()
    endif()
    if(result EQUAL 0)
        message(FATAL_ERROR "language-options: ${name} passed the check.")
    endif()
    foreach(line IN LISTS A_LINES)
        string(FIND "${output}" "${line}" at)
        if(at EQUAL -1)
            message(FATAL_ERROR "language-options: ${name}: no \"${line}\" in what the stop said:\n${output}")
        endif()
    endforeach()
    set(others ${_wxl_test_lines})
    list(REMOVE_ITEM others ${A_LINES})
    foreach(line IN LISTS others)
        string(FIND "${output}" "${line}" at)
        if(NOT at EQUAL -1)
            message(FATAL_ERROR "language-options: ${name}: \"${line}\" in what the stop said:\n${output}")
        endif()
    endforeach()
    string(REGEX MATCHALL "add_compile_options\\(" calls "${output}")
    list(FILTER A_LINES INCLUDE REGEX "^add_compile_options\\(")
    list(LENGTH calls got)
    list(LENGTH A_LINES expected)
    if(NOT got EQUAL expected)
        message(FATAL_ERROR
            "language-options: ${name}: ${got} add_compile_options lines, ${expected} expected:\n${output}")
    endif()
endfunction()

_wxl_test_case("everything"
    OPTIONS "/permissive-,/utf-8,/Zc:throwingNew"
    SET ${_wxl_test_all})

_wxl_test_case("more options than wxl's, in another order"
    OPTIONS "/W4,/Zc:throwingNew,/Zc:__cplusplus,/utf-8,/permissive-"
    SET ${_wxl_test_all})

_wxl_test_case("nothing"
    OPTIONS ""
    LINES "add_compile_options(/permissive- /utf-8 /Zc:throwingNew)" ${_wxl_test_lines})

_wxl_test_case("one option"
    OPTIONS "/permissive-,/utf-8"
    SET ${_wxl_test_all}
    LINES "add_compile_options(/Zc:throwingNew)")

_wxl_test_case("an option behind a generator expression"
    OPTIONS "/permissive-,/utf-8,$<$<CXX_COMPILER_ID:MSVC>:/Zc:throwingNew>"
    SET ${_wxl_test_all}
    LINES "add_compile_options(/Zc:throwingNew)")

_wxl_test_case("C++20"
    OPTIONS "/permissive-,/utf-8,/Zc:throwingNew"
    SET ${_wxl_test_all} -DCMAKE_CXX_STANDARD=20
    LINES "set(CMAKE_CXX_STANDARD 23)")

_wxl_test_case("extensions on, and no std module"
    OPTIONS "/permissive-,/utf-8,/Zc:throwingNew"
    SET -DCMAKE_CXX_STANDARD=23 -DCMAKE_CXX_STANDARD_REQUIRED=ON -DCMAKE_CXX_EXTENSIONS=ON
        -DCMAKE_CXX_SCAN_FOR_MODULES=ON
    LINES "set(CMAKE_CXX_EXTENSIONS OFF)" "set(CMAKE_CXX_MODULE_STD 1)")

_wxl_test_case("extensions unset, and the standard not required"
    OPTIONS "/permissive-,/utf-8,/Zc:throwingNew"
    SET -DCMAKE_CXX_STANDARD=23 -DCMAKE_CXX_STANDARD_REQUIRED=OFF -DCMAKE_CXX_MODULE_STD=1
        -DCMAKE_CXX_SCAN_FOR_MODULES=ON
    LINES "set(CMAKE_CXX_EXTENSIONS OFF)" "set(CMAKE_CXX_STANDARD_REQUIRED ON)")

message(STATUS "language-options: passed")

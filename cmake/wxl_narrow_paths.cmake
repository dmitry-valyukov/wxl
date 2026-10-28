# /cmake/wxl_narrow_paths.cmake
#
# Keeps the narrow interfaces of a path out of the tree's C++ sources. Run as a
# test (ctest -R narrow-paths), and by hand:
#
#     cmake -DWXL_TREE=<source tree> -DWXL_GIT=<git> -P wxl_narrow_paths.cmake
#
# On MSVC every narrow interface of std::filesystem::path goes through the
# process's ANSI code page, and this tree keeps text in UTF-8: Cyrillic comes
# out as another page's bytes, and a character the page lacks throws. So a path
# stays UTF-16 from where it enters to the system call, and crosses to text
# only through wxl::core::unicode::to_utf8(path) and back through to_path(); an
# environment variable is read by wxl::core::environment::variable().
#
# What is found, by spelling alone, is a call of one of these:
#
#   - string and generic_string as a member of anything (after . or ->, also
#     as .template string<char>()): nothing else in the tree has a member of
#     those names. Their u8 forms as well: u8string and generic_u8string do
#     not go through the ANSI page, they convert as UTF-8, but they have no
#     answer for an unpaired surrogate, which a Windows file name may hold, and
#     to_utf8(path) is the one conversion that has;
#   - the narrow environment of the C runtime: getenv, getenv_s, _dupenv_s,
#     _putenv, _putenv_s;
#   - u8path, and the C runtime opening a file by a narrow name: fopen,
#     fopen_s, freopen, freopen_s, _fsopen, _open, _sopen, _sopen_s;
#   - std::system and ::system, which hand the system a narrow command line;
#   - the Windows functions that take a path or the environment, in their
#     narrow form and without a suffix (CreateFileA and CreateFile, and the
#     rest of the list below): the tree does not define UNICODE, so a name
#     without the suffix is the A form.
#
# What is not found, because it takes the types a compiler knows: a path built
# from narrow text in a variable or a macro (std::filesystem::path{name},
# directory / name), a stream opened by a narrow name (std::ifstream{name}), a
# path written to a stream with operator<<, which goes through string(), a
# main() whose narrow argv becomes paths, and system called without a
# qualifier. Those are kept out by review.
#
# Comments are scanned like code. One that names a function of the list writes
# it without the parentheses of a call, or qualified as path::string(), which
# is not taken for a call of a member.

cmake_minimum_required(VERSION 3.28)

if(NOT WXL_TREE OR NOT WXL_GIT)
    message(FATAL_ERROR "wxl_narrow_paths.cmake: -DWXL_TREE=<source tree> and -DWXL_GIT=<git> are required")
endif()

file(TO_CMAKE_PATH "${WXL_TREE}" _tree)
if(NOT _tree MATCHES "/$")
    string(APPEND _tree "/")
endif()

# The sources are what git tracks: not a build tree, the projection cppwinrt
# writes or the wrappers winui-srcgen writes, all kept out of the repository,
# nor anything else that lies in the directory without belonging to it. The
# templates CMake makes sources of are sources too. The names come as they are
# spelled rather than quoted, and are read as UTF-8.
execute_process(
    COMMAND "${WXL_GIT}" -c core.quotePath=false ls-files --
            "*.cpp" "*.h" "*.ixx" "*.inc" "*.cpp.in" "*.h.in" "*.ixx.in"
    WORKING_DIRECTORY "${_tree}"
    ENCODING UTF-8
    RESULT_VARIABLE _result
    OUTPUT_VARIABLE _listed
    ERROR_VARIABLE _error)
if(NOT _result EQUAL 0)
    message(FATAL_ERROR "wxl_narrow_paths.cmake: git cannot list the sources of ${_tree}: ${_error}")
endif()
string(STRIP "${_listed}" _listed)
string(REPLACE "\n" ";" _sources "${_listed}")

# One expression for all of them, so that a file's findings come in the order of
# its lines. A name of a function stands on its own: what comes before it is not
# part of an identifier.
set(_alone "(^|[^A-Za-z0-9_])")
set(_windows_calls
    "CreateFile|CreateDirectory|RemoveDirectory|DeleteFile|CopyFile|MoveFile|MoveFileEx|FindFirstFile|"
    "FindFirstFileEx|GetFileAttributes|GetFileAttributesEx|SetFileAttributes|GetFullPathName|"
    "GetLongPathName|GetShortPathName|GetTempPath|GetTempFileName|GetCurrentDirectory|"
    "SetCurrentDirectory|GetModuleFileName|LoadLibrary|LoadLibraryEx|ShellExecute|ShellExecuteEx|"
    "GetEnvironmentVariable|SetEnvironmentVariable|ExpandEnvironmentStrings")
string(JOIN "" _windows_calls ${_windows_calls})
set(_functions
    "getenv|getenv_s|_dupenv_s|_putenv|_putenv_s|u8path|fopen|fopen_s|freopen|freopen_s|"
    "_fsopen|_open|_sopen|_sopen_s|(${_windows_calls})A?")
string(JOIN "" _functions ${_functions})
set(_narrow
    "[.>]([ \t]*template[ \t]+)?(generic_)?(u8)?string[ \t]*[(<]|${_alone}(${_functions})[ \t]*\\(|(std)?::system[ \t]*\\(")

# Reports every match in <text>, the contents of <file>, as file(line): the name
# found, then the line it is on. Adds their number to _reported.
function(_wxl_report_matches file text)
    set(_rest "${text}")
    set(_offset 0)
    set(_found 0)
    while(TRUE)
        string(REGEX MATCH "${_narrow}" _match "${_rest}")
        if(_match STREQUAL "")
            break()
        endif()
        string(FIND "${_rest}" "${_match}" _at)
        string(LENGTH "${_match}" _size)
        math(EXPR _end "${_offset} + ${_at} + ${_size}")

        # The line of the match's last character: a match spans no line break
        # but the one its leading character may be.
        string(SUBSTRING "${text}" 0 ${_end} _before)
        string(REGEX MATCHALL "\n" _breaks "${_before}")
        list(LENGTH _breaks _line)
        math(EXPR _line "${_line} + 1")

        string(FIND "${_before}" "\n" _line_start REVERSE)
        math(EXPR _line_start "${_line_start} + 1")
        string(SUBSTRING "${text}" ${_line_start} -1 _from_line)
        string(FIND "${_from_line}" "\n" _line_end)
        string(SUBSTRING "${_from_line}" 0 ${_line_end} _source_line)
        string(STRIP "${_source_line}" _source_line)

        string(REGEX REPLACE "^[^A-Za-z0-9_]+" "" _name "${_match}")
        string(REGEX REPLACE "^template[ \t]+" "" _name "${_name}")
        string(REGEX REPLACE "[ \t]*[(<]$" "" _name "${_name}")
        message("${file}(${_line}): narrow path interface ${_name}: ${_source_line}")
        math(EXPR _found "${_found} + 1")

        math(EXPR _skip "${_at} + ${_size}")
        string(SUBSTRING "${_rest}" ${_skip} -1 _rest)
        set(_offset ${_end})
    endwhile()
    math(EXPR _reported "${_reported} + ${_found}")
    set(_reported ${_reported} PARENT_SCOPE)
endfunction()

# A file git still lists but the working tree no longer has, deleted without
# git rm, is passed over: there is nothing in it to scan.
set(_reported 0)
set(_scanned 0)
foreach(_file IN LISTS _sources)
    if(NOT EXISTS "${_tree}${_file}")
        continue()
    endif()
    file(READ "${_tree}${_file}" _text)
    _wxl_report_matches("${_file}" "${_text}")
    math(EXPR _scanned "${_scanned} + 1")
endforeach()

# Nothing to scan is a tree given wrongly, not a clean one.
if(_scanned EQUAL 0)
    message(FATAL_ERROR "wxl_narrow_paths.cmake: no C++ sources in ${_tree}")
endif()

if(_reported GREATER 0)
    message(FATAL_ERROR
        "${_reported} narrow path interface(s) in the sources: a path stays UTF-16 to the system call "
        "and becomes text through wxl::core::unicode::to_utf8(path), text a path through to_path(), "
        "and an environment variable is read by wxl::core::environment::variable(). See ${CMAKE_CURRENT_LIST_FILE}.")
endif()

message(STATUS "No narrow path interface in ${_scanned} sources of ${_tree}")

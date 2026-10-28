# A test of cmake/wxl_restored_packages.cmake: the restore result it reads, the
# list of the restore it writes and the entries a lock file records, on a
# restore result, a response file and package folders made up under
# WXL_TEST_DIR and the text of a lock file. Run as a test (ctest -R
# restored-packages), and by hand:
#
#     cmake -DWXL_TEST_DIR=<directory> -P wxl_restored_packages_test.cmake
#
# The directory is emptied first. What is checked: the escaping of a backslash
# and a quote in a JSON string; package folders and Windows SDK files whose
# paths hold Cyrillic, a space and a ; arriving in the list as they are; the
# list left untouched when its text is the same; the entries a lock file
# records, and a broken lock file; and a response file that names what is not
# a .winmd file stopping the writer with the remedy. That last one runs in a
# cmake process of its own (WXL_TEST_STEP=stop), since a stop ends the script
# it happens in.

include("${CMAKE_CURRENT_LIST_DIR}/wxl_restored_packages.cmake")

if(NOT WXL_TEST_DIR)
    message(FATAL_ERROR "restored-packages: WXL_TEST_DIR names no directory to work in.")
endif()

function(_wxl_test_expect what actual expected)
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "restored-packages: ${what}:\n  got      ${actual}\n  expected ${expected}")
    endif()
endfunction()

# A response file in the form the CppWinRT targets write one, naming `first` and
# the arguments after it: an entry a line, with the native path in quotes. Each
# is read as ARGV<n>, which holds the argument as given; ARGN would be a list,
# and a ; in a path is what the test is about.
function(_wxl_test_response file first)
    set(text "")
    math(EXPR last "${ARGC} - 1")
    foreach(at RANGE 1 ${last})
        cmake_path(NATIVE_PATH ARGV${at} native)
        string(APPEND text "-in \"${native}\"\r\n")
    endforeach()
    string(APPEND text "-out \"${WXL_TEST_DIR}/projection/.\"\r\n")
    file(WRITE "${file}" "${text}")
endfunction()

if(WXL_TEST_STEP STREQUAL "stop")
    file(MAKE_DIRECTORY "${WXL_TEST_DIR}/int/directory.winmd")
    _wxl_test_response("${WXL_TEST_DIR}/int/stop.rsp" "${WXL_TEST_DIR}/int/directory.winmd")
    _wxl_packages_write_list("${WXL_TEST_DIR}/stop.json" "10.0.26100.0" "${WXL_TEST_DIR}/int/stop.rsp")
    return()
endif()

file(REMOVE_RECURSE "${WXL_TEST_DIR}")
file(MAKE_DIRECTORY "${WXL_TEST_DIR}")

# A backslash and a quote are escaped, and the string reads back as it was.
set(raw "C:\\a \"b\"\\c")
_wxl_packages_json_string(json "${raw}")
_wxl_test_expect("a JSON string" "${json}" "\"C:\\\\a \\\"b\\\"\\\\c\"")
string(JSON back GET "{\"x\": ${json}}" x)
_wxl_test_expect("a JSON string read back" "${back}" "${raw}")

# The package folder, in Cyrillic ("пакеты") and with a space and a ;, as a
# NuGet configuration may put it; the folder of the SDK's files likewise.
set(root "${WXL_TEST_DIR}/пакеты; nuget")
set(sdk_dir "${WXL_TEST_DIR}/Windows Kits; пробный")
file(MAKE_DIRECTORY "${root}/microsoft.windowsappsdk/2.5.4-experimental" "${root}/test.package/1.0.0")
file(WRITE "${sdk_dir}/Windows.Foundation.FoundationContract.winmd" "")
file(WRITE "${sdk_dir}/Windows.Foundation.UniversalApiContract.winmd" "")

set(native_root "${root}/")
cmake_path(NATIVE_PATH native_root native_root)
_wxl_packages_json_string(folder "${native_root}")
file(WRITE "${WXL_TEST_DIR}/project.assets.json" "{
  \"version\": 3,
  \"libraries\": {
    \"Test.Package/1.0.0\": { \"type\": \"package\", \"path\": \"test.package/1.0.0\" },
    \"Microsoft.WindowsAppSDK/2.5.4-experimental\": {
      \"type\": \"package\", \"path\": \"microsoft.windowsappsdk/2.5.4-experimental\" },
    \"wxl.packages/1.0.0\": { \"type\": \"project\", \"path\": \"wxl.packages.vcxproj\" }
  },
  \"packageFolders\": { ${folder}: {} }
}
")
_wxl_test_response("${WXL_TEST_DIR}/int/plat.rsp"
    "${sdk_dir}/Windows.Foundation.FoundationContract.winmd"
    "${sdk_dir}/Windows.Foundation.UniversalApiContract.winmd")

_wxl_packages_read_assets("${WXL_TEST_DIR}/project.assets.json" count)
_wxl_test_expect("the packages read" "${count}" "2")

set(list "${WXL_TEST_DIR}/restored-packages.json")
_wxl_packages_write_list("${list}" "10.0.26100.0" "${WXL_TEST_DIR}/int/plat.rsp")
file(READ "${list}" text)
string(JSON packages LENGTH "${text}" packages)
_wxl_test_expect("the packages listed" "${packages}" "2")
# By id, ignoring case: the umbrella package first.
string(JSON id GET "${text}" packages 0 id)
_wxl_test_expect("the first package" "${id}" "Microsoft.WindowsAppSDK")
string(JSON version GET "${text}" packages 0 version)
_wxl_test_expect("its version" "${version}" "2.5.4-experimental")
string(JSON directory GET "${text}" packages 0 directory)
_wxl_test_expect("its directory" "${directory}" "${root}/microsoft.windowsappsdk/2.5.4-experimental")
string(JSON id GET "${text}" packages 1 id)
_wxl_test_expect("the second package" "${id}" "Test.Package")
string(JSON directory GET "${text}" packages 1 directory)
_wxl_test_expect("its directory" "${directory}" "${root}/test.package/1.0.0")
string(JSON version GET "${text}" windowsSdk version)
_wxl_test_expect("the Windows SDK" "${version}" "10.0.26100.0")
string(JSON winmd GET "${text}" windowsSdk metadata 0)
_wxl_test_expect("its first .winmd" "${winmd}" "${sdk_dir}/Windows.Foundation.FoundationContract.winmd")
string(JSON winmd GET "${text}" windowsSdk metadata 1)
_wxl_test_expect("its second .winmd" "${winmd}" "${sdk_dir}/Windows.Foundation.UniversalApiContract.winmd")

# The same text leaves the file as it was; another is written. The file times
# are in seconds, hence the waits.
file(TIMESTAMP "${list}" before "%Y-%m-%dT%H:%M:%S" UTC)
execute_process(COMMAND "${CMAKE_COMMAND}" -E sleep 1.5)
_wxl_packages_write_list("${list}" "10.0.26100.0" "${WXL_TEST_DIR}/int/plat.rsp")
file(TIMESTAMP "${list}" after "%Y-%m-%dT%H:%M:%S" UTC)
_wxl_test_expect("the time of a list written again with the same text" "${after}" "${before}")
_wxl_packages_write_list("${list}" "10.0.26200.0" "${WXL_TEST_DIR}/int/plat.rsp")
file(READ "${list}" text)
string(JSON version GET "${text}" windowsSdk version)
_wxl_test_expect("the Windows SDK of a list written with another" "${version}" "10.0.26200.0")
file(TIMESTAMP "${list}" changed "%Y-%m-%dT%H:%M:%S" UTC)
if(changed STREQUAL before)
    message(FATAL_ERROR "restored-packages: a list with another text kept its time ${before}.")
endif()

# The lock file: the direct entries of the section of the target framework,
# the first one, by id in lower case. A package that is only a dependency
# there is not one of them, and the section of a runtime that follows names
# other versions, which are not read. An id is matched ignoring case.
set(lock "${WXL_TEST_DIR}/packages.lock.json")
set(lock_json [=[{
  "version": 1,
  "dependencies": {
    "native,Version=v0.0": {
      "Microsoft.WindowsAppSDK": {
        "type": "Direct", "requested": "[2.5.4-experimental, 2.5.4-experimental]",
        "resolved": "2.5.4-experimental" },
      "microsoft.graphics.WIN2D": { "type": "Direct", "requested": "[1.*, )", "resolved": "1.4.0" },
      "Microsoft.WindowsAppSDK.Base": { "type": "Transitive", "resolved": "2.0.5-experimental2" }
    },
    "native,Version=v0.0/win-x64": {
      "Microsoft.Graphics.Win2D": { "type": "Direct", "requested": "[1.*, )", "resolved": "1.9.9" },
      "Microsoft.Web.WebView2": { "type": "Direct", "requested": "[1.*, )", "resolved": "1.0.1" }
    }
  }
}]=])
_wxl_packages_lock_problem(problem "${lock_json}")
_wxl_test_expect("what keeps the lock file from being one" "${problem}" "")
_wxl_packages_read_lock(locked "${lock}" "${lock_json}")
_wxl_test_expect("the direct entries" "${locked_IDS}"
                 "microsoft.windowsappsdk;microsoft.graphics.win2d")
foreach(case
        "Microsoft.WindowsAppSDK|[2.5.4-experimental, 2.5.4-experimental]|2.5.4-experimental"
        "Microsoft.Graphics.Win2D|[1.*, )|1.4.0")
    string(REGEX MATCH "^([^|]+)\\|([^|]+)\\|([^|]+)$" case "${case}")
    set(id "${CMAKE_MATCH_1}")
    set(requested "${CMAKE_MATCH_2}")
    set(resolved "${CMAKE_MATCH_3}")
    string(TOLOWER "${id}" lower)
    _wxl_test_expect("the request of ${id}" "${locked_REQUESTED_${lower}}" "${requested}")
    _wxl_test_expect("the version of ${id}" "${locked_RESOLVED_${lower}}" "${resolved}")
endforeach()
_wxl_test_expect("the id of Win2D as the file writes it" "${locked_ID_microsoft.graphics.win2d}"
                 "microsoft.graphics.WIN2D")

# A broken lock file -- conflict markers, no "dependencies" -- is named as one.
foreach(case
        "<<<<<<< HEAD\n{}\n=======\n{}\n>>>>>>> other=is not JSON with an object \"dependencies\""
        "{\"dependencies\": {}}=names no target framework in \"dependencies\""
        "{\"dependencies\": {\"native,Version=v0.0\": []}}=has no object for the target framework")
    string(REGEX MATCH "^(.*)=([^=]+)$" case "${case}")
    set(text "${CMAKE_MATCH_1}")
    set(expected "${CMAKE_MATCH_2}")
    _wxl_packages_lock_problem(problem "${text}")
    string(FIND "${problem}" "${expected}" at)
    if(NOT at EQUAL 0)
        message(FATAL_ERROR "restored-packages: the broken lock file ${text} said \"${problem}\".")
    endif()
endforeach()

# A response file that names a directory where a .winmd should be stops with
# the remedy: delete the intermediate directory and configure again.
execute_process(
    COMMAND "${CMAKE_COMMAND}" "-DWXL_TEST_DIR=${WXL_TEST_DIR}" -DWXL_TEST_STEP=stop
            -P "${CMAKE_CURRENT_LIST_FILE}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE output)
if(result EQUAL 0)
    message(FATAL_ERROR "restored-packages: a response file naming a directory did not stop the list.")
endif()
string(REPLACE "\n" " " output "${output}")
string(REGEX REPLACE " +" " " output "${output}")
foreach(part "is not a .winmd file on disk" "delete ${WXL_TEST_DIR}/int and configure again")
    string(FIND "${output}" "${part}" at)
    if(at EQUAL -1)
        message(FATAL_ERROR "restored-packages: no \"${part}\" in what the stop said:\n${output}")
    endif()
endforeach()
if(EXISTS "${WXL_TEST_DIR}/stop.json")
    message(FATAL_ERROR "restored-packages: a stopped list was written anyway.")
endif()

message(STATUS "restored-packages: passed")

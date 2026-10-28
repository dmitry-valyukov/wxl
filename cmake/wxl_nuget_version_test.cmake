# A test of cmake/wxl_nuget_version.cmake: the reading and the order of NuGet
# versions, the channel of a label, and the version a pin admits out of the
# index of a package. Run as a test (ctest -R nuget-version), and by hand:
#
#     cmake -DWXL_TEST_DIR=<directory> -P wxl_nuget_version_test.cmake
#
# The directory is emptied first. The versions read and their order are those
# of wxl.gen.common/tests/nuget_version_test.cpp, which tests the same rules in
# the generator. The index is that of Microsoft.WindowsAppSDK on nuget.org
# (2026-10-28), its 2.x versions with one below and one above them. A pin is
# a minimum or one version held in brackets, [X]: a held pin is referenced as
# written and admits that version only, and any other range is no pin. The
# requested range a lock file records is matched with a reference. A pin that
# is neither stops the comparison, a held pin stops the search of the index,
# and so does an entry of the index that is not a version or not a string;
# each runs in a cmake process of its own (WXL_TEST_STEP), since a stop ends
# the script it happens in.

include("${CMAKE_CURRENT_LIST_DIR}/wxl_nuget_version.cmake")

if(NOT WXL_TEST_DIR)
    message(FATAL_ERROR "nuget-version: WXL_TEST_DIR names no directory to work in.")
endif()

if(WXL_TEST_STEP STREQUAL "range")
    wxl_nuget_version_mismatch(mismatch "2.5.4" "[2.5.1, 3.0.0)")
    return()
elseif(WXL_TEST_STEP STREQUAL "held")
    wxl_nuget_newest_of_major(newest "[\"1.4.0\", \"1.5.0\"]" "[1.4.0]")
    return()
elseif(WXL_TEST_STEP STREQUAL "entry")
    wxl_nuget_newest_of_major(newest "[\"2.6.0\", \"2.6.4.x\", \"2.7.0\"]" "2.5.1"
                              "https://example.org/flat/test.package/index.json")
    return()
elseif(WXL_TEST_STEP STREQUAL "number")
    wxl_nuget_newest_of_major(newest "[\"2.6.0\", 27]" "2.5.1"
                              "https://example.org/flat/test.package/index.json")
    return()
endif()

file(REMOVE_RECURSE "${WXL_TEST_DIR}")
file(MAKE_DIRECTORY "${WXL_TEST_DIR}")

function(_wxl_test_expect what actual expected)
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "nuget-version: ${what}:\n  got      ${actual}\n  expected ${expected}")
    endif()
endfunction()

# Each argument a version, in ascending order: every one below the next, above
# the one before, and equal to itself.
function(_wxl_test_ascending)
    math(EXPR last "${ARGC} - 1")
    foreach(at RANGE 1 ${last})
        math(EXPR before "${at} - 1")
        wxl_nuget_version_compare(order "${ARGV${before}}" "${ARGV${at}}")
        _wxl_test_expect("${ARGV${before}} against ${ARGV${at}}" "${order}" "-1")
        wxl_nuget_version_compare(order "${ARGV${at}}" "${ARGV${before}}")
        _wxl_test_expect("${ARGV${at}} against ${ARGV${before}}" "${order}" "1")
    endforeach()
    foreach(version IN LISTS ARGV)
        wxl_nuget_version_compare(order "${version}" "${version}")
        _wxl_test_expect("${version} against itself" "${order}" "0")
    endforeach()
endfunction()

function(_wxl_test_equal a b)
    wxl_nuget_version_compare(order "${a}" "${b}")
    _wxl_test_expect("${a} against ${b}" "${order}" "0")
    wxl_nuget_version_compare(order "${b}" "${a}")
    _wxl_test_expect("${b} against ${a}" "${order}" "0")
endfunction()

# What NuGet writes reads as a version.
foreach(text
        "1" "1.2" "1.2.3" "1.2.3.4" "2.5.4-experimental" "2.0.5-experimental2"
        "10.0.26100.4654" "2.0.250303.1" "1.0.0-rc.1+build.5" "1.0.0+20261028"
        "1.0.0-x-y.z--1" "01.002.0003" "2147483647.0" "1.0.0-0" "1.0.0-rc.0"
        "1.0.0-0a" "1.0.0+001" "1.0.0-rc.99999999999999999999")
    wxl_nuget_version_parse(version "${text}")
    _wxl_test_expect("${text} read as a version" "${version_VALID}" "TRUE")
endforeach()

# What is not a version does not.
foreach(text
        "" "1.2.3.4.5" "1..2" ".1" "1." "a" "1.a" "1.0-" "1.0-a..b" "1.0-a." "1.0+"
        "1.0+a..b" "1.0.0-a_b" " 1.0" "1.0 " "-1.0" "[1.0]" "1.0,2.0" "1.0.0-é"
        "99999999999999999999999.0" "2147483648.0" "1.2147483648" "1.0.0-rc.01"
        "1.0.0-00" "1.0.0-007" "2.*" "[2.5.4-experimental]")
    wxl_nuget_version_parse(version "${text}")
    _wxl_test_expect("\"${text}\" read as a version" "${version_VALID}" "FALSE")
endforeach()
# A ; is a character of the text, which a CMake list would take for a
# separator.
set(text "1.0;2.0")
wxl_nuget_version_parse(version "${text}")
_wxl_test_expect("\"${text}\" read as a version" "${version_VALID}" "FALSE")

# The parts read.
wxl_nuget_version_parse(version "02.5.4-Experimental.3+build")
_wxl_test_expect("the numbers of 02.5.4-Experimental.3+build" "${version_NUMBERS}" "2;5;4;0")
_wxl_test_expect("its label" "${version_LABEL}" "Experimental;3")
_wxl_test_expect("its channel" "${version_CHANNEL}" "experimental")
wxl_nuget_version_parse(version "2.6.4")
_wxl_test_expect("the numbers of 2.6.4" "${version_NUMBERS}" "2;6;4;0")
_wxl_test_expect("its label" "${version_LABEL}" "")
_wxl_test_expect("its channel" "${version_CHANNEL}" "")

# The channel is the word the label begins with: the letters before the first
# character that is not one.
foreach(case
        "2.5.4-experimental=experimental" "2.2.2-experimental9=experimental"
        "2.3.2-experimentala=experimentala" "2.0.0-preview1=preview" "2.0.0-Preview2=preview"
        "1.0.0-rc.1=rc" "2.0.250930001-experimental1=experimental" "1.0.0-0a=" "2.5.1=")
    string(REGEX MATCH "^([^=]+)=(.*)$" case "${case}")
    set(text "${CMAKE_MATCH_1}")
    set(word "${CMAKE_MATCH_2}")
    wxl_nuget_version_parse(version "${text}")
    _wxl_test_expect("the channel of ${text}" "${version_CHANNEL}" "${word}")
endforeach()

# The numeric parts compare as numbers; a part not written is 0, and a leading
# zero changes nothing.
_wxl_test_ascending("1.9.0" "1.10.0")
_wxl_test_ascending("2.0.250303" "2.0.250303.1")
_wxl_test_ascending("10.0.26100.0" "10.0.26100.4654")
_wxl_test_ascending("2.5.1" "2.6.4" "2147483647.0")
_wxl_test_equal("1" "1.0.0.0")
_wxl_test_equal("1.0" "1.0.0")
_wxl_test_equal("01.002.0003" "1.2.3")

# A release is above its prereleases.
_wxl_test_ascending("2.5.1" "2.5.3" "2.5.4-experimental" "2.5.4" "2.6.4")
_wxl_test_ascending("1.0.0" "1.0.0.1-rc" "1.0.0.1")
_wxl_test_ascending("1.0.0-rc.1" "1.0.0-rc.2" "1.0.0")
_wxl_test_ascending("2.0.0-experimental2" "2.0.0-experimental7" "2.0.0-preview1" "2.0.1")

# The order of the SemVer 2.0 specification, which NuGet keeps.
_wxl_test_ascending("1.0.0-alpha" "1.0.0-alpha.1" "1.0.0-alpha.beta" "1.0.0-beta" "1.0.0-beta.2"
                    "1.0.0-beta.11" "1.0.0-rc.1" "1.0.0")

# A numeric identifier compares as a number and below text; Int32.TryParse
# takes a '-' as the sign, so the identifier -1 is the number -1.
_wxl_test_ascending("1.0.0-rc.2" "1.0.0-rc.10")
_wxl_test_ascending("1.0.0-rc.3" "1.0.0-rc.2147483647")
_wxl_test_ascending("1.0.0-1" "1.0.0-a")
_wxl_test_ascending("1.0.0-999" "1.0.0-0a")
_wxl_test_ascending("1.0.0--1" "1.0.0-0")

# A run of digits beyond Int32 compares as text.
_wxl_test_ascending("1.0.0-rc.3" "1.0.0-rc.2147483648")
_wxl_test_ascending("1.0.0-rc.10000000000" "1.0.0-rc.2147483648")
_wxl_test_ascending("1.0.0-rc.123456789012345678901234567890" "1.0.0-rc.99999999999999999999")

# Text compares as text, ignoring case: experimental10 is below experimental2.
_wxl_test_equal("1.0.0-RC.1" "1.0.0-rc.1")
_wxl_test_equal("2.5.4-Experimental" "2.5.4-experimental")
_wxl_test_ascending("2.0.5-experimental10" "2.0.5-experimental2")
_wxl_test_ascending("2.0.5-experimental" "2.0.5-experimental2")
_wxl_test_ascending("1.0.0-A" "1.0.0-b")
_wxl_test_ascending("1.0.0-a" "1.0.0-B")
_wxl_test_ascending("1.0.0--" "1.0.0-0a")

# Build metadata is not compared.
_wxl_test_equal("1.0.0+a" "1.0.0+b")
_wxl_test_equal("1.0.0+a" "1.0.0")
_wxl_test_equal("1.0.0-rc+x.1" "1.0.0-rc")
_wxl_test_ascending("1.0.0-rc+zzz" "1.0.0+aaa")

# The forms of a pin: a minimum, one version in brackets, and a range, which
# is no pin.
foreach(case
        "2.5.4-experimental=minimum=2.5.4-experimental" "1.4.0=minimum=1.4.0"
        "[1.4.0]=exact=1.4.0" "[2.5.4-experimental]=exact=2.5.4-experimental"
        "[1.4.0, 2.0.0)=range=" "[1.4.0,1.4.0]=range=" "(1.4.0, )=range=" "[1.4.0, )=range="
        "(1.4.0)=range=" "1.4.0,=range=" "[1.x]=invalid=" "[]=invalid=" "2.*=invalid=")
    string(REGEX MATCH "^(.*)=([a-z]+)=(.*)$" case "${case}")
    set(text "${CMAKE_MATCH_1}")
    set(form "${CMAKE_MATCH_2}")
    set(version "${CMAKE_MATCH_3}")
    wxl_nuget_pin(pin "${text}")
    _wxl_test_expect("the form of the pin ${text}" "${pin_FORM}" "${form}")
    _wxl_test_expect("the version of the pin ${text}" "${pin_VERSION}" "${version}")
endforeach()

# What the helper project references: the floating version of the major of a
# stable minimum, a held pin as it is written, and nothing for a prerelease
# minimum, whose version is chosen.
foreach(case
        "1.4.0=1.*" "02.0.250303.1=2.*" "1.0.4078.44=1.*" "[1.4.0]=[1.4.0]"
        "[2.5.4-experimental]=[2.5.4-experimental]" "2.5.4-experimental=")
    string(REGEX MATCH "^([^=]+)=(.*)$" case "${case}")
    set(text "${CMAKE_MATCH_1}")
    set(expected "${CMAKE_MATCH_2}")
    wxl_nuget_pin_reference(reference "${text}")
    _wxl_test_expect("the reference of the pin ${text}" "${reference}" "${expected}")
endforeach()

# The requested range NuGet writes into a lock file for a reference: a floating
# version as it is, one version as [X, X], compared by NuGet's rules.
foreach(case
        "1.*|1.*|TRUE" "2.*|2.*|TRUE" "1.*|[1.4.0, )|FALSE" "1.*|2.*|FALSE"
        "[2.5.4-experimental]|[2.5.4-experimental, 2.5.4-experimental]|TRUE"
        "[2.5.4-experimental]|[2.5.4-Experimental, 2.5.4-experimental]|TRUE"
        "[1.4.0]|[1.4.0, 1.4.0]|TRUE" "[1.4.0]|[1.4.0.0, 1.4]|TRUE"
        "[1.4.0]|[1.4.0, )|FALSE" "[1.4.0]|[1.4.0, 1.5.0]|FALSE" "[1.4.0]|[1.4.0]|FALSE"
        "[2.5.4-experimental]|[2.4.1-experimental, 2.4.1-experimental]|FALSE"
        "[1.4.0]|1.*|FALSE")
    # Split by hand: a [ in a CMake list holds the ; after it.
    string(REGEX MATCH "^([^|]*)\\|([^|]*)\\|([^|]*)$" case "${case}")
    set(reference "${CMAKE_MATCH_1}")
    set(requested "${CMAKE_MATCH_2}")
    set(expected "${CMAKE_MATCH_3}")
    wxl_nuget_requested_matches(matches "${reference}" "${requested}")
    _wxl_test_expect("${requested} as the request of ${reference}" "${matches}" "${expected}")
endforeach()

# What keeps a pin from admitting a version. A prerelease pin admits the labels
# that begin with its word, so that experimentala and experimental9 are on the
# channel experimental, as the Windows App SDK names its releases.
foreach(case
        "2.5.4-experimental 2.5.4-experimental ok"
        "2.6.0-Experimental3 2.5.4-experimental ok"
        "2.3.2-experimentala 2.3.0-experimental ok"
        "2.2.2-experimental9 2.2.0-experimental ok"
        "2.5.4-experimental 2.3.2-experimentala channel"
        "2.6.0-pre 2.5.4-preview channel"
        "2.6.0-x.experimental 2.5.4-experimental channel"
        "2.5.4 2.5.1 ok"
        "2.5.3-experimental 2.5.4-experimental below"
        "2.5.0 2.5.1 below"
        "1.9.0 2.5.1 major"
        "3.0.0-experimental1 2.5.4-experimental major"
        "2.6.4 2.5.4-experimental channel"
        "2.6.0-preview1 2.5.4-experimental channel"
        "2.6.0-experimental1 2.5.1 channel"
        "2.6.0-1 2.5.1 channel"
        "2.6 2.5.1 ok"
        "2.6.x 2.5.1 invalid"
        "1.4.0 [1.4.0] ok"
        "1.4.0.0+build [1.4.0] ok"
        "1.4.1 [1.4.0] exact"
        "1.3.9 [1.4.0] exact"
        "1.4.0-rc [1.4.0] exact"
        "2.5.4-Experimental [2.5.4-experimental] ok"
        "1.x [1.4.0] invalid")
    string(REPLACE " " ";" case "${case}")
    list(GET case 0 version)
    list(GET case 1 minimum)
    list(GET case 2 expected)
    if(expected STREQUAL "ok")
        set(expected "")
    endif()
    wxl_nuget_version_mismatch(mismatch "${version}" "${minimum}")
    _wxl_test_expect("what keeps ${minimum} from admitting ${version}" "${mismatch}" "${expected}")
endforeach()

# The newest version of the major on the channel, in the index of
# Microsoft.WindowsAppSDK as nuget.org lists it, ascending, with 1.7.0 and
# 3.0.0-experimental1 around the 2.x versions and a version nuget.org would
# not list.
set(index [=[{
  "versions": [
    "1.7.0",
    "2.0.0-experimental2",
    "2.0.0-experimental3",
    "2.0.0-experimental4",
    "2.0.0-experimental5",
    "2.0.0-experimental6",
    "2.0.0-experimental7",
    "2.0.0-preview1",
    "2.0.0-preview2",
    "2.0.1",
    "2.0.250930001-experimental1",
    "2.1.3",
    "2.1.4-experimental8",
    "2.2.0",
    "2.2.2-experimental9",
    "2.3.1",
    "2.3.2-experimentala",
    "2.4.0",
    "2.4.1-experimental",
    "2.5.1",
    "2.5.4-experimental",
    "2.6.4",
    "3.0.0-experimental1"
  ]
}]=])
string(JSON versions GET "${index}" versions)
foreach(case
        "2.5.4-experimental 2.5.4-experimental"
        "2.4.1-experimental 2.5.4-experimental"
        "2.0.0-experimental2 2.5.4-experimental"
        "2.5.1 2.6.4"
        "1.0.0 1.7.0"
        "2.0.0-preview1 2.0.0-preview2"
        "3.0.0-experimental1 3.0.0-experimental1"
        "2.7.0 none"
        "2.6.0-experimental none"
        "2.0.0-rc.1 none"
        "4.0.0 none")
    string(REPLACE " " ";" case "${case}")
    list(GET case 0 minimum)
    list(GET case 1 expected)
    if(expected STREQUAL "none")
        set(expected "")
    endif()
    wxl_nuget_newest_of_major(newest "${versions}" "${minimum}")
    _wxl_test_expect("the newest version ${minimum} admits" "${newest}" "${expected}")
endforeach()
wxl_nuget_newest_of_major(newest "[]" "2.5.1")
_wxl_test_expect("the newest version of an empty index" "${newest}" "")

# A range stops the comparison, and a held pin the search of the index, each
# with what the pin has to be.
foreach(case
        "range=is neither a minimum NuGet version nor exactly one version in brackets"
        "held=holds one version, and a held pin is not searched"
        "entry=index.json lists 2.6.4.x (entry 1, a JSON STRING), which is not the string of a"
        "number=index.json lists 27 (entry 1, a JSON NUMBER), which is not the string of a")
    string(REGEX MATCH "^([^=]+)=(.*)$" case "${case}")
    set(step "${CMAKE_MATCH_1}")
    set(part "${CMAKE_MATCH_2}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" "-DWXL_TEST_DIR=${WXL_TEST_DIR}" "-DWXL_TEST_STEP=${step}"
                -P "${CMAKE_CURRENT_LIST_FILE}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE output)
    if(result EQUAL 0)
        message(FATAL_ERROR "nuget-version: the step ${step} did not stop.")
    endif()
    string(REPLACE "\n" " " output "${output}")
    string(REGEX REPLACE " +" " " output "${output}")
    string(FIND "${output}" "${part}" at)
    if(at EQUAL -1)
        message(FATAL_ERROR
            "nuget-version: no \"${part}\" in what the step ${step} said:\n${output}")
    endif()
endforeach()

message(STATUS "nuget-version: passed")

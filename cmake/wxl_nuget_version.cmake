# NuGet versions, ordered as NuGet orders them, and the versions a pin of
# tools/packages.json admits: functions only, which cmake/wxl_packages.cmake
# calls and cmake/wxl_nuget_version_test.cmake tests.
#
# A version is one to four numeric parts separated by dots, each an Int32 and a
# leading zero allowed; then optionally '-' and a prerelease label of
# dot-separated identifiers of [0-9A-Za-z-], none of them a number with a
# leading zero; then optionally '+' and build metadata of identifiers of the
# same characters, which no comparison looks at. The order is NuGet's: the
# numeric parts as numbers, a part not written being 0; a version with a label
# below the same numbers without one; labels identifier by identifier, where an
# identifier that Int32.TryParse takes is a number, two numbers compare as
# numbers, a number is below any text, and text compares character by
# character in upper case; a label that is a prefix of the other one is below
# it. These are the rules of nuget_version in wxl.gen.common/profile.cpp, by
# which the generator compares the versions a profile needs with the restored
# ones; the two are kept the same.
#
# A pin is either a minimum version or exactly one version in brackets, [X],
# NuGet's exact range. A held pin admits that version only. A minimum admits
# the versions that have the major version of the minimum, are not below it
# and are on its channel. A stable minimum (no label) is on the stable
# channel, which admits the versions without a label. A prerelease minimum
# names its channel by the word its label begins with, the letters before the
# first character that is not one, and admits the prerelease versions whose
# label begins with that word, ignoring case -- what NuGet's floating label
# <word>* matches. The Windows App SDK counts the releases of a channel in a
# character after the word, a digit or a letter: 2.2.2-experimental9,
# 2.3.2-experimentala and 2.5.4-experimental are all on the channel of
# 2.5.4-experimental, "experimental". So the minimum of a channel is written
# with the bare word of the channel, as 2.5.4-experimental is; a minimum whose
# label carries a release suffix (2.3.2-experimentala, 2.2.2-experimental9)
# names that one suffix as its word and admits none of the other releases of
# the channel.
#
# Defined here:
#
#   wxl_nuget_pin(<prefix> <text>)
#       sets <prefix>_FORM to what the pin <text> is: "minimum" (a version),
#       "exact" (one version in brackets), "range" (any other text with a
#       bracket, a parenthesis or a comma, which NuGet reads as a range) or
#       "invalid"; and <prefix>_VERSION to the minimum or the held version.
#
#   wxl_nuget_pin_reference(<variable> <pin>)
#       sets <variable> to the version the helper project references for the
#       pin <pin> when the pin alone gives it: M.* for a stable minimum of major
#       M, NuGet's floating version of the stable versions of M, and [X] for a
#       pin held at X. Empty for a prerelease minimum, whose exact version is
#       taken from the lock file or chosen out of the index of the source.
#
#   wxl_nuget_requested_matches(<variable> <reference> <requested>)
#       sets <variable> to TRUE when <requested>, the range a lock file records
#       as requested for a reference, is the one NuGet writes for <reference>,
#       and to FALSE when it is not: M.* as [M.*, ) (or M.*), [X] as [X, X],
#       the versions compared by NuGet's rules.
#
#   wxl_nuget_version_parse(<prefix> <text>)
#       sets <prefix>_VALID to TRUE when <text> is a version and to FALSE when
#       it is not, and for a version <prefix>_NUMBERS (the four numeric parts,
#       without leading zeros), <prefix>_LABEL (the identifiers of the label as
#       a list, empty for a stable version) and <prefix>_CHANNEL (the word of
#       the label in lower case, empty for a stable version).
#
#   wxl_nuget_version_compare(<variable> <a> <b>)
#       sets <variable> to -1, 0 or 1 as the version <a> is below, equal to or
#       above <b>. Either one not a version stops the configure.
#
#   wxl_nuget_version_mismatch(<variable> <version> <pin>)
#       sets <variable> to what keeps the pin <pin> from admitting <version>:
#       "invalid" (not a version), "exact" (not the version a held pin names),
#       "major", "channel" or "below"; empty when the pin admits it. A pin
#       that is neither a minimum nor one held version stops the configure.
#
#   wxl_nuget_newest_of_major(<variable> <versions> <minimum> [<source>])
#       sets <variable> to the highest of the versions the JSON array
#       <versions> lists that the minimum <minimum> admits, empty when there
#       is none. The array is "versions" of the index of a package in NuGet's
#       flat container (<flat container>/<id in lower case>/index.json), and
#       <source>, its URL, is named in the messages. An entry that is not a
#       string or not a version stops the configure: the choice would be made
#       over a list this file does not read whole. A held pin is not searched,
#       and passing one stops the configure.

include_guard(GLOBAL)

# The Int32 that `text` spells, as Int32.TryParse reads it -- digits, after a
# '-' for a negative number -- written without leading zeros; empty when the
# text is not one or the number is beyond the range of Int32. The range is
# checked on the digits, since math() would wrap a longer run of them.
function(_wxl_nuget_int32 out text)
    set(number "")
    if(text MATCHES "^(-?)0*([0-9]+)$")
        set(sign "${CMAKE_MATCH_1}")
        set(digits "${CMAKE_MATCH_2}")
        set(limit "2147483647")
        if(sign STREQUAL "-")
            set(limit "2147483648")
        endif()
        string(LENGTH "${digits}" length)
        if(length LESS 10 OR (length EQUAL 10 AND NOT digits STRGREATER limit))
            set(number "${sign}${digits}")
        endif()
    endif()
    set(${out} "${number}" PARENT_SCOPE)
endfunction()

function(wxl_nuget_version_parse prefix text)
    set(${prefix}_VALID FALSE PARENT_SCOPE)
    set(${prefix}_NUMBERS "" PARENT_SCOPE)
    set(${prefix}_LABEL "" PARENT_SCOPE)
    set(${prefix}_CHANNEL "" PARENT_SCOPE)

    set(identifiers "^[0-9A-Za-z-]+(\\.[0-9A-Za-z-]+)*$")
    string(FIND "${text}" "+" plus)
    if(NOT plus EQUAL -1)
        math(EXPR after "${plus} + 1")
        string(SUBSTRING "${text}" ${after} -1 metadata)
        if(NOT metadata MATCHES "${identifiers}")
            return()
        endif()
        string(SUBSTRING "${text}" 0 ${plus} text)
    endif()

    # The numeric parts have no '-', so the first one begins the label.
    set(release "${text}")
    set(label "")
    string(FIND "${text}" "-" dash)
    if(NOT dash EQUAL -1)
        string(SUBSTRING "${text}" 0 ${dash} release)
        math(EXPR after "${dash} + 1")
        string(SUBSTRING "${text}" ${after} -1 label_text)
        if(NOT label_text MATCHES "${identifiers}")
            return()
        endif()
        string(REPLACE "." ";" label "${label_text}")
        foreach(identifier IN LISTS label)
            if(identifier MATCHES "^0[0-9]+$")
                return()
            endif()
        endforeach()
    endif()

    if(NOT release MATCHES "^[0-9]+(\\.[0-9]+)?(\\.[0-9]+)?(\\.[0-9]+)?$")
        return()
    endif()
    string(REPLACE "." ";" parts "${release}")
    set(numbers "")
    foreach(part IN LISTS parts)
        _wxl_nuget_int32(number "${part}")
        if(number STREQUAL "")
            return()
        endif()
        list(APPEND numbers "${number}")
    endforeach()
    list(LENGTH numbers count)
    while(count LESS 4)
        list(APPEND numbers 0)
        math(EXPR count "${count} + 1")
    endwhile()

    set(channel "")
    if(NOT label STREQUAL "")
        list(GET label 0 first)
        if(first MATCHES "^([A-Za-z]+)")
            string(TOLOWER "${CMAKE_MATCH_1}" channel)
        endif()
    endif()

    set(${prefix}_VALID TRUE PARENT_SCOPE)
    set(${prefix}_NUMBERS "${numbers}" PARENT_SCOPE)
    set(${prefix}_LABEL "${label}" PARENT_SCOPE)
    set(${prefix}_CHANNEL "${channel}" PARENT_SCOPE)
endfunction()

function(wxl_nuget_pin prefix text)
    set(form "invalid")
    set(version "")
    if(text MATCHES "^\\[([^],]*)\\]$")
        set(held "${CMAKE_MATCH_1}")
        wxl_nuget_version_parse(exact "${held}")
        if(exact_VALID)
            set(form "exact")
            set(version "${held}")
        endif()
    elseif(text MATCHES "[][(),]")
        set(form "range")
    else()
        wxl_nuget_version_parse(minimum "${text}")
        if(minimum_VALID)
            set(form "minimum")
            set(version "${text}")
        endif()
    endif()
    set(${prefix}_FORM "${form}" PARENT_SCOPE)
    set(${prefix}_VERSION "${version}" PARENT_SCOPE)
endfunction()

# The pin `pin`, read into the variables of the prefix `prefix` of the caller:
# its form and version as wxl_nuget_pin() sets them, and the parts of the
# version as wxl_nuget_version_parse() does. A pin that is neither stops the
# configure, since the pins are checked before anything compares a version
# with them.
macro(_wxl_nuget_read_pin prefix pin)
    wxl_nuget_pin(${prefix} "${pin}")
    if(NOT ${prefix}_FORM MATCHES "^(minimum|exact)$")
        message(FATAL_ERROR
            "The pin \"${pin}\" is neither a minimum NuGet version nor exactly one version in "
            "brackets.")
    endif()
    wxl_nuget_version_parse(${prefix} "${${prefix}_VERSION}")
endmacro()

function(wxl_nuget_pin_reference out pin)
    _wxl_nuget_read_pin(wanted "${pin}")
    set(reference "")
    if(wanted_FORM STREQUAL "exact")
        set(reference "[${wanted_VERSION}]")
    elseif(wanted_LABEL STREQUAL "")
        list(GET wanted_NUMBERS 0 major)
        set(reference "${major}.*")
    endif()
    set(${out} "${reference}" PARENT_SCOPE)
endfunction()

# The order of two versions wxl_nuget_version_parse() read into the variables
# of the prefixes `a` and `b` of the caller: -1, 0 or 1.
function(_wxl_nuget_order out a b)
    foreach(at RANGE 3)
        list(GET ${a}_NUMBERS ${at} x)
        list(GET ${b}_NUMBERS ${at} y)
        if(x LESS y)
            set(${out} -1 PARENT_SCOPE)
            return()
        elseif(x GREATER y)
            set(${out} 1 PARENT_SCOPE)
            return()
        endif()
    endforeach()

    set(x_label "${${a}_LABEL}")
    set(y_label "${${b}_LABEL}")
    if(x_label STREQUAL "" AND y_label STREQUAL "")
        set(${out} 0 PARENT_SCOPE)
        return()
    elseif(x_label STREQUAL "")
        set(${out} 1 PARENT_SCOPE)
        return()
    elseif(y_label STREQUAL "")
        set(${out} -1 PARENT_SCOPE)
        return()
    endif()

    list(LENGTH x_label x_count)
    list(LENGTH y_label y_count)
    set(count ${x_count})
    if(y_count LESS count)
        set(count ${y_count})
    endif()
    math(EXPR last "${count} - 1")
    foreach(at RANGE ${last})
        list(GET x_label ${at} x)
        list(GET y_label ${at} y)
        _wxl_nuget_int32(x_number "${x}")
        _wxl_nuget_int32(y_number "${y}")
        if(NOT x_number STREQUAL "" AND NOT y_number STREQUAL "")
            set(x "${x_number}")
            set(y "${y_number}")
            set(less LESS)
            set(greater GREATER)
        elseif(NOT x_number STREQUAL "")
            set(${out} -1 PARENT_SCOPE)
            return()
        elseif(NOT y_number STREQUAL "")
            set(${out} 1 PARENT_SCOPE)
            return()
        else()
            # Upper case, as OrdinalIgnoreCase orders ASCII; STRLESS orders
            # byte by byte, a prefix below the longer text.
            string(TOUPPER "${x}" x)
            string(TOUPPER "${y}" y)
            set(less STRLESS)
            set(greater STRGREATER)
        endif()
        if(x ${less} y)
            set(${out} -1 PARENT_SCOPE)
            return()
        elseif(x ${greater} y)
            set(${out} 1 PARENT_SCOPE)
            return()
        endif()
    endforeach()

    if(x_count LESS y_count)
        set(${out} -1 PARENT_SCOPE)
    elseif(x_count GREATER y_count)
        set(${out} 1 PARENT_SCOPE)
    else()
        set(${out} 0 PARENT_SCOPE)
    endif()
endfunction()

function(wxl_nuget_version_compare out a b)
    wxl_nuget_version_parse(left "${a}")
    wxl_nuget_version_parse(right "${b}")
    if(NOT left_VALID OR NOT right_VALID)
        message(FATAL_ERROR "wxl_nuget_version_compare: \"${a}\" or \"${b}\" is not a NuGet version.")
    endif()
    _wxl_nuget_order(order left right)
    set(${out} ${order} PARENT_SCOPE)
endfunction()

# What keeps the pin the caller read into the variables of the prefix
# `minimum` from admitting the version read into those of `version`, as
# wxl_nuget_version_mismatch() names it.
function(_wxl_nuget_mismatch out version minimum)
    if(NOT ${version}_VALID)
        set(${out} "invalid" PARENT_SCOPE)
        return()
    endif()
    if(${minimum}_FORM STREQUAL "exact")
        _wxl_nuget_order(order ${version} ${minimum})
        if(order EQUAL 0)
            set(${out} "" PARENT_SCOPE)
        else()
            set(${out} "exact" PARENT_SCOPE)
        endif()
        return()
    endif()
    list(GET ${version}_NUMBERS 0 x)
    list(GET ${minimum}_NUMBERS 0 y)
    if(NOT x EQUAL y)
        set(${out} "major" PARENT_SCOPE)
        return()
    endif()
    set(x_stable FALSE)
    set(y_stable FALSE)
    if("${${version}_LABEL}" STREQUAL "")
        set(x_stable TRUE)
    endif()
    if("${${minimum}_LABEL}" STREQUAL "")
        set(y_stable TRUE)
    endif()
    if(NOT x_stable STREQUAL y_stable)
        set(${out} "channel" PARENT_SCOPE)
        return()
    endif()
    if(NOT y_stable)
        # The label begins with the word of the minimum. The word of the
        # version is not compared with it: the counter after the word may be a
        # letter, as in experimentala.
        list(GET ${version}_LABEL 0 first)
        string(TOLOWER "${first}" first)
        string(FIND "${first}" "${${minimum}_CHANNEL}" at)
        if(NOT at EQUAL 0)
            set(${out} "channel" PARENT_SCOPE)
            return()
        endif()
    endif()
    _wxl_nuget_order(order ${version} ${minimum})
    if(order LESS 0)
        set(${out} "below" PARENT_SCOPE)
    else()
        set(${out} "" PARENT_SCOPE)
    endif()
endfunction()

function(wxl_nuget_version_mismatch out version pin)
    _wxl_nuget_read_pin(pin "${pin}")
    wxl_nuget_version_parse(candidate "${version}")
    _wxl_nuget_mismatch(mismatch candidate pin)
    set(${out} "${mismatch}" PARENT_SCOPE)
endfunction()

function(wxl_nuget_newest_of_major out versions minimum)
    set(source "the versions")
    if(ARGC GREATER 3)
        set(source "${ARGV3}")
    endif()
    _wxl_nuget_read_pin(pin "${minimum}")
    if(pin_FORM STREQUAL "exact")
        message(FATAL_ERROR
            "wxl_nuget_newest_of_major: ${minimum} holds one version, and a held pin is not "
            "searched.")
    endif()
    string(JSON type ERROR_VARIABLE error TYPE "${versions}")
    if(error OR NOT type STREQUAL "ARRAY")
        message(FATAL_ERROR "${source}: the versions are not a JSON array: ${versions}")
    endif()
    string(JSON count LENGTH "${versions}")
    set(newest "")
    if(count GREATER 0)
        math(EXPR last "${count} - 1")
        foreach(at RANGE ${last})
            string(JSON type TYPE "${versions}" ${at})
            string(JSON text GET "${versions}" ${at})
            wxl_nuget_version_parse(candidate "${text}")
            if(NOT type STREQUAL "STRING" OR NOT candidate_VALID)
                message(FATAL_ERROR
                    "${source} lists ${text} (entry ${at}, a JSON ${type}), which is not the "
                    "string of a NuGet version, so the newest version ${minimum} admits cannot be "
                    "chosen out of it. Check "
                    "WXL_NUGET_FLAT_CONTAINER, which has to name the flat container of a NuGet "
                    "source.")
            endif()
            _wxl_nuget_mismatch(mismatch candidate pin)
            if(NOT mismatch STREQUAL "")
                continue()
            endif()
            # The index lists the versions in ascending order; they are compared
            # all the same, so that the result does not depend on it.
            if(newest STREQUAL "")
                set(order 1)
            else()
                _wxl_nuget_order(order candidate best)
            endif()
            if(order GREATER 0)
                set(newest "${text}")
                set(best_NUMBERS "${candidate_NUMBERS}")
                set(best_LABEL "${candidate_LABEL}")
            endif()
        endforeach()
    endif()
    set(${out} "${newest}" PARENT_SCOPE)
endfunction()

function(wxl_nuget_requested_matches out reference requested)
    set(matches FALSE)
    if(reference MATCHES "^\\[([^],]+)\\]$")
        wxl_nuget_version_parse(held "${CMAKE_MATCH_1}")
        if(requested MATCHES "^\\[([^],]+), ([^],]+)\\]$")
            wxl_nuget_version_parse(lower "${CMAKE_MATCH_1}")
            wxl_nuget_version_parse(upper "${CMAKE_MATCH_2}")
            if(held_VALID AND lower_VALID AND upper_VALID)
                _wxl_nuget_order(first lower held)
                _wxl_nuget_order(second upper held)
                if(first EQUAL 0 AND second EQUAL 0)
                    set(matches TRUE)
                endif()
            endif()
        endif()
    elseif(requested STREQUAL "[${reference}, )" OR reference STREQUAL requested)
        # NuGet writes a floating reference M.* as the range [M.*, ): a
        # restore wrote [1.*, ) and [2.*, ) for 1.* and 2.*. The bare M.* is
        # taken as well, in case another NuGet writes that.
        set(matches TRUE)
    endif()
    set(${out} ${matches} PARENT_SCOPE)
endfunction()

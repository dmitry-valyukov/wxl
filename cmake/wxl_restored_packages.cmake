# The result of the NuGet restore of cmake/wxl_packages.cmake, the list
# written from it and the entries its lock file records: functions only, which
# that file calls and cmake/wxl_restored_packages_test.cmake tests on a restore
# result and a lock file of its own.
#
# The list, <build>/wxl.packages/restored-packages.json:
#
#   {
#       "packages": [
#           { "id": "<id>", "version": "<version>", "directory": "<directory>" },
#           ...
#       ],
#       "windowsSdk": {
#           "version": "<version>",
#           "metadata": [ "<file>.winmd", ... ]
#       }
#   }
#
# with every package of obj/project.assets.json -- its id and version as the
# restore names it, the directory it is in -- and the Windows SDK of the
# developer environment with the .winmd files the projection of the platform
# was made from (the -in entries of its response file). Paths are absolute,
# with / as the separator, and every string is written as JSON escapes it.
# winui-srcgen and wxl.gen.ui take the list with --packages and read every
# package at the version listed and the Windows metadata from those files. The
# list is written only when its text changes, so that a configure that changes
# nothing touches no file.

include_guard(GLOBAL)

# Reads the restore result: the package folders and the version and directory
# of every package, which wxl_nuget_package_dir() and the list of the restore
# give out. The folders are read out of the JSON where they are needed, not
# kept as a CMake list, so that a ; in one is a character like any other.
function(_wxl_packages_read_assets assets out_count)
    if(NOT EXISTS "${assets}")
        message(FATAL_ERROR "The restore wrote no ${assets}.")
    endif()
    file(READ "${assets}" json)
    foreach(member packageFolders libraries)
        string(JSON type ERROR_VARIABLE error TYPE "${json}" ${member})
        if(error OR NOT type STREQUAL "OBJECT")
            message(FATAL_ERROR "${assets} has no object \"${member}\".")
        endif()
    endforeach()
    string(JSON folders GET "${json}" packageFolders)
    string(JSON libraries GET "${json}" libraries)
    string(JSON folder_count LENGTH "${folders}")
    string(JSON library_count LENGTH "${libraries}")
    if(folder_count EQUAL 0 OR library_count EQUAL 0)
        message(FATAL_ERROR "${assets} names no package folder or no package.")
    endif()

    math(EXPR last_folder "${folder_count} - 1")

    # A member of "libraries" is named <id>/<version>, and its "path" is the
    # directory of the package relative to one of the package folders.
    set(count 0)
    math(EXPR last "${library_count} - 1")
    foreach(i RANGE ${last})
        string(JSON key MEMBER "${libraries}" ${i})
        string(JSON entry GET "${libraries}" "${key}")
        # A project reference has no package directory; the helper project has
        # none, and any other type is not one this file knows how to read.
        string(JSON type ERROR_VARIABLE error GET "${entry}" type)
        if(error)
            message(FATAL_ERROR "${assets}: the entry \"${key}\" has no readable type: ${error}")
        endif()
        if(type STREQUAL "project")
            continue()
        endif()
        if(NOT type STREQUAL "package")
            message(FATAL_ERROR "${assets}: the entry \"${key}\" is of type \"${type}\", neither a package nor a project.")
        endif()
        string(JSON path ERROR_VARIABLE error GET "${entry}" path)
        string(FIND "${key}" "/" slash)
        if(error OR slash LESS 1)
            message(FATAL_ERROR "${assets}: the package \"${key}\" has no path, or its name is not <id>/<version>.")
        endif()
        string(SUBSTRING "${key}" 0 ${slash} id)
        math(EXPR after "${slash} + 1")
        string(SUBSTRING "${key}" ${after} -1 version)

        set(dir "")
        foreach(f RANGE ${last_folder})
            string(JSON root MEMBER "${folders}" ${f})
            cmake_path(SET root "${root}")
            cmake_path(APPEND root "${path}" OUTPUT_VARIABLE candidate)
            if(IS_DIRECTORY "${candidate}")
                set(dir "${candidate}")
                break()
            endif()
        endforeach()
        if(dir STREQUAL "")
            message(FATAL_ERROR "${assets} names ${key} at ${path}, which is in none of its package folders (${folders}).")
        endif()

        string(TOLOWER "${id}" lower)
        set_property(GLOBAL PROPERTY _WXL_NUGET_PACKAGE_${lower} "${dir}")
        set_property(GLOBAL PROPERTY _WXL_NUGET_PACKAGE_ID_${lower} "${id}")
        set_property(GLOBAL PROPERTY _WXL_NUGET_PACKAGE_VERSION_${lower} "${version}")
        set_property(GLOBAL APPEND PROPERTY _WXL_NUGET_PACKAGES "${lower}")
        math(EXPR count "${count} + 1")
    endforeach()

    set_property(GLOBAL PROPERTY _WXL_NUGET_ASSETS "${assets}")
    set(${out_count} ${count} PARENT_SCOPE)
endfunction()

# What keeps the text `json` from being a lock file the configure reads, in a
# message; empty when it is one: an object "dependencies" whose first member,
# the section of the target framework, is an object. The sections of the
# runtime identifiers, named <framework>/<runtime>, follow that one.
function(_wxl_packages_lock_problem out json)
    set(problem "")
    string(JSON type ERROR_VARIABLE error TYPE "${json}" dependencies)
    if(error OR NOT type STREQUAL "OBJECT")
        set(problem "is not JSON with an object \"dependencies\"")
    else()
        string(JSON framework ERROR_VARIABLE error MEMBER "${json}" dependencies 0)
        if(error)
            set(problem "names no target framework in \"dependencies\"")
        else()
            string(JSON type TYPE "${json}" dependencies "${framework}")
            if(NOT type STREQUAL "OBJECT")
                set(problem "has no object for the target framework \"${framework}\"")
            endif()
        endif()
    endif()
    set(${out} "${problem}" PARENT_SCOPE)
endfunction()

# The direct entries of the lock file `lock`, of the text `json`, in the
# section of its target framework, as variables of the prefix `prefix` of the
# caller: <prefix>_IDS, the ids in lower case, and per id <lower>
# <prefix>_ID_<lower> (as the file writes it), <prefix>_REQUESTED_<lower> (the
# range the project asked for) and <prefix>_RESOLVED_<lower>. The ids are
# matched ignoring case, as NuGet matches them. A file that is not one, or an
# entry without what a direct entry has, stops the configure with the remedy.
function(_wxl_packages_read_lock prefix lock json)
    string(CONCAT remedy
        "Take tools/packages.lock.json back from git, or delete it and run the update, "
        "tools\\build.ps1 -UpdatePackages, which writes it anew.")
    _wxl_packages_lock_problem(problem "${json}")
    if(NOT problem STREQUAL "")
        message(FATAL_ERROR "The lock file ${lock} ${problem}. ${remedy}")
    endif()
    string(JSON framework MEMBER "${json}" dependencies 0)
    string(JSON section GET "${json}" dependencies "${framework}")
    string(JSON count LENGTH "${section}")
    set(ids "")
    if(count GREATER 0)
        math(EXPR last "${count} - 1")
        foreach(at RANGE ${last})
            string(JSON id MEMBER "${section}" ${at})
            string(JSON type ERROR_VARIABLE error GET "${section}" "${id}" type)
            if(error OR NOT type STREQUAL "Direct")
                continue()
            endif()
            string(JSON requested ERROR_VARIABLE error GET "${section}" "${id}" requested)
            string(JSON resolved ERROR_VARIABLE missing GET "${section}" "${id}" resolved)
            if(error OR missing OR NOT id MATCHES "^[A-Za-z0-9._-]+$")
                message(FATAL_ERROR
                    "The lock file ${lock} has a direct entry \"${id}\" in \"${framework}\" that "
                    "is not a package id with a \"requested\" range and a \"resolved\" version. "
                    "${remedy}")
            endif()
            string(TOLOWER "${id}" lower)
            list(APPEND ids "${lower}")
            set(${prefix}_ID_${lower} "${id}" PARENT_SCOPE)
            set(${prefix}_REQUESTED_${lower} "${requested}" PARENT_SCOPE)
            set(${prefix}_RESOLVED_${lower} "${resolved}" PARENT_SCOPE)
        endforeach()
    endif()
    set(${prefix}_IDS "${ids}" PARENT_SCOPE)
endfunction()

# Text as a JSON string, quotes included: a \ and a " escaped. A control
# character, which neither a Windows path nor a NuGet id or version holds,
# stops the configure rather than being written unescaped.
function(_wxl_packages_json_string out text)
    string(ASCII 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31
           controls)
    if(text MATCHES "[${controls}]")
        message(FATAL_ERROR "\"${text}\" has a control character, which the list of the restore does not write.")
    endif()
    string(REPLACE "\\" "\\\\" text "${text}")
    string(REPLACE "\"" "\\\"" text "${text}")
    set(${out} "\"${text}\"" PARENT_SCOPE)
endfunction()

# Writes the list of the restore (see the top of this file) into `file`: the
# packages _wxl_packages_read_assets() read, by id, and the Windows SDK `sdk`
# with the -in entries of `platform_response`, the response file of the
# platform projection. The entries are taken out of the text one at a time, not
# as a CMake list, so that a ; in a path is a character like any other.
function(_wxl_packages_write_list file sdk platform_response)
    cmake_path(GET platform_response PARENT_PATH int)
    file(READ "${platform_response}" rest)
    set(metadata "")
    set(separator "")
    while(rest MATCHES "-in \"([^\"]+)\"")
        set(input "${CMAKE_MATCH_0}")
        cmake_path(SET winmd "${CMAKE_MATCH_1}")
        string(FIND "${rest}" "${input}" at)
        string(LENGTH "${input}" length)
        math(EXPR at "${at} + ${length}")
        string(SUBSTRING "${rest}" ${at} -1 rest)

        string(TOLOWER "${winmd}" lower)
        if(NOT lower MATCHES "\\.winmd$" OR IS_DIRECTORY "${winmd}" OR NOT EXISTS "${winmd}")
            message(FATAL_ERROR
                "${platform_response} names ${winmd}, which is not a .winmd file on disk. MSBuild "
                "skips the projection targets while their response files in ${int} are newer than "
                "their inputs, so a response file can name a file that is gone: delete ${int} and "
                "configure again.")
        endif()
        _wxl_packages_json_string(json "${winmd}")
        string(APPEND metadata "${separator}            ${json}")
        set(separator ",\n")
    endwhile()
    if(metadata STREQUAL "")
        message(FATAL_ERROR
            "${platform_response} names no Windows SDK metadata (-in): delete ${int} and configure "
            "again.")
    endif()

    get_property(packages GLOBAL PROPERTY _WXL_NUGET_PACKAGES)
    list(SORT packages)
    set(entries "")
    set(separator "")
    foreach(lower IN LISTS packages)
        get_property(id GLOBAL PROPERTY _WXL_NUGET_PACKAGE_ID_${lower})
        get_property(version GLOBAL PROPERTY _WXL_NUGET_PACKAGE_VERSION_${lower})
        get_property(dir GLOBAL PROPERTY _WXL_NUGET_PACKAGE_${lower})
        _wxl_packages_json_string(id "${id}")
        _wxl_packages_json_string(version "${version}")
        _wxl_packages_json_string(dir "${dir}")
        string(APPEND entries
            "${separator}        { \"id\": ${id}, \"version\": ${version}, \"directory\": ${dir} }")
        set(separator ",\n")
    endforeach()

    _wxl_packages_json_string(sdk "${sdk}")
    string(CONCAT text
        "{\n"
        "    \"packages\": [\n${entries}\n    ],\n"
        "    \"windowsSdk\": {\n"
        "        \"version\": ${sdk},\n"
        "        \"metadata\": [\n${metadata}\n        ]\n"
        "    }\n"
        "}\n")

    set(written "")
    if(EXISTS "${file}")
        file(READ "${file}" written)
    endif()
    if(NOT written STREQUAL text)
        file(WRITE "${file}" "${text}")
    endif()
endfunction()

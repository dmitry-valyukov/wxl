# The NuGet packages the tree builds against and the C++/WinRT projection over
# them, both made at configure by the MSBuild of the developer environment.
#
# Including this file writes a helper project, <build>/wxl.packages/
# wxl.packages.vcxproj (from wxl_packages.vcxproj.in), and runs on it
#
#   MSBuild.exe wxl.packages.vcxproj -restore -t:ResolveReferences,CppWinRTMakeProjections
#
# -restore restores the PackageReference items of the project first: the
# "packages" of tools/packages.json, among them the umbrella package
# Microsoft.WindowsAppSDK of the Windows App SDK release the tree builds
# against, whose dependencies are the packages of that release. The targets are
# those of the Microsoft.Windows.CppWinRT package.
# ResolveReferences adds the metadata of the Windows SDK to the references:
# CppWinRTImplicitlyExpandTargetPlatform is in ResolveReferencesDependsOn, and
# CppWinRTMakeProjections alone does not run it, which leaves the projection of
# the platform without input. CppWinRTMakeProjections then writes the
# projection of the Windows SDK and of the packages into
# <build>/wxl.packages/projection.
#
# Both run at configure rather than as build steps, because the configure needs
# the directories of the packages. MSBuild skips a restore that is up to date
# and the targets whose outputs are up to date, so a repeated configure stays
# short.
#
# Each version of tools/packages.json is a minimum, and the restore takes the
# newest version of the minimum's major version on the minimum's channel --
# cmake/wxl_nuget_version.cmake says which versions a minimum admits. The
# major version is raised only by hand, by editing the minimum. The lock file
# tools/packages.lock.json records the versions the restore took, and a newer
# version comes only with an update, which writes that file anew:
#
#   - a stable minimum M.m.p is referenced as M.*, NuGet's floating version of
#     the stable versions of major M;
#   - a prerelease minimum is referenced as the exact version [X]. NuGet has no
#     floating version of one channel: M.*-<word>* takes the stable versions as
#     well, and a stable version orders above every prerelease of the same or
#     lower numbers. X is the version the lock file records for the pin. The
#     update chooses it anew, and so does an unlocked configure for a pin the
#     file does not record: the newest version the minimum admits in the index
#     of the package in the flat container of the NuGet source,
#     WXL_NUGET_FLAT_CONTAINER.
#
# After the restore the version of every pin is checked against its minimum: of
# another major version, on another channel or below the minimum, it stops the
# configure with what to do. An ordinary configure reads the lock file once,
# and while the file records every prerelease pin it downloads no index; a
# prerelease pin the file does not record yet is resolved through the source as
# the update resolves it, unless the restore is locked, which stops on it.
#
# The update is a configure with WXL_UPDATE_PACKAGES=ON, as tools\build.ps1
# -UpdatePackages runs it. It chooses the versions of the prerelease pins
# anew, restores with RestoreForceEvaluate=true, so that NuGet resolves the
# floating versions anew instead of taking those of the lock file, prints every
# pin with its version before and after, and removes WXL_UPDATE_PACKAGES from
# the cache when it succeeds: the next configure is an ordinary one. The
# rewritten lock file is then committed. A locked configure and the configure
# of a project that adds wxl refuse the update.
#
# What the restore resolved, and the Windows SDK of the projection, are then
# written into the list <build>/wxl.packages/restored-packages.json, which
# winui-srcgen and wxl.gen.ui take with --packages. Its format, the reading of
# the restore result and the writing of the list are in
# cmake/wxl_restored_packages.cmake.
#
# A project that adds wxl -- as a subdirectory or through FetchContent --
# restores in locked mode: its configure resolves the versions the lock file of
# the wxl tree records and does not write that file.
#
# Defined here:
#
#   WXL_UPDATE_PACKAGES
#       a cache option, OFF by default; ON makes one configure the update.
#
#   WXL_NUGET_FLAT_CONTAINER
#       a cache variable, the flat container of the NuGet source the update
#       chooses the versions of prerelease pins from, by default nuget.org's,
#       https://api.nuget.org/v3-flatcontainer. The restore takes the packages
#       from the sources of the NuGet configuration, and the two have to hold
#       the same versions.
#
#   Microsoft::CppWinRT
#       an INTERFACE IMPORTED GLOBAL target over the projection, the C++/WinRT
#       headers of the Windows SDK and of the packages.
#
#   wxl_nuget_package_dir(<variable> <package id>)
#       sets <variable> to the directory of a package the restore resolved, as
#       obj/project.assets.json of the helper project names it. A package the
#       restore did not resolve stops the configure.
#
#   wxl_restored_packages_file(<variable>)
#       sets <variable> to the path of the list of the restore,
#       <build>/wxl.packages/restored-packages.json.
#
# What this relies on -- the developer environment, the component of the
# installation, the Windows SDK, the restore result, the response files the
# CppWinRT targets write and the headers of the projection -- is checked here,
# and a failed check stops the configure.

include_guard(GLOBAL)

set(_WXL_PACKAGES_SCRIPT_DIR "${CMAKE_CURRENT_LIST_DIR}")
cmake_path(GET _WXL_PACKAGES_SCRIPT_DIR PARENT_PATH _WXL_PACKAGES_TREE)

include("${_WXL_PACKAGES_SCRIPT_DIR}/wxl_nuget_version.cmake")
include("${_WXL_PACKAGES_SCRIPT_DIR}/wxl_restored_packages.cmake")

# A one-shot switch, unset by a successful update (above); a failed update
# leaves it set, and the next configure tries again.
option(WXL_UPDATE_PACKAGES
    "Update the NuGet packages once, rewriting tools/packages.lock.json; unset when done" OFF)
set(WXL_NUGET_FLAT_CONTAINER "https://api.nuget.org/v3-flatcontainer" CACHE STRING
    "The NuGet flat container the update chooses the versions of prerelease pins from")

# The oldest Windows SDK the tree builds against. The developer environment
# selects the newest installed one, and the configure stops when that is older.
set(_WXL_PACKAGES_WINDOWS_SDK_MINIMUM 10.0.26100.0)

string(CONCAT _WXL_PACKAGES_INSTALL_HINT
    "Visual Studio 2026: the workload \"WinUI application development\" "
    "(Microsoft.VisualStudio.Workload.Universal) with its component \"C++ WinUI app development "
    "tools\" (Microsoft.VisualStudio.Component.WindowsAppSdkSupport.Cpp). Build Tools for Visual "
    "Studio 2026: the workload \"WinUI application development build tools\" "
    "(Microsoft.VisualStudio.Workload.UniversalBuildTools) with \"C++ WinUI app development build "
    "tools\" (Microsoft.VisualStudio.ComponentGroup.WindowsAppDevelopment.VC.BuildTools).")

# The developer environment the configure runs in: VsDevCmd.bat, which
# vcvars64.bat and tools/build.ps1 run, sets VSINSTALLDIR, VCToolsVersion and
# WindowsSDKVersion. The installation it names has to carry MSBuild and the
# C++ WinUI tools; its Windows SDK has to be the minimum or newer.
function(_wxl_packages_environment out_msbuild out_sdk out_vc_tools)
    if("$ENV{VSINSTALLDIR}" STREQUAL "")
        message(FATAL_ERROR
            "No Visual Studio developer environment: VSINSTALLDIR is not set. Configure the tree "
            "from a Developer Command Prompt or Developer PowerShell (VsDevCmd.bat), or through "
            "tools\\build.ps1, which sets that environment up itself. The installation needs the "
            "C++ WinUI tools -- ${_WXL_PACKAGES_INSTALL_HINT}")
    endif()
    file(TO_CMAKE_PATH "$ENV{VSINSTALLDIR}" installation)
    string(REGEX REPLACE "/+$" "" installation "${installation}")

    set(msbuild "${installation}/MSBuild/Current/Bin/amd64/MSBuild.exe")
    if(NOT EXISTS "${msbuild}")
        message(FATAL_ERROR
            "No MSBuild at ${msbuild}, in the Visual Studio installation of the developer "
            "environment (VSINSTALLDIR). The configure restores the NuGet packages and builds the "
            "C++/WinRT projection with it. Install the C++ WinUI tools into that installation -- "
            "${_WXL_PACKAGES_INSTALL_HINT}")
    endif()

    # vswhere does not combine -path with -requires, so it lists the
    # installations that carry one of the two IDs and the list is searched for
    # this one.
    set(program_files_x86 "ProgramFiles(x86)")
    file(TO_CMAKE_PATH "$ENV{${program_files_x86}}/Microsoft Visual Studio/Installer/vswhere.exe" vswhere)
    if(NOT EXISTS "${vswhere}")
        message(FATAL_ERROR
            "No vswhere.exe at ${vswhere}. It comes with the Visual Studio Installer, and the "
            "configure asks it whether ${installation} carries the C++ WinUI tools.")
    endif()
    execute_process(
        COMMAND "${vswhere}" -nologo -prerelease -products * -requiresAny
                -requires Microsoft.VisualStudio.Component.WindowsAppSdkSupport.Cpp
                          Microsoft.VisualStudio.ComponentGroup.WindowsAppDevelopment.VC.BuildTools
                -property installationPath
        RESULT_VARIABLE result
        OUTPUT_VARIABLE listed
        ERROR_VARIABLE listed)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "${vswhere} failed (${result}):\n${listed}")
    endif()
    string(TOLOWER "${installation}" wanted)
    string(REPLACE "\r" "" listed "${listed}")
    string(REPLACE "\n" ";" listed "${listed}")
    set(has_component FALSE)
    foreach(path IN LISTS listed)
        file(TO_CMAKE_PATH "${path}" path)
        string(REGEX REPLACE "/+$" "" path "${path}")
        string(TOLOWER "${path}" path)
        if(path STREQUAL wanted)
            set(has_component TRUE)
        endif()
    endforeach()
    if(NOT has_component)
        message(FATAL_ERROR
            "The Visual Studio installation ${installation} of the developer environment has no C++ "
            "WinUI tools, which the restore of the NuGet packages and the C++/WinRT projection "
            "need. Install them with the Visual Studio Installer -- ${_WXL_PACKAGES_INSTALL_HINT}")
    endif()

    string(REGEX REPLACE "[\\/]+$" "" sdk "$ENV{WindowsSDKVersion}")
    if(NOT sdk MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+\\.[0-9]+$")
        message(FATAL_ERROR
            "The developer environment names no Windows SDK (WindowsSDKVersion is "
            "\"$ENV{WindowsSDKVersion}\"). Install Windows SDK ${_WXL_PACKAGES_WINDOWS_SDK_MINIMUM} "
            "or newer: the Visual Studio Installer component \"Windows 11 SDK\" 10.0.26100 "
            "(Microsoft.VisualStudio.Component.Windows11SDK.26100).")
    endif()
    if(sdk VERSION_LESS _WXL_PACKAGES_WINDOWS_SDK_MINIMUM)
        message(FATAL_ERROR
            "The newest Windows SDK of the developer environment is ${sdk}; the tree needs "
            "${_WXL_PACKAGES_WINDOWS_SDK_MINIMUM} or newer. Install the Visual Studio Installer "
            "component \"Windows 11 SDK\" 10.0.26100 (Microsoft.VisualStudio.Component.Windows11SDK.26100) "
            "or a newer Windows SDK, then configure from a new developer environment.")
    endif()

    set(vc_tools "$ENV{VCToolsVersion}")
    if(NOT vc_tools MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
        message(FATAL_ERROR
            "The developer environment names no MSVC toolset (VCToolsVersion is \"${vc_tools}\"). "
            "Configure from a Developer Command Prompt or through tools\\build.ps1.")
    endif()

    set(${out_msbuild} "${msbuild}" PARENT_SCOPE)
    set(${out_sdk} "${sdk}" PARENT_SCOPE)
    set(${out_vc_tools} "${vc_tools}" PARENT_SCOPE)
endfunction()

# The pins as pairs of a package id and its minimum version: the "packages" of
# tools/packages.json, an input of the configure.
function(_wxl_packages_pins out_pins)
    set(manifest_file "${_WXL_PACKAGES_TREE}/tools/packages.json")
    file(READ "${manifest_file}" manifest)
    string(JSON count ERROR_VARIABLE error LENGTH "${manifest}" packages)
    if(error)
        message(FATAL_ERROR "${manifest_file} has no list \"packages\": ${error}")
    endif()
    if(count EQUAL 0)
        message(FATAL_ERROR "${manifest_file} lists no packages.")
    endif()
    set(pins "")
    math(EXPR last "${count} - 1")
    foreach(i RANGE ${last})
        string(JSON id GET "${manifest}" packages ${i} id)
        string(JSON version GET "${manifest}" packages ${i} version)
        list(APPEND pins "${id}" "${version}")
    endforeach()

    set(has_cppwinrt FALSE)
    list(LENGTH pins length)
    math(EXPR last "${length} - 1")
    foreach(i RANGE 0 ${last} 2)
        math(EXPR j "${i} + 1")
        list(GET pins ${i} id)
        list(GET pins ${j} version)
        # Written into the project as they are: neither may carry what XML or
        # MSBuild reads as syntax.
        if(NOT id MATCHES "^[A-Za-z0-9._-]+$" OR NOT version MATCHES "^[A-Za-z0-9.+-]+$")
            message(FATAL_ERROR "Not a NuGet package id and version: \"${id}\" \"${version}\" (${manifest_file}).")
        endif()
        wxl_nuget_version_parse(minimum "${version}")
        if(NOT minimum_VALID)
            message(FATAL_ERROR "${manifest_file} pins ${id} at \"${version}\", which is not a NuGet version.")
        endif()
        if(NOT minimum_LABEL STREQUAL "" AND minimum_CHANNEL STREQUAL "")
            message(FATAL_ERROR
                "${manifest_file} pins ${id} at ${version}, a prerelease whose label begins with "
                "no letter. A prerelease minimum names its channel by the word its label begins with, "
                "as 2.5.4-experimental names the channel \"experimental\", and the restore takes "
                "the versions of that channel only.")
        endif()
        if(id STREQUAL "Microsoft.Windows.CppWinRT")
            set(has_cppwinrt TRUE)
        endif()
    endforeach()
    if(NOT has_cppwinrt)
        message(FATAL_ERROR
            "${manifest_file} pins no Microsoft.Windows.CppWinRT: the projection is built by the "
            "targets of that package.")
    endif()

    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${manifest_file}")
    set(${out_pins} "${pins}" PARENT_SCOPE)
endfunction()

# A path the helper project names in an element. MSBuild reads % $ @ ; ' as
# syntax in a property, and XML reads & < > and the quote.
function(_wxl_packages_check_path what path)
    if(path MATCHES "[%$@;'\"&<>]")
        message(FATAL_ERROR
            "${what} ${path} has a character that MSBuild or XML reads as syntax (% $ @ ; ' \" & < >), "
            "so the helper project of cmake/wxl_packages.cmake cannot name it. Use a path without one.")
    endif()
endfunction()

# The channel of the minimum `minimum`, in a message.
function(_wxl_packages_channel out minimum)
    wxl_nuget_version_parse(pin "${minimum}")
    if(pin_LABEL STREQUAL "")
        set(${out} "the stable versions" PARENT_SCOPE)
    else()
        set(${out} "the prereleases whose label begins with \"${pin_CHANNEL}\"" PARENT_SCOPE)
    endif()
endfunction()

# The version of the pin `id` that the lock file records or the restore resolved
# (`what` says which), checked against the pin's minimum. A version the
# minimum does not admit stops the configure with the remedy; `updating` says
# whether this configure is the update, which takes the newest versions the
# NuGet sources hold, so that the update is not the remedy.
function(_wxl_packages_check_version what id version minimum updating)
    wxl_nuget_version_mismatch(mismatch "${version}" "${minimum}")
    if(mismatch STREQUAL "")
        return()
    endif()

    set(pinned "${id} ${version}, which the minimum ${minimum} of tools/packages.json does not admit")
    set(own "")
    if(mismatch STREQUAL "invalid")
        set(cause "${id} ${version}, which is not a NuGet version.")
    elseif(mismatch STREQUAL "below")
        set(cause "${pinned}: it is below the minimum.")
        set(own "lower the minimum")
    elseif(mismatch STREQUAL "major")
        wxl_nuget_version_parse(pin "${minimum}")
        list(GET pin_NUMBERS 0 major)
        set(cause "${pinned}: its major version is not ${major}.")
        string(CONCAT own
            "edit the minimum by hand to one of the major version you mean to take, since the "
            "major version is raised only by hand")
    else()
        _wxl_packages_channel(channel "${minimum}")
        set(cause "${pinned}: it is not on the channel of the minimum, ${channel}.")
        set(own "write a minimum on the channel you mean to take")
    endif()

    if(updating)
        string(CONCAT remedy
            "The update takes the newest versions the NuGet sources hold, so they hold none that "
            "the minimum admits: check the minimum and the NuGet sources of the restore")
    else()
        string(CONCAT remedy
            "The lock file tools/packages.lock.json does not match the pins. Run the update, "
            "tools\\build.ps1 -UpdatePackages (a configure with -DWXL_UPDATE_PACKAGES=ON), and "
            "commit the lock file it writes")
    endif()
    if(NOT own STREQUAL "")
        string(APPEND remedy ", or ${own}")
    endif()
    message(FATAL_ERROR "${what} ${cause} ${remedy}.")
endfunction()

# The newest version of the prerelease pin `id` that its minimum admits, out of
# the index of the package in the flat container of the NuGet source, which is
# downloaded into the directory `dir`. The index lists every version the source
# ever published, unlisted ones included, without saying which are unlisted;
# NuGet's floating references resolve over the same list, and the newest
# version of it is taken as they take it. What the update chose is reviewed in
# the versions it prints and in the change of the lock file before it is
# committed. A failed download, or an index without such a version, stops the
# configure.
function(_wxl_packages_newest out id minimum dir)
    wxl_nuget_version_parse(pin "${minimum}")
    list(GET pin_NUMBERS 0 major)
    set(mask "${major}.*-${pin_CHANNEL}*")
    string(TOLOWER "${id}" lower)
    string(REGEX REPLACE "/+$" "" container "${WXL_NUGET_FLAT_CONTAINER}")
    set(url "${container}/${lower}/index.json")
    set(index_file "${dir}/${lower}.index.json")
    file(DOWNLOAD "${url}" "${index_file}" STATUS status TLS_VERIFY ON)
    list(GET status 0 code)
    if(NOT code EQUAL 0)
        list(GET status 1 reason)
        message(FATAL_ERROR
            "The configure could not download the versions of ${id} from ${url} (${code}, "
            "${reason}). The update, and an unlocked configure for a pin the lock file does not "
            "record, take the version of a prerelease pin (${minimum}: the newest of ${mask} that "
            "is not below the minimum) from the index of the package in the flat container of the "
            "NuGet source, WXL_NUGET_FLAT_CONTAINER (${WXL_NUGET_FLAT_CONTAINER}). Check the network and "
            "that variable, then configure again.")
    endif()
    file(READ "${index_file}" index)
    string(JSON type ERROR_VARIABLE error TYPE "${index}" versions)
    if(error OR NOT type STREQUAL "ARRAY")
        message(FATAL_ERROR
            "${url} has no array \"versions\": it is not the index of a package in a NuGet flat "
            "container. Check WXL_NUGET_FLAT_CONTAINER (${WXL_NUGET_FLAT_CONTAINER}).")
    endif()
    string(JSON versions GET "${index}" versions)
    wxl_nuget_newest_of_major(newest "${versions}" "${minimum}")
    if(newest STREQUAL "")
        # The index lists the versions in ascending order: the last ones are
        # the newest.
        string(JSON count LENGTH "${versions}")
        set(seen "")
        set(separator "")
        set(first 0)
        if(count GREATER 5)
            math(EXPR first "${count} - 5")
        endif()
        if(count GREATER 0)
            math(EXPR last "${count} - 1")
            foreach(at RANGE ${first} ${last})
                string(JSON version GET "${versions}" ${at})
                string(APPEND seen "${separator}${version}")
                set(separator ", ")
            endforeach()
        endif()
        message(FATAL_ERROR
            "${url} lists no version of ${id} that the minimum ${minimum} of tools/packages.json "
            "admits: one of ${mask}, the major version ${major} on the channel \"${pin_CHANNEL}\", "
            "that is not below the minimum. The newest versions it lists: ${seen}. Write a minimum "
            "that a version of the source meets into tools/packages.json, or check "
            "WXL_NUGET_FLAT_CONTAINER, then configure again.")
    endif()
    set(${out} "${newest}" PARENT_SCOPE)
endfunction()

# The response file of a projection target and what it has to name: inputs of
# the expected kind, and the projection directory as the output.
function(_wxl_packages_check_response file projection_native)
    if(NOT EXISTS "${file}")
        message(FATAL_ERROR
            "The CppWinRT targets wrote no ${file}: the projection was not built where "
            "cmake/wxl_packages.cmake expects its response files.")
    endif()
    file(READ "${file}" text)
    string(FIND "${text}" "-out \"${projection_native}\\.\"" out)
    if(out EQUAL -1)
        message(FATAL_ERROR
            "${file} does not write the projection into ${projection_native}: GeneratedFilesDir of "
            "the helper project did not reach the CppWinRT targets.")
    endif()
    if(text MATCHES "-fastabi")
        message(FATAL_ERROR
            "${file} asks cppwinrt for -fastabi, which changes the headers and needs "
            "cppwinrt_fast_forwarder.lib; the projection of the tree is made without it.")
    endif()
    foreach(option IN LISTS ARGN)
        if(NOT text MATCHES "${option} \"[^\"]+\\.winmd\"")
            message(FATAL_ERROR "${file} passes no ${option} metadata to cppwinrt.")
        endif()
    endforeach()
endfunction()

function(_wxl_packages)
    if(TARGET Microsoft::CppWinRT)
        message(FATAL_ERROR
            "A target Microsoft::CppWinRT exists already, before wxl defines its own over the "
            "projection of cmake/wxl_packages.cmake (vcpkg's cppwinrt package, for one, defines a "
            "target of that name). Two projections in one build are an error. Remove the other one "
            "(find_package(cppwinrt) and its package) and link wxl's Microsoft::CppWinRT.")
    endif()

    # The top-level source directory is the wxl tree itself (its root, or
    # wxl.ui configured on its own), or a project that adds wxl as a
    # subdirectory or through FetchContent.
    cmake_path(IS_PREFIX _WXL_PACKAGES_TREE "${CMAKE_SOURCE_DIR}" NORMALIZE wxl_is_top_level)

    # The tree is configured without a toolchain file. A cache that still names
    # vcpkg's would load it into every configure of the build directory.
    if(wxl_is_top_level AND CMAKE_TOOLCHAIN_FILE MATCHES "vcpkg\\.cmake$")
        message(FATAL_ERROR
            "CMAKE_TOOLCHAIN_FILE names vcpkg's toolchain (${CMAKE_TOOLCHAIN_FILE}), which the wxl "
            "tree does not use: the cache of ${CMAKE_BINARY_DIR} keeps it from an earlier "
            "configure. Configure the build directory anew, with an empty cache: "
            "cmake --preset x64 --fresh in a developer prompt, or delete the directory and run "
            "tools\\build.ps1 -Configure.")
    endif()

    _wxl_packages_environment(msbuild sdk vc_tools)
    _wxl_packages_pins(pins)

    set(helper "${CMAKE_BINARY_DIR}/wxl.packages")
    set(project "${helper}/wxl.packages.vcxproj")
    set(int "${helper}/int")
    set(projection "${helper}/projection")
    set(log "${helper}/msbuild.log")
    set(lock "${_WXL_PACKAGES_TREE}/tools/packages.lock.json")
    _wxl_packages_check_path("The build directory" "${helper}")
    _wxl_packages_check_path("The lock file" "${lock}")

    # Locked mode. NuGet reads RestoreLockedMode as an MSBuild property: from the
    # environment, as on CI, or from the command line. With the lock file there,
    # a locked restore resolves the versions the file records and stops when the
    # references do not match it; without the file NuGet resolves the versions
    # anew and writes one, so a locked configure without the file stops here. A
    # project that adds wxl restores in locked mode always, so that its configure
    # does not rewrite the lock file the wxl tree tracks.
    string(TOLOWER "$ENV{RestoreLockedMode}" locked)
    set(lock_arguments "")
    if(NOT wxl_is_top_level)
        set(locked "true")
        set(lock_arguments "-p:RestoreLockedMode=true")
    endif()
    if(locked STREQUAL "true" AND NOT EXISTS "${lock}")
        if(wxl_is_top_level)
            message(FATAL_ERROR
                "RestoreLockedMode is true in the environment, and there is no lock file ${lock}. "
                "A locked restore without the file resolves the versions anew and writes it instead "
                "of checking them. Commit the lock file that the update writes, "
                "tools\\build.ps1 -UpdatePackages without RestoreLockedMode.")
        else()
            message(FATAL_ERROR
                "${CMAKE_SOURCE_DIR} adds the wxl tree ${_WXL_PACKAGES_TREE}, which has no lock file "
                "${lock}. The configure of a project that adds wxl restores the packages of wxl in "
                "locked mode, so that it does not rewrite the lock file of the wxl tree, and the "
                "file has to be there: the update of the wxl tree on its own writes it, "
                "tools\\build.ps1 -UpdatePackages.")
        endif()
    endif()

    # The update rewrites the lock file, which neither a locked restore nor the
    # restore of a project that adds wxl does.
    set(update_arguments "")
    if(WXL_UPDATE_PACKAGES)
        if(NOT wxl_is_top_level)
            message(FATAL_ERROR
                "WXL_UPDATE_PACKAGES is ON in ${CMAKE_SOURCE_DIR}, which adds the wxl tree "
                "${_WXL_PACKAGES_TREE}. The update writes the lock file of the wxl tree anew, and "
                "the configure of a project that adds wxl restores in locked mode so as not to "
                "change it: run the update in the wxl tree on its own, tools\\build.ps1 "
                "-UpdatePackages, and configure this project with WXL_UPDATE_PACKAGES=OFF.")
        endif()
        if(locked STREQUAL "true")
            message(FATAL_ERROR
                "WXL_UPDATE_PACKAGES is ON, and RestoreLockedMode is true in the environment. The "
                "update writes the lock file anew, which a locked restore does not do: run the "
                "update from an environment without RestoreLockedMode.")
        endif()
        # NuGet takes a floating version from the lock file while the
        # references match it; RestoreForceEvaluate resolves it anew.
        set(update_arguments "-p:RestoreForceEvaluate=true")
        message(STATUS "NuGet packages: the update, WXL_UPDATE_PACKAGES=ON")
    endif()

    # The lock file, read once: the version of a prerelease pin, and on the
    # update the versions before it.
    set(lock_json "")
    if(EXISTS "${lock}")
        file(READ "${lock}" lock_json)
    endif()

    # A stable minimum is referenced as the floating version of its major
    # version, a prerelease one as the exact version that the lock file records
    # or the update chooses (see the top of this file). Either is made of what
    # a NuGet version is written in, as wxl_nuget_version_parse() checks it.
    set(WXL_PACKAGES_REFERENCES "")
    list(LENGTH pins length)
    math(EXPR last "${length} - 1")
    foreach(i RANGE 0 ${last} 2)
        math(EXPR j "${i} + 1")
        list(GET pins ${i} id)
        list(GET pins ${j} minimum)
        wxl_nuget_version_parse(pin "${minimum}")
        set(locked_version "")
        if(NOT lock_json STREQUAL "" AND (WXL_UPDATE_PACKAGES OR NOT pin_LABEL STREQUAL ""))
            _wxl_packages_locked_version(locked_version "${lock}" "${lock_json}" "${id}")
        endif()
        if(pin_LABEL STREQUAL "")
            list(GET pin_NUMBERS 0 major)
            set(version "${major}.*")
        elseif(WXL_UPDATE_PACKAGES)
            _wxl_packages_newest(chosen "${id}" "${minimum}" "${helper}/index")
            set(version "[${chosen}]")
        elseif(locked_version STREQUAL "")
            # A pin the lock file does not record yet, or no lock file: an
            # unlocked restore resolves the pin anew and writes the file, and
            # reaches the network for it anyway, so the version is chosen as
            # the update chooses it. A locked one would stop on the reference.
            if(locked STREQUAL "true")
                message(FATAL_ERROR
                    "The lock file ${lock} records no version of ${id}, which tools/packages.json "
                    "pins at the prerelease minimum ${minimum}. In locked mode "
                    "(RestoreLockedMode=true in the environment, as on CI, or the configure of a "
                    "project that adds wxl) the version of a prerelease pin is the one the lock "
                    "file records: run the update in the wxl tree, tools\\build.ps1 "
                    "-UpdatePackages, and commit the lock file it writes.")
            endif()
            _wxl_packages_newest(chosen "${id}" "${minimum}" "${helper}/index")
            message(STATUS
                "NuGet packages: ${lock} records no version of ${id}; the restore takes ${chosen}, "
                "the newest version the minimum ${minimum} admits.")
            set(version "[${chosen}]")
        else()
            _wxl_packages_check_version("The lock file ${lock} records"
                "${id}" "${locked_version}" "${minimum}" FALSE)
            set(version "[${locked_version}]")
        endif()
        if(WXL_UPDATE_PACKAGES)
            if(locked_version STREQUAL "")
                set(locked_version "none")
            endif()
            set(before_${i} "${locked_version}")
        endif()
        string(APPEND WXL_PACKAGES_REFERENCES
            "    <PackageReference Include=\"${id}\" Version=\"${version}\" />\n")
    endforeach()
    set(WXL_PACKAGES_VC_TOOLS_VERSION "${vc_tools}")
    set(WXL_PACKAGES_WINDOWS_SDK_VERSION "${sdk}")
    file(TO_NATIVE_PATH "${lock}" WXL_PACKAGES_LOCK_FILE)
    file(TO_NATIVE_PATH "${int}" WXL_PACKAGES_INT_DIR)
    file(TO_NATIVE_PATH "${projection}" WXL_PACKAGES_PROJECTION_DIR)
    # configure_file leaves the file alone when its text has not changed. The
    # projection targets take the project file and its imports as inputs
    # ($(MSBuildAllProjects)), so an unchanged project keeps them up to date.
    configure_file("${_WXL_PACKAGES_SCRIPT_DIR}/wxl_packages.vcxproj.in" "${project}" @ONLY)

    # One MSBuild run: -restore restores in an evaluation of its own, then the
    # targets run on the project evaluated anew with the restored packages. The
    # targets are separated by a comma, which MSBuild accepts as well as a
    # semicolon, so that the argument stays one in CMake.
    file(TO_NATIVE_PATH "${project}" project_native)
    file(TO_NATIVE_PATH "${log}" log_native)
    message(STATUS "NuGet packages and the C++/WinRT projection: ${project}")
    execute_process(
        COMMAND "${msbuild}" "${project_native}"
                -restore
                -t:ResolveReferences,CppWinRTMakeProjections
                -nologo -noAutoResponse -nodeReuse:false -verbosity:minimal
                "-fileLoggerParameters:LogFile=${log_native};Verbosity=normal;Encoding=UTF-8"
                ${lock_arguments} ${update_arguments}
        WORKING_DIRECTORY "${helper}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE output)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR
            "MSBuild failed (${result}) on ${project}, restoring the NuGet packages or building the "
            "C++/WinRT projection. In locked mode (RestoreLockedMode=true in the environment, as on "
            "CI, or the configure of a project that adds wxl) the restore stops when "
            "tools/packages.lock.json does not match the pins. The update of the wxl tree, "
            "tools\\build.ps1 -UpdatePackages without locked mode, writes the lock file anew, and "
            "the changed file is committed together with the pins.\n"
            "${output}\nThe whole log: ${log}")
    endif()
    if(output MATCHES "warning")
        message(WARNING "MSBuild on ${project} reported warnings (the whole log: ${log}):\n${output}")
    endif()

    if(NOT EXISTS "${lock}")
        message(FATAL_ERROR
            "The restore of ${project} wrote no lock file ${lock}. Delete ${helper}/obj, which "
            "contains the result of the earlier restore, and configure again.")
    endif()
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${lock}")

    _wxl_packages_read_assets("${helper}/obj/project.assets.json" count)
    foreach(i RANGE 0 ${last} 2)
        math(EXPR j "${i} + 1")
        list(GET pins ${i} id)
        list(GET pins ${j} minimum)
        string(TOLOWER "${id}" lower)
        get_property(restored GLOBAL PROPERTY _WXL_NUGET_PACKAGE_${lower} SET)
        if(NOT restored)
            message(FATAL_ERROR "The restore of ${project} did not resolve ${id}, which the helper project references.")
        endif()
        get_property(version GLOBAL PROPERTY _WXL_NUGET_PACKAGE_VERSION_${lower})
        _wxl_packages_check_version("The restore of ${project} resolved"
            "${id}" "${version}" "${minimum}" "${WXL_UPDATE_PACKAGES}")
        if(WXL_UPDATE_PACKAGES)
            message(STATUS "NuGet update: ${id} ${before_${i}} -> ${version}")
        endif()
    endforeach()

    # The response files are $(IntDir)$(MSBuildProjectFile).cppwinrt_*.rsp: the
    # platform projection reads the Windows SDK, the reference projection reads
    # the packages and refers to the SDK.
    cmake_path(GET project FILENAME project_file)
    _wxl_packages_check_response("${int}/${project_file}.cppwinrt_plat.rsp" "${WXL_PACKAGES_PROJECTION_DIR}" -in)
    _wxl_packages_check_response("${int}/${project_file}.cppwinrt_ref.rsp" "${WXL_PACKAGES_PROJECTION_DIR}" -in -ref)
    set(restored_list "${helper}/restored-packages.json")
    _wxl_packages_write_list("${restored_list}" "${sdk}" "${int}/${project_file}.cppwinrt_plat.rsp")
    set_property(GLOBAL PROPERTY _WXL_RESTORED_PACKAGES "${restored_list}")
    # The response files are the outputs of the projection targets, so MSBuild
    # skips a projection whose response file is newer than its inputs, whether
    # or not the headers are still there.
    foreach(header base.h Windows.Foundation.h Microsoft.UI.Xaml.h)
        if(NOT EXISTS "${projection}/winrt/${header}")
            message(FATAL_ERROR
                "The C++/WinRT projection in ${projection} has no winrt/${header}. MSBuild skips "
                "the projection targets while their response files in ${int} are newer than their "
                "inputs: delete ${int} and configure again.")
        endif()
    endforeach()

    # The headers, and the language level C++/WinRT needs.
    add_library(Microsoft::CppWinRT INTERFACE IMPORTED GLOBAL)
    set_target_properties(Microsoft::CppWinRT PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${projection}"
        INTERFACE_COMPILE_FEATURES cxx_std_17)

    message(STATUS
        "NuGet packages: ${count} restored (${lock}), listed in ${restored_list}; C++/WinRT projection: "
        "${projection}")

    if(WXL_UPDATE_PACKAGES)
        message(STATUS
            "NuGet update: done. Commit ${lock} if it changed; WXL_UPDATE_PACKAGES is removed from "
            "the cache, and the next configure takes the versions it records.")
        unset(WXL_UPDATE_PACKAGES CACHE)
    endif()
endfunction()

function(wxl_nuget_package_dir variable id)
    string(TOLOWER "${id}" lower)
    get_property(restored GLOBAL PROPERTY _WXL_NUGET_PACKAGE_${lower} SET)
    if(NOT restored)
        get_property(assets GLOBAL PROPERTY _WXL_NUGET_ASSETS)
        message(FATAL_ERROR
            "${id} is not among the packages the restore resolved (${assets}). A package reaches the "
            "restore as a pin in tools/packages.json or as a dependency of one.")
    endif()
    get_property(dir GLOBAL PROPERTY _WXL_NUGET_PACKAGE_${lower})
    set(${variable} "${dir}" PARENT_SCOPE)
endfunction()

function(wxl_restored_packages_file variable)
    get_property(restored_list GLOBAL PROPERTY _WXL_RESTORED_PACKAGES)
    set(${variable} "${restored_list}" PARENT_SCOPE)
endfunction()

_wxl_packages()

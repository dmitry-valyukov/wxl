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
# A version of tools/packages.json is either a minimum or exactly one version
# in brackets, [X]. Of a minimum the restore takes the newest version of the
# minimum's major version on the minimum's channel --
# cmake/wxl_nuget_version.cmake says which versions a minimum admits -- and
# the major version is raised only by hand, by editing the minimum. A pin in
# brackets holds its package at that version: a satellite of the release
# (Win2D, WebView2, CppWinRT) moves on its own, and a newer one can need a
# Windows App SDK component newer than the release has, which the Runtime
# package's check of the referenced components then stops the restore on, so
# such a package is held until the release moves. The references:
#
#   - a pin [X] is referenced as it is written, and no configure chooses
#     another version;
#   - a stable minimum M.m.p is referenced as M.*, NuGet's floating version of
#     the stable versions of major M (1.* took WebView2 from 1.0.4078.44 to
#     1.0.4314.35, the newest stable 1.x, and passed over prereleases such as
#     1.0.4312-prerelease);
#   - a prerelease minimum is referenced as the exact version [X]. NuGet has no
#     floating version of one channel: M.*-<word>* takes the stable versions as
#     well, and a stable version orders above every prerelease of the same or
#     lower numbers. X is the version the lock file records for the pin, or the
#     one the update chooses: the newest version the minimum admits in the
#     index of the package in the flat container of the NuGet source,
#     WXL_NUGET_FLAT_CONTAINER.
#
# The lock file tools/packages.lock.json records the versions the restore took,
# and a newer version comes only with the update, which writes that file anew.
# Outside the update that is an invariant: every pin has a direct entry in the
# file, the file has none that is not a pin, and the reference of every pin is
# the one the entry was written for, its "requested" range (M.* as [M.*, ),
# [X] as [X, X]). Otherwise the pins changed -- a pin added or removed, a hold set or
# released, a major version raised -- and NuGet would resolve every package
# anew, floating ones included, and rewrite the file without saying what
# moved; so the configure stops before MSBuild and names the update. The one
# configure that resolves anew without the update is the one with no lock file
# at all, unlocked and of the wxl tree itself: it chooses the prerelease
# versions as the update does, and the restore writes the file.
#
# Before the restore, a lock file newer than the last restore -- one a pull
# brought, its references unchanged -- makes the restore run anew, and after it
# the version of every pin has to be the one the lock file records. Then the
# version of every pin is checked against the pin: another version than the
# one a pin holds, or, of a minimum, a version of another major version, on
# another channel or below the minimum, stops the configure with what to do.
# An ordinary configure reads the lock file once and downloads no index.
#
# The update is a configure with WXL_UPDATE_PACKAGES=ON, as tools\build.ps1
# -UpdatePackages runs it. It chooses the versions of the prerelease minimums
# anew, restores with RestoreForceEvaluate=true, so that NuGet resolves the
# floating versions anew instead of taking those of the lock file, prints every
# pin with its version before and after, and removes WXL_UPDATE_PACKAGES from
# the cache when it succeeds: the next configure is an ordinary one. The
# rewritten lock file is reviewed and committed. A locked configure and the
# configure of a project that adds wxl refuse the update.
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
    set(has_cppwinrt FALSE)
    math(EXPR last "${count} - 1")
    foreach(i RANGE ${last})
        string(JSON id GET "${manifest}" packages ${i} id)
        string(JSON version GET "${manifest}" packages ${i} version)
        # Each is checked before it joins the CMake list of the pins, where a
        # ; or an unbalanced [ would run into the next element. The id is
        # written into the project as it is, and may not carry what XML or
        # MSBuild reads as syntax. The version is written as
        # wxl_nuget_pin_reference() makes it of a pin, of the characters of a
        # NuGet version and brackets.
        if(NOT id MATCHES "^[A-Za-z0-9._-]+$")
            message(FATAL_ERROR "Not a NuGet package id: \"${id}\" (${manifest_file}).")
        endif()
        wxl_nuget_pin(pin "${version}")
        if(pin_FORM STREQUAL "invalid")
            message(FATAL_ERROR "${manifest_file} pins ${id} at \"${version}\", which is not a NuGet version.")
        endif()
        if(pin_FORM STREQUAL "range")
            message(FATAL_ERROR
                "${manifest_file} pins ${id} at \"${version}\", a range. A pin is either a "
                "minimum, a version such as 1.4.0, of which the restore takes the newest version "
                "of the same major version and channel, or exactly one version in brackets, such "
                "as [1.4.0], which holds the package at that version.")
        endif()
        wxl_nuget_version_parse(minimum "${pin_VERSION}")
        if(pin_FORM STREQUAL "minimum" AND NOT minimum_LABEL STREQUAL ""
           AND minimum_CHANNEL STREQUAL "")
            message(FATAL_ERROR
                "${manifest_file} pins ${id} at ${version}, a prerelease whose label begins with "
                "no letter. A prerelease minimum names its channel by the word its label begins with, "
                "as 2.5.4-experimental names the channel \"experimental\", and the restore takes "
                "the versions of that channel only.")
        endif()
        if(id STREQUAL "Microsoft.Windows.CppWinRT")
            set(has_cppwinrt TRUE)
        endif()
        list(APPEND pins "${id}" "${version}")
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
# (`what` says which), checked against the pin, `minimum` -- a minimum or one
# held version. A version the pin does not admit stops the configure with the
# remedy, which `context` chooses: "lock" for the lock file read before the
# restore, which git or the update puts right; "restore" for the result of an
# ordinary restore; "update" for the result of the update, which takes the
# newest versions the NuGet sources hold, so that the update is not the remedy.
function(_wxl_packages_check_version what id version minimum context)
    wxl_nuget_version_mismatch(mismatch "${version}" "${minimum}")
    if(mismatch STREQUAL "")
        return()
    endif()

    set(pinned "${id} ${version}, which the minimum ${minimum} of tools/packages.json does not admit")
    set(own "")
    if(mismatch STREQUAL "invalid")
        set(cause "${id} ${version}, which is not a NuGet version.")
    elseif(mismatch STREQUAL "exact")
        set(cause "${id} ${version}, and tools/packages.json holds it at exactly ${minimum}.")
        set(own "change the version it is held at")
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

    if(context STREQUAL "update" AND mismatch STREQUAL "exact")
        string(CONCAT remedy
            "The update does not choose a held version: check the NuGet sources of the restore")
    elseif(context STREQUAL "update")
        string(CONCAT remedy
            "The update takes the newest versions the NuGet sources hold, so they hold none that "
            "the minimum admits: check the minimum and the NuGet sources of the restore")
    elseif(context STREQUAL "lock")
        string(CONCAT remedy
            "The lock file tools/packages.lock.json does not match the pins. Take it back from "
            "git, or run the update, tools\\build.ps1 -UpdatePackages (a configure with "
            "-DWXL_UPDATE_PACKAGES=ON), and commit the lock file it writes")
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
# ever published, unlisted ones included, without saying which are unlisted,
# and the newest version of it is taken. NuGet's floating references are
# assumed to resolve over the same list. What the update chose is reviewed in the versions it prints and in
# the change of the lock file before it is committed. A failed download, or an
# index without such a version, stops the configure.
function(_wxl_packages_newest out id minimum dir)
    wxl_nuget_version_parse(pin "${minimum}")
    list(GET pin_NUMBERS 0 major)
    set(mask "${major}.*-${pin_CHANNEL}*")
    string(TOLOWER "${id}" lower)
    string(REGEX REPLACE "/+$" "" container "${WXL_NUGET_FLAT_CONTAINER}")
    set(url "${container}/${lower}/index.json")
    set(index_file "${dir}/${lower}.index.json")
    # A stalled connection stops after 30 seconds without data, a slow one after
    # two minutes, so that the configure does not hang on the network.
    file(DOWNLOAD "${url}" "${index_file}" STATUS status TLS_VERIFY ON
         INACTIVITY_TIMEOUT 30 TIMEOUT 120)
    list(GET status 0 code)
    if(NOT code EQUAL 0)
        list(GET status 1 reason)
        message(FATAL_ERROR
            "The configure could not download the versions of ${id} from ${url}: ${reason} "
            "(${code}). The update, and the configure without a lock file, take the version of a "
            "prerelease pin (${minimum}: the newest of ${mask} that is not below the minimum) "
            "from the index of the package in the flat container of the NuGet source, "
            "WXL_NUGET_FLAT_CONTAINER (${WXL_NUGET_FLAT_CONTAINER}). Check the network and that "
            "variable, then configure again.")
    endif()
    file(READ "${index_file}" index)
    string(JSON type ERROR_VARIABLE error TYPE "${index}" versions)
    if(error OR NOT type STREQUAL "ARRAY")
        message(FATAL_ERROR
            "${url} has no array \"versions\": it is not the index of a package in a NuGet flat "
            "container. Check WXL_NUGET_FLAT_CONTAINER (${WXL_NUGET_FLAT_CONTAINER}).")
    endif()
    string(JSON versions GET "${index}" versions)
    wxl_nuget_newest_of_major(newest "${versions}" "${minimum}" "${url}")
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

    # The lock file, read once. Outside the update it gives the versions of the
    # prerelease pins and is checked against the pins; on the update it gives
    # the versions before. A file that is not a lock file stops an ordinary
    # configure. The update, which writes the file anew, leaves it in place and
    # does not read it: with RestoreForceEvaluate NuGet does not read it either,
    # and the file stays as it was should the update stop before the restore.
    set(lock_json "")
    set(locked_IDS "")
    set(before_unknown "none")
    if(EXISTS "${lock}")
        file(READ "${lock}" lock_json)
        _wxl_packages_lock_problem(lock_problem "${lock_json}")
        if(NOT lock_problem STREQUAL "" AND WXL_UPDATE_PACKAGES)
            message(STATUS
                "NuGet update: the lock file ${lock} ${lock_problem}; the update writes it anew, "
                "and the versions before are shown as unreadable.")
            set(lock_json "")
            set(before_unknown "unreadable")
        else()
            _wxl_packages_read_lock(locked "${lock}" "${lock_json}")
        endif()
    endif()

    # The references. A stable minimum is referenced as the floating version
    # of its major version and a held pin as it is written, by the pin alone; a
    # prerelease minimum as the exact version that the lock file records or the
    # update chooses (see the top of this file). Each is made of the characters
    # of a NuGet version and brackets, as wxl_nuget_version_parse() checks them.
    #
    # Outside the update the references are those the lock file was written
    # for: NuGet resolves every package anew when they are not, floating ones
    # included, and rewrites the file, which only the update is to do.
    string(CONCAT pins_changed
        "An ordinary configure does not choose new versions: run tools\\build.ps1 "
        "-UpdatePackages in the wxl tree and commit tools/packages.lock.json.")
    set(WXL_PACKAGES_REFERENCES "")
    set(pinned_ids "")
    list(LENGTH pins length)
    math(EXPR last "${length} - 1")
    foreach(i RANGE 0 ${last} 2)
        math(EXPR j "${i} + 1")
        list(GET pins ${i} id)
        list(GET pins ${j} minimum)
        string(TOLOWER "${id}" lower)
        list(APPEND pinned_ids "${lower}")
        wxl_nuget_pin_reference(version "${minimum}")
        set(has_entry FALSE)
        if(lower IN_LIST locked_IDS)
            set(has_entry TRUE)
        endif()

        if(WXL_UPDATE_PACKAGES)
            if(version STREQUAL "")
                _wxl_packages_newest(chosen "${id}" "${minimum}" "${helper}/index")
                set(version "[${chosen}]")
            endif()
            set(before_${i} "${before_unknown}")
            if(has_entry)
                set(before_${i} "${locked_RESOLVED_${lower}}")
            endif()
        elseif(lock_json STREQUAL "")
            # No lock file: an unlocked configure of the wxl tree (a locked one
            # stopped above) resolves every package anew and writes the file,
            # and a prerelease pin is chosen as the update chooses it.
            if(version STREQUAL "")
                _wxl_packages_newest(chosen "${id}" "${minimum}" "${helper}/index")
                message(STATUS
                    "NuGet packages: no lock file; the restore takes ${id} ${chosen}, the newest "
                    "version the minimum ${minimum} admits.")
                set(version "[${chosen}]")
            endif()
        else()
            if(NOT has_entry)
                message(FATAL_ERROR
                    "The pins changed: tools/packages.json pins ${id}, which the lock file "
                    "${lock} has no direct entry of. ${pins_changed}")
            endif()
            # The version the lock file records has to be one the pin admits,
            # for every pin: a version a held pin does not name, recorded under
            # the right request, makes NuGet drop the package from the restore
            # without an error. A prerelease minimum's is checked first, since
            # its reference is made of it.
            set(requested "${locked_REQUESTED_${lower}}")
            set(resolved "${locked_RESOLVED_${lower}}")
            if(version STREQUAL "")
                _wxl_packages_check_version("The lock file ${lock} records"
                    "${id}" "${resolved}" "${minimum}" lock)
                set(version "[${resolved}]")
            endif()
            wxl_nuget_requested_matches(matches "${version}" "${requested}")
            if(NOT matches)
                message(FATAL_ERROR
                    "The pins changed: tools/packages.json pins ${id} at ${minimum}, referenced as "
                    "${version}, and the lock file ${lock} was written for the reference "
                    "${requested}. ${pins_changed}")
            endif()
            _wxl_packages_check_version("The lock file ${lock} records"
                "${id}" "${resolved}" "${minimum}" lock)
        endif()
        string(APPEND WXL_PACKAGES_REFERENCES
            "    <PackageReference Include=\"${id}\" Version=\"${version}\" />\n")
    endforeach()
    if(NOT WXL_UPDATE_PACKAGES)
        foreach(lower IN LISTS locked_IDS)
            if(NOT lower IN_LIST pinned_ids)
                message(FATAL_ERROR
                    "The pins changed: the lock file ${lock} has a direct entry of "
                    "${locked_ID_${lower}}, which tools/packages.json does not pin. "
                    "${pins_changed}")
            endif()
        endforeach()
    endif()
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

    # A lock file newer than the last restore -- one that came with a pull, its
    # references unchanged -- is applied anew. NuGet skips a restore whose
    # inputs hash as in obj/project.nuget.cache, and the lock file is not among
    # them (RestoreCommand's no-op check reads the hash of the dependency graph
    # and only asks that the lock file exist), so the restore result of the
    # earlier versions would stay. Without the cache file NuGet restores for
    # real and takes the versions the lock file records -- from the global
    # packages folder without the network when the packages are there, it is
    # assumed.
    # IS_NEWER_THAN is true for equal times as well, so the test is asked the
    # other way round: the cache older than the lock file, strictly.
    set(nuget_cache "${helper}/obj/project.nuget.cache")
    if(NOT WXL_UPDATE_PACKAGES AND NOT lock_json STREQUAL "" AND EXISTS "${nuget_cache}"
       AND NOT "${nuget_cache}" IS_NEWER_THAN "${lock}")
        message(STATUS "NuGet packages: ${lock} is newer than the last restore, which runs anew.")
        file(REMOVE "${nuget_cache}")
    endif()

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
    if(NOT result EQUAL 0 AND WXL_UPDATE_PACKAGES)
        message(FATAL_ERROR
            "MSBuild failed (${result}) on ${project} in the update of the NuGet packages, "
            "restoring the newest versions the pins admit or building the C++/WinRT projection. When a "
            "satellite of the release (Win2D, WebView2, CppWinRT) needs a Windows App SDK "
            "component newer than the release has, the check of Microsoft.WindowsAppSDK.Runtime "
            "(VerifyReferencedWindowsAppSdkComponentVersions) stops the restore. Either hold the "
            "satellite at the version it had, \"[X]\" in tools/packages.json, and run the update "
            "again; or take tools/packages.lock.json back from git and configure with "
            "-DWXL_UPDATE_PACKAGES=OFF -- a failed update leaves WXL_UPDATE_PACKAGES ON in the "
            "cache, and the next configure would run it again.\n"
            "${output}\nThe whole log: ${log}")
    endif()
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
    # A restore that wrote the lock file -- the update, or a configure without
    # one -- writes it after obj/project.nuget.cache, so the lock file would be
    # newer than the cache and the next configure would take it for one a pull
    # brought and restore once more for nothing. The cache is touched to put the
    # two in step.
    if(WXL_UPDATE_PACKAGES OR lock_json STREQUAL "")
        file(TOUCH "${nuget_cache}")
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
        # Outside the update the restore takes what the lock file read above
        # records; another version is the result of an earlier restore.
        if(NOT WXL_UPDATE_PACKAGES AND NOT lock_json STREQUAL "")
            string(TOLOWER "${version}" restored_lower)
            string(TOLOWER "${locked_RESOLVED_${lower}}" locked_lower)
            if(NOT restored_lower STREQUAL locked_lower)
                message(FATAL_ERROR
                    "The restore of ${project} resolved ${id} ${version}, and the lock file "
                    "${lock} records ${locked_RESOLVED_${lower}}: the restore kept the result of "
                    "an earlier one. Delete ${helper}/obj and configure again.")
            endif()
        endif()
        set(context restore)
        if(WXL_UPDATE_PACKAGES)
            set(context update)
        endif()
        _wxl_packages_check_version("The restore of ${project} resolved"
            "${id}" "${version}" "${minimum}" ${context})
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

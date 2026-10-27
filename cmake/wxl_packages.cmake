# The NuGet packages the tree builds against and the C++/WinRT projection over
# them, both made at configure by the MSBuild of the developer environment.
#
# Including this file writes a helper project, <build>/wxl.packages/
# wxl.packages.vcxproj (from wxl_packages.vcxproj.in), and runs on it
#
#   MSBuild.exe wxl.packages.vcxproj -restore -t:ResolveReferences,CppWinRTMakeProjections
#
# -restore restores the PackageReference items of the project first: the
# umbrella package Microsoft.WindowsAppSDK at the release "windowsAppSdk" names
# in wxl.gen/profiles/base.json, whose dependencies are the packages of that
# release, and the "extra" packages of tools/packages.json. Every version is a
# minimum: NuGet takes the lowest version that satisfies every reference, and
# the lock file tools/packages.lock.json records the versions it took. The
# targets are those of the Microsoft.Windows.CppWinRT package.
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
# Defined here:
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
# What this relies on -- the developer environment, the component of the
# installation, the Windows SDK, the restore result, the response files the
# CppWinRT targets write and the headers of the projection -- is checked here,
# and a failed check stops the configure.

include_guard(GLOBAL)

set(_WXL_PACKAGES_SCRIPT_DIR "${CMAKE_CURRENT_LIST_DIR}")
cmake_path(GET _WXL_PACKAGES_SCRIPT_DIR PARENT_PATH _WXL_PACKAGES_TREE)

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

# The pins as pairs of a package id and its minimum version: the release of
# base.json as the umbrella package, then the "extra" packages of
# tools/packages.json. Both files are inputs of the configure.
function(_wxl_packages_pins out_pins)
    set(base_file "${_WXL_PACKAGES_TREE}/wxl.gen/profiles/base.json")
    file(READ "${base_file}" base)
    if(NOT base MATCHES "\"windowsAppSdk\"[ \t]*:[ \t]*\"([^\"]+)\"")
        message(FATAL_ERROR "${base_file} names no Windows App SDK release (\"windowsAppSdk\").")
    endif()
    set(pins Microsoft.WindowsAppSDK "${CMAKE_MATCH_1}")

    set(manifest_file "${_WXL_PACKAGES_TREE}/tools/packages.json")
    file(READ "${manifest_file}" manifest)
    string(JSON count ERROR_VARIABLE error LENGTH "${manifest}" extra)
    if(error)
        message(FATAL_ERROR "${manifest_file} has no list \"extra\": ${error}")
    endif()
    if(count GREATER 0)
        math(EXPR last "${count} - 1")
        foreach(i RANGE ${last})
            string(JSON id GET "${manifest}" extra ${i} id)
            string(JSON version GET "${manifest}" extra ${i} version)
            list(APPEND pins "${id}" "${version}")
        endforeach()
    endif()

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
            message(FATAL_ERROR "Not a NuGet package id and version: \"${id}\" \"${version}\" (${manifest_file}, ${base_file}).")
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

    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${base_file}" "${manifest_file}")
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

# Reads the restore result: the package folders and the directory of every
# package, which wxl_nuget_package_dir() returns.
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

    set(roots "")
    math(EXPR last "${folder_count} - 1")
    foreach(i RANGE ${last})
        string(JSON folder MEMBER "${folders}" ${i})
        file(TO_CMAKE_PATH "${folder}" folder)
        list(APPEND roots "${folder}")
    endforeach()

    # A member of "libraries" is named <id>/<version>, and its "path" is the
    # directory of the package relative to one of the package folders.
    set(count 0)
    math(EXPR last "${library_count} - 1")
    foreach(i RANGE ${last})
        string(JSON key MEMBER "${libraries}" ${i})
        string(JSON entry GET "${libraries}" "${key}")
        string(JSON type ERROR_VARIABLE error GET "${entry}" type)
        if(error OR NOT type STREQUAL "package")
            continue()
        endif()
        string(JSON path ERROR_VARIABLE error GET "${entry}" path)
        string(FIND "${key}" "/" slash)
        if(error OR slash LESS 1)
            message(FATAL_ERROR "${assets}: the package \"${key}\" has no path, or its name is not <id>/<version>.")
        endif()
        string(SUBSTRING "${key}" 0 ${slash} id)

        set(dir "")
        foreach(root IN LISTS roots)
            cmake_path(APPEND root "${path}" OUTPUT_VARIABLE candidate)
            if(IS_DIRECTORY "${candidate}")
                set(dir "${candidate}")
                break()
            endif()
        endforeach()
        if(dir STREQUAL "")
            message(FATAL_ERROR "${assets} names ${key} at ${path}, which is in none of its package folders (${roots}).")
        endif()

        string(TOLOWER "${id}" lower)
        set_property(GLOBAL PROPERTY _WXL_NUGET_PACKAGE_${lower} "${dir}")
        math(EXPR count "${count} + 1")
    endforeach()

    set_property(GLOBAL PROPERTY _WXL_NUGET_ASSETS "${assets}")
    set(${out_count} ${count} PARENT_SCOPE)
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

    _wxl_packages_environment(msbuild sdk vc_tools)
    _wxl_packages_pins(pins)

    set(helper "${CMAKE_BINARY_DIR}/wxl.packages")
    set(project "${helper}/wxl.packages.vcxproj")
    set(projection "${helper}/projection")
    set(log "${helper}/msbuild.log")
    set(lock "${_WXL_PACKAGES_TREE}/tools/packages.lock.json")
    _wxl_packages_check_path("The build directory" "${helper}")
    _wxl_packages_check_path("The lock file" "${lock}")

    set(WXL_PACKAGES_REFERENCES "")
    list(LENGTH pins length)
    math(EXPR last "${length} - 1")
    foreach(i RANGE 0 ${last} 2)
        math(EXPR j "${i} + 1")
        list(GET pins ${i} id)
        list(GET pins ${j} version)
        string(APPEND WXL_PACKAGES_REFERENCES
            "    <PackageReference Include=\"${id}\" Version=\"${version}\" />\n")
    endforeach()
    set(WXL_PACKAGES_VC_TOOLS_VERSION "${vc_tools}")
    set(WXL_PACKAGES_WINDOWS_SDK_VERSION "${sdk}")
    file(TO_NATIVE_PATH "${lock}" WXL_PACKAGES_LOCK_FILE)
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
        WORKING_DIRECTORY "${helper}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE output)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR
            "MSBuild failed (${result}) on ${project}, restoring the NuGet packages or building the "
            "C++/WinRT projection. A restore with RestoreLockedMode=true, as on CI, stops when "
            "tools/packages.lock.json does not match the pins; without it the restore writes the "
            "lock file anew, and the changed file is committed together with the pins.\n"
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
        list(GET pins ${i} id)
        string(TOLOWER "${id}" lower)
        get_property(restored GLOBAL PROPERTY _WXL_NUGET_PACKAGE_${lower} SET)
        if(NOT restored)
            message(FATAL_ERROR "The restore of ${project} did not resolve ${id}, which the helper project references.")
        endif()
    endforeach()

    # The response files are written in IntDir, x64\Release\ of the project
    # directory; the platform projection reads the Windows SDK, the reference
    # projection reads the packages and refers to the SDK.
    set(int "${helper}/x64/Release")
    _wxl_packages_check_response("${int}/wxl.packages.vcxproj.cppwinrt_plat.rsp" "${WXL_PACKAGES_PROJECTION_DIR}" -in)
    _wxl_packages_check_response("${int}/wxl.packages.vcxproj.cppwinrt_ref.rsp" "${WXL_PACKAGES_PROJECTION_DIR}" -in -ref)
    foreach(header base.h Windows.Foundation.h Microsoft.UI.Xaml.h)
        if(NOT EXISTS "${projection}/winrt/${header}")
            message(FATAL_ERROR "The C++/WinRT projection in ${projection} has no winrt/${header}.")
        endif()
    endforeach()

    # The headers, and the language level C++/WinRT needs.
    add_library(Microsoft::CppWinRT INTERFACE IMPORTED GLOBAL)
    set_target_properties(Microsoft::CppWinRT PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${projection}"
        INTERFACE_COMPILE_FEATURES cxx_std_17)

    message(STATUS "NuGet packages: ${count} restored (${lock}); C++/WinRT projection: ${projection}")
endfunction()

function(wxl_nuget_package_dir variable id)
    string(TOLOWER "${id}" lower)
    get_property(restored GLOBAL PROPERTY _WXL_NUGET_PACKAGE_${lower} SET)
    if(NOT restored)
        get_property(assets GLOBAL PROPERTY _WXL_NUGET_ASSETS)
        message(FATAL_ERROR
            "${id} is not among the packages the restore resolved (${assets}). A package reaches the "
            "restore as a dependency of the pins in wxl.gen/profiles/base.json and "
            "tools/packages.json, or as a pin of its own in tools/packages.json.")
    endif()
    get_property(dir GLOBAL PROPERTY _WXL_NUGET_PACKAGE_${lower})
    set(${variable} "${dir}" PARENT_SCOPE)
endfunction()

_wxl_packages()

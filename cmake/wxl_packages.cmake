# The NuGet packages the tree builds against, found the way every other script
# and the generator find them -- never by a path written into a CMakeLists.
#
#   wxl_nuget_package_dir(<variable> <package id>)
#
# sets <variable> to the package's directory inside the NuGet package folder,
# or fails naming tools/restore-packages.ps1, which is what puts it there.
#
# The folder: %NUGET_PACKAGES%, else the globalPackagesFolder of NuGet.Config,
# else %USERPROFILE%\.nuget\packages -- NuGet's own order. The version: the
# "experimental" and "extra" pins of tools/packages.json, else the one the
# Windows App SDK release of wxl.gen/profiles/base.json declares.

set(_WXL_PACKAGES_DIR "${CMAKE_CURRENT_LIST_DIR}")

function(_wxl_nuget_root out)
    if(DEFINED ENV{NUGET_PACKAGES} AND NOT "$ENV{NUGET_PACKAGES}" STREQUAL "")
        set(${out} "$ENV{NUGET_PACKAGES}" PARENT_SCOPE)
        return()
    endif()
    set(config "$ENV{APPDATA}/NuGet/NuGet.Config")
    if(EXISTS "${config}")
        file(READ "${config}" text)
        if(text MATCHES "key=\"globalPackagesFolder\"[ \t]+value=\"([^\"]+)\"")
            file(TO_CMAKE_PATH "${CMAKE_MATCH_1}" folder)
            set(${out} "${folder}" PARENT_SCOPE)
            return()
        endif()
    endif()
    file(TO_CMAKE_PATH "$ENV{USERPROFILE}/.nuget/packages" folder)
    set(${out} "${folder}" PARENT_SCOPE)
endfunction()

function(wxl_nuget_package_dir variable id)
    _wxl_nuget_root(root)
    string(TOLOWER "${id}" lower)

    set(version "")
    file(READ "${_WXL_PACKAGES_DIR}/../tools/packages.json" manifest)
    foreach(section experimental extra)
        string(JSON count LENGTH "${manifest}" ${section})
        math(EXPR last "${count} - 1")
        foreach(i RANGE ${last})
            string(JSON entry_id GET "${manifest}" ${section} ${i} id)
            if(entry_id STREQUAL id)
                string(JSON version GET "${manifest}" ${section} ${i} version)
            endif()
        endforeach()
    endforeach()

    if(version STREQUAL "")
        file(READ "${_WXL_PACKAGES_DIR}/../wxl.gen/profiles/base.json" base)
        if(NOT base MATCHES "\"windowsAppSdk\"[ \t]*:[ \t]*\"([^\"]+)\"")
            message(FATAL_ERROR "wxl.gen/profiles/base.json names no windowsAppSdk release")
        endif()
        set(sdk "${CMAKE_MATCH_1}")
        set(nuspec "${root}/microsoft.windowsappsdk/${sdk}/microsoft.windowsappsdk.nuspec")
        if(NOT EXISTS "${nuspec}")
            message(FATAL_ERROR "Windows App SDK ${sdk} is not in ${root}: run tools\restore-packages.ps1")
        endif()
        file(READ "${nuspec}" spec)
        if(NOT spec MATCHES "<dependency id=\"${id}\" version=\"\\[?([^]\"]+)")
            message(FATAL_ERROR "Windows App SDK ${sdk} declares no ${id}")
        endif()
        set(version "${CMAKE_MATCH_1}")
    endif()

    set(dir "${root}/${lower}/${version}")
    if(NOT IS_DIRECTORY "${dir}")
        message(FATAL_ERROR "${id} ${version} is not in ${root}: run tools\restore-packages.ps1")
    endif()
    set(${variable} "${dir}" PARENT_SCOPE)
endfunction()

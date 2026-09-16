# Резолвит VCPKG_ROOT: явная кэш-переменная CMake, затем vcpkg.path.txt (его
# пишет сам vcpkg), а на прочих платформах — переменная окружения VCPKG_ROOT.
#
# На Windows переменная окружения сознательно не читается: vcvars64.bat
# подменяет её на vcpkg из состава Visual Studio, в котором нет порта cppwinrt,
# и сборка из developer-окружения уехала бы не в тот корень (см. комментарий в
# tools/build.ps1).
function(get_vcpkg_root)
    if(VCPKG_ROOT)
        return()
    endif()

    if(WIN32)
        set(_vcpkg_path_txt "$ENV{USERPROFILE}/Local Settings/vcpkg/vcpkg.path.txt")
        cmake_path(NORMAL_PATH _vcpkg_path_txt)

        if(EXISTS "${_vcpkg_path_txt}")
            file(READ "${_vcpkg_path_txt}" _vcpkg_path)
            string(STRIP "${_vcpkg_path}" _vcpkg_path)

            if(_vcpkg_path)
                set(VCPKG_ROOT "${_vcpkg_path}" PARENT_SCOPE)
            endif()
        endif()
    elseif(DEFINED ENV{VCPKG_ROOT} AND NOT "$ENV{VCPKG_ROOT}" STREQUAL "")
        set(_vcpkg_root "$ENV{VCPKG_ROOT}")
        cmake_path(NORMAL_PATH _vcpkg_root)
        set(VCPKG_ROOT "${_vcpkg_root}" PARENT_SCOPE)
    endif()
endfunction()

function(vcpkg_bootstrap)
    get_vcpkg_root()

    if(NOT VCPKG_ROOT)
        message(FATAL_ERROR
            "\n[ERROR]: VCPKG_ROOT not found!\n"
            "Please set VCPKG_ROOT environment variable or CMake variable.\n"
        )
    endif()

    set(_vcpkg_toolchain_path "${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")

    # Установка toolchain файла должна происходить ДО первого вызова project()
    if(NOT CMAKE_TOOLCHAIN_FILE)
        set(CMAKE_TOOLCHAIN_FILE "${_vcpkg_toolchain_path}" CACHE FILEPATH "vcpkg toolchain" FORCE)
    endif()

    if(NOT "${VCPKG_ROOT}/installed/x64-windows-static-md" IN_LIST CMAKE_PREFIX_PATH)
        list(APPEND CMAKE_PREFIX_PATH "${VCPKG_ROOT}/installed/x64-windows-static-md")
        set(CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH}" PARENT_SCOPE)
    endif()

    message(INFO "\n[INFO]: VCPKG_ROOT=${VCPKG_ROOT}\n")
    message(INFO "\n[INFO]: CMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}\n")
    message(INFO "\n[INFO]: CMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH}\n")

endfunction()

vcpkg_bootstrap()

# ---- CMakeList.txt : wxl.core ----
#
# Builds both as part of the tree's common top-level project and on its own.
# Standalone, it has to do the setup the top level otherwise does: resolve
# vcpkg, open the `import std;` gate, call project(), find the shared
# dependencies.
cmake_minimum_required(VERSION 4.3)

# Every CMake script in this tree is shared, one level up.
set(WXL_CMAKE_SCRIPT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../cmake")

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    set(WXL_CORE_STANDALONE ON)

    # Resolves VCPKG_ROOT and the toolchain file; must run before project().
    include(${WXL_CMAKE_SCRIPT_DIR}/vcpkg_bootstrap.cmake)

    # src/CMakeLists.txt sets CXX_MODULE_STD ON, which CMake keeps behind an
    # experimental gate that only the top-level project can open, and only
    # before project().
    include(${WXL_CMAKE_SCRIPT_DIR}/wxl_import_std_gate.cmake)

    project(
        wxl.core VERSION 0.1.0
        DESCRIPTION "Winui Xaml-Less library (wxl) -- core runtime"
        LANGUAGES CXX
    )

    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
    set(CMAKE_CXX_STANDARD 23)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

# No link-time optimization here either, and for the reason spelled out at the
# root of the wxl tree: measured, /GL + /LTCG neither shrinks the binaries nor
# speeds up anything this tree does -- it makes one hot loop slower by inlining
# it across a library boundary -- while the static libraries it produces are an
# order of magnitude larger, since a /GL object holds an intermediate
# representation rather than machine code.

option(BUILD_TESTING "Build the testing tree." ON)

# Precompiled header built from src/pch.h (platform.h + abi.h -- i.e. <windows.h>
# and the fixed-width/assert plumbing). Purely a build-speed knob: every source
# already includes those headers explicitly, so turning it off changes nothing
# but compile time. Kept as an option because it is MSVC-specific behaviour and
# the module-unit interaction is worth being able to bisect out.
option(WXL_ENABLE_PCH "Use a precompiled header (src/pch.h) for wxl.core." ON)

# Off by default: the benchmarks are measurement tools, run deliberately, and
# an ordinary build has no reason to spend time on them.
option(WXL_BUILD_BENCHMARKS "Build the wxl.core benchmark executables." OFF)

# Install/export rules. Off by default when wxl is a subproject (FetchContent):
# a consumer's `cmake --install` should not start dropping wxl's archives and
# module sources into its prefix unless it asks for it.
option(WXL_INSTALL "Generate install and export rules for wxl." ${WXL_CORE_STANDALONE})

# Refresh .vscode/c_cpp_properties.json after building, so that the editor sees
# the module interfaces of the configuration just built (see tools/
# gen-intellisense-config.ps1 and the readme). The post-build step is a no-op
# unless the build was started from an IDE, and it is off entirely when wxl is
# somebody else's subproject: its editor configuration is none of our business.
option(WXL_UPDATE_INTELLISENSE
    "Refresh .vscode/c_cpp_properties.json after an IDE-driven build."
    ${WXL_CORE_STANDALONE})

if(BUILD_TESTING)
    enable_testing()
    find_package(GTest REQUIRED CONFIG)
    include(GoogleTest)
endif()

find_package(cppwinrt CONFIG REQUIRED)

# winmd::reader is a header-only WinMD metadata parsing library; not packaged
# by vcpkg, so it's fetched directly from its upstream repo.
include(FetchContent)
FetchContent_Declare(
    winmd
    GIT_REPOSITORY https://github.com/microsoft/winmd.git
    GIT_TAG a363c851a65c4e15743d6589acc1723e9e21b143 # master, 2026-08
)
FetchContent_MakeAvailable(winmd)

include(${WXL_CMAKE_SCRIPT_DIR}/config.cmake)

option(WXL_BACKEND "Backend to be used for WXL." "WinUI3")

# The top-level project defines the same target when this is a subproject.
if(NOT TARGET winmd_reader)
    add_library(winmd_reader INTERFACE)
    target_include_directories(winmd_reader INTERFACE ${winmd_SOURCE_DIR}/src)
endif()

add_subdirectory(src)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

if(WXL_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()

# ---- Package: find_package(wxl CONFIG) ----
# What ships: the archive, the module *interface sources* (.ixx) and the CMake
# glue. Built module interfaces (.ifc) are deliberately absent -- they are tied
# to the exact compiler build and flags, so CMake never installs them and the
# consumer compiles the interfaces once in its own build tree.
if(WXL_INSTALL)
    include(CMakePackageConfigHelpers)

    set(WXL_CMAKE_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/wxl")
    set(WXL_HELPERS_RELATIVE_DIR "helpers")

    # One file per component, named wxl-<component>-targets.cmake -- that naming
    # is the contract wxl-config.cmake relies on to resolve COMPONENTS.
    #
    # CXX_MODULES_DIRECTORY is what carries the C++ module information of the
    # exported target (CMake 3.28+); without it the export is just an archive.
    install(EXPORT wxl-core-targets
        FILE wxl-core-targets.cmake
        NAMESPACE wxl::
        DESTINATION "${WXL_CMAKE_DIR}"
        CXX_MODULES_DIRECTORY cxx-modules
        COMPONENT core
    )

    # The helpers travel with the package; wxl-config.cmake includes them.
    install(FILES
            ${WXL_CMAKE_SCRIPT_DIR}/wxl_helpers.cmake
            ${WXL_CMAKE_SCRIPT_DIR}/wxl_add_module_library.cmake
            ${WXL_CMAKE_SCRIPT_DIR}/wxl_target_folders.cmake
            ${WXL_CMAKE_SCRIPT_DIR}/wxl_add_gtest.cmake
            ${WXL_CMAKE_SCRIPT_DIR}/wxl_copy_assets.cmake
        DESTINATION "${WXL_CMAKE_DIR}/${WXL_HELPERS_RELATIVE_DIR}"
    )

    configure_package_config_file(
        ${WXL_CMAKE_SCRIPT_DIR}/wxl-config.cmake.in
        "${CMAKE_CURRENT_BINARY_DIR}/install/wxl-config.cmake"
        INSTALL_DESTINATION "${WXL_CMAKE_DIR}"
    )

    # ExactVersion while the library is pre-1.0: nothing here is stable enough
    # for a consumer to assume compatibility across versions yet.
    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/install/wxl-config-version.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY ExactVersion
    )

    install(FILES
            "${CMAKE_CURRENT_BINARY_DIR}/install/wxl-config.cmake"
            "${CMAKE_CURRENT_BINARY_DIR}/install/wxl-config-version.cmake"
        DESTINATION "${WXL_CMAKE_DIR}"
    )

    # Build-tree consumption: find_package(wxl CONFIG PATHS "<wxl-build-dir>")
    # works against an un-installed build, which is handy for local debugging of
    # a consumer without an install step.
    set(WXL_SOURCE_CMAKE_DIR "${WXL_CMAKE_SCRIPT_DIR}")
    export(EXPORT wxl-core-targets
        FILE "${CMAKE_CURRENT_BINARY_DIR}/wxl-core-targets.cmake"
        NAMESPACE wxl::
        CXX_MODULES_DIRECTORY cxx-modules
    )
    configure_file(
        ${WXL_CMAKE_SCRIPT_DIR}/wxl-build-tree-config.cmake.in
        "${CMAKE_CURRENT_BINARY_DIR}/wxl-config.cmake"
        @ONLY
    )
    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/wxl-config-version.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY ExactVersion
    )
endif()

# Служебный таргет для отображения общих CMake-скриптов в IDE -- только когда
# wxl.core собирается сам по себе: в общем проекте такой таргет уже создан на
# верхнем уровне.
if(WXL_CORE_STANDALONE)
    file(GLOB_RECURSE _wxl_cmake_scripts CONFIGURE_DEPENDS "${WXL_CMAKE_SCRIPT_DIR}/*.*")
    add_custom_target(cmake_files SOURCES ${_wxl_cmake_scripts})
    source_group("cmake" FILES ${_wxl_cmake_scripts})
endif()

# ---- CMakeList.txt : wxl.core ----
#
# A subdirectory of the tree's project (the root CMakeLists.txt), which sets the
# language and includes the helpers before it.

# Every CMake script in this tree is shared, one level up.
set(WXL_CMAKE_SCRIPT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../cmake")

# No link-time optimization here either, and for the reason spelled out at the
# root of the wxl tree: measured, /GL + /LTCG neither shrinks the binaries nor
# speeds up anything this tree does -- it makes one hot loop slower by inlining
# it across a library boundary -- while the static libraries it produces are an
# order of magnitude larger, since a /GL object holds an intermediate
# representation rather than machine code.

# Precompiled header built from src/pch.h (platform.h + abi.h -- i.e. <windows.h>
# and the fixed-width/assert plumbing). Purely a build-speed knob: every source
# already includes those headers explicitly, so turning it off changes nothing
# but compile time. Kept as an option because it is MSVC-specific behaviour and
# the module-unit interaction is worth being able to bisect out.
option(WXL_ENABLE_PCH "Use a precompiled header (src/pch.h) for wxl.core." ON)

# Off by default: the checks of how coroutines are used -- a task awaited twice at
# once, a value read twice, a task dropped while it runs or while somebody joins it,
# a task used after a move -- are then assert's, a Debug build's alone, and a
# Release build has neither a byte nor an instruction of them. On, they stay in every
# build, Release included, and a broken one ends the process through wxl::core::abort,
# naming the reason and the line of the code that broke the rule where the language
# gives it (std::source_location).
option(WXL_STRICT_CORO "Keep the coroutine checks of the async area in every build (defines STRICT_CORO)." OFF)

# Install/export rules. Off by default: a consumer's `cmake --install` should
# not start dropping wxl's archives and module sources into its prefix unless it
# asks for it.
option(WXL_INSTALL "Generate install and export rules for wxl." OFF)

add_subdirectory(src)

if(WXL_TESTS)
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

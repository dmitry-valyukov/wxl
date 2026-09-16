# ---- CMakeList.txt : wxl.xml ----
#
# The XML reader the generator uses to walk WinUI3's XAML resource
# dictionaries -- the one input of the project that metadata does not carry.
#
# Builds both as a subdirectory of the tree's common project and on its own
# (its own preset, see CMakePresets.json). Standalone it has to do the setup
# the top level otherwise does, and it needs wxl.core either way: the reader
# allocates through the STA pool, so consuming that C++20 module means a Ninja
# generator and the `import std;` gate opened before project().
cmake_minimum_required(VERSION 3.28)

# Every CMake script in this tree is shared, one level up.
set(WXL_CMAKE_SCRIPT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../cmake")

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    # Resolves VCPKG_ROOT and the toolchain file; must run before project().
    include(${WXL_CMAKE_SCRIPT_DIR}/vcpkg_bootstrap.cmake)

    # Only the top-level project may open the `import std;` gate, and only
    # before project() -- wxl.core's module interfaces are built with it.
    include(${WXL_CMAKE_SCRIPT_DIR}/wxl_import_std_gate.cmake)

    project(
        wxl-xml VERSION 0.1.0
        DESCRIPTION "Winui Xaml-Less library (wxl) -- XML reader"
        LANGUAGES CXX
    )

    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
    set(CMAKE_CXX_STANDARD 23)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
    # Nothing here is a module, but consuming wxl.core's means being scanned
    # for imports and getting the std module the same way it did. Set for the
    # whole project rather than per target: CMake builds one `std` per set of
    # compile settings, and a target that asks separately gets one of its own.
    set(CMAKE_CXX_MODULE_STD 1)
    set(CMAKE_CXX_SCAN_FOR_MODULES ON)

    option(BUILD_TESTING "Build the testing tree." ON)

    if(BUILD_TESTING)
        enable_testing()
        find_package(GTest REQUIRED CONFIG)
        include(GoogleTest)
    endif()

    include(${WXL_CMAKE_SCRIPT_DIR}/config.cmake)
endif()

# ---- wxl.core: the STA allocator ----
# A top-level project of its own, consumed either as an installed package
# (point CMAKE_PREFIX_PATH at the install prefix) or as a target a larger
# build already created.
if(NOT TARGET wxl::core)
    find_package(wxl CONFIG REQUIRED COMPONENTS core)
endif()

add_subdirectory(src)
add_subdirectory(bench)

if(BUILD_TESTING)
    add_subdirectory(test)
endif()

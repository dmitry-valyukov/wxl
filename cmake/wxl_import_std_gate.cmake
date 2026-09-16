# /cmake/wxl_import_std_gate.cmake
#
# Opens CMake's experimental `import std;` gate (needed by CXX_MODULE_STD).
#
# The gate variable must hold the exact UUID of the installed CMake release, and
# it must be set BEFORE the CXX language is enabled -- i.e. before project(), not
# after it. The UUID changes with every CMake release (it is CMake's way of
# forcing projects to re-confirm they accept the churn). It is documented in
# CMake's own Help/dev/experimental.rst, and can also be read straight out of
# cmake.exe: it is the UUID stored immediately before the string
# "CMAKE_EXPERIMENTAL_CXX_IMPORT_STD". Add a branch below when bumping CMake --
# a UUID that belongs to another release is silently ignored, and the failure
# surfaces far away, as "CXX_MODULE_STD requires toolchain support".
#
# The compiler detection caches whether the gate was open, so a build tree
# configured once without it keeps failing until it is deleted.
#
# Include this file from the top-level CMakeLists.txt *before* project(). When
# wxl is consumed as a subproject (FetchContent / add_subdirectory), the
# consumer's project() has already run by the time this file is read, so the
# gate can no longer be opened from here -- the consumer has to set it in its own
# top-level CMakeLists.txt. Rather than letting that surface later as a confusing
# "no module named std" failure, this file says exactly what to add and where.

include_guard(GLOBAL)

if(CMAKE_VERSION VERSION_GREATER_EQUAL 4.4 AND CMAKE_VERSION VERSION_LESS 4.5)
    set(WXL_IMPORT_STD_UUID "f35a9ac6-8463-4d38-8eec-5d6008153e7d")
elseif(CMAKE_VERSION VERSION_GREATER_EQUAL 4.1 AND CMAKE_VERSION VERSION_LESS 4.4)
    set(WXL_IMPORT_STD_UUID "d0edc3af-4c50-42ea-a356-e2862fe7a444")
else()
    message(FATAL_ERROR
        "\n[ERROR]: No known CMAKE_EXPERIMENTAL_CXX_IMPORT_STD UUID for CMake ${CMAKE_VERSION}.\n"
        "`import std;` (CXX_MODULE_STD) cannot be enabled without it.\n"
        "Look the UUID up in CMake's Help/dev/experimental.rst and add a branch\n"
        "for this release in cmake/wxl_import_std_gate.cmake.\n"
    )
endif()

# `import std;` is supported by the Ninja generators only: the Visual Studio
# generators cannot build BMIs for IMPORTED targets (see CMake's
# cmake-cxxmodules(7) manual). Fail here rather than deep inside the build.
#if(CMAKE_GENERATOR MATCHES "Visual Studio")
#    message(FATAL_ERROR
#        "\n[ERROR]: wxl requires a Ninja generator (\"Ninja\" or \"Ninja Multi-Config\").\n"
#        "The Visual Studio generators do not support `import std;` nor building\n"
#        "BMIs for imported targets, so the module build cannot work there.\n"
#        "Current generator: ${CMAKE_GENERATOR}\n"
#    )
#endif()

if(DEFINED CMAKE_EXPERIMENTAL_CXX_IMPORT_STD)
    if(NOT CMAKE_EXPERIMENTAL_CXX_IMPORT_STD STREQUAL WXL_IMPORT_STD_UUID)
        message(FATAL_ERROR
            "\n[ERROR]: CMAKE_EXPERIMENTAL_CXX_IMPORT_STD holds a UUID that does not\n"
            "match CMake ${CMAKE_VERSION}.\n"
            "  expected: ${WXL_IMPORT_STD_UUID}\n"
            "  found:    ${CMAKE_EXPERIMENTAL_CXX_IMPORT_STD}\n"
        )
    endif()
elseif(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    # wxl is the top-level project and this file is included before project(),
    # so the gate can still be opened on our own.
    set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "${WXL_IMPORT_STD_UUID}")
else()
    message(FATAL_ERROR
        "\n[ERROR]: wxl is being built as a subproject, but the experimental\n"
        "`import std;` gate is not open. It has to be set before your project()\n"
        "call -- which has already run, so wxl cannot do it for you.\n"
        "\n"
        "Add this ABOVE project() in your top-level CMakeLists.txt:\n"
        "\n"
        "    set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD \"${WXL_IMPORT_STD_UUID}\")\n"
        "\n"
        "The UUID is tied to CMake ${CMAKE_VERSION} and changes with every CMake\n"
        "release; see cmake/wxl_import_std_gate.cmake in wxl for the current one.\n"
    )
endif()

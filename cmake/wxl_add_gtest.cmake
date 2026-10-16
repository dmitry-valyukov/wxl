# /cmake/wxl_add_gtest.cmake

include_guard(GLOBAL)

macro(wxl_add_gtest target)
    cmake_parse_arguments(ARG "" "" "SRC;DEPS" ${ARGN})

    add_executable(${target} ${ARG_SRC})

    target_link_libraries(${target}
        PRIVATE
            GTest::gtest_main
            ${ARG_DEPS}
    )

    # A test target links wxl.core, and consuming a module built with
    # `import std;` means being scanned and getting the std module too.
    set_target_properties(${target} PROPERTIES
        CXX_MODULE_STD ON
        CXX_SCAN_FOR_MODULES ON
    )

    wxl_set_target_folders(${target} PREFIX "tests")

    # Listed when ctest starts, from the binary of the configuration it tests. A
    # listing taken after the link is one file per target for every configuration
    # of a multi-config tree (CMake before 4.4): after a Release link, ctest -C Debug
    # took the Release binary's tests and ran the Release binary. A program that
    # cannot list itself stops ctest with an error, as it stopped the build.
    gtest_discover_tests(${target} DISCOVERY_MODE PRE_TEST)
endmacro()

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
    gtest_discover_tests(${target})
endmacro()

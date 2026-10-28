# ---- CMakeList.txt : wxl.test ----
#
# What the tests and the benchmarks of wxl are written with, and those of a project that adds wxl:
#
#   wxl::test        headers alone, linked by whoever needs them: sta_box.h, the stress-test harness
#                    (stress_test.h: STRESS_TEST_CASE, become_true, the timeouts the timing-sensitive
#                    tests agree on) and the running test's own directory (test_directory.h). A
#                    benchmark has no GoogleTest and uses sta_box.h alone; the other two include gtest
#                    and are for a test, which links wxl::test.gtest.
#   wxl::test.gtest  the global fixture of a GoogleTest program (test_initializer.cpp): the C locale
#                    set before the tests, and the call stack printed on a failed assert. Brings
#                    wxl::test and GTest::gtest; the program takes its main() from GTest::gtest_main,
#                    as wxl_add_gtest links it, or defines its own.
#
# The fixture is an object library rather than a static one: a gtest environment registers itself
# from the initializer of a global, which nothing in the program refers to, and the linker takes from
# a static library only the objects that something refers to. The objects of an object library go
# into each target that links it directly -- through another library's usage requirements they do
# not -- so a test program names wxl::test.gtest itself.
add_library(wxl.test INTERFACE)
add_library(wxl::test ALIAS wxl.test)

target_include_directories(wxl.test INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(wxl.test INTERFACE wxl::core)

# Where there is GoogleTest: the tree's own when its tests are built (cmake/wxl_gtest.cmake, WXL_TESTS
# at the root), or one a project that adds wxl made available before it. Out of ALL: a project that
# has GoogleTest for tests of its own and does not link the fixture should not compile it.
if(TARGET GTest::gtest)
    add_library(wxl.test.gtest OBJECT test_initializer.cpp)
    add_library(wxl::test.gtest ALIAS wxl.test.gtest)

    target_link_libraries(wxl.test.gtest PUBLIC GTest::gtest wxl::test)

    # The fixture imports std and wxl.core, as wxl_add_gtest's programs do.
    set_target_properties(wxl.test.gtest PROPERTIES
        CXX_MODULE_STD ON
        CXX_SCAN_FOR_MODULES ON
        EXCLUDE_FROM_ALL ON
    )
endif()

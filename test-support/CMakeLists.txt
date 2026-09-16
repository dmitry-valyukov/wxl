# ---- CMakeList.txt : test-support ----
#
# Test infrastructure shared by every subproject's test executable: the gtest
# global fixture and the stress-test harness (STRESS_TEST_CASE, become_true,
# the timeouts the timing-sensitive tests agree on).
#
# An INTERFACE target rather than a library: a gtest environment has to be
# registered in the executable that runs it, so the fixture is compiled into
# each of them.
add_library(wxl.test-support INTERFACE)
add_library(wxl::test-support ALIAS wxl.test-support)

target_sources(wxl.test-support INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/test_initializer.cpp"
)

target_include_directories(wxl.test-support INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(wxl.test-support INTERFACE
    wxl::core
    GTest::gtest
    GTest::gtest_main
)

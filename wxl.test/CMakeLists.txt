# ---- CMakeList.txt : wxl.test ----
#
# Helpers every test and benchmark of wxl may use: headers alone, linked by whoever needs them.
# Beside test-support rather than in it: test-support is the gtest side -- the global fixture compiled
# into each test executable, the stress harness, the test's own directory -- and a benchmark has no
# gtest to take it from.
add_library(wxl.test INTERFACE)
add_library(wxl::test ALIAS wxl.test)

target_include_directories(wxl.test INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(wxl.test INTERFACE wxl::core)

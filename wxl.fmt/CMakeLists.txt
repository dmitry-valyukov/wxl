# ---- CMakeList.txt : wxl.fmt ----
#
# The text buffer and the builder over fmt, kept apart from wxl.core so that
# the core depends on nothing but the std module: fmt's headers are compiled by
# every consumer of a module whose interface names them, and only the modules
# that assemble text need to.
#
# A subdirectory of the tree's project (the root CMakeLists.txt).

add_subdirectory(src)

if(WXL_TESTS)
    add_subdirectory(tests)
endif()

if(WXL_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()

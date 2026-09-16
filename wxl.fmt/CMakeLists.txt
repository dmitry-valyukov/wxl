# ---- CMakeList.txt : wxl.fmt ----
#
# The text buffer and the builder over fmt, kept apart from wxl.core so that
# the core depends on nothing but the std module: fmt's headers are compiled by
# every consumer of a module whose interface names them, and only the modules
# that assemble text need to.
#
# Built as part of the tree's common top-level project; it has no standalone
# configuration of its own, because that setup already lives at the root.

# Off by default, like wxl.core's: the benchmark is a measurement tool, run
# deliberately, and an ordinary build has no reason to spend time on it.
option(WXL_BUILD_BENCHMARKS "Build the wxl benchmark executables." OFF)

add_subdirectory(src)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

if(WXL_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()

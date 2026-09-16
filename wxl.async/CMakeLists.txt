# ---- CMakeList.txt : wxl.async ----
#
# The layer above wxl.core's primitives: everything with non-trivial behaviour
# of its own -- the future family, task_tree, threads and the lock-free
# machinery built on them -- plus the GUI coroutine library and what is left of
# the component model: component and threaded_component, which sta_loop's worker
# and wxl.logging's async_output stand on.
#
# The rest of the model -- the containers, the hosted half, the apartments, the
# IOCP port and the task queue -- had no caller in any built tree and now waits
# in .claude/wxl.components beside wxl.io, the module it was written for. The
# survey is in .claude/TODO/wxl.async.txt.
#
# Built as part of the tree's common top-level project; it has no standalone
# configuration of its own, because consuming wxl.core means consuming a C++20
# module and that setup already lives at the root.
option(WXL_ENABLE_PCH "Use a precompiled header (src/pch.h)." ON)

# Off by default: the benchmarks are measurement tools, run deliberately, and
# an ordinary build has no reason to spend time on them.
option(WXL_BUILD_BENCHMARKS "Build the wxl.async benchmark executables." OFF)

add_subdirectory(src)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

if(WXL_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()

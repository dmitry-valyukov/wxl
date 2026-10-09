# ---- CMakeList.txt : wxl.async ----
#
# The layer above wxl.core's primitives: everything with non-trivial behaviour
# of its own -- the future family, task_tree, threads and the lock-free
# machinery built on them -- plus the GUI coroutine library and what is left of
# the component model: component and threaded_component, which sta_loop's worker
# and wxl.logging's async_output stand on.
#
# The rest of the model -- the containers, the hosted half, the apartments, the
# IOCP port and the task queue -- had no caller in any built tree and is kept
# outside this one, together with wxl.io, the module it was written for.
#
# Built as part of the tree's common top-level project; it has no standalone
# configuration of its own, because consuming wxl.core means consuming a C++20
# module and that setup already lives at the root.
option(WXL_ENABLE_PCH "Use a precompiled header (src/pch.h)." ON)

# Off by default, an experiment beside the explicit token: with it on, every task takes
# the cancellation token of the coroutine that calls it, and every co_await in a task or
# a detached task stands under its frame's token -- two words more in every promise and
# a few instructions more on every co_await, whether or not anything is ever cancelled.
option(WXL_AMBIENT_CANCELLATION "Give every coroutine of wxl.async an implicit cancellation token (experiment)." OFF)

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

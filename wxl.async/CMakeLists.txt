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
# A subdirectory of the tree's project (the root CMakeLists.txt).
option(WXL_ENABLE_PCH "Use a precompiled header (src/pch.h)." ON)

# Off by default: the checks of how coroutines are used -- a task awaited twice at
# once, a value read twice, a task dropped while it runs or while somebody joins it,
# a task used after a move -- are then assert's, a Debug build's alone, and a
# Release build has neither a byte nor an instruction of them. On, they stay in every
# build, Release included, and a broken one ends the process through wxl::core::abort,
# naming the reason and the line of the code that broke the rule where the language
# gives it (std::source_location).
option(WXL_STRICT_CORO "Keep the coroutine checks of wxl.async in every build (defines STRICT_CORO)." OFF)

add_subdirectory(src)

if(WXL_TESTS)
    add_subdirectory(tests)
endif()

if(WXL_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()

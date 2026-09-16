# ---- CMakeList.txt : wxl.logging ----
#
# The log. A port of a library written in 2003-era C++, whose messages were
# built with operator<< into a typed tuple and formatted only if the level let
# them through; here the same promise -- pay nothing for a message nobody will
# read, and never interleave two threads' halves of a line -- is kept with a
# format string, checked while it compiles.
#
# It sits on wxl.core and wxl.fmt for the synchronous half, and on wxl.async
# for the thread the asynchronous half writes from.

add_subdirectory(src)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

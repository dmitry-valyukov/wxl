# ---- CMakeList.txt : wxl.json ----
#
# The JSON reader: a tree of views into a buffer the reader owns, in an arena
# from wxl.xml.

# The policies of this directory are those of CMake 3.28, and not the root's:
# the later ones change what its targets compile (CMP0204, for one, defines
# _MBCS in each target created under it).
cmake_minimum_required(VERSION 3.28)

add_subdirectory(src)

if(WXL_TESTS)
    add_subdirectory(tests)
endif()

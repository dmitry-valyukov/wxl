# ---- CMakeList.txt : wxl.highlight ----
#
# Syntax highlighting: language tables and a scanner without regular expressions.
# Depends on nothing else in wxl, only on the standard library.

# The policies of this directory are those of CMake 3.28, and not the root's:
# the later ones change what its targets compile (CMP0204, for one, defines
# _MBCS in each target created under it).
cmake_minimum_required(VERSION 3.28)

add_subdirectory(src)

if(WXL_TESTS)
    add_subdirectory(tests)
endif()

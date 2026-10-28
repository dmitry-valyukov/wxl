# ---- CMakeList.txt : wxl.xml ----
#
# The XML reader the generator uses to walk WinUI3's XAML resource
# dictionaries -- the one input of the project that metadata does not carry.
# The reader allocates through the STA pool of wxl.core.

# The policies of this directory are those of CMake 3.28, and not the root's:
# the later ones change what its targets compile (CMP0204, for one, defines
# _MBCS in each target created under it).
cmake_minimum_required(VERSION 3.28)

add_subdirectory(src)

# Built whenever the tree is the project; in a project that adds wxl, only with
# the other benchmarks (WXL_BUILD_BENCHMARKS).
if(PROJECT_IS_TOP_LEVEL OR WXL_BUILD_BENCHMARKS)
    add_subdirectory(bench)
endif()

if(WXL_TESTS)
    add_subdirectory(test)
endif()

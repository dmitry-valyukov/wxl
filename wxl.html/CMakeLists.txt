# ---- CMakeList.txt : wxl.html ----
#
# The HTML subset behind HtmlBlock: a tolerant parser for foreign markup --
# forum posts, chat messages -- producing a lightweight DOM for one walk.
# The specification is design.md next to this file. The tree is carved from an
# arena over the STA pool of wxl.core.

# The policies of this directory are those of CMake 3.28, and not the root's:
# the later ones change what its targets compile (CMP0204, for one, defines
# _MBCS in each target created under it).
cmake_minimum_required(VERSION 3.28)

add_subdirectory(src)

if(WXL_TESTS)
    add_subdirectory(tests)
endif()

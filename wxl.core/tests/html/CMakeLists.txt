# The policies of this directory are those of CMake 3.28, and not the root's:
# the later ones change what its targets compile (CMP0204, for one, defines
# _MBCS in each target created under it).
cmake_minimum_required(VERSION 3.28)

wxl_add_gtest(wxl.html.tests
    SRC
        parse_test.cpp
    DEPS
        wxl::core
)

# The policies of this directory are those of CMake 3.28, and not the root's:
# the later ones change what its targets compile (CMP0204, for one, defines
# _MBCS in each target created under it).
cmake_minimum_required(VERSION 3.28)

add_executable(wxl.xml.bench bench.cpp)

target_link_libraries(wxl.xml.bench PRIVATE wxl::core)

# Consuming a module built with `import std;` means being scanned for imports
# and getting the std module too; scanning an ordinary source is CMP0155-gated
# and the project leaves that policy at OLD.
set_target_properties(wxl.xml.bench PROPERTIES
    CXX_MODULE_STD ON
    CXX_SCAN_FOR_MODULES ON
)

# The manifest of wxl's executables, as every tool of the tree carries it.
wxl_target_manifest(wxl.xml.bench)
wxl_set_target_folders(wxl.xml.bench)

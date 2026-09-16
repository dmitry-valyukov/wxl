add_executable(wxl.xml.bench bench.cpp)

target_link_libraries(wxl.xml.bench PRIVATE wxl::xml wxl::core)

# Consuming a module built with `import std;` means being scanned for imports
# and getting the std module too; scanning an ordinary source is CMP0155-gated
# and the project leaves that policy at OLD.
set_target_properties(wxl.xml.bench PROPERTIES
    CXX_MODULE_STD ON
    CXX_SCAN_FOR_MODULES ON
)

wxl_set_target_folders(wxl.xml.bench)

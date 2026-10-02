add_executable(wxl.gen.common.bench bench.cpp)

target_link_libraries(wxl.gen.common.bench PRIVATE wxl.gen.common wxl::xml wxl::core)

set_target_properties(wxl.gen.common.bench PROPERTIES
    CXX_MODULE_STD ON
    CXX_SCAN_FOR_MODULES ON
)

wxl_set_target_folders(wxl.gen.common.bench)

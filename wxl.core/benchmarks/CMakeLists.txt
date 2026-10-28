# Measurement tools, built only when WXL_BUILD_BENCHMARKS is on: each is a plain
# executable timing itself, because what is compared here is a handful of
# nanoseconds per operation and no test framework should be standing in that.
# Each carries the manifest of wxl's executables (wxl_target_manifest).

add_executable(wxl.core.sta-allocator-benchmark sta_allocator_benchmark.cpp)

target_link_libraries(wxl.core.sta-allocator-benchmark PRIVATE wxl::core)

wxl_target_manifest(wxl.core.sta-allocator-benchmark)
wxl_set_target_folders(wxl.core.sta-allocator-benchmark)

add_executable(wxl.core.sta-scenario-benchmark sta_allocator_scenario_benchmark.cpp)

target_link_libraries(wxl.core.sta-scenario-benchmark PRIVATE wxl::core)

wxl_target_manifest(wxl.core.sta-scenario-benchmark)
wxl_set_target_folders(wxl.core.sta-scenario-benchmark)

add_executable(wxl.core.sta-bump-only-benchmark sta_bump_only_benchmark.cpp)

target_link_libraries(wxl.core.sta-bump-only-benchmark PRIVATE wxl::core)

wxl_target_manifest(wxl.core.sta-bump-only-benchmark)
wxl_set_target_folders(wxl.core.sta-bump-only-benchmark)

add_executable(wxl.core.sta-get-benchmark sta_allocator_get_benchmark.cpp)

target_link_libraries(wxl.core.sta-get-benchmark PRIVATE wxl::core)

wxl_target_manifest(wxl.core.sta-get-benchmark)
wxl_set_target_folders(wxl.core.sta-get-benchmark)

add_executable(wxl.core.page-allocation-benchmark page_allocation_benchmark.cpp)

target_link_libraries(wxl.core.page-allocation-benchmark PRIVATE wxl::core)

wxl_target_manifest(wxl.core.page-allocation-benchmark)
wxl_set_target_folders(wxl.core.page-allocation-benchmark)

# The text half of the module: the transcoders against MultiByteToWideChar and
# WideCharToMultiByte. What it found is in doc/unicode.md.
add_executable(wxl.core.transcode-benchmark transcode_benchmark.cpp)

target_link_libraries(wxl.core.transcode-benchmark PRIVATE wxl::core)

wxl_target_manifest(wxl.core.transcode-benchmark)
wxl_set_target_folders(wxl.core.transcode-benchmark)

# The benchmarks of the areas of the module, one directory each.
add_subdirectory(fmt)
add_subdirectory(async)

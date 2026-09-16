# Measurement tools, built only when WXL_BUILD_BENCHMARKS is on: each is a plain
# executable timing itself, because what is compared here is a handful of
# nanoseconds per operation and no test framework should be standing in that.

add_executable(wxl.core.sta-allocator-benchmark sta_allocator_benchmark.cpp)

target_link_libraries(wxl.core.sta-allocator-benchmark PRIVATE wxl::core)

wxl_set_target_folders(wxl.core.sta-allocator-benchmark)

add_executable(wxl.core.sta-scenario-benchmark sta_allocator_scenario_benchmark.cpp)

target_link_libraries(wxl.core.sta-scenario-benchmark PRIVATE wxl::core)

wxl_set_target_folders(wxl.core.sta-scenario-benchmark)

add_executable(wxl.core.sta-bump-only-benchmark sta_bump_only_benchmark.cpp)

target_link_libraries(wxl.core.sta-bump-only-benchmark PRIVATE wxl::core)

wxl_set_target_folders(wxl.core.sta-bump-only-benchmark)

# Lets the compiler assume lzcnt is available and drop the runtime __isa_available dispatch
# std::countl_zero's MSVC STL implementation otherwise contains (see pool_index()'s codegen
# investigation) -- the pure bump-allocator path this benchmark isolates is exactly where that
# per-call branch would show up.
#
# Set on the source rather than on the target: a target-level compile option is part of what
# CMake fingerprints when it synthesizes the `std` module target, so putting it there buys this
# one benchmark a whole second build of the std module interface. On the source it reaches the
# same translation unit and costs nothing.
if (MSVC)
    set_source_files_properties(sta_bump_only_benchmark.cpp
        PROPERTIES COMPILE_OPTIONS /arch:AVX2)
endif()

add_executable(wxl.core.sta-get-benchmark sta_allocator_get_benchmark.cpp)

target_link_libraries(wxl.core.sta-get-benchmark PRIVATE wxl::core)

wxl_set_target_folders(wxl.core.sta-get-benchmark)

add_executable(wxl.core.page-allocation-benchmark page_allocation_benchmark.cpp)

target_link_libraries(wxl.core.page-allocation-benchmark PRIVATE wxl::core)

wxl_set_target_folders(wxl.core.page-allocation-benchmark)

# The text half of the module: the transcoders against MultiByteToWideChar and
# WideCharToMultiByte. What it found is in doc/unicode.md.
add_executable(wxl.core.transcode-benchmark transcode_benchmark.cpp)

target_link_libraries(wxl.core.transcode-benchmark PRIVATE wxl::core)

wxl_set_target_folders(wxl.core.transcode-benchmark)

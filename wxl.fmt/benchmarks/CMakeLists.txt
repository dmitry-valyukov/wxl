# A measurement tool, built only when WXL_BUILD_BENCHMARKS is on: a plain
# executable timing itself, because what is compared here is a handful of
# nanoseconds per call and no test framework should be standing in that.
# It carries the manifest of wxl's executables (wxl_target_manifest).

add_executable(wxl.fmt.text-builder-benchmark text_builder_benchmark.cpp)

# wxl::core for the STA pool the pooled rows allocate from.
target_link_libraries(wxl.fmt.text-builder-benchmark PRIVATE wxl::fmt wxl::core)

wxl_target_manifest(wxl.fmt.text-builder-benchmark)
wxl_set_target_folders(wxl.fmt.text-builder-benchmark)

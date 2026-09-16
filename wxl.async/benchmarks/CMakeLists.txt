# Measurement tools, built only when WXL_BUILD_BENCHMARKS is on: each is a plain
# executable timing itself, because what is compared here is a handful of
# nanoseconds per element and no test framework should be standing in that.

add_executable(wxl.async.mpsc-queue-benchmark mpsc_queue_benchmark.cpp)

target_link_libraries(wxl.async.mpsc-queue-benchmark PRIVATE wxl::async)

wxl_set_target_folders(wxl.async.mpsc-queue-benchmark)

add_executable(wxl.async.spsc-queues-benchmark spsc_queues_benchmark.cpp)

target_link_libraries(wxl.async.spsc-queues-benchmark PRIVATE wxl::async)

wxl_set_target_folders(wxl.async.spsc-queues-benchmark)

add_executable(wxl.async.threaded-allocator-benchmark threaded_allocator_benchmark.cpp)

target_link_libraries(wxl.async.threaded-allocator-benchmark PRIVATE wxl::async)

wxl_set_target_folders(wxl.async.threaded-allocator-benchmark)

add_executable(wxl.async.coroutine-frame-benchmark coroutine_frame_benchmark.cpp)

target_link_libraries(wxl.async.coroutine-frame-benchmark PRIVATE wxl::async)

wxl_set_target_folders(wxl.async.coroutine-frame-benchmark)

# The loop end to end: what one co_await costs through it, with the real sta_loop in the
# first row and the variants beside it. synchronization.lib is WaitOnAddress, which the
# address rows stand on.
add_executable(wxl.async.sta-loop-benchmark sta_loop_benchmark.cpp)

target_link_libraries(wxl.async.sta-loop-benchmark PRIVATE wxl::async synchronization)

wxl_set_target_folders(wxl.async.sta-loop-benchmark)

# The wake on its own: how long a thread takes to be woken, by primitive and by how long
# it slept -- the number the loop's round trip is made of.
add_executable(wxl.async.wake-latency-benchmark wake_latency_benchmark.cpp)

target_link_libraries(wxl.async.wake-latency-benchmark PRIVATE wxl::async synchronization)

wxl_set_target_folders(wxl.async.wake-latency-benchmark)

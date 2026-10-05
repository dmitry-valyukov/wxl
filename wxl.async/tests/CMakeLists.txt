# MSVC 14.51 miscompiles a translation unit that instantiates future::next_unwrap()
# while inlining is off: intrusive_ptr<future_shared_state>'s module-attached copy
# constructor and destructor land in two COMDATs of one object file, and the linker
# refuses that with LNK1179. Allowing inline functions to be expanded avoids it, and
# replacing the flag rather than appending one keeps cl from warning about the
# override on every file.
string(REPLACE "/Ob0" "/Ob1" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")

add_executable(wxl.async.tests
    bump_buffer_tests.cpp
    completion_counter_tests.cpp
    detached_task_tests.cpp
    drain_stack_tests.cpp
    future_tests.cpp
    future_tests_t.cpp
    mpsc_channel_tests.cpp
    mpsc_queue_tests.cpp
    one_shot_event_tests.cpp
    pool_tests.cpp
    public_future_tests.cpp
    async_directory_tests.cpp
    async_file_pipe_tests.cpp
    async_file_tests.cpp
    spsc_channel_tests.cpp
    spsc_queue_tests.cpp
    sta_loop_abandon_tests.cpp
    sta_loop_tests.cpp
    countdown_event_tests.cpp
    task_tests.cpp
    task_tree_core_tests.cpp
    task_tree_tests.cpp
    thread_core_tests.cpp
    thread_group_core_tests.cpp
    thread_group_tests.cpp
    threaded_allocator_tests.cpp
    turnstile_tests.cpp

    # What is left of the component model here: component itself and threaded_component.
    # The suites for the containers, the hosted half and the apartments left the tree
    # with the code they exercise.
    component_core_tests.cpp
    exception_matrix_helper.cpp
    shared_data_tests.cpp
    sync_component_tests.cpp
    system_exception_tests.cpp
    threaded_component_core_tests.cpp
    threaded_component_create_destroy_tests.cpp
    threaded_component_error_from_barrier_tests.cpp
    threaded_component_exception_in_thread_tests.cpp
    threaded_component_full_exception_matrix_tests.cpp
    threaded_component_self_delete_tests.cpp
    threaded_component_shared_resources_tests.cpp
    threaded_component_simple_async_start_stop_tests.cpp
    threaded_component_simple_start_stop_tests.cpp
    threaded_component_start_async_without_stop_tests.cpp
    threaded_component_start_stop_events_tests.cpp
    threaded_component_start_stop_request_tests.cpp
    threaded_component_start_stop_tickets_private_thread_tests.cpp
    threaded_component_start_without_stop_tests.cpp
    threaded_component_stop_by_error_tests.cpp
    threaded_test_component.cpp
)

target_link_libraries(wxl.async.tests
    PRIVATE
        wxl::async
        wxl::test-support
)

# The queues keep their hot fields on cache lines of their own with alignas, and C4324
# ("structure was padded due to alignment specifier", level 4) is silenced at each of
# those declarations, since the padding it reports is the intent. That silence has to
# reach the point of instantiation through the module interface, and this is where it is
# checked: the suites instantiate every one of those classes, and here C4324 is pulled up
# to level 1 and made an error. A failure names a padded class the pragma no longer covers.
target_compile_options(wxl.async.tests PRIVATE /w14324 /we4324)

wxl_set_target_folders(wxl.async.tests)

include(GoogleTest)

gtest_discover_tests(
    wxl.async.tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

# The loop on a dispatcher queue -- start_dispatched(), the shape an application with a
# window runs -- in a binary of its own, for the same reason as the one below. The file
# and directory suites are built here a second time: under this loop what goes by a name
# is carried out on the thread pool and comes back through the queue.
add_executable(wxl.async.dispatched-tests
    async_directory_tests.cpp
    async_file_pipe_tests.cpp
    async_file_tests.cpp
    sta_loop_dispatched_tests.cpp
)

target_compile_definitions(wxl.async.dispatched-tests PRIVATE WXL_ASYNC_TESTS_DISPATCHED)

target_link_libraries(wxl.async.dispatched-tests
    PRIVATE
        wxl::async
        wxl::test-support
)

wxl_set_target_folders(wxl.async.dispatched-tests)

gtest_discover_tests(
    wxl.async.dispatched-tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

# The driven shape of sta_loop -- start_driven(), a dispatcher callback, run_pending()
# -- in a binary of its own: a loop starts once per process and cannot be restarted,
# and the binary above starts the sleeping shape.
add_executable(wxl.async.driven-tests
    sta_loop_driven_tests.cpp
)

target_link_libraries(wxl.async.driven-tests
    PRIVATE
        wxl::async
        wxl::test-support
)

wxl_set_target_folders(wxl.async.driven-tests)

gtest_discover_tests(
    wxl.async.driven-tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

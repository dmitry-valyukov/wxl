wxl_add_module_library(wxl.async STATIC
    PUBLIC
        wxl.async.ixx

        async_directory.ixx
        async_file.ixx
        async_op.ixx
        awaitable.ixx
        barrier.ixx
        bump_buffer.ixx
        cancellation.ixx
        completion_counter.ixx
        countdown_event.ixx
        detached_task.ixx
        drain_stack.ixx
        future.ixx
        future_shared_state.ixx
        mpsc_channel.ixx
        mpsc_queue.ixx
        one_shot_event.ixx
        pool.ixx
        safe_pool.ixx
        spsc_channel.ixx
        spsc_queue.ixx
        spsc_queue_reference_implementation.ixx
        sta_loop.ixx
        task.ixx
        task_tree.ixx
        thread.ixx
        thread_group.ixx
        threaded_allocator.ixx
        turnstile.ixx

        # What is left of the component model: the base, and threaded_component, the one
        # derivative with callers of its own. The containers, the hosted half and the
        # apartments are kept outside this tree.
        # system_exception stays because async_file and async_directory throw it.
        component.ixx
        stop_reason.ixx
        system_exception.ixx
        threaded_component.ixx
    SRC
        pch.h

        async_directory.cpp
        async_file.cpp
        future.cpp
        future_shared_state.cpp
        one_shot_event.cpp
        task_tree.cpp
        thread.cpp
        thread_group.cpp
        threaded_allocator.cpp

        # The private half of component: its state, shared by every component .cpp
        # that touches it, and included from inside the module purview.
        component.impl.h

        component.cpp
        stop_reason.cpp
        system_exception.cpp
        threaded_component.cpp
)

add_library(wxl::async ALIAS wxl.async)

set_target_properties(wxl.async PROPERTIES
    EXPORT_NAME async
)

# PUBLIC: the module interfaces name wxl.core's types, so whoever compiles
# them needs the module and the headers its global module fragments include
# (abi.h, platform.h travel with wxl::core's header file set).
target_link_libraries(wxl.async PUBLIC wxl::core)

# PRIVATE: every worker thread brings COM up as MTA (thread_group.cpp), so a
# worker body can make WinRT calls off the UI thread -- Win2D, say.
# CoInitializeEx/CoUninitialize live in ole32.
target_link_libraries(wxl.async PRIVATE ole32)

# See the same block in wxl.core/src/CMakeLists.txt: MSVC injects the PCH ahead
# of the module declaration, which is where these sources include the headers
# by hand anyway.
if(WXL_ENABLE_PCH)
    target_precompile_headers(wxl.async PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/pch.h")
endif()

wxl_set_target_folders(wxl.async)

# A consumer compiles wxl's module interfaces in its own build tree, so the
# language level has to travel with the target.
target_compile_features(wxl.async PUBLIC cxx_std_23)

export module wxl.async;

// The asynchronous machinery proper: the future family, task trees, the task queue,
// threads, and the lock-free primitives underneath them.
export import :async_directory;
export import :async_file;
export import :async_op;
export import :awaitable;
export import :barrier;
export import :bump_buffer;
export import :cancellation;
export import :completion_counter;
export import :countdown_event;
export import :drain_stack;
export import :future;
export import :future_shared_state;
export import :mpsc_channel;
export import :mpsc_queue;
export import :one_shot_event;
export import :pool;
export import :safe_pool;
export import :spsc_channel;
export import :spsc_queue;
export import :spsc_queue_reference_implementation;
export import :sta_loop;
export import :managed_task;
export import :task;
export import :task_tree;
export import :thread;
export import :thread_group;
export import :threaded_allocator;
export import :turnstile;

// What is left of the component model: the base, and the one derivative that still has
// callers -- threaded_component, on which wxl.logging's async_output and sta_loop's
// worker stand.
//
// The rest of it -- the static and dynamic containers, the externally managed component,
// the hosted half with its apartments, and the IOCP completion port -- has no caller in
// any built tree and is kept outside this one, together with wxl.io, the module it
// was written for.
export import :component;
export import :stop_reason;
export import :system_exception;
export import :threaded_component;

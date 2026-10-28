export module wxl.core;

// Not a partition: the C type names are their own little module, so that a consumer
// wanting only them need not take all of wxl.core. Re-exported here because everything
// already imports wxl.core, and this is what makes `size_t` spellable everywhere.
export import wxl.stdint;

export import :allocator;
export import :atomic_trigger;
export import :binding_scope;
export import :bit_vector;
export import :checks;
export import :compressed_optional;
export import :directory;
export import :environment;
export import :event;
export import :file;
export import :function;
export import :grapheme;
export import :hevent;
export import :hstring;
export import :intrusive_list;
export import :intrusive_slist;
export import :intrusive_ptr;
export import :lightweight_semaphore;
export import :lock_guard;
export import :member_offset;
export import :module_cleanup;
export import :mutex;
export import :not_null;
export import :noncopyable;
export import :numbers;
export import :observable;
export import :observable_list;
export import :path;
export import :pool_ptr;
export import :refcounted;
export import :semaphore;
export import :spin_lock;
export import :sta_allocator;
export import :sta_stl;
export import :strings;
export import :sync_root;
export import :thread_guard;
export import :thread_heap;
export import :thread_id;
export import :time;
export import :timeout_timer;
export import :traceable;
export import :unicode;
export import :weak_link;
export import :zstring_view;

// fmt -- the buffer text is assembled in, the builder over it, and format().
export import :fmt.text_buffer;
export import :fmt.text_builder;
export import :fmt.format;

// async -- the future family, task trees, the coroutines and their loop, threads, lock-free queues.
export import :async.async_directory;
export import :async.async_file;
export import :async.async_mutex;
export import :async.async_op;
export import :async.async_semaphore;
export import :async.barrier;
export import :async.bump_buffer;
export import :async.cancellation;
export import :async.completion_counter;
export import :async.coroutine_checks;
export import :async.countdown_event;
export import :async.drain_stack;
export import :async.future;
export import :async.future_shared_state;
export import :async.overlapped_op;
export import :async.io_port;
export import :async.mpsc_channel;
export import :async.mpsc_queue;
export import :async.one_shot_event;
export import :async.pool;
export import :async.safe_pool;
export import :async.spsc_channel;
export import :async.spsc_queue;
export import :async.spsc_queue_reference_implementation;
export import :async.sta_loop;
export import :async.sta_thread;
export import :async.task;
export import :async.task_tree;
export import :async.thread;
export import :async.thread_group;
export import :async.threaded_allocator;
export import :async.turnstile;
export import :async.component;
export import :async.stop_reason;
export import :async.last_error;
export import :async.threaded_component;

// logging -- a log built around one call per message.
export import :logging.severity;
export import :logging.entry;
export import :logging.facility;
export import :logging.output;
export import :logging.console_output;
export import :logging.file_output;
export import :logging.logger;
export import :logging.async_logger;

// xml -- an XML reader whose tree is views into the document it takes over.
export import :xml.arena;
export import :xml.core;
export import :xml.exception;
export import :xml.node;
export import :xml.encoding;
export import :xml.parser;
export import :xml.document;

// json -- a JSON reader of the same make, its tree carved from the same arena.
export import :json.exception;
export import :json.value;
export import :json.document;

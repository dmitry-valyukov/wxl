export module wxl.core;

// Not a partition: the C type names are their own little module, so that a consumer
// wanting only them need not take all of wxl.core. Re-exported here because everything
// already imports wxl.core, and this is what makes `size_t` spellable everywhere.
export import wxl.stdint;

export import :allocator;
export import :atomic_trigger;
export import :bit_vector;
export import :checks;
export import :compressed_optional;
export import :directory;
export import :environment;
export import :event;
export import :file;
export import :function;
export import :hevent;
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
export import :path;
export import :pool_ptr;
export import :refcounted;
export import :semaphore;
export import :spin_lock;
export import :sta_allocator;
export import :strings;
export import :sync_root;
export import :thread_guard;
export import :thread_heap;
export import :thread_id;
export import :time;
export import :timeout_timer;
export import :traceable;
export import :unicode;

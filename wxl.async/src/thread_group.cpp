module;
#include "pch.h"
#include <objbase.h>   // CoInitializeEx/CoUninitialize: WIN32_LEAN_AND_MEAN прячет их с ole2.h

module wxl.async;
import wxl.core;
import std;

namespace wxl::async {

namespace {

/// Sets the calling thread's debugger-visible name. Best-effort: failures are ignored.
///
/// Checked text and nothing else, so that a name nobody has vouched for is a
/// compilation error at the call rather than a surprise on the new thread,
/// where nobody is looking.
void set_current_thread_name(const core::u8_view name) {
    if (name.empty()) return;

    ::SetThreadDescription(::GetCurrentThread(), name.to_utf16().c_str());
}

/// The same for a name that is already UTF-16. A view has no terminator, and
/// SetThreadDescription takes a pointer and no length, so the copy is what
/// makes one -- a dozen characters, once per thread.
void set_current_thread_name(const core::u16_view name) {
    if (name.empty()) return;

    ::SetThreadDescription(::GetCurrentThread(), core::u16_text(name).c_str());
}

/// Global counter of detached threads. A thread's procedure takes the thread off it as its
/// last step, with release, and the count is read with acquire: a reader that sees zero sees
/// everything every detached thread did before that -- its body, its promise, the destruction
/// of its callable -- since every decrement and increment is a read-modify-write and carries
/// on the release sequences of those before it. So what the threads were given may go once
/// the wait returns.
std::atomic<size_t> s_detached_thread_counter{0};

core::thread_id s_main_thread_id = core::current_thread_id();

/// Will wait for detached threads on program's exit.
struct detached_thread_guard {
    ~detached_thread_guard() {
        try {
            if (!thread_group::join_detached_threads_for(core::duration::from_sec(30)))
                std::cerr << thread_group::detached_thread_count()
                          << " thread(s) unfinished on program exit.\n";
        } catch (const std::exception& ex) {
            std::cerr << "Error in ~detached_thread_guard: " << ex.what() << '\n';
            assert(!"Unexpected exception");
        } catch (...) {
            std::cerr << "Unknown error in ~detached_thread_guard\n";
            assert(!"Unexpected exception");
        }
    }
} s_detached_thread_guard;

enum class thread_state
{
    undefined,
    running,
    finished
};

/// Windows HANDLE RAII wrapper.
class windows_handle : public core::noncopyable
{
public:
    explicit windows_handle(HANDLE handle = INVALID_HANDLE_VALUE) noexcept : handle_(handle) {}

    ~windows_handle() { close_handle_if_needed(); }

    bool valid() const { return (handle_ != INVALID_HANDLE_VALUE); }

    HANDLE release() {
        HANDLE tmp = handle_;
        handle_ = INVALID_HANDLE_VALUE;
        return tmp;
    }

    void reset(HANDLE const handle = INVALID_HANDLE_VALUE) {
        if (handle == handle_) return;

        close_handle_if_needed();
        handle_ = handle;
    }

    // non-const because we return the handle.
    HANDLE get() { return handle_; }

private:
    void close_handle_if_needed() noexcept {
        if (handle_ != INVALID_HANDLE_VALUE) ::CloseHandle(handle_);
    }

    HANDLE handle_;
};

/// Information about the managed thread.
class thread_info : public core::refcounted_mt
{
public:
    explicit thread_info(thread_state state = thread_state::undefined) : thread_state_(state) {}

    /// The state is written by the thread itself and read by whoever reaps it, so the pair
    /// is release/acquire: seeing `finished` has to mean seeing everything the thread body
    /// published before it.
    thread_state state() const { return thread_state_.load(std::memory_order_acquire); }

    void state(thread_state thread_state) {
        thread_state_.store(thread_state, std::memory_order_release);
    }

    /// Stored by the spawning thread while the thread is still suspended, and not changed until
    /// thread_info goes. The thread's start orders the store before everything the thread
    /// does, `finished` included, so whoever has seen `finished` reads the handle with no lock
    /// of its own.
    windows_handle& thread_handle() { return thread_handle_; }

private:
    std::atomic<thread_state> thread_state_;
    windows_handle thread_handle_;
};

using thread_info_ptr = core::intrusive_ptr<thread_info>;

/// Manages ref-counting of thread_info objects.
class thread_info_container : public core::noncopyable
{
public:
    /// What is still here goes with the group: threads that finished and that nobody joined.
    /// The group cannot go any earlier -- a thread holds a reference to it from its spawn
    /// until it has reported its exit -- and letting go of an entry closes the thread's
    /// handle without waiting for the thread, as for a detached one.
    ~thread_info_container() {
        for (thread_info* element : container_) {
            assert(element->state() == thread_state::finished);
            intrusive_ptr_release(element);
        }
    }

    size_t size() const noexcept { return container_.size(); }

    bool empty() const noexcept { return container_.empty(); }

    void add(thread_info* element) {
        assert(contains(element) == false);

        container_.insert(element);
        intrusive_ptr_add_ref(element);
    }

    void remove(thread_info* element) {
        if (container_.erase(element))
            intrusive_ptr_release(element);
        else
            assert(false && "should never be here");
    }

    bool contains(thread_info* element) const {
        return container_.contains(element);
    }

    thread_info_ptr find_and_remove_first_finished() {
        for (auto it = container_.begin(), e = container_.end(); it != e; ++it) {
            if (((*it)->state() == thread_state::finished)) {
                // A thread runs only once its handle is stored, so a finished one has it.
                assert((*it)->thread_handle().valid());

                const bool add_ref_param = false;
                thread_info_ptr tmp(*it, add_ref_param);
                container_.erase(it);
                return tmp;
            }
        }

        return nullptr;
    }

private:
    using container = std::set<thread_info*>;
    container container_;
};
}  // namespace

class thread_starter;

class thread_group::impl : public core::noncopyable
{
public:
    impl() noexcept;

    ~impl();

    /// Spawns managed thread
    void spawn(thread_starter* starter);

    void join_all();

    void close();

    /// Called before start the system thread
    void register_thread(thread_info* created_thread);

    /// Called on unsuccessful start of thread
    void unregister_thread(thread_info* created_thread);

    /// RAII-style guard, notifies thread_group about thread enter/exit
    class thread_enter_exit_guard : public core::noncopyable
    {
    public:
        thread_enter_exit_guard(thread_info* thread_info, thread_group* group)
            : thread_info_(thread_info), group_(group) {
            if (group_.get()) {
                assert(thread_info);
                group_.get()->impl_->notify_thread_enter(thread_info_);
            }
        }

        ~thread_enter_exit_guard() {
            try {
                if (group_.get())
                    group_.get()->impl_->notify_thread_exit(thread_info_);
                else
                    s_detached_thread_counter.fetch_sub(1, std::memory_order_release);
            } catch (...) {
                assert(!"Unexpected exception");
            }
        }

    private:
        thread_info* thread_info_;
        thread_group_ptr group_;
    };

private:
    /// Called from thread procedure.
    void notify_thread_enter(thread_info* thread_info);

    /// Called from thread procedure.
    void notify_thread_exit(thread_info* thread_info);

    /// join the thread
    void join_thread(thread_info* thread_info);

    /// join all threads in finished state.
    void join_finished_threads();

    /// Finds any ready to join thread except of current thread.
    thread_info_ptr get_next_thread_for_join();

    /// Publishes a change that can bring join_all closer to an empty registry -- a thread
    /// reaching `finished`, an entry leaving threads_ -- and wakes the waiter. The counter
    /// itself carries no meaning beyond "something moved"; join_all reads it before looking at
    /// the registry and waits on that exact value, so a change racing with the check cannot be
    /// missed.
    void notify_progress() noexcept {
        progress_.fetch_add(1, std::memory_order_release);
        progress_.notify_all();
    }

private:
    thread_info_container threads_;
    std::atomic<bool> disposing_{false};
    std::atomic<uint64_t> progress_{0};
    core::recursive_mutex mutex_;

    /// The group the calling thread was spawned by, from its entry to its exit; null on any
    /// other thread. Only the thread itself reads and writes its own, so nothing is shared:
    /// join_all asks it to refuse a call from one of the group's own threads, which would wait
    /// for itself.
    static inline thread_local const impl* current_ = nullptr;
};

/// Carries everything a new thread needs across the thread boundary, and the promise its body
/// reports through. Deleted by the thread it starts, as soon as that thread has taken the
/// contents over.
class thread_starter : public core::noncopyable
{
public:
    thread_starter(thread_group* group, core::u8_view thread_name,
                   const std::function<void()>& thread_proc)
        : group_(group),
          thread_name_(thread_name),
          thread_proc_(thread_proc),
          info_(group ? core::make_refcounted<thread_info>() : nullptr) {}

    thread_group* group() { return group_.get(); }

    thread_info* info() { return info_.get(); }

    future<void> get_future() const { return result_.get_future(); }

    void start_thread();

    static unsigned __stdcall thread_proc(void* arg);

private:
    thread_group_ptr group_;
    core::u8_text thread_name_;
    std::function<void()> thread_proc_;
    thread_info_ptr info_;
    promise<void> result_;
};

thread_group::thread_group() : impl_(new impl()) {}

thread_group::~thread_group() = default;

future<void> thread_group::spawn_named(const core::u8_view thread_name,
                                       const std::function<void()>& thread_proc) {
    std::unique_ptr<thread_starter> starter(new thread_starter(this, thread_name, thread_proc));

    // Taken before the starter is handed over: the thread may run and delete it immediately.
    future<void> result = starter->get_future();

    impl_->spawn(starter.release());

    return result;
}

future<void> thread_group::spawn_detached_named(const core::u8_view thread_name,
                                                const std::function<void()>& thread_proc) {
    std::unique_ptr<thread_starter> starter(
        new thread_starter(nullptr, thread_name, thread_proc));

    future<void> result = starter->get_future();

    starter->start_thread();

    starter.release();

    return result;
}

size_t thread_group::detached_thread_count() noexcept {
    return s_detached_thread_counter.load(std::memory_order_acquire);
}

void thread_group::join_detached_threads() {
    while (0 != detached_thread_count()) {
        std::this_thread::yield();
    }
}

bool thread_group::join_detached_threads_for(core::duration timeout) {
    if (0 == detached_thread_count()) {
        return true;
    }

    const core::timeout_timer timer(timeout);

    while (timer.remaining()) {
        if (0 == detached_thread_count()) {
            return true;
        }

        std::this_thread::yield();
    }

    return 0 == detached_thread_count();
}

core::thread_id thread_group::main_thread_id() { return s_main_thread_id; }

void thread_group::join_all() { impl_->join_all(); }

void thread_group::close() { impl_->close(); }

thread_scope::~thread_scope() {
    try {
        group_->close();
    } catch (const std::exception& ex) {
        std::cerr << "Unexpected exception in ~thread_scope: " << ex.what() << '\n';
        assert(!"Unexpected exception");
    } catch (...) {
        std::cerr << "Unexpected unknown exception in ~thread_scope\n";
        assert(!"Unexpected exception");
    }
}

thread_group::impl::impl() noexcept = default;

thread_group::impl::~impl() = default;

void thread_group::impl::spawn(thread_starter* starter) {
    assert(starter);

    std::unique_ptr<thread_starter> starter_ptr(starter);
    register_thread(starter->info());

    try {
        starter->start_thread();
        starter_ptr.release();
    } catch (...) {
        unregister_thread(starter->info());
        throw;
    }
}

void thread_group::impl::register_thread(thread_info* thread_info) {
    assert(thread_info);

    core::lock_guard<core::recursive_mutex> guard(mutex_);

    if (disposing_.load(std::memory_order_relaxed))
        throw std::logic_error("thread_group is disposing.");

    assert(!threads_.contains(thread_info));
    threads_.add(thread_info);
}

void thread_group::impl::unregister_thread(thread_info* thread_info) {
    {
        core::lock_guard<core::recursive_mutex> guard(mutex_);

        assert(threads_.contains(thread_info));
        assert(thread_info->state() == thread_state::undefined);

        threads_.remove(thread_info);
    }

    notify_progress();
}

void thread_group::impl::notify_thread_enter(thread_info* thread_info) {
    assert(thread_info);

#ifdef WXL_CFG_DEBUG
    {
        core::lock_guard<core::recursive_mutex> guard(mutex_);

        if (!threads_.contains(thread_info)) {
            assert(!"Thread should be already managed");
            return;
        }
    }
#endif

    current_ = this;
    thread_info->state(thread_state::running);
}

/// Called on thread procedure exit
void thread_group::impl::notify_thread_exit(thread_info* thread_info) {
    assert(thread_info);

    current_ = nullptr;

    join_finished_threads();

#ifndef NDEBUG
    {
        core::lock_guard<core::recursive_mutex> guard(mutex_);

        if (!threads_.contains(thread_info)) {
            assert(!"Thread should be already managed");
            return;
        }
    }
#endif

    thread_info->state(thread_state::finished);

    notify_progress();
}

void thread_group::impl::join_finished_threads() {
    while (const thread_info_ptr thread_to_join = get_next_thread_for_join()) {
        join_thread(thread_to_join.get());
    }
}

thread_info_ptr thread_group::impl::get_next_thread_for_join() {
    thread_info_ptr next;

    {
        core::lock_guard<core::recursive_mutex> guard(mutex_);
        next = threads_.find_and_remove_first_finished();
    }

    // Reaping is what empties the registry, and join_all may be running on two threads at
    // once -- so the one that takes the last entry has to wake the one waiting for it.
    if (next) notify_progress();

    return next;
}

void thread_group::impl::join_thread(thread_info* thread_info) {
    HANDLE thread_handle = thread_info->thread_handle().get();
    const DWORD result = ::WaitForSingleObject(thread_handle, INFINITE);

    assert(result == WAIT_OBJECT_0);

    if (result != WAIT_OBJECT_0) {
        std::cerr << "Error during join thread: "
                  << std::system_error(std::error_code(static_cast<int>(::GetLastError()),
                                                       std::system_category()))
                         .what()
                  << '\n';
    }
}

void thread_group::impl::join_all() {
    // Called by join_all() or close() from one of this group's threads, it would wait for itself.
    if (current_ == this) {
        throw std::logic_error(
            "Cannot call thread_group::join_all from a thread spawned by the same instance "
            "of thread_group.");
    }

    while (true) {
        // Read before looking at the registry: anything that moves afterwards changes the
        // counter away from this value, and wait() then returns at once instead of parking
        // on a state that is already stale.
        const uint64_t seen = progress_.load(std::memory_order_acquire);

        join_finished_threads();

        {
            core::lock_guard<core::recursive_mutex> guard(mutex_);

            if (threads_.empty()) return;
        }

        progress_.wait(seen, std::memory_order_acquire);
    }
}

void thread_group::impl::close() {
    {
        core::lock_guard<core::recursive_mutex> guard(mutex_);
        disposing_.store(true, std::memory_order_relaxed);
    }

    join_all();
}

void thread_starter::start_thread() {
    // Read while the starter is certainly alive: a running thread deletes it.
    thread_info* const info = info_.get();
    core::thread_id id = 0;

    struct detached_thread_error_guard {
        bool ok;
        ~detached_thread_error_guard() {
            if (!ok) s_detached_thread_counter.fetch_sub(1, std::memory_order_relaxed);
        }
    } detached_thread_error_guard = {false};

    if (!info)  // if thread is detached
        s_detached_thread_counter.fetch_add(1, std::memory_order_relaxed);
    else
        detached_thread_error_guard.ok = true;  // do not decrement in any case

    // We should use _beginthreadex instead of ::CreateThread, related link:
    // http://support2.microsoft.com/kb/104641/ru _beginthreadex is safe even if the new thread
    // exits before this call returns (unlike _beginthread).
    //
    // A thread of a group starts suspended and is let go only once its handle is in its
    // thread_info. Whoever reaps the thread does it on seeing `finished`, and the thread's start
    // orders the handle before that -- so the handle needs neither the group's lock nor a flag
    // of its own, and a finished thread is never one that cannot be reaped yet. A detached
    // thread has no thread_info and runs at once.
    const uintptr_t thread_handle = ::_beginthreadex(nullptr, 0, thread_starter::thread_proc, this,
                                                     info ? CREATE_SUSPENDED : 0u, &id);

    // WARNING: a detached thread is running already, and 'this' could be deleted by it.

    if (!thread_handle || thread_handle == static_cast<uintptr_t>(-1)) {
        throw std::runtime_error("Cannot start thread, threadHandle=" +
                                 std::to_string(thread_handle));
    }

    if (info) {
        info->thread_handle().reset(HANDLE(thread_handle));

        // A thread that never ran has not taken the starter over: the caller still owns it,
        // and the thread, left suspended, never touches it.
        if (::ResumeThread(HANDLE(thread_handle)) == static_cast<DWORD>(-1))
            throw last_error("ResumeThread");

        // WARNING: from here on 'this' could be already deleted by the thread.
    } else {
        ::CloseHandle(HANDLE(thread_handle));
    }

    detached_thread_error_guard.ok = true;
}

namespace {
struct cleanup_func_node {
    cleanup_func_node(thread_group::cleanup_func func, thread_group::cleanup_func1 func1,
                      void* arg, cleanup_func_node* prev)
        : cleanup_func_(func), cleanup_func1_(func1), arg_(arg), prev_(prev) {
        assert(func || func1);
    }

    void call_cleanup() const {
        try {
            if (cleanup_func_)
                (*cleanup_func_)();
            else
                (*cleanup_func1_)(arg_);
        } catch (...) {
        }
    }

    thread_group::cleanup_func cleanup_func_;
    thread_group::cleanup_func1 cleanup_func1_;
    void* arg_;
    cleanup_func_node* prev_;
};

struct thread_data {
    cleanup_func_node* cleanup_stack_ = nullptr;

    void register_cleanup_func(thread_group::cleanup_func f, thread_group::cleanup_func1 f1,
                               void* arg) {
        cleanup_stack_ = new cleanup_func_node(f, f1, arg, cleanup_stack_);
    }

    void execute_cleanup_funcs() noexcept {
        while (cleanup_func_node* node = cleanup_stack_) {
            cleanup_stack_ = node->prev_;
            node->call_cleanup();
            delete node;
        }
    }
};

/// Thread-local data for the current (managed) thread; null on threads not spawned by
/// thread_group.
thread_local thread_data* s_thread_data = nullptr;

cleanup_func_node* s_main_thread_cleanup_stack = nullptr;

void main_thread_cleanup() {
    while (cleanup_func_node* node = s_main_thread_cleanup_stack) {
        s_main_thread_cleanup_stack = node->prev_;
        node->call_cleanup();
        delete node;
    }
}

/// Runs main_thread_cleanup() once, after every other module_cleanup::cleanup_first/
/// cleanup_normal callback in the process, but still as part of the same exit event.
core::module_cleanup s_module_cleanup(main_thread_cleanup, core::module_cleanup::cleanup_last);

void register_main_thread_cleanup(thread_group::cleanup_func f, thread_group::cleanup_func1 f1,
                                  void* arg) {
    assert(f || f1);

    if (core::module_cleanup::process_is_terminating()) {
        const cleanup_func_node node(f, f1, arg, nullptr);
        node.call_cleanup();
        return;
    }

    s_main_thread_cleanup_stack = new cleanup_func_node(f, f1, arg, s_main_thread_cleanup_stack);
}

}  // namespace

void thread_group::at_thread_exit_impl(thread_group::cleanup_func f,
                                         thread_group::cleanup_func1 f1, void* arg) {
    if (core::current_thread_id() == main_thread_id()) {
        register_main_thread_cleanup(f, f1, arg);
        return;
    }

    thread_data* data = s_thread_data;

    if (!data) {
        throw std::logic_error("Can not register thread cleanup procedure for external thread");
    }

    data->register_cleanup_func(f, f1, arg);
}

bool thread_group::is_internal_thread() {
    return s_thread_data != nullptr || core::current_thread_id() == main_thread_id();
}

unsigned __stdcall thread_starter::thread_proc(void* arg) {
    std::unique_ptr<thread_starter> starter(static_cast<thread_starter*>(arg));

    thread_info* thread_info = starter->info();

    using enter_exit_guard = thread_group::impl::thread_enter_exit_guard;
    enter_exit_guard lifetime_guard(thread_info, starter->group());

    const core::u8_text thread_name = std::move(starter->thread_name_);
    set_current_thread_name(thread_name);

    // Каждый рабочий поток wxl держит COM поднятым как MTA на всё своё время: на
    // этих потоках могут делаться вызовы WinRT (например, Win2D), а WinRT требует,
    // чтобы на потоке был поднят COM. Именно MTA, а не STA: STA потребовала бы
    // качать оконные сообщения, а рабочий поток только спит в receive(). Главный
    // поток сюда не попадает -- его заводит не thread_group, а сам процесс, и
    // апартамент STA ему ставит launch. Живёт до конца thread_proc, поэтому
    // CoUninitialize приходит после тела и его уборки, на том же потоке.
    struct com_mta_apartment {
        com_mta_apartment() noexcept { (void)::CoInitializeEx(nullptr, COINIT_MULTITHREADED); }
        ~com_mta_apartment() { ::CoUninitialize(); }
    } com_apartment;

    std::function<void()> target_thread_proc = std::move(starter->thread_proc_);

    // Everything the body needs is taken over before the starter dies, the promise included:
    // it is what the exception leaves through, since an exception cannot cross a thread
    // boundary on its own.
    promise<void> result = std::move(starter->result_);

    // @todo: join thread_starter and thread_data into a single structure
    thread_data data;

    try {
        s_thread_data = &data;
        starter.reset();  // deletes thread_starter.
        target_thread_proc();
        result.set_value();
    } catch (...) {
        result.set_exception(std::current_exception());
    }

    data.execute_cleanup_funcs();

    return 0;
}

}  // namespace wxl::async

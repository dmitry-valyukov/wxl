module;
#include "pch.h"

module wxl.core;
import std;

#include "component.impl.h"

namespace wxl::async {

/// What a threaded component keeps: the group its thread belongs to (none, when the thread
/// is detached), the id of that thread while it runs, and the thread's handle until the
/// component is disposed of. They live in the state, like every other field of a
/// component -- see component::impl_t.
class threaded_component::impl_t : public component::impl_t
{
    using base = component::impl_t;

public:
    /// Constructs the state that synchronizes on \p sync_root_arg, for a thread of \p group,
    /// or for a detached thread if \p group is null.
    impl_t(core::sync_root_holder sync_root_arg, thread_group* group)
        : base(sync_root_arg), group_(group) {}

    /// The owner, as its own type. A protected member of component may be reached only
    /// through an object of the class that inherits it, and owner_unsafe() knows nothing
    /// finer than component.
    threaded_component* owner() { return static_cast<threaded_component*>(owner_unsafe()); }

    /// The thread itself, and the step it takes before entering run(). Here rather than in
    /// threaded_component because nothing inline calls them -- and because the running
    /// thread then holds the state, which outlives the component, instead of a raw pointer
    /// to the component, which may not.
    void thread_proc(task_tree_handle thread_starting_task, task_tree_handle thread_running_task);

    /// Completes the start on the component's thread: opens the handle that `dispose()` waits
    /// on, tests whether the start has been canceled or a stop requested, and waits for the
    /// call to `on_starting()` to complete.
    /// \return `false` if the start has failed; \p thread_starting_task is then failed.
    bool complete_start(task_tree_handle thread_starting_task);

    /// The component's disposal, and then the wait for its thread to exit.
    void dispose() override;

    /// The group of the component's thread, or null for a detached thread.
    const thread_group_ptr group_;

    /// The identifier of the component's thread while the thread runs the component;
    /// otherwise 0.
    std::atomic<core::thread_id> thread_id_{0};

    /// The component's thread, opened by that thread as its first step and closed by
    /// dispose(). Null while no thread has been started, and for good when none was.
    ///
    /// A plain field: the thread writes it before it reports anything, and dispose() reads
    /// it after waiting for the stop ticket, which the thread's report is part of. A
    /// component whose start was never accepted is not waited for there -- and has no
    /// thread either.
    HANDLE thread_ = nullptr;
};

inline core::not_null<threaded_component::impl_t> threaded_component::impl() {
    return core::as_not_null(static_cast<impl_t*>(component::impl().get()));
}

inline core::not_null<const threaded_component::impl_t> threaded_component::impl() const {
    return core::as_not_null(static_cast<const impl_t*>(component::impl().get()));
}

core::thread_id threaded_component::get_thread_id() const {
    return impl()->thread_id_.load(std::memory_order_relaxed);
}

threaded_component::threaded_component(std::string_view thread_name,
                                       core::nullable<core::sync_root> sync_root_arg)
    : component(thread_name, new impl_t(core::auto_sync_root_holder(sync_root_arg), nullptr)) {}

threaded_component::threaded_component(std::string_view thread_name, thread_group* group,
                                       core::nullable<core::sync_root> sync_root_arg)
    : component(thread_name, new impl_t(core::auto_sync_root_holder(sync_root_arg), group)) {}

void threaded_component::on_starting() {
    component::on_starting();

    task_tree_handle thread_starting_task = starting_task().create_subtask();
    task_tree_handle thread_running_task = running_task().create_subtask();

    try {
        thread_group::thread_proc run_func = [keeper = core::intrusive_ptr<impl_t>(impl().get()),
                                              thread_starting_task, thread_running_task] {
            keeper->thread_proc(thread_starting_task, thread_running_task);
        };

        // The component's name is its own -- given in the source or built by the
        // program, never read from anywhere -- so there is nothing here to check.
        const core::u8_view thread_name = core::unicode::assume_valid(name());

        if (thread_group* group = impl()->group_.get())
            group->spawn_named(thread_name, run_func);
        else
            thread_group::spawn_detached_named(thread_name, run_func);
    } catch (...) {
        // cannot start the thread
        std::exception_ptr error = std::current_exception();
        thread_starting_task.set_failed(error);
        thread_running_task.set_failed(error);
        throw;
    }
}

void threaded_component::impl_t::thread_proc(task_tree_handle thread_starting_task,
                                             task_tree_handle thread_running_task) {
    try {
        if (!complete_start(thread_starting_task)) {
            assert(owner()->stop_requested());
            assert(thread_starting_task.finished());

            if (thread_starting_task.failed())
                thread_running_task.set_failed(thread_starting_task.get_exception_ptr());
            else
                thread_running_task.set_succeeded();

            return;
        }

        thread_id_.store(core::current_thread_id(), std::memory_order_relaxed);

        // Clears the thread id on every exit path out of the block below, normal or exceptional.
        struct clear_thread_id_on_exit {
            impl_t* impl;
            ~clear_thread_id_on_exit() { impl->thread_id_.store(0, std::memory_order_relaxed); }
        } const clear_thread_id{this};

        try {
            lock_guard g(*this);

            if (owner()->stop_requested()) throw operation_canceled_exception();

            owner()->on_started();
        } catch (...) {
            thread_starting_task.set_failed(std::current_exception());
            throw;
        }

        thread_starting_task.set_succeeded();
        thread_starting_task.root()
            .get_future()
            .get();  // blocking wait for the whole compound task

        owner()->run();
        owner()->stop_async();
    } catch (...) {
        std::exception_ptr error = std::current_exception();
        owner()->stop_async(error);
    }

    thread_running_task.set_succeeded_or_failed(*stop_reason_ptr());
}

bool threaded_component::impl_t::complete_start(task_tree_handle thread_starting_task) {
    assert(thread_starting_task.valid());

    // First, before any answer below: the thread exists whatever this start comes to, and
    // dispose() waits for it through this handle. A thread that could not be waited for
    // does not get to run -- its start fails instead.
    HANDLE thread = nullptr;

    if (!::DuplicateHandle(::GetCurrentProcess(), ::GetCurrentThread(), ::GetCurrentProcess(),
                           &thread, SYNCHRONIZE | THREAD_QUERY_LIMITED_INFORMATION, FALSE, 0)) {
        std::exception_ptr error =
            std::make_exception_ptr(last_error("Cannot open the component's thread"));
        thread_starting_task.set_failed(error);
        owner()->stop_async(error);
        return false;
    }

    thread_ = thread;

    task_tree_root root_task = thread_starting_task.root();
    ticket cancellation_token = root_task.cancellation_token();

    if (cancellation_token.has_exception()) {
        thread_starting_task.set_failed(cancellation_token.get_exception_ptr());
        owner()->stop_async(cancellation_token.get_exception_ptr());
        return false;
    }

    if (owner()->stop_requested()) {
        thread_starting_task.set_failed(std::make_exception_ptr(
            operation_canceled_exception("Starting procedure was canceled")));
        return false;
    }

    assert(!root_task.finished());

    ticket on_starting_call_completed;
    {
        guard g(*this);
        on_starting_call_completed = owner()->on_starting_ticket();
    }

    on_starting_call_completed.wait();

    if (on_starting_call_completed.has_exception()) {
        std::exception_ptr error = on_starting_call_completed.get_exception_ptr();
        thread_starting_task.set_failed(error);
        owner()->stop_async(error);
        return false;
    }

    return true;
}

void threaded_component::impl_t::dispose() {
    base::dispose();

    // Taken out, so that a second dispose() has nothing left to wait for.
    const HANDLE thread = std::exchange(thread_, nullptr);

    if (!thread) return;

    // The component says it has stopped from inside its thread, and the thread still has
    // its way out to go: the callbacks on the stop ticket that ran there, its at-exit
    // callbacks, the callable it came in with the keeper in it, COM, the group's registry
    // (it joins the group's other finished threads, waiting for each, marks its own entry
    // finished and lets go of the group), and the runtime's own goodbye -- thread_local
    // data, DLL_THREAD_DETACH under the loader lock. A process that exits meanwhile kills
    // the thread on that way, with whatever it holds. So the component is done with only
    // once its thread has exited.
    //
    // Unless it is disposed of on that very thread -- deleted by a callback on its stop
    // ticket, which runs where the stop finished: a thread cannot wait for its own end.
    // That thread is past run() and past its report, and exits once the callback returns.
    if (::GetThreadId(thread) != core::current_thread_id()) {
        [[maybe_unused]] const DWORD waited = ::WaitForSingleObject(thread, INFINITE);
        assert(waited == WAIT_OBJECT_0);
    }

    ::CloseHandle(thread);
}

}  // namespace wxl::async

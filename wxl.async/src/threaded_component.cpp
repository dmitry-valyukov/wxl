module;
#include "pch.h"

module wxl.async;
import wxl.core;
import std;

#include "component.impl.h"

namespace wxl::async {

/// What a threaded component keeps: the group its thread belongs to (none, when the thread
/// is detached) and the id of that thread while it runs. They live in the state, like every
/// other field of a component -- see component::impl_t.
class threaded_component::impl_t : public component::impl_t
{
    using base = component::impl_t;

public:
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
    void thread_proc(task_tree_handle thread_starting_task,
                     task_tree_handle thread_running_task);

    bool complete_start(task_tree_handle thread_starting_task);

    const thread_group_ptr group_;

    std::atomic<core::thread_id> thread_id_{0};
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
        thread_group::thread_proc run_func =
            [keeper = core::intrusive_ptr<impl_t>(impl().get()), thread_starting_task,
             thread_running_task] { keeper->thread_proc(thread_starting_task, thread_running_task); };

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

}  // namespace wxl::async

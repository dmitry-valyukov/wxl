module;
#include "pch.h"

module wxl.async;
import wxl.core;
import std;

#include "component.impl.h"

namespace wxl::async {

component::component(std::string_view name,
                                         core::nullable<core::sync_root> sync_root_arg)
    : core::traceable(name), core::synchronized_(sync_root_arg), impl_(new impl_t(sync_root())) {
    impl_->owner_ = this;
}

component::component(std::string_view name, impl_t* initial_impl)
    : core::traceable(name), core::synchronized_(initial_impl->sync_root()), impl_(initial_impl) {
    impl_->owner_ = this;
}

component::~component() { assert(is_disposed()); }

bool component::was_started() const {
    guard g(*this);
    return was_started_unsafe();
}

bool component::was_started_unsafe() const {
    assert(is_synchronized());
    return impl()->running_task_.valid();
}

bool component::is_running() const {
    assert(is_synchronized());  // it makes sense to check only under lock
    return was_started_unsafe() && !impl()->ready();
}

bool component::is_disposed() const noexcept { return impl()->is_disposed(); }

const std::exception_ptr* component::stop_reason_ptr() const {
    return impl_->stop_reason_ptr();
}

ticket component::stop_ticket() const { return impl_->stop_ticket(); }

const task_tree& component::starting_task() const {
    assert(is_synchronized());
    return impl()->starting_task_;
}

const task_tree& component::running_task() const {
    assert(is_synchronized());
    return impl()->running_task_;
}

const task_tree& component::on_stopping_task() const {
    assert(is_synchronized());
    return impl()->on_stopping_task_;
}

ticket component::on_starting_ticket() {
    assert(is_synchronized());
    assert(impl()->on_starting_task_.valid());
    return impl()->on_starting_task_.get_future();
}

ticket component::on_stopping_ticket() const {
    const task_tree& stopping_task_ref = on_stopping_task();
    assert(stopping_task_ref.valid());
    return stopping_task_ref.get_future();
}

void component::dispose() { impl_->dispose(); }

void component::impl_t::dispose() {
    if (is_disposed()) return;

    if (!stop_reason_ptr()) stop_async_unsafe(stop_reason::abort());

    task_tree running_task_copy;
    {
        guard g(*this);
        running_task_copy = running_task_;
    }

    if (running_task_copy.valid()) this->wait();

    // Компонент, который так и не стартовал, разрешить некому: цепочка остановки
    // висит на running_task_, а его нет. Ждущий на тикете остановки ждал бы вечно,
    // поэтому тикет закрывается тем же, чем закрывается брошенный promise. Для
    // стартовавшего компонента тикет уже разрешён, и это ничего не меняет.
    abandon();

    assert(!is_disposed());
    owner_ = nullptr;  // null owner_ marks the disposed state.
}

ticket component::start_async() {
    return start_async(task_tree(), task_tree());
}

ticket component::start_async_of(core::not_null<component> child,
                                 core::not_null<component> parent) {
    return child->start_async(parent);
}

ticket component::start_async_of(core::not_null<component> child,
                                 const task_tree& parent_starting_task,
                                 const task_tree& parent_running_task) {
    return child->start_async(parent_starting_task, parent_running_task);
}

ticket component::start_async(core::not_null<component> parent) {
    // The parent's lock is taken here rather than demanded of the caller: everything read
    // below is the parent's state, and it has to still say the same thing when the subtasks
    // are created under it. sync_root is recursive, so a caller that already holds it -- a
    // container starting its children -- passes straight through.
    guard g(*parent);

    task_tree parent_starting_task = parent->starting_task();
    task_tree parent_running_task = parent->running_task();

    if (!parent_running_task.valid())
        throw std::logic_error(
            "Cannot start the component because the parent component is not started yet");

    if (parent->impl()->on_stopping_task_.valid())
        throw std::logic_error(
            "Cannot start the component because the parent component is stopping or already "
            "stopped");

    // Not finished(), which asks whether the whole node is resolved: a task stops taking
    // subtasks as soon as it reports its own work, and the parent's start_async() did that
    // before it returned. So a child started after the parent is up gets a starting tree of
    // its own -- its failure then reaches nobody else, which is right: the parent has
    // already started, and there is no group left to cancel.
    if (!parent_starting_task.accepts_subtasks()) parent_starting_task = task_tree();

    return start_async(parent_starting_task, parent_running_task);
}

namespace {

task_tree_handle create_subtask_checked(const task_tree& parent_task) {
    if (parent_task.valid()) {
        ticket cancellation_token = parent_task.root().cancellation_token();

        if (cancellation_token.has_exception())
            std::rethrow_exception(cancellation_token.get_exception_ptr());

        if (cancellation_token.is_ready())
            throw operation_canceled_exception("Starting was cancelled");
    }

    return task_tree::create(parent_task);
}

struct subtask_tuple {
    task_tree_handle starting;
    task_tree_handle running;
};

subtask_tuple make_subtasks(const task_tree& parent_starting_task,
                            const task_tree& parent_running_task) {
    task_tree_handle starting = create_subtask_checked(parent_starting_task);

    try {
        task_tree_handle running = create_subtask_checked(parent_running_task);
        return subtask_tuple{starting, running};
    } catch (...) {
        starting.set_failed(std::current_exception());
        throw;
    }
}

}  // namespace

ticket component::start_async(const task_tree& parent_starting_task,
                              const task_tree& parent_running_task) {
    const core::not_null<impl_t> my_impl = impl();

    impl_ptr impl_keeper(my_impl.get());  // keep me alive

    subtask_tuple tasks;
    task_tree_handle on_starting_call_task;

    // Set once the call has been accepted. Until then a throw is a *refusal* -- the caller
    // asked for something the component cannot be asked for -- and the catch below must not
    // treat it as a failed start: stopping a component that is running perfectly well
    // because somebody asked to start it twice is no answer to the question.
    bool accepted = false;

    try {
        lock_guard g(*this);

        // do not remove this check!
        if (stop_requested())
            throw operation_canceled_exception("Component was already requested to stop");

        if (was_started_unsafe()) throw std::logic_error("Component was already started");

        accepted = true;

        tasks = make_subtasks(parent_starting_task, parent_running_task);

        my_impl->starting_task_ = tasks.starting;
        my_impl->running_task_ = tasks.running;

        tasks.running.get_future().when_ready(
            [keeper = impl_ptr(my_impl.get())]() noexcept { keeper->confirm_stopped(); });

        on_starting_call_task = starting_task().create_subtask();
        my_impl->on_starting_task_ = on_starting_call_task;

        on_starting();
    } catch (...) {
        if (!accepted) throw;  // refused before anything was done, so there is nothing to undo

        std::exception_ptr error = std::current_exception();

        if (on_starting_call_task.valid()) on_starting_call_task.set_failed(error);

        // Остановка идёт своим ходом, и дожидаться её здесь нельзя: корень мог
        // взять вызывающий -- контейнер, стартующий своих детей, -- и ожидание
        // под его замком встало бы намертво. Отпустить чужой замок ради
        // ожидания тем более нельзя: он не наш, и вернуть его к моменту выхода
        // не в нашей власти.
        //
        // Ждать и не за чем: единственное, ради чего ожидание стояло, -- чтобы
        // состояние дожило до конца остановки. Это делает продолжение, которое
        // держит impl_keeper до самого её конца и ничего больше.
        stop_async(error).when_ready([keeper = impl_keeper]() noexcept {});

        if (tasks.starting.valid()) {
            assert(tasks.running.valid());
            tasks.starting.set_failed(error);
        }

        throw;
    }

    on_starting_call_task.set_succeeded();

    ticket resolved = tasks.starting.root().cancellation_token();
    resolved.when_failed(stop_async_func(impl_keeper));

    tasks.starting.set_succeeded();

    return tasks.starting.get_future();
}

void component::on_starting() {
    assert(is_synchronized());

    if (stop_requested())
        throw operation_canceled_exception("Component was already requested to stop");
}

void component::on_started() {
    // Fired out of a list of its own: a component starts once, so the callbacks are called
    // and gone, and one that unsubscribes from inside the call finds an event with nothing
    // in it rather than the list it is being walked through.
    core::event<void()> callbacks;
    callbacks.swap(impl()->on_started_);
    callbacks.fire();
}

ticket component::stop_async(const stop_reason& reason) {
    return stop_async(reason.error());
}

ticket component::stop_async(const std::exception_ptr& error) {
    const core::not_null<impl_t> ss = impl();
    // The component can delete itself on stop.
    ticket stop_ticket_result(ss.get());

    if (ss->try_to_win_stopping_race(error)) ss->stop_async_impl();

    return stop_ticket_result;
}

void component::impl_t::stop_async_unsafe(const std::exception_ptr& error) {
    if (try_to_win_stopping_race(error)) stop_async_impl();
}

void component::impl_t::stop_async_impl() {
    impl_t* const ss = this;
    component* const owner = owner_unsafe();
    const std::exception_ptr& error = *ss->stop_reason_ptr();

    bool on_stopping_called = false;
    {
        lock_guard g(*this);

        if (!owner->was_started_unsafe()) return;

        assert(ss->starting_task_.valid());
        ss->on_stopping_task_ = ss->running_task_.create_subtask();

        if (!ss->starting_task_.finished()) owner->rollback_start();

        if (ss->starting_task_.finished()) {
            try {
                assert(owner->stop_requested());
                owner->on_stopping();
            } catch (...) {
                // probably in a corrupted state
                std::cerr << "Unexpected error during on_stopping call." << '\n';
            }

            on_stopping_called = true;
        }
    }

    if (on_stopping_called) {
        ss->on_stopping_task_.set_succeeded_or_failed(error);
        ss->running_task_.set_succeeded_or_failed(error);
    } else {
        ss->starting_task_.get_future().when_ready(
            [keeper = impl_ptr(ss)]() noexcept { keeper->late_call_on_stopping(); });
    }
}

void component::rollback_start() {}

void component::impl_t::late_call_on_stopping() {
    impl_t* const ss = this;

    stop_reason reason;
    task_tree_handle running_task_copy;
    task_tree_handle stopping_task_copy;
    {
        lock_guard g(*this);

        reason = *ss->stop_reason_ptr();
        running_task_copy = ss->running_task_;
        stopping_task_copy = ss->on_stopping_task_;

        try {
            owner_unsafe()->on_stopping();
        } catch (const std::exception& ex) {
            // probably in a corrupted state
            std::cerr << "Unexpected error during on_stopping call: " << ex.what() << '\n';
        } catch (...) {
            // probably in a corrupted state
            std::cerr << "Unexpected error during on_stopping call." << '\n';
        }
    }

    assert(stopping_task_copy.valid());

    try {
        ss->on_stopping_task_.set_succeeded_or_failed(reason.error());
    } catch (...) {
        assert(false);
    }

    assert(running_task_copy.valid());

    try {
        running_task_copy.set_succeeded_or_failed(reason.error());
    } catch (...) {
        assert(false);
    }
}

void component::on_stopping() {}

void component::impl_t::confirm_stopped() {
    assert(try_to_win_stopping_race(std::exception_ptr()) == false);
    assert(!stop_ticket().is_ready());

    impl_ptr impl_keeper(this);

    stop_reason reason;
    {
        lock_guard g(*this);

        reason = *impl_keeper->stop_reason_ptr();

        try {
            owner_unsafe()->on_stopped();
            // we can be deleted here
        } catch (const std::exception& ex) {
            // probably in a corrupted state
            std::cerr << "Unexpected error during on_stopped call: " << ex.what() << '\n';
        } catch (...) {
            // probably in a corrupted state
            std::cerr << "Unexpected error during on_stopped call." << '\n';
        }
    }

    if (reason.stopped_due_to_error())
        impl_keeper->set_exception(reason.error());
    else
        impl_keeper->set_value();
}

void component::on_stopped() {
    const core::not_null<impl_t> my_impl = impl();

    // Nobody is going to be told about a start any more.
    my_impl->on_started_.clear();

    const stop_reason reason(*my_impl->stop_reason_ptr());

    core::event<void(const stop_reason&)> callbacks;
    callbacks.swap(my_impl->on_stopped_);

    // The component can be deleted in here, which is why the state was taken above: nothing
    // below may go through `this` again.
    callbacks.fire(reason);
}

component::impl_t::impl_t(core::sync_root_holder sync_root_arg) : core::synchronized_(sync_root_arg) {}

component::impl_t::impl_t(core::not_null<core::sync_root> sync_root_arg) : core::synchronized_(sync_root_arg) {}

component* component::impl_t::owner_unsafe() {
    if (is_disposed()) [[unlikely]]
        core::abort("component::impl_t::owner_unsafe() called on a disposed component -- "
                    "this is an architecture-level invariant violation, not an application error");

    return owner_;
}

ticket component::impl_t::stop_ticket() { return ticket(this); }

const std::exception_ptr* component::impl_t::stop_reason_ptr() const noexcept {
    return stop_reason_.load(std::memory_order_acquire);
}

void component::impl_t::stop_async_unsafe(const stop_reason& reason) {
    stop_async_unsafe(reason.error());
}

bool component::impl_t::is_disposed() const noexcept { return !owner_; }

const task_tree& component::impl_t::starting_task() const { return starting_task_; }

const task_tree& component::impl_t::running_task() const { return running_task_; }

const task_tree& component::impl_t::on_starting_task() const { return on_starting_task_; }

component::stop_async_func::stop_async_func(const impl_ptr& component_impl)
    : impl_(component_impl) {}

void component::stop_async_func::operator()(const std::exception_ptr& error) const noexcept {
    impl_->stop_async_unsafe(error);
}

void component::stop_async_func::operator()(const ticket& result) const noexcept {
    if (result.has_exception())
        impl_->stop_async_unsafe(result.get_exception_ptr());
    else
        impl_->stop_async_unsafe();
}


namespace {

// Avoids allocating a fresh std::exception_ptr for the two common "stopped without error" /
// "stopped via abort()" paths, by caching one of each in a ref-counted singleton.
class exception_ptr_cache : public core::refcounted_mt
{
public:
    exception_ptr_cache() : abort_error_(std::make_exception_ptr(operation_canceled_exception())) {}

    std::exception_ptr* empty_error() { return &empty_error_; }
    std::exception_ptr* abort_error() { return &abort_error_; }

private:
    ~exception_ptr_cache() override { assert(core::module_cleanup::process_is_terminating()); }

    std::exception_ptr empty_error_;
    std::exception_ptr abort_error_;
};

exception_ptr_cache* s_error_cache = new exception_ptr_cache();
std::exception_ptr* s_empty_error = s_error_cache->empty_error();
std::exception_ptr* s_abort_error = s_error_cache->abort_error();

struct module_initializer : core::module_cleanup {
    module_initializer() : core::module_cleanup(&cleanup, cleanup_last) {
        intrusive_ptr_add_ref(s_error_cache);
    }

    static void cleanup() {
        assert(core::module_cleanup::process_is_terminating());
        intrusive_ptr_release(s_error_cache);
    }
} s_module_initializer;

const std::exception_ptr* create_stop_request_copy(const std::exception_ptr& error) {
    if (error == *s_abort_error) return s_abort_error;

    return new std::exception_ptr(error);
}

void destroy_stop_request(const std::exception_ptr* error) noexcept {
    if (error == s_empty_error || error == s_abort_error)
        intrusive_ptr_release(s_error_cache);
    else
        delete error;
}

}  // namespace

const stop_reason stop_reason::abort() {
    if (core::module_cleanup::process_is_terminating())
        return stop_reason(std::make_exception_ptr(operation_canceled_exception()));

    return stop_reason(*s_abort_error);
}

component::impl_t::~impl_t() {
    assert(is_disposed());

    if (const std::exception_ptr* reason = stop_reason_.load(std::memory_order_relaxed))
        destroy_stop_request(reason);
}

bool component::impl_t::try_to_win_stopping_race(const std::exception_ptr& error) {
    if (stop_reason_.load(std::memory_order_acquire)) return false;

    // Takes a copy (via new) of std::exception_ptr, or the address of a static empty/abort
    // std::exception_ptr object.
    const std::exception_ptr* stop_request = nullptr;

    if (core::module_cleanup::process_is_terminating()) {
        stop_request = new std::exception_ptr(error);
    } else {
        if (error) {
            stop_request = create_stop_request_copy(error);
        } else {
            if (starting_task_.valid()) {
                if (starting_task_.failed())
                    stop_request = create_stop_request_copy(starting_task_.get_exception_ptr());
                else if (!starting_task_.finished())
                    stop_request = s_abort_error;
                else
                    stop_request = s_empty_error;
            } else
                stop_request = s_empty_error;
        }

        if (stop_request == s_empty_error || stop_request == s_abort_error)
            intrusive_ptr_add_ref(s_error_cache);
    }

    const std::exception_ptr* no_request = nullptr;

    if (stop_reason_.compare_exchange_strong(no_request, stop_request, std::memory_order_acq_rel,
                                             std::memory_order_acquire))
        return true;

    destroy_stop_request(stop_request);  // we lost the race
    return false;
}

// The lock is taken in all four of these, and not just around the debug check as it once
// was: the callbacks are no longer a field the subscriber alone touches, but a list in the
// state that a stop on another thread empties.

core::cookie_t component::subscribe_on_started(core::not_null<on_started_callback> callback) {
    guard g(*this);

    // Subscribing after the start has begun would be pointless: on_started() fires once, and
    // by then it has taken the callbacks with it.
    assert(!impl()->running_task_.valid());

    return impl()->on_started_.add(callback);
}

bool component::unsubscribe_on_started(core::cookie_t cookie) {
    guard g(*this);
    return impl()->on_started_.remove(cookie);
}

core::cookie_t component::subscribe_on_stopped(core::not_null<on_stopped_callback> callback) {
    guard g(*this);
    assert(!impl()->running_task_.valid());

    return impl()->on_stopped_.add(callback);
}

bool component::unsubscribe_on_stopped(core::cookie_t cookie) {
    guard g(*this);
    return impl()->on_stopped_.remove(cookie);
}

}  // namespace wxl::async

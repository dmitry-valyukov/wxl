module;

#include "abi.h"

export module wxl.async:future_shared_state;

import :bump_buffer;
import :one_shot_event;
import wxl.core;
import std;

export namespace wxl::async {

template <typename T>
class future;

future<void> make_ready_future();

template <typename T>
future<void> make_ready_future(const T& value);

/// State of a \c Future object (similar to std::future_status, @see
/// http://en.cppreference.com/w/cpp/thread/future_status ).
enum class future_status
{
    ready,    ///< the shared state is ready.
    timeout,  ///< the shared state did not become ready before specified timeout duration has
              ///< passed.
    deferred  ///< the shared state contains a deferred function, so the result will be computed
              ///< only when explicitly requested. \todo not yet implemented: wait() cannot return
              ///< this yet.
};

const char* to_string(future_status status);

std::ostream& operator<<(std::ostream& stream, future_status status);

class future_shared_state;
using future_shared_state_ptr = core::intrusive_ptr<future_shared_state>;

/// Future implementation namespace
namespace future_detail {

[[noreturn]] void throw_invalid_future();

/// A callback a subscription may take. It must be noexcept: a subscription is the light form --
/// it creates no shared state of its own -- so an exception coming out of the callback has
/// nowhere to go. next() is the form that has a future to put one in.
template <class t_callback, class... t_args>
concept nothrow_callback = std::is_nothrow_invocable_v<t_callback&, t_args...>;

/// The same, for a success callback of a future<T>: it takes the value, or nothing at all when
/// T is void.
template <class t_callback, class T>
concept nothrow_success_callback = (std::is_void_v<T> && nothrow_callback<t_callback>) ||
                                   (!std::is_void_v<T> && nothrow_callback<t_callback, const T&>);

/// Holder of a shared state.
template <typename T>
class shared_state_holder
{
public:
    /// Checks if the holder refers to a shared state.
    bool valid() const noexcept { return !!state_.get(); }

    void swap(shared_state_holder<T>& other) noexcept { state_.swap(other.state_); }

protected:
    explicit shared_state_holder(T* state) noexcept : state_(state) {}

    /// Copy constructor.
    shared_state_holder(const shared_state_holder<T>& other) noexcept : state_(other.state_) {}

    /// Move constructor.
    shared_state_holder(shared_state_holder<T>&& other) noexcept
        : state_(other.state_.detach(), false) {}

    /// Copy assignment.
    shared_state_holder<T>& operator=(const shared_state_holder<T>& other) noexcept {
        state_ = other.state_;
        return *this;
    }

    /// Move assignment.
    shared_state_holder<T>& operator=(shared_state_holder<T>&& other) noexcept {
        state_.reset(other.state_.detach(), false);
        return *this;
    }

    /// In-class, not out of line: an out-of-line definition in a module interface gets
    /// module linkage and a strong symbol, so every importing translation unit would
    /// define it and the link would fail on the duplicate.
    ~shared_state_holder() = default;

    /// @throw std::logic_error if this instance does not refer to a shared state.
    T* checked_state() const {
        if (T* state = state_.get()) return state;

        throw_invalid_future();
    }

    T* state() const noexcept { return state_.get(); }

    T* detach_state() noexcept { return state_.detach(); }

    void reset(T* new_ptr = nullptr) { state_.reset(new_ptr); }

private:
    core::intrusive_ptr<T> state_;
};

/// Future's state Id.
enum class state_id : size_t
{
    pending,
    resolved,
    failed
};

/// Future's state.
struct state : public core::noncopyable {
    virtual ~state() = default;

    state_id id() const noexcept { return id_; }

protected:
    explicit state(state_id kind) noexcept : id_(kind) {}

private:
    const state_id id_;
};

struct resolved_state_base : state {
    const void* raw_data() const noexcept { return this + 1; }

protected:
    explicit resolved_state_base() noexcept : state(state_id::resolved) {}
};

/// Future's shared state has a value.
template <typename T>
struct resolved_state : resolved_state_base {
    explicit resolved_state(const T& value) : resolved_state_base(), value_(value) {}

    static const resolved_state<T>* from(const state* state) noexcept {
        assert(state && state->id() == state_id::resolved);
        assert(dynamic_cast<const resolved_state<T>*>(state));

        return static_cast<const resolved_state<T>*>(state);
    }

    const T& value() const noexcept { return value_; }

private:
    T value_;
};

/// Future's shared state has a void value.
template <>
struct resolved_state<void> : state {
    resolved_state() noexcept : state(state_id::resolved) {}

    static const resolved_state<void>* from(const state* state) noexcept {
        assert(state && state->id() == state_id::resolved);
        assert(dynamic_cast<const resolved_state<void>*>(state));

        return static_cast<const resolved_state<void>*>(state);
    }
};

/// Future's shared state has an exception.
struct failed_state : state {
    explicit failed_state(const std::exception_ptr& ep)
        : state(state_id::failed), exception_ptr_(ep) {}

    static const failed_state* from(const state* state) noexcept {
        assert(state && state->id() == state_id::failed);
        assert(dynamic_cast<const failed_state*>(state));

        return static_cast<const failed_state*>(state);
    }

    const std::exception_ptr exception_ptr_;
};

/// Future is not ready yet.
struct pending_state : state {
    explicit pending_state(pending_state* prev) noexcept : state(state_id::pending), prev_(prev) {}

    static pending_state* from(state* state) noexcept {
        assert(state == nullptr || state->id() == state_id::pending);
        assert(state == nullptr || dynamic_cast<const pending_state*>(state));

        return static_cast<pending_state*>(state);
    }

    template <typename t_obj, typename t_member>
    static t_obj* from(t_member t_obj::* member_ptr, t_member* member_addr) {
        return core::object_from_field(member_ptr, member_addr);
    }

    /// Runs the continuation this node carries. noexcept by contract: a subscription callback is
    /// required to be noexcept, and next() catches inside itself, so nothing may escape into the
    /// lock-free resolution walking this list.
    virtual void resolve(const future_shared_state*, const state*) noexcept = 0;

    std::atomic<pending_state*> prev_;  ///< Previous state.
};
/// Every pending state below takes its callback by value and moves it into place: the callback
/// travels from the subscribing call straight into the node, without a copy on the way, and the
/// call itself goes through std::invoke so a pointer to a member function is a valid callback.
template <typename T, typename t_when_succeeded>
struct when_succeeded_pending_state : pending_state {
    when_succeeded_pending_state(pending_state* prev, t_when_succeeded when_succeeded) noexcept
        : pending_state(prev), when_succeeded_(std::move(when_succeeded)) {}

    void resolve(const future_shared_state*, const state* by_state) noexcept final {
        if (by_state->id() == state_id::resolved) {
            std::invoke(when_succeeded_, resolved_state<T>::from(by_state)->value());
        }
    }

    t_when_succeeded when_succeeded_;
};

template <typename T, typename t_when_succeeded>
struct void_when_succeeded_pending_state : pending_state {
    void_when_succeeded_pending_state(pending_state* prev, t_when_succeeded when_succeeded) noexcept
        : pending_state(prev), when_succeeded_(std::move(when_succeeded)) {}

    void resolve(const future_shared_state*, const state* by_state) noexcept final {
        if (by_state->id() == state_id::resolved) {
            std::invoke(when_succeeded_);
        }
    }

    t_when_succeeded when_succeeded_;
};

template <typename t_when_failed>
struct when_failed_pending_state : pending_state {
    when_failed_pending_state(pending_state* prev, t_when_failed when_failed) noexcept
        : pending_state(prev), when_failed_(std::move(when_failed)) {}

    void resolve(const future_shared_state*, const state* by_state) noexcept final {
        if (by_state->id() == state_id::failed) {
            std::invoke(when_failed_, failed_state::from(by_state)->exception_ptr_);
        }
    }

    t_when_failed when_failed_;
};

template <typename t_when_ready>
struct when_ready_pending_state : pending_state {
    when_ready_pending_state(pending_state* prev, t_when_ready when_ready) noexcept
        : pending_state(prev), when_ready_(std::move(when_ready)) {}

    void resolve(const future_shared_state*, const state*) noexcept final { std::invoke(when_ready_); }

    t_when_ready when_ready_;
};

/// The callback of this one takes the future itself.
template <class T, typename t_when_ready>
struct when_ready_pending_state_ex : pending_state {
    when_ready_pending_state_ex(pending_state* prev, t_when_ready when_ready) noexcept
        : pending_state(prev), when_ready_(std::move(when_ready)) {}

    void resolve(const future_shared_state* shared_state, const state*) noexcept final {
        std::invoke(when_ready_, future<T>(shared_state));
    }

    t_when_ready when_ready_;
};

struct set_exception_func {
    explicit set_exception_func(future_shared_state* result) noexcept : result_(result) {}

    explicit set_exception_func(const future_shared_state_ptr& result) noexcept : result_(result) {}

    void operator()(const std::exception_ptr& e) noexcept;

    future_shared_state_ptr result_;
};

template <typename T>
struct set_value_func {
    explicit set_value_func(future_shared_state* result) noexcept : result_(result) {}

    explicit set_value_func(const future_shared_state_ptr& result) noexcept : result_(result) {}

    void operator()(const T& value) noexcept;

    future_shared_state_ptr result_;
};

}  // namespace future_detail

/// Future's \e shared state (with reference counting).
///
/// It contains some \c Future state information and a \e result which may be not yet evaluated,
/// evaluated to a value (possibly void) or evaluated to an exception.
class future_shared_state : public core::refcounted_mt
{
    using state = future_detail::state;
    using state_id = future_detail::state_id;

public:
    enum init_promise_tag
    {
        init_promise
    };

    explicit future_shared_state(future_detail::state* state = nullptr) noexcept
        : state_(state), promise_count_() {}

    explicit future_shared_state(init_promise_tag) noexcept : state_(), promise_count_(1) {}

    /// Returns \c true if the Future result is not yet evaluated, otherwise - \c false.
    bool pending() const noexcept { return pending(state_.load()); }

    /// Returns \c true if the Future result has been evaluated to a value (possibly void) or an
    /// exception, otherwise - \c false.
    bool ready() const noexcept { return !pending(); }

    /// Returns \c true if the Future result has been evaluated to a value (possibly void),
    /// otherwise - \c false.
    bool has_value() const noexcept {
        const state* const state = state_.load();
        return state && state->id() == state_id::resolved;
    }

    /// Returns \c true if the Future result has been evaluated to an exception, otherwise - \c
    /// false.
    bool has_exception() const noexcept {
        const state* const state = state_.load();
        return state && state->id() == state_id::failed;
    }

    const std::exception_ptr& get_exception_ptr() const {
        const state* const state = state_.load();

        if (nullptr == state || state->id() != state_id::failed) {
            throw std::logic_error("Operation has NOT finished with an error.");
        }

        return future_detail::failed_state::from(state)->exception_ptr_;
    }

    /// Blocks until the shared state becomes ready. Nothing to report: this call cannot
    /// come back for any other reason.
    void wait() const {
        if (!ready()) event_.wait();
    }

    /// Blocks until the shared state becomes ready or \p timeout elapses.
    future_status wait_for(core::duration timeout) const {
        const bool became_ready = ready() || event_.wait_for(timeout);

        return became_ready ? future_status::ready : future_status::timeout;
    }

    /// Atomically stores the void value into the shared state and makes the state ready.
    /// \return true if this call resolved the state; false if it was already resolved by
    ///         another concurrent set_value()/set_exception() call. Since the shared state
    ///         may legitimately be resolved from multiple racing sources (see promise's
    ///         class comment), callers that don't care who won may ignore the result.
    bool set_value() noexcept {
        // Owner may be deleted during subsequent calls, so wee need our own pointer.
        future_shared_state_ptr keep_me(this);

        return resolve(&void_resolved_state_);
    }

    /// Atomically stores the value into the shared state and makes the state ready.
    /// \return true if this call resolved the state; false if it was already resolved by
    ///         another concurrent set_value()/set_exception() call. May be ignored.
    template <typename T>
    bool set_value(const T& value) {
        if (pending()) {
            // Owner may be deleted during subsequent calls, so wee need our own pointer.
            future_shared_state_ptr keep_me(this);

            state* new_state = construct<future_detail::resolved_state<T>>(value);

            if (resolve(new_state)) {
                return true;
            }

            destroy(new_state);
        }

        return false;
    }

    /// Atomically stores the exception pointer into the shared state and makes the state ready.
    /// \return true if this call resolved the state; false if it was already resolved by
    ///         another concurrent set_value()/set_exception() call. May be ignored.
    bool set_exception(const std::exception_ptr& exception = std::current_exception()) {
        assert(exception);

        if (pending()) {
            // Owner may be deleted during subsequent calls, so wee need our own pointer.
            future_shared_state_ptr keep_me(this);

            state* new_state = construct<future_detail::failed_state>(exception);

            if (resolve(new_state)) return true;

            destroy(new_state);
        }

        return false;
    }

    bool set_exception(const future_detail::state* failed_state) {
        return set_exception(future_detail::failed_state::from(failed_state)->exception_ptr_);
    }

    const state* get() const {
        state* state = state_.load();

        if (pending(state)) {
            event_.wait();
            state = state_.load();
        }

        assert(state);

        if (state->id() == state_id::failed) {
            std::rethrow_exception(future_detail::failed_state::from(state)->exception_ptr_);
        }

        return state;
    }

    /// @{ Subscriptions. The callback is a sink: it is moved into the pending state, so the
    /// caller's copy is spent here rather than duplicated.
    template <typename T, class t_when_succeeded>
    void when_succeeded(t_when_succeeded callback) const;

    template <class t_when_succeeded>
    void when_succeeded_void(t_when_succeeded callback) const;

    template <class t_when_failed>
    void when_failed(t_when_failed callback) const;

    /// The callback takes no argument.
    template <class t_when_ready>
    void when_ready0(t_when_ready callback) const;

    /// The callback takes the future itself.
    template <class T, class t_when_ready>
    void when_ready1(t_when_ready callback) const;

    template <typename T, typename t_result, class t_next_task>
    void next(t_next_task next_task, future_shared_state* result) const;
    /// @}

    template <class t_next_pending_state, class t_callback>
    bool try_attach_continuation(t_next_pending_state*& new_state, state*& state,
                                 t_callback&& callback) const;

    template <class t_next_pending_state, class t_callback>
    bool try_attach_continuation(t_next_pending_state*& new_state, state*& state,
                                 t_callback&& callback, future_shared_state* result) const;

    void increment_promise_count() noexcept { ++promise_count_; }

    void checked_decrement_promise_count();

    /// Closes an unresolved shared state whose producer is gone, with
    /// std::future_error(std::future_errc::broken_promise) -- the answer a waiter gets
    /// when the promise it was waiting on is destroyed without resolving.
    ///
    /// Does nothing to a state that is already resolved. A producer that is not a
    /// promise -- a component, whose stop ticket is its own shared state -- calls this
    /// on its way out, so that nobody is left waiting for something that will never
    /// happen.
    void abandon();

protected:
    ~future_shared_state() override;

    static bool pending(const state* state) noexcept {
        return nullptr == state || state_id::pending == state->id();
    }

    static bool ready(const state* state) noexcept { return !pending(state); }

    ssize_t decrement_promise_count() { return --promise_count_; }

    class allocator : public core::noncopyable
    {
    public:
        void* alloc(size_t size);
        void free(void* mem);

    private:
        static constexpr size_t buff_size = 32 * sizeof(size_t);

        bump_buffer<buff_size> buf_;
    };

    /// Allocates from al_ and placement-constructs a T there, freeing the block again if
    /// the constructor throws. Replaces the C++03 fixed-arity (0..3 argument) overload set.
    template <typename T, typename... t_args>
        requires std::constructible_from<T, t_args...>
    T* construct(t_args&&... args) const {
        void* mem = al_.alloc(sizeof(T));

        try {
            return new (mem) T(std::forward<t_args>(args)...);
        } catch (...) {
            al_.free(mem);
            throw;
        }
    }

    template <typename T>
    void destroy(T* obj) const {
        if (obj) {
            obj->~T();
            al_.free(obj);
        }
    }

private:
    bool resolve(state* new_state) noexcept {
        using namespace future_detail;

        state* const old_state = try_complete(new_state);

        if (old_state == new_state) {
            return false;  // somebody else has already completed the operation.
        }

        assert(nullptr == old_state || future_detail::state_id::pending == old_state->id());

        if (pending_state* to_resolve = pending_state::from(old_state)) {
            do {
                to_resolve->resolve(this, new_state);

                pending_state* to_delete = to_resolve;
                to_resolve = to_resolve->prev_.load();
                destroy(to_delete);
            } while (to_resolve);
        }

        event_.signal();

        return true;
    }

    /// Tries to complete the operation.
    ///
    /// \return the previous state.
    state* try_complete(state* new_state) noexcept {
        assert(new_state);

        while (true) {
            state* current_state = state_.load(std::memory_order_acquire);

            if (ready(current_state))
                return new_state;  // somebody else has already completed the operation.

            if (ref_cas(new_state, current_state)) return current_state;
        }
    }

    bool ref_cas(state* new_state, state*& old_state) const noexcept {
        return state_.compare_exchange_weak(old_state, new_state, std::memory_order_acq_rel,
                                            std::memory_order_acquire);
    }

    mutable std::atomic<state*> state_;
    one_shot_event event_;
    std::atomic<ssize_t>
        promise_count_;  ///< Number of Promise instances that refer to this shared state instance.
    mutable allocator al_;

    static future_detail::resolved_state<void> void_resolved_state_;
    static future_shared_state_ptr void_ready_future_;

    friend future<void> make_ready_future();
};

namespace future_detail {

template <typename T, typename t_result, typename t_next_task>
struct next_helper {
    static void call(t_next_task& task, const state* state, future_shared_state* ticket) {
        try {
            const resolved_state<T>* by_state = resolved_state<T>::from(state);
            ticket->set_value(std::invoke(task, by_state->value()));
        } catch (...) {
            ticket->set_exception();
        }
    }
};

template <typename t_result, typename t_next_task>
struct next_helper<void, t_result, t_next_task> {
    static void call(t_next_task& task, const state*, future_shared_state* ticket) {
        try {
            ticket->set_value(std::invoke(task));
        } catch (...) {
            ticket->set_exception();
        }
    }
};

template <typename T, typename t_next_task>
struct next_helper<T, void, t_next_task> {
    static void call(t_next_task& task, const state* state, future_shared_state* ticket) {
        try {
            const resolved_state<T>* by_state = resolved_state<T>::from(state);
            std::invoke(task, by_state->value());
            ticket->set_value();
        } catch (...) {
            ticket->set_exception();
        }
    }
};

template <typename t_next_task>
struct next_helper<void, void, t_next_task> {
    static void call(t_next_task& task, const state*, future_shared_state* ticket) {
        try {
            std::invoke(task);
            ticket->set_value();
        } catch (...) {
            ticket->set_exception();
        }
    }
};

template <typename T, typename t_result, typename t_next_task>
struct next_pending_state : pending_state {
    using my_state = next_pending_state<T, t_result, t_next_task>;

    next_pending_state(pending_state* prev, t_next_task next_task,
                       future_shared_state* result)
        : pending_state(prev), next_task_(std::move(next_task)), result_(result) {}

    static void do_call(t_next_task& task, const state* state, future_shared_state* result) {
        if (state->id() == state_id::resolved) {
            using helper = next_helper<T, t_result, t_next_task>;
            helper::call(task, state, result);
        } else {
            assert(state->id() == state_id::failed);
            result->set_exception(state);
        }
    }

    void resolve(const future_shared_state*, const state* by_state) noexcept final {
        do_call(next_task_, by_state, result_.get());
    }

    t_next_task next_task_;
    future_shared_state_ptr result_;
};

}  // namespace future_detail

// The callback is a sink parameter throughout: it is moved into the pending state the first time
// one is built, and every later use in the same call goes through that state's copy -- which is
// why the already-resolved branches invoke `new_state ? new_state->cb : callback` rather than the
// parameter, whose contents have moved on by then.

template <typename T, class t_when_succeeded>
void future_shared_state::when_succeeded(t_when_succeeded callback) const {
    using WDPS = future_detail::when_succeeded_pending_state<T, t_when_succeeded>;

    WDPS* new_state = nullptr;
    state* state = state_.load();

    while (true) {
        if (state) {
            switch (state->id()) {
                case state_id::resolved:
                    std::invoke(new_state ? new_state->when_succeeded_ : callback,
                                future_detail::resolved_state<T>::from(state)->value());

                    destroy(new_state);
                    return;

                case state_id::failed:
                    destroy(new_state);
                    return;

                default:
                    assert(state->id() == state_id::pending);
                    break;
            }
        }

        if (try_attach_continuation(new_state, state, std::move(callback))) return;
    }
}

template <class t_when_succeeded>
void future_shared_state::when_succeeded_void(t_when_succeeded callback) const {
    using void_when_succeeded_pending_state =
        future_detail::void_when_succeeded_pending_state<void, t_when_succeeded>;

    void_when_succeeded_pending_state* new_state = nullptr;
    state* state = state_.load();

    while (true) {
        if (state) {
            switch (state->id()) {
                case state_id::resolved:
                    std::invoke(new_state ? new_state->when_succeeded_ : callback);

                    destroy(new_state);
                    return;

                case state_id::failed:
                    destroy(new_state);
                    return;

                default:
                    assert(state->id() == state_id::pending);
                    break;
            }
        }

        if (try_attach_continuation(new_state, state, std::move(callback))) return;
    }
}

template <class t_when_failed>
void future_shared_state::when_failed(t_when_failed callback) const {
    using when_failed_pending_state = future_detail::when_failed_pending_state<t_when_failed>;

    when_failed_pending_state* new_state = nullptr;
    state* state = state_.load();

    while (true) {
        if (state) {
            switch (state->id()) {
                case state_id::resolved:
                    destroy(new_state);
                    return;

                case state_id::failed:
                    std::invoke(new_state ? new_state->when_failed_ : callback,
                                future_detail::failed_state::from(state)->exception_ptr_);

                    destroy(new_state);
                    return;

                default:
                    assert(state->id() == state_id::pending);
                    break;
            }
        }

        if (try_attach_continuation(new_state, state, std::move(callback))) return;
    }
}

template <class t_when_ready>
void future_shared_state::when_ready0(t_when_ready callback) const {
    using when_ready_pending_state = future_detail::when_ready_pending_state<t_when_ready>;

    when_ready_pending_state* new_state = nullptr;
    state* state = state_.load();

    while (true) {
        if (state && state->id() != state_id::pending) {
            std::invoke(new_state ? new_state->when_ready_ : callback);

            destroy(new_state);
            return;
        }

        if (try_attach_continuation(new_state, state, std::move(callback))) return;
    }
}

template <class T, class t_when_ready>
void future_shared_state::when_ready1(t_when_ready callback) const {
    using when_ready_pending_state_ex = future_detail::when_ready_pending_state_ex<T, t_when_ready>;

    when_ready_pending_state_ex* new_state = nullptr;
    state* state = state_.load();

    while (true) {
        if (ready(state)) {
            std::invoke(new_state ? new_state->when_ready_ : callback, future<T>(this));

            destroy(new_state);
            return;
        }

        if (try_attach_continuation(new_state, state, std::move(callback))) return;
    }
}

template <typename T, typename t_result, class t_next_task>
void future_shared_state::next(t_next_task next_task, future_shared_state* result) const {
    using next_pending_state = future_detail::next_pending_state<T, t_result, t_next_task>;

    next_pending_state* new_state = nullptr;
    state* state = state_.load();

    while (true) {
        // ready(), not succeeded(): do_call handles both outcomes -- it runs the task on a
        // value and forwards the exception on a failure -- and a failed state must never
        // reach try_attach_continuation, which would CAS a pending node over it and push a
        // resolved future back to pending.
        if (ready(state)) {
            next_pending_state::do_call(new_state ? new_state->next_task_ : next_task, state,
                                        result);
            destroy(new_state);
            return;
        }

        if (try_attach_continuation(new_state, state, std::move(next_task), result)) return;
    }
}

template <class t_next_pending_state, class t_callback>
inline bool future_shared_state::try_attach_continuation(t_next_pending_state*& new_state,
                                                         state*& state,
                                                         t_callback&& callback) const {
    future_detail::pending_state* prev_state = future_detail::pending_state::from(state);

    if (new_state) {
        new_state->prev_ = prev_state;
    } else {
        new_state = construct<t_next_pending_state>(prev_state, std::forward<t_callback>(callback));
    }

    if (ref_cas(new_state, state)) {
        return true;
    }

    new_state->prev_ = nullptr;

    return false;
}

template <class t_next_pending_state, class t_callback>
inline bool future_shared_state::try_attach_continuation(t_next_pending_state*& new_state,
                                                         state*& state, t_callback&& callback,
                                                         future_shared_state* result) const {
    future_detail::pending_state* prev_state = future_detail::pending_state::from(state);

    if (new_state)
        new_state->prev_ = prev_state;
    else
        new_state = construct<t_next_pending_state>(prev_state, std::forward<t_callback>(callback),
                                                    result);

    if (ref_cas(new_state, state)) return true;

    new_state->prev_ = nullptr;

    return false;
}

namespace future_detail {

inline void set_exception_func::operator()(const std::exception_ptr& e) noexcept {
    result_->set_exception(e);
}

template <typename T>
void set_value_func<T>::operator()(const T& value) noexcept {
    result_->set_value(value);
}

template <>
struct set_value_func<void> {
    explicit set_value_func(future_shared_state* result) : result_(result) {}

    explicit set_value_func(const future_shared_state_ptr& result) : result_(result) {}

    void operator()() const noexcept { result_->set_value(); }
    future_shared_state_ptr result_;
};

template <class t_next_async_task, class T>
struct next_async_task {
    next_async_task(const t_next_async_task& task, const future_shared_state_ptr& result)
        : task_(task), result_(result) {}

    void operator()(const T& value) noexcept {
        try {
            task_(value)
                .when_succeeded(set_value_func<void>(result_))
                .when_failed(set_exception_func(result_));
        } catch (...) {
            result_->set_exception();
        }
    }

    t_next_async_task task_;
    future_shared_state_ptr result_;
};

// @todo: do we need this template specialization anymore?
template <class t_next_async_task>
struct next_async_task<t_next_async_task, void> {
    next_async_task(const t_next_async_task& task, const future_shared_state_ptr& result)
        : task_(task), result_(result) {}

    void operator()() noexcept {
        try {
            task_()
                .when_succeeded(set_value_func<void>(result_))
                .when_failed(set_exception_func(result_));
        } catch (...) {
            result_->set_exception();
        }
    }

    t_next_async_task task_;
    future_shared_state_ptr result_;
};

/// Routes a success subscription to the arity the value type calls for: with the value for a
/// future<T>, with nothing for a future<void>.
template <typename T>
struct when_succeeded_helper {
    template <class t_when_succeeded>
    static void when_succeeded(const future_shared_state* state,
                               t_when_succeeded&& when_succeeded_callback) {
        state->when_succeeded<T>(std::forward<t_when_succeeded>(when_succeeded_callback));
    }
};

template <>
struct when_succeeded_helper<void> {
    template <class t_when_succeeded>
    static void when_succeeded(const future_shared_state* state,
                               t_when_succeeded&& when_succeeded_callback) {
        state->when_succeeded_void(std::forward<t_when_succeeded>(when_succeeded_callback));
    }
};

}  // namespace future_detail

}  // export namespace wxl::async

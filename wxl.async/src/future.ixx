module;

#include "abi.h"

export module wxl.async:future;

import :future_shared_state;
import wxl.core;
import std;

export namespace wxl::async {

namespace future_detail {

/// Base untyped future.
class future_base : public shared_state_holder<const future_shared_state>
{
public:
    /// Returns \c true if the asynchronous result associated with this future is ready (has a value
    /// or exception stored in the shared state), \c false otherwise.
    ///
    /// There are often situations where a \c get() call on a future may not be a blocking call, or
    /// is only a blocking call under certain circumstances. This method gives the ability to test
    /// for early completion and allows us to avoid associating a continuation, which needs to be
    /// scheduled with some non-trivial overhead and near-certain loss of cache efficiency.
    ///
    /// @throw std::logic_error if this instance does not refer to a shared state.
    inline bool is_ready() const { return checked_state()->ready(); }

    /// Returns \c true if the asynchronous result associated with this future has a stored
    /// exception, \c false otherwise.
    ///
    /// @throw std::logic_error if this instance does not refer to a shared state.
    inline bool has_exception() const { return checked_state()->has_exception(); }

    /// Returns \c true if the asynchronous result associated with this future has a stored value,
    /// \c false otherwise.
    ///
    /// @throw std::logic_error if this instance does not refer to a shared state.
    inline bool has_value() const { return checked_state()->has_value(); }

    /// Returns the stored exception.
    ///
    /// @throw std::logic_error if this instance does not refer to a shared state.
    /// @throw std::logic_error if the operation has *not* finished with an error.
    inline const std::exception_ptr& get_exception_ptr() const {
        return checked_state()->get_exception_ptr();
    }

    ///@{
    /// Waits for the result to become available.
    ///
    /// \note Calling \c wait on the same future from multiple threads is \e not safe; the intended
    /// use is for each thread that waits on the same shared state to have a \e copy of a future.
    ///
    /// \throw std::logic_error if this instance does not refer to a shared state.
    inline void wait() const { checked_state()->wait(); }

    inline future_status wait_for(core::duration timeout) const {
        return checked_state()->wait_for(timeout);
    }
    ///@}

protected:
    ~future_base();

    using base = shared_state_holder<const future_shared_state>;

    future_base(const future_base&) = default;

    inline future_base(future_base&& other) noexcept : base(std::move(other)) {}

    inline future_base& operator=(const future_base& other)  // Copy assign
    {
        base::operator=(other);
        return *this;
    }

    inline future_base& operator=(future_base&& other) noexcept  // Move assign
    {
        base::operator=(std::move(other));
        return *this;
    }

    inline explicit future_base(const future_shared_state* state) noexcept
        : shared_state_holder<const future_shared_state>(state) {}

private:
    using shared_state_holder<const future_shared_state>::detach_state;

    friend class future_accessor;
};

class future_accessor
{
public:
    inline static const future_shared_state* state_of(const future_base& f) { return f.state(); }

    inline static const future_shared_state* checked_state_of(const future_base& f) {
        return f.checked_state();
    }

    inline static const future_shared_state* checked(const future_shared_state* state) {
        if (state) return state;

        throw_invalid_future();
    }

    template <typename T>
    static const future_shared_state* detach_state(future<T>& f) {
        return f.detach_state();
    }
};

/// Satisfied by future<T> for any T. It is what tells a pack of futures apart from a container
/// of them, so the when_all_* forms taking either can coexist as plain overloads.
template <class T>
concept future_like = std::derived_from<std::remove_cvref_t<T>, future_base>;

template <typename T>
struct future_get_return {
    using type = const T&;
};

template <>
struct future_get_return<void> {
    using type = void;
};

template <typename T>
struct future_get_return<T&> {
    using type = T&;
};

/// Result of a continuation attached to a future<T>: it is invoked with the future's value, or
/// with no argument at all when T is void. Naming the two invoke_result specializations rather
/// than their ::type is what keeps the unused branch from being instantiated.
template <typename T, class t_task>
using result_of_t = typename std::conditional_t<std::is_void_v<T>, std::invoke_result<t_task>,
                                                std::invoke_result<t_task, T>>::type;

}  // namespace future_detail

/// Represents a future result of an asynchronous operation - a result that will eventually appear
/// in the future after the processing is complete.
///
/// The class template future provides a mechanism to access the result of \e asynchronous
/// operations :
/// \li An asynchronous operation (e.g. created via promise) can provide a future object to the
/// creator of that asynchronous operation (e.g. via promise::get_future).
/// \li The creator of the asynchronous operation can then use a variety of methods to query, wait
/// for, or extract a value from future. These methods may block if the asynchronous operation has
/// not yet provided a value.
/// \li When the asynchronous operation is ready to send a result to the creator, it can do so by
/// modifying <em> shared state </em> (e.g. via promise::set_value) that is linked to the creator's
/// future.
///
/// future is the synchronization object constructed around the \e receiving end of the promise
/// channel. It allows for the separation of the initiation of an operation and the act of waiting
/// for its result.
///
/// future is \e copyable and multiple shared future objects may refer to the same shared state.
///
/// \note Access to the same shared state from multiple threads is safe if each thread does it
/// through its own copy of a future object.
///
/// \see http://en.cppreference.com/w/cpp/thread/shared_future .
template <typename T>
class future : public future_detail::future_base
{
public:
    /// Copy constructor.
    future(const future<T>& other) : base(other) {}

    /// Move constructor.
    future(future<T>&& other) noexcept : base(std::move(other)) {}

    /// Copy assignment.
    future<T>& operator=(const future<T>& other) noexcept {
        base::operator=(other);
        return *this;
    }

    /// Move assignment.
    future<T>& operator=(future<T>&& other) noexcept {
        base::operator=(std::move(other));
        return *this;
    }

    /// Constructor.
    explicit future(const future_shared_state* state = nullptr) noexcept : future_base(state) {}

    /// Constructor.
    explicit future(const future_shared_state_ptr& state) noexcept : future_base(state.get()) {}

    /// Returns the result. If the result is not ready, the method will block. When completes, it
    /// either returns a value or throws an exception.
    ///
    /// This method waits until the future has a valid result and retrieves it. It effectively calls
    /// wait() in order to wait for the result.
    ///
    /// \exception If an exception was stored in the shared state referenced by this future (e.g.
    /// via a call to promise::set_exception()) then that exception will be thrown.
    typename future_detail::future_get_return<T>::type get() const {
        const future_detail::state* state = checked_state()->get();
        return future_detail::resolved_state<T>::from(state)->value();
    }

    /// Subscribes to the successful finish of the asynchronous operation (a value is stored in the
    /// shared state).
    ///
    /// The callback takes the operation's result -- `void(const T&)`, or `void()` when T is void.
    /// It is taken by forwarding reference and moved into the subscription, so a callback that
    /// carries state is never copied on the way in.
    ///
    /// \return The const reference to itself.
    ///
    /// \note The callback must be noexcept: a subscription has nowhere to report an exception.
    template <class t_when_succeeded>
        requires future_detail::nothrow_success_callback<std::remove_cvref_t<t_when_succeeded>, T>
    const future<T>& when_succeeded(t_when_succeeded&& when_succeeded_callback) const {
        future_detail::when_succeeded_helper<T>::when_succeeded(
            checked_state(), std::forward<t_when_succeeded>(when_succeeded_callback));
        return *this;
    }

    /// Subscribes to the failure of the asynchronous operation (an exception is stored in the
    /// shared state).
    ///
    /// The callback takes the stored exception -- `void(const std::exception_ptr&)`.
    ///
    /// \return The const reference to itself.
    ///
    /// \note The callback must be noexcept: a subscription has nowhere to report an exception.
    template <class t_when_failed>
        requires future_detail::nothrow_callback<std::remove_cvref_t<t_when_failed>, const std::exception_ptr&>
    const future<T>& when_failed(t_when_failed&& when_failed_callback) const {
        checked_state()->when_failed(std::forward<t_when_failed>(when_failed_callback));
        return *this;
    }

    /// Subscribes to the completion of the asynchronous operation (a value or exception is stored
    /// in the shared state).
    ///
    /// The callback takes either this future itself -- `void(const future<T>&)`, which is how the
    /// outcome is read -- or nothing at all, `void()`. A functor that accepts both forms is
    /// given the future: it is the form that carries more.
    ///
    /// \note The callback must be noexcept: a subscription has nowhere to report an exception.
    ///@{
    template <class t_when_ready>
        requires future_detail::nothrow_callback<std::remove_cvref_t<t_when_ready>, future<T>>
    const future<T>& when_ready(t_when_ready&& when_ready_callback) const {
        checked_state()->template when_ready1<T>(std::forward<t_when_ready>(when_ready_callback));
        return *this;
    }

    template <class t_when_ready>
        requires future_detail::nothrow_callback<std::remove_cvref_t<t_when_ready>> &&
                 (!std::invocable<std::remove_cvref_t<t_when_ready>&, future<T>>)
    const future<T>& when_ready(t_when_ready&& when_ready_callback) const {
        checked_state()->when_ready0(std::forward<t_when_ready>(when_ready_callback));
        return *this;
    }
    ///@}

    /// Attaches a synchronous continuation to this future.
    ///
    /// The continuation will be invoked when this future completes successfully and will receive
    /// the value returned by this future.
    ///
    /// The callback is `t_result(const T&)`, or `t_result()` when T is void; whatever it returns
    /// becomes the result of the returned future. A pointer to a member function works too: the
    /// call goes through std::invoke.
    ///
    /// \param task Operation to be started when this future has a value.
    ///
    /// \return The future the continuation's own result will appear in.
    template <class t_next_task>
    future<future_detail::result_of_t<T, std::remove_cvref_t<t_next_task>>> next(
        t_next_task&& task) const {
        using t_task = std::remove_cvref_t<t_next_task>;
        using t_result = future_detail::result_of_t<T, t_task>;

        future_shared_state_ptr ticket(new future_shared_state());

        checked_state()->template next<T, t_result>(std::forward<t_next_task>(task), ticket.get());

        return future<t_result>(ticket.get());
    }

    /// Attaches an asynchronous continuation (that starts its own asynchronous operation and
    /// returns the corresponding future) to this future.
    ///
    /// The continuation will be invoked when this future completes successfully and will receive
    /// the value returned by this future.
    ///
    /// The callback is `future<void>(const T&)`, or `future<void>()` when T is void: it starts its
    /// own operation and hands back the future for it.
    ///
    /// \param task Operation to be started when this future has a value.
    ///
    /// \return The future the continuation's own result will appear in.
    ///
    /// \note Only a `future<void>` continuation result is supported.
    template <class t_next_async_task>
    future<void> next_unwrap(t_next_async_task&& task) const {
        using t_task = std::remove_cvref_t<t_next_async_task>;

        future_shared_state_ptr ticket(new future_shared_state());

        when_succeeded(future_detail::next_async_task<t_task, T>(
            std::forward<t_next_async_task>(task), ticket));
        when_failed(future_detail::set_exception_func(ticket));

        return future<void>(ticket.get());
    }

protected:
    using base = future_detail::future_base;
};

template <>
inline void future<void>::get() const {
    checked_state()->get();
}

namespace future_detail {

/// The shared state of a when_all_* combination, and the subscriptions that feed it. Nothing
/// about it depends on what the futures carry -- a combinator only ever asks whether they
/// finished -- so the whole machinery is compiled once, in future.cpp, instead of once per pack
/// of types. Only the counting handle is visible here.
class countdown
{
public:
    countdown();

    countdown(const countdown&) = delete;
    countdown& operator=(const countdown&) = delete;

    /// Counts one future in, to be awaited whichever way it ends.
    void add_ready(const future_shared_state* f);

    /// Counts one future in; the first failure among them fails the combination as a whole.
    void add_succeeded_or_failed(const future_shared_state* f);

    /// Takes off the count the constructor put on -- which is what kept the countdown from
    /// reaching zero while futures were still being added -- and hands out the combined future.
    /// Called once, after the last add.
    future<void> get_future();

private:
    future_shared_state_ptr state_;
};

/// The shared state of a when_any combination: resolved by whichever future finishes first, with
/// that future's outcome. Defined in future.cpp for the same reason as countdown.
class first_of
{
public:
    first_of();

    first_of(const first_of&) = delete;
    first_of& operator=(const first_of&) = delete;

    void add(const future_shared_state* f);

    future<void> get_future() const;

private:
    future_shared_state_ptr state_;
};

/// Shorthand for the combinators below: every one of them starts by demanding a live state.
inline const future_shared_state* state_of(const future_base& f) {
    return future_accessor::checked_state_of(f);
}

}  // namespace future_detail

///@{
/// Creates a combined operation that succeeds once all of the given operations have succeeded,
/// and fails as soon as any one of them fails.
///
/// Every combinator below comes in the same three forms, and the concepts are what keep them
/// apart: a future is neither an iterator nor a range, so a call matches exactly one of them.
/// The pack form accepts futures of differing result types and copies none of them.
template <std::input_iterator t_iterator, std::sentinel_for<t_iterator> t_sentinel>
inline future<void> when_all_succeeded_or_any_failed(t_iterator begin, t_sentinel end) {
    future_detail::countdown all;

    for (; begin != end; ++begin) all.add_succeeded_or_failed(future_detail::state_of(*begin));

    return all.get_future();
}

template <std::ranges::input_range t_range>
inline future<void> when_all_succeeded_or_any_failed(t_range&& futures) {
    return when_all_succeeded_or_any_failed(std::ranges::begin(futures), std::ranges::end(futures));
}

template <future_detail::future_like... t_futures>
inline future<void> when_all_succeeded_or_any_failed(const t_futures&... futures) {
    future_detail::countdown all;

    (all.add_succeeded_or_failed(future_detail::state_of(futures)), ...);

    return all.get_future();
}
///@}

///@{
/// Creates a future that becomes ready once all of the given futures are ready, whether they
/// succeeded or failed.
template <std::input_iterator t_iterator, std::sentinel_for<t_iterator> t_sentinel>
inline future<void> when_all_ready(t_iterator begin, t_sentinel end) {
    future_detail::countdown all;

    for (; begin != end; ++begin) all.add_ready(future_detail::state_of(*begin));

    return all.get_future();
}

template <std::ranges::input_range t_range>
inline future<void> when_all_ready(t_range&& futures) {
    return when_all_ready(std::ranges::begin(futures), std::ranges::end(futures));
}

template <future_detail::future_like... t_futures>
inline future<void> when_all_ready(const t_futures&... futures) {
    future_detail::countdown all;

    (all.add_ready(future_detail::state_of(futures)), ...);

    return all.get_future();
}
///@}

///@{
/// Creates a future that becomes ready once all of the given futures are ready, and carries them:
/// a tuple for a pack of arguments, a vector for a pair of iterators or a range. The combination
/// itself never fails -- the outcome of an operation stays with its own future, and the caller
/// reads them out of the result one by one.
///
/// These are Boost.Thread's `when_all` signatures, less what wxl has no counterpart for: nothing
/// here is deferred, so there is no task to start on the way in, and a wxl future is a shared one,
/// copied into the result instead of being moved out of the caller's hands -- the arguments stay
/// valid. With nothing to wait for, the result is ready at once and holds an empty tuple or an
/// empty vector.
template <std::input_iterator t_iterator, std::sentinel_for<t_iterator> t_sentinel>
    requires future_detail::future_like<std::iter_value_t<t_iterator>>
inline future<std::vector<std::iter_value_t<t_iterator>>> when_all(t_iterator begin,
                                                                   t_sentinel end) {
    // Collected before subscribing rather than in the continuation: an input iterator is
    // single-pass, and the one vector both feeds the subscriptions and becomes the result.
    std::vector<std::iter_value_t<t_iterator>> futures(begin, end);
    future<void> ready = when_all_ready(futures);

    return ready.next([futures = std::move(futures)]() mutable { return std::move(futures); });
}

template <std::ranges::input_range t_range>
    requires future_detail::future_like<std::ranges::range_value_t<t_range>>
inline future<std::vector<std::ranges::range_value_t<t_range>>> when_all(t_range&& futures) {
    return when_all(std::ranges::begin(futures), std::ranges::end(futures));
}

template <future_detail::future_like... t_futures>
inline future<std::tuple<t_futures...>> when_all(const t_futures&... futures) {
    return when_all_ready(futures...).next(
        [futures...]() { return std::tuple<t_futures...>(futures...); });
}
///@}

///@{
/// Creates a future that becomes ready as soon as any one of the given futures does, and takes
/// that first outcome: ready if it succeeded, failed with its exception if it did not. With
/// nothing to wait for, the result is ready at once.
template <std::input_iterator t_iterator, std::sentinel_for<t_iterator> t_sentinel>
inline future<void> when_any(t_iterator begin, t_sentinel end) {
    if (begin == end) return make_ready_future();

    future_detail::first_of first;

    for (; begin != end; ++begin) first.add(future_detail::state_of(*begin));

    return first.get_future();
}

template <std::ranges::input_range t_range>
inline future<void> when_any(t_range&& futures) {
    return when_any(std::ranges::begin(futures), std::ranges::end(futures));
}

template <future_detail::future_like... t_futures>
inline future<void> when_any(const t_futures&... futures) {
    if constexpr (sizeof...(futures) == 0) {
        return make_ready_future();
    } else {
        future_detail::first_of first;

        (first.add(future_detail::state_of(futures)), ...);

        return first.get_future();
    }
}
///@}

namespace future_detail {

/// Base untyped promise.
class promise_base : public shared_state_holder<future_shared_state>
{
public:
    /// Sets the result to indicate an exception.
    ///
    /// Atomically stores the exception pointer \c error into the shared state and makes the
    /// state ready.
    ///
    /// \note Does nothing if the shared state already stores a value or exception.
    ///
    /// \return true if this call resolved the state; false if it was already resolved by
    ///         another concurrent set_value()/set_exception() call. May be ignored.
    ///
    /// \exception std:logic_exception if there is no shared state.
    inline bool set_exception(const std::exception_ptr& error) {
        return checked_state()->set_exception(error);
    }

protected:
    using base = shared_state_holder<future_shared_state>;

    /// Copy constructor.
    inline promise_base(const promise_base& other) : base(other) {
        if (future_shared_state* state = other.state()) state->increment_promise_count();
    }

    /// Move constructor.
    inline promise_base(promise_base&& other) noexcept : base(std::move(other)) {}

    /// Copy assignmnet.
    inline promise_base& operator=(const promise_base& other) {
        future_shared_state* const my_state = state();
        future_shared_state* const other_state = other.state();

        if (my_state != other_state) {
            if (other_state) other_state->increment_promise_count();

            if (my_state) my_state->checked_decrement_promise_count();

            base::operator=(other);
        }

        return *this;
    }

    /// Move assignmnet.
    inline promise_base& operator=(promise_base&& other) noexcept {
        future_shared_state* const my_state = state();

        if (my_state) my_state->checked_decrement_promise_count();

        base::operator=(std::move(other));
        return *this;
    }

    inline explicit promise_base(future_shared_state* state)
        : shared_state_holder<future_shared_state>(state) {
        if (state) state->increment_promise_count();
    }

    inline promise_base(future_shared_state* state, future_shared_state::init_promise_tag)
        : shared_state_holder<future_shared_state>(state) {
        assert(state);
    }

    inline ~promise_base() {
        if (future_shared_state* state = base::state()) {
            state->checked_decrement_promise_count();
        }
    }
};

}  // namespace future_detail

/// The class template promise provides a facility to store a value or an exception that is later
/// acquired asynchronously via a future object created by the promise object.
///
/// Each promise is associated with a \e shared state (future_shared_state), which contains some state
/// information and a \e result which may be not yet evaluated, evaluated to a value (possibly void)
/// or evaluated to an exception.
///
/// The promise stores the result or the exception in the shared state. Marks the state ready and
/// unblocks any thread waiting on a future associated with the shared state.
///
/// The promise is the "push" end of the promise - future communication channel: the operation that
/// stores a value in the shared state synchronizes with the successful return from any function
/// that is waiting on the shared state (such as future::get).
///
/// @see http://en.cppreference.com/w/cpp/thread/promise.
template <typename T>
class promise : public future_detail::promise_base
{
    using base = future_detail::promise_base;
    using init_tag = future_shared_state::init_promise_tag;

public:
    /// Constructor.
    promise() noexcept : base(new future_shared_state(init_tag()), init_tag()) {}

    /// Copy constructor.
    promise(const promise<T>& other) : base(other) {}

    /// Move constructor.
    promise(promise<T>&& other) noexcept : base(std::move(other)) {}

    /// Copy assignment.
    promise<T>& operator=(const promise<T>& other) {
        base::operator=(other);
        return *this;
    }

    /// Move assignment.
    promise<T>& operator=(promise<T>&& other) noexcept {
        base::operator=(std::move(other));
        return *this;
    }

    /// Returns a future associated with the promised result.
    future<T> get_future() const { return future<T>(checked_state()); }

    /// Sets the result to the given value.
    /// \return true if this call resolved the state; false if it was already resolved by
    ///         another concurrent set_value()/set_exception() call. May be ignored.
    bool set_value(const T& value) { return checked_state()->set_value(value); }

    static promise<T> invalid_promise() { return promise(nullptr); }

private:
    explicit promise(future_shared_state* state) : promise_base(state) {}
};

/// promise void specialization, used to communicate stateless events.
template <>
class promise<void> : public future_detail::promise_base
{
    using base = future_detail::promise_base;
    using init_tag = future_shared_state::init_promise_tag;

public:
    inline promise() noexcept : base(new future_shared_state(init_tag()), init_tag()) {}

    promise(const promise<void>&) = default;

    inline promise(promise<void>&& other) noexcept  // Move ctor
        : base(std::move(other)) {}

    inline promise<void>& operator=(const promise<void>& other) {  // Copy assign
        base::operator=(other);
        return *this;
    }

    inline promise<void>& operator=(promise<void>&& other) noexcept {  // Move assign
        base::operator=(std::move(other));
        return *this;
    }

    /// \return true if this call resolved the state; false if it was already resolved by
    ///         another concurrent set_value()/set_exception() call. May be ignored.
    inline bool set_value() { return checked_state()->set_value(); }

    inline future<void> get_future() const { return future<void>(checked_state()); }

    inline void swap(promise<void>& other) { promise_base::swap(other); }

    inline static promise<void> invalid_promise() { return promise(nullptr); }

private:
    inline explicit promise(future_shared_state* dummy) : promise_base(dummy) {}
};

// future utils.

/// Returns a promise without a shared state.
template <typename T>
inline promise<T> make_empty_promise() {
    return promise<T>::invalid_promise();
}

///@{
/// Some functions may know the value at the point of construction. In these cases the value is
/// immediately available, but needs to be returned as a future.
inline future<void> make_ready_future() {
    return future<void>(future_shared_state::void_ready_future_);
}

template <typename T>
inline future<T> make_ready_future(const T& value) {
    promise<T> p;
    p.set_value(value);
    return p.get_future();
}
///@}

}  // export namespace wxl::async

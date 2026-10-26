#pragma once

// The private half of component: its shared state, and the functor that stops it. Included from
// the purview of a module implementation unit (after `module wxl.async;`), so what it declares is
// attached to wxl.async exactly as if it stood in the interface -- and it names no standard header
// of its own, the including unit's `import std;` covers that.
//
// Nothing here carries a body. An inline function attached to a named module may be defined in
// only one translation unit, and this header is included by several; the definitions live in
// component.cpp.

namespace wxl::async {

/// component's state.
class component::impl_t : public future_shared_state, public core::synchronized_
{
    using base = future_shared_state;

public:
    /// Constructs the state that synchronizes on \p sync_root_arg.
    explicit impl_t(core::sync_root_holder sync_root_arg);
    /// Constructs the state that synchronizes on \p sync_root_arg.
    explicit impl_t(core::not_null<core::sync_root> sync_root_arg);

    /// \note Aborts if the component has already been disposed -- that is an architecture-level
    ///       invariant violation, not an application error.
    component* owner_unsafe();

    /// Returns the stop ticket: a ticket on this state.
    ticket stop_ticket();

    /// Returns the recorded stop reason, or `nullptr` if no stop has been requested.
    const std::exception_ptr* stop_reason_ptr() const noexcept;

    /// Records the stop reason, unless a reason has been recorded already.
    ///
    /// An empty \p error is replaced by the exception of a failed start, or by
    /// `operation_canceled_exception` if a start is under way; during process termination it
    /// is recorded as it is. The reason is published by a compare-and-exchange, so of several
    /// callers exactly one records its reason.
    /// \return `true` if this call recorded the reason.
    bool try_to_win_stopping_race(const std::exception_ptr& error);

    /// Requests the stop with \p error as the reason, and carries out the stop if this call
    /// recorded the reason.
    void stop_async_unsafe(const std::exception_ptr& error = std::exception_ptr());

    /// Requests the stop with \p reason; the same as the form that takes the exception.
    void stop_async_unsafe(const stop_reason& reason);


    /// Tests whether the component has been disposed of, which is when the state no longer
    /// has an owner.
    bool is_disposed() const noexcept;

    /// Virtual: a state that holds more than the lifecycle -- the thread of a threaded
    /// component -- lets go of it here, on every way a component is disposed of.
    virtual void dispose();

protected:
    ~impl_t() override;

    /// Gets the starting task of the component.
    const task_tree& starting_task() const;

    /// Gets the running task of the component.
    const task_tree& running_task() const;

    /// Gets the subtask of the starting task that covers the call to `on_starting()`.
    const task_tree& on_starting_task() const;

private:
    /// The three steps of stopping. They live here rather than in component because
    /// nothing inline calls them: declared there, they would ride into every unit that
    /// imports wxl.async and tell it nothing. Being a member class of component, the
    /// state reaches its owner's virtuals just as well.
    ///@{
    void stop_async_impl();

    void late_call_on_stopping();

    void confirm_stopped();
    ///@}

    std::atomic<const std::exception_ptr*> stop_reason_{nullptr};

    std::atomic<component*> owner_{nullptr};

    task_tree_handle starting_task_;
    task_tree_handle running_task_;
    task_tree on_starting_task_;
    task_tree_handle on_stopping_task_;

    /// The lifecycle callbacks. They live in the state, like every other field of a
    /// component: the state is the half that a callback is allowed to hold on to, and the
    /// half that outlives the component object itself.
    core::event_mt<void()> on_started_;
    core::event_mt<void(const stop_reason&)> on_stopped_;

    friend class component;
};

/// Stops the component whatever it is handed -- an exception, or the ticket of an operation
/// that may have failed.
struct component::stop_async_func {
    /// Constructs the functor that stops the component whose state is \p component_impl.
    explicit stop_async_func(const impl_ptr& component_impl);

    /// Requests the stop with \p error as the reason.
    void operator()(const std::exception_ptr& error) const noexcept;
    /// Requests the stop with the exception of \p result as the reason, or with an empty
    /// reason if \p result holds no exception.
    void operator()(const ticket& result) const noexcept;

    impl_ptr impl_;  ///< The state of the component to stop.
};

}  // namespace wxl::async

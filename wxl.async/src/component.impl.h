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
    explicit impl_t(core::sync_root_holder sync_root_arg);
    explicit impl_t(core::not_null<core::sync_root> sync_root_arg);

    /// \note Aborts if the component has already been disposed -- that is an architecture-level
    ///       invariant violation, not an application error.
    component* owner_unsafe();

    ticket stop_ticket();

    const std::exception_ptr* stop_reason_ptr() const noexcept;

    bool try_to_win_stopping_race(const std::exception_ptr& error);

    void stop_async_unsafe(const std::exception_ptr& error = std::exception_ptr());

    void stop_async_unsafe(const stop_reason& reason);


    bool is_disposed() const noexcept;

    void dispose();

protected:
    ~impl_t() override;

    const task_tree& starting_task() const;

    const task_tree& running_task() const;

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
    core::event<void()> on_started_;
    core::event<void(const stop_reason&)> on_stopped_;

    friend class component;
};

/// Stops the component whatever it is handed -- an exception, or the ticket of an operation
/// that may have failed.
struct component::stop_async_func {
    explicit stop_async_func(const impl_ptr& component_impl);

    void operator()(const std::exception_ptr& error) const noexcept;
    void operator()(const ticket& result) const noexcept;

    impl_ptr impl_;
};

}  // namespace wxl::async

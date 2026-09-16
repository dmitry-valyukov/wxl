export module wxl.async:component;

import :stop_reason;
import wxl.core;
import :future;
import :task_tree;
import std;

export namespace wxl::async {

/// Represents an asynchronous operation.
using ticket = future<void>;

/// Base of the component model: an object with an asynchronous start/stop lifecycle that
/// can be made part of the lifecycle of another such object.
///
/// A component starts once and stops once; there is no way back to the beginning. Both
/// halves are asynchronous -- start_async(), stop_async() -- and both report through
/// tickets. A ticket is a future<void> whose value means "this went the ordinary way" and
/// whose exception carries the reason it did not: a start that failed, or a stop caused by
/// an error rather than requested. A caller that has to wait waits on the ticket; there is
/// no synchronous form, because a component whose start or stop is finished by another
/// thread has no business pretending the wait is part of the operation.
///
/// **Derived types fill in the lifecycle, not the protocol.** on_starting(), on_started(),
/// on_stopping() and on_stopped() are the four places where a component does its own work,
/// and rollback_start() undoes a start that was interrupted halfway. When that work is
/// itself asynchronous, the component hangs subtasks on starting_task() or running_task()
/// and is not considered started -- or stopped -- until they finish.
///
/// **The parent is what makes components a tree.** Starting with a parent makes this
/// component's starting and running tasks subtasks of the parent's, so the parent is not
/// started until this one is, and does not count as stopped while this one still runs.
/// A container says the same thing about all of its children at once.
///
/// **A running component must not be destroyed.** The destructor of the most derived type
/// calls dispose(), which holds it until the component has stopped completely -- otherwise
/// a callback still in flight would reach a half-destroyed object.
class component : public core::traceable, public core::synchronized_
{
public:
    ~component() override;

    /// Starts the component on its own, in trees of its own.
    ///
    /// The start is asynchronous: what it returns says when it finished and how, and a
    /// caller that has to wait waits on the ticket it returns.
    ///
    /// A start that fails after it has begun also stops the component, and does not return
    /// until it has stopped completely. A call that is merely *refused* -- the component was
    /// already started, or has already been asked to stop -- changes nothing at all: it
    /// throws and leaves whatever was there running.
    ///
    /// \return the start ticket. The stop half of the lifecycle is deliberately not
    ///         returned here: a component has exactly one stop ticket, and stop_ticket()
    ///         hands it out at any moment -- before the start as readily as after it.
    ///
    /// \throw an exception if the asynchronous process has failed to start.
    /// \throw std::logic_error if the component was already started.
    /// \throw operation_canceled_exception if the component was already asked to stop.
    ticket start_async();

    bool was_started() const;

    /// Registers a callback -- `void callback()` -- called when the component has started
    /// and is ready to work, before the start ticket is signaled.
    ///
    /// \warning The callback must not throw. Callbacks are called through a noexcept
    ///          operator(), so an exception out of one ends the process. There is nothing
    ///          sensible to do with it here anyway: the component has started, the other
    ///          subscribers are still waiting to be told, and the ticket is about to be
    ///          signalled either way.
    ///
    /// \note The callback runs under the component's sync_root mutex, the same one
    ///       subscribing takes. It may call back into the component -- the mutex is
    ///       recursive -- but while it runs the component can do nothing else, so it has
    ///       no business being slow, and a second lock taken inside it is a lock order to
    ///       think about rather than to discover.
    ///
    /// There can be any number of subscribers; they are called in the order they subscribed
    /// and then dropped, because a component starts once and there is nothing left to
    /// announce afterwards.
    ///
    /// A template, and so is subscribe_on_stopped(): the callback is built out of what the
    /// caller wrote and handed over ready, instead of being copied into a std::function on
    /// the way -- which also means a move-only callable goes in as it is. The constraint is
    /// what that std::function parameter did for free, and it names the shape it wants:
    /// `void()` here, `void(const stop_reason &)` there.
    ///
    /// \return the cookie unsubscribe_on_started() cancels this subscription by.
    template <core::invocable<void() noexcept> F>
    core::cookie_t subscribe_on_started(F && callback) {
        return subscribe_on_started(on_started_callback::create(std::forward<F>(callback)));
    }

    /// Cancels a subscription made by subscribe_on_started().
    /// \return false if the component has no such subscription: the cookie belongs to
    ///         another event, or was cancelled already, or the callbacks have been called
    ///         and dropped.
    bool unsubscribe_on_started(core::cookie_t cookie);

    /// Requests the component to stop.
    ///
    /// Asynchronous as well: the ticket is ready once the component has stopped completely,
    /// including everything hanging on its running task, and a caller that has to wait waits
    /// on it -- with a deadline of its own, if it wants one.
    ///
    /// \return the same stop ticket that stop_ticket() returns.
    ///@{
    /// \param reason Can be empty (stop requested) or can hold the std::exception_ptr reason
    ///               that will be set as an exception result for the stop ticket.
    ticket stop_async(const stop_reason & reason = {});

    ticket stop_async(const std::exception_ptr & error);
    ///@}

    /// Kept in the class: it is a call to stop_reason_ptr() and a comparison, so the call
    /// would cost more than the work.
    bool stop_requested() const { return stop_reason_ptr() != nullptr; }

    /// \return the stop ticket that will be ready when the component has stopped.
    /// If its has_value() is true then the component was requested to stop in the usual
    /// way. If its has_exception() is true then the component stopped due to an error.
    ticket stop_ticket() const;

    /// \return the reason the component stopped, or nullptr if it hasn't stopped.
    const std::exception_ptr * stop_reason_ptr() const;

    /// Registers a callback -- `void callback(const stop_reason &)` -- called when the
    /// component has stopped, before the stop ticket is signaled. Any number of them, called
    /// once, as with subscribe_on_started().
    /// \warning The callback must not throw, for the reason given at
    ///          subscribe_on_started(): an exception out of a noexcept callback ends
    ///          the process.
    /// \note Called under the sync_root mutex, as with subscribe_on_started().
    /// \return the cookie unsubscribe_on_stopped() cancels this subscription by.
    template <core::invocable<void(const stop_reason &) noexcept> F>
    core::cookie_t subscribe_on_stopped(F && callback) {
        return subscribe_on_stopped(on_stopped_callback::create(std::forward<F>(callback)));
    }

    /// Cancels a subscription made by subscribe_on_stopped().
    /// \return false if the component has no such subscription, see unsubscribe_on_started().
    bool unsubscribe_on_stopped(core::cookie_t cookie);

protected:
    /// \param name Name of the component; can be used in diagnostics, can be empty.
    /// \param sync_root_arg External sync_root for the base synchronized_ class, or nullptr.
    explicit component(std::string_view name, core::nullable<core::sync_root> sync_root_arg = nullptr);

    class impl_t;
    /// A keeper: one more reference to the state, for whoever has to outlive the component
    /// object -- a callback, a container's list, a task still in flight.
    using impl_ptr = core::intrusive_ptr<impl_t>;

    /// The component's own hold on its state. Never null, and it only ever releases: the
    /// state is created with the component and adopted here, and nothing may put a second
    /// reference in through this field.
    using impl_ref = core::not_null<core::release_only_ptr<impl_t>>;

    component(std::string_view name, impl_t * initial_impl);

    /// Starts the component as a child of another one. This is the machinery of a
    /// composite component, not something its user does: whoever owns a component
    /// starts it with start_async(), and a component that is part of another's
    /// lifecycle is started by that other one, through here.
    ///@{
    /// param parent The component whose start and stop this one becomes part of: this
    ///               component's starting and running tasks are created as subtasks of the
    ///               parent's, so the parent is not started until this one is and does not
    ///               count as stopped while this one still runs.
    ///
    ///               Joining the parent's *starting* tree is possible only until the parent
    ///               reports its own start, which its own start_async() does before
    ///               returning -- in practice, from inside the parent's on_starting(). A
    ///               child started later gets a starting tree of its own and joins only the
    ///               running one, so its failure to start reaches nobody else.
    /// \throw std::logic_error if the parent has not started, or is stopping or stopped.
    ticket start_async(core::not_null<component> parent);

    ticket start_async(const task_tree & parent_starting_task, const task_tree & parent_running_task = {});

    /// The same two starts, but of some other component: what a parent does to a child
    /// it holds, and a container to the components in it.
    ///
    /// They exist because protected access does not reach through a `component*`: a
    /// derived class may use a protected member only on objects of its own type, and a
    /// child is known here as a plain component. A static member is not bound by that
    /// rule -- the same way impl_of() gets at another component's state.
    static ticket start_async_of(core::not_null<component> child,
                                 core::not_null<component> parent);

    static ticket start_async_of(core::not_null<component> child,
                                 const task_tree & parent_starting_task,
                                 const task_tree & parent_running_task);
    ///@}

    bool was_started_unsafe() const;

    bool is_running() const;

    /// Called during the start of the component. Derived classes can add extra
    /// start-related logic here.
    /// \note Overriding methods should call the base on_starting() first.
    /// \note Called under the protection of the sync_root mutex.
    virtual void on_starting();

    /// Called inside stop_async() if the component is in the 'starting' phase, so
    /// derived classes can roll back the start procedure.
    /// \note Called under the protection of the sync_root mutex.
    virtual void rollback_start();

    /// Called when the component has started and is ready to work, before the start
    /// ticket is signaled. If this throws, the component fails to start.
    /// \note Called under the protection of the sync_root mutex.
    /// \note Overriding methods should call the base on_started().
    virtual void on_started();

    /// Extra stop-related logic for derived classes.
    /// \note The stop reason is available via stop_reason_ptr().
    /// \note Called under the protection of the sync_root mutex.
    virtual void on_stopping();

    /// Called when the component has stopped, before the stop ticket is signaled.
    /// \note The stop reason is available via stop_reason_ptr().
    /// \note Called under the protection of the sync_root mutex.
    /// \note Do not throw from this method -- it will be ignored.
    /// \note Overriding methods should call the base on_stopped().
    virtual void on_stopped();

    /// Should be called from the destructor of the top-most derived type, to protect
    /// the component from destruction until it has stopped completely.
    void dispose();

    bool is_disposed() const noexcept;

    /// Valid during the starting process; derived components can create their own
    /// starting subtasks for a compounded starting operation.
    const task_tree & starting_task() const;

    /// Valid during the whole running process; derived components can create their own
    /// running subtasks for a compounded running operation.
    const task_tree & running_task() const;

    /// Set to ready after on_starting() has been called.
    ticket on_starting_ticket();

    /// Set to ready after on_stopping() has been called.
    ticket on_stopping_ticket() const;

    /// Valid during the call to on_stopping().
    const task_tree & on_stopping_task() const;

    /// Functor that stops the component when the operation it is subscribed to completes.
    struct stop_async_func;

    ///@{
    /// \return the internal shared state of the component. Callbacks can safely hold
    /// this by reference-counted smart pointer.
    core::not_null<impl_t> impl() { return impl_.get(); }

    core::not_null<const impl_t> impl() const { return impl_.get(); }

    static core::not_null<impl_t> impl_of(core::not_null<component> c) { return c->impl(); }
    ///@}

private:
    /// The callback objects the two subscribe_ templates build out of what they are given.
    ///@{
    using on_started_callback = core::event<void()>::func_t;
    using on_stopped_callback = core::event<void(const stop_reason &)>::func_t;
    ///@}

    /// The same subscription, of a callback that is already built -- where the two templates
    /// above end up. This is the only place a callback crosses into the state, and it happens
    /// under the lock. Private, so that the way in stays the templates, which build the
    /// callback themselves and cannot be handed a pointer to something else.
    ///@{
    core::cookie_t subscribe_on_started(core::not_null<on_started_callback> callback);

    core::cookie_t subscribe_on_stopped(core::not_null<on_stopped_callback> callback);
    ///@}

    /// The shared state of the component, and its only field: everything a component keeps
    /// lives there, where it outlives the component object and where a callback can hold on
    /// to it.
    impl_ref const impl_;
};

}  // export namespace wxl::async

export module wxl.async:task_tree;

import :future;
import :future_shared_state;
import wxl.core;
import std;

export namespace wxl::async {

namespace task_tree_detail {

/// Shared state of a task_tree; defined in task_tree.cpp, the only place that needs it.
class state;

}  // namespace task_tree_detail

class task_tree;
class task_tree_root;
class task_tree_handle;

/// A node of a tree of tasks: its own piece of work plus every subtask created under it, and
/// each of those subtasks is a node of exactly this shape again. A node is finished only once
/// its own work and its whole subtree are, and it fails as a whole as soon as any one task in
/// that subtree does.
///
/// The recursion is why part and whole share a type: a subtree is a tree. What a caller holds
/// is therefore always "my work and everything that grew out of it", whether that is the top
/// of the hierarchy or a single leaf with nothing under it.
///
/// This is the reading end; the work itself is reported through a task_tree_handle.
///
/// Nothing here is defined in place, down to the copy constructor: the state is only complete
/// inside task_tree.cpp, and every one of these touches it.
class task_tree : public future_detail::shared_state_holder<task_tree_detail::state>
{
    using base = future_detail::shared_state_holder<task_tree_detail::state>;

public:
    task_tree() noexcept;

    task_tree(const task_tree& other);

    task_tree(task_tree&& other) noexcept;

    task_tree& operator=(const task_tree& other);

    task_tree& operator=(task_tree&& other) noexcept;

    ~task_tree();

    /// Returns a future for this node and its whole subtree.
    future<void> get_future() const;

    /// Returns `true` if the task and *all* subtasks (if any) have been finished successfully,
    /// otherwise - `false`.
    ///
    /// \throw std::logic_error if this instance does not refer to a shared state.
    bool succeeded() const;

    /// Returns `true` if this task and *all* subtask (if any) have finished and either this task or
    /// one of subtasks (if any) has failed, otherwise - `false`.
    ///
    /// \note the failed status is *not* known until this task is finished. \see
    /// task_tree_root::cancellation_token().
    ///
    /// \throw std::logic_error if this instance does not refer to a shared state.
    bool failed() const;

    /// Returns `true` if this task and *all* subtasks (if any) have finished, otherwise - `false`.
    ///
    /// \throw std::logic_error if this instance does not refer to a shared state.
    bool finished() const;

    /// Returns `true` while create_subtask() would still be accepted.
    ///
    /// A node stops taking new subtasks the moment its *own* work is reported through the
    /// handle -- set_succeeded() or set_failed() -- which is earlier than the node finishing:
    /// subtasks already under it may well be running still. So this is not the negation of
    /// finished(), and it is the question to ask before hanging work on a node someone else
    /// owns.
    ///
    /// \throw std::logic_error if this instance does not refer to a shared state.
    bool accepts_subtasks() const;

    /// Returns the exception that was the reason of the task cancellation/failure.
    ///
    /// \throw std::logic_error if this instance does not refer to a shared state.
    /// \throw std::logic_error if the task has *not* finished with an error.
    const std::exception_ptr& get_exception_ptr() const;

    /// Returns the root (top-most) task representation.
    task_tree_root root() const;

    /// Creates a subtask.
    task_tree_handle create_subtask() const;

    /// Creates a subtask that finishes the way the given future does.
    ///
    /// No handle comes back from this one: the future is what finishes the task, so there would
    /// be nothing to report through the handle. The node holds one internally until the future
    /// resolves, which is also what keeps it from being reported as abandoned.
    task_tree create_subtask(const future<void>& f) const;

    /// Creates a root task or a subtask.
    static task_tree_handle create(const task_tree& parent = task_tree());

    /// Creates a root task that finishes the way the given future does. \see create_subtask.
    static task_tree create(const future<void>& f);

    /// Two instances are equal when they refer to the same shared state; operator!= is
    /// synthesized from this one. Comparing the pointers needs no state definition, so this one
    /// can stay here.
    friend bool operator==(const task_tree& l, const task_tree& r) noexcept {
        return l.state() == r.state();
    }

    /// Diagnostics only, and the reason no std::formatter comes with it: a specialization of one
    /// would be attached to this module, which is not allowed.
    friend std::ostream& operator<<(std::ostream& s, const task_tree& value) {
        return s << "task_tree(" << value.state() << ")";
    }

protected:
    explicit task_tree(task_tree_detail::state* state);
};

/// The node at the top of a tree -- the one that has no parent above it.
class task_tree_root : public task_tree
{
    using base = task_tree;

public:
    /// Returns a future for the successful completion of this task and *all* subtasks (if any),
    /// or for the cancellation of *any* task in the group.
    ///
    /// This is a fast way to get or set the cancel/failure signal from any subtask within the full
    /// hierarchy.
    future<void> cancellation_token() const;

private:
    explicit task_tree_root(task_tree_detail::state* state);

    friend class task_tree;
};

/// The writing end of a task_tree node: what a piece of work holds while it runs, reports
/// through, and grows new branches from.
///
/// Destroying the last handle of a node that has not finished fails it with
/// std::future_error(std::future_errc::broken_promise), so abandoned work is never left looking
/// like work still in progress.
class task_tree_handle : public task_tree
{
    using base = task_tree;

public:
    /// Creates an empty handle (no shared state).
    task_tree_handle() noexcept;

    task_tree_handle(const task_tree_handle& other);

    /// Takes the state over without touching the handle count: the moved-from handle is left
    /// empty, so the number of handles the node knows about does not change.
    task_tree_handle(task_tree_handle&& other) noexcept;

    ~task_tree_handle();

    task_tree_handle& operator=(const task_tree_handle& other);

    task_tree_handle& operator=(task_tree_handle&& other);

    /// Marks the task as succeeded.
    ///
    /// \throws std::logic_error if the task is already finished.
    void set_succeeded();

    /// Marks the task as failed.
    ///
    /// \throws std::logic_error if the task is already finished.
    void set_failed(const std::exception_ptr& error);

    /// Marks the task as succeeded (if \c error is empty) or as failed (if \c error is \e not
    /// empty).
    void set_succeeded_or_failed(const std::exception_ptr& error) {
        if (error)
            set_failed(error);
        else
            set_succeeded();
    }

private:
    explicit task_tree_handle(task_tree_detail::state* state);

    friend class task_tree;
};

}  // export namespace wxl::async

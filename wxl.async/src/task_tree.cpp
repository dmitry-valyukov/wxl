module;
#include "pch.h"

module wxl.async;
import wxl.core;
import std;

namespace wxl::async {
namespace task_tree_detail {

/// Shared state of a task_tree.
class state : public future_shared_state
{
    using base = future_shared_state;

public:
    /// Constructor.
    explicit state(state* parent);

    ~state() override;

    /// Returns a future for the successful completion of this task and *all* subtasks (if any),
    /// or for the failure of *any* task in the group.
    future<void> cancellation_token() const { return root_cancellation_promise_.get_future(); }

    /// Marks the task state as finished successfully.
    ///
    /// \throw std::logic_error if this task is already finished.
    void set_succeeded();

    /// Marks the task state as failed.
    ///
    /// \throw std::logic_error if this task is already finished.
    void set_failed(const std::exception_ptr& result);

    /// Increments the counter of unresolved subtasks.
    ///
    /// \throw std::logic_error if this task has already reported its own work.
    void increment_subtask_counter();

    /// Whether increment_subtask_counter() would still be accepted -- i.e. whether this
    /// task has yet to report its own work.
    bool accepts_subtasks() const noexcept;

    /// Returns the root state.
    ///
    /// If this is a root task then returns itself.
    state* root();

    /// Increments the number of task_tree_handle instances that refer to this shared state.
    void increment_task_handle_count() { base::increment_promise_count(); }

    /// Decrements the number of task_tree_handle instances that refer to this shared state.
    void decrement_task_handle_count();

private:
    /// Is called when this task is finished.
    void on_finished();

    ///@{
    /// Decrements the counter of unresolved subtasks.
    void decrement_subtask_counter();

    void decrement_subtask_counter(const std::exception_ptr& result);
    ///@}

    using base::checked_decrement_promise_count;
    using base::increment_promise_count;

    /// The counter of linked shared unresolved tasks (this task and subtasks) as well as the
    /// 'finished' flag. Both values should be changed atomically, so they are stored together.
    std::atomic<size_t> counter_;

    core::intrusive_ptr<state> parent_;  ///< The parent shared state.

    /// Holds a shared state only when this task is a root.
    ///
    /// Resolved once this root task and *all* its subtasks (if any) have succeeded, or failed if
    /// *any* of the tasks within the full hierarchy has failed.
    promise<void> root_cancellation_promise_;

    core::atomic_trigger error_in_group_;
    std::exception_ptr group_result_;
};

}  // namespace task_tree_detail


using namespace task_tree_detail;

task_tree::task_tree() noexcept : base(nullptr) {}

task_tree::task_tree(const task_tree& other) = default;

task_tree::task_tree(task_tree&& other) noexcept = default;

task_tree& task_tree::operator=(const task_tree& other) = default;

task_tree& task_tree::operator=(task_tree&& other) noexcept = default;

// let's destroy intrusive_ptr in single place
task_tree::~task_tree() = default;

task_tree::task_tree(task_tree_detail::state* state) : base(state) {}

future<void> task_tree::get_future() const { return future<void>(checked_state()); }

bool task_tree::succeeded() const { return checked_state()->has_value(); }

bool task_tree::failed() const { return checked_state()->has_exception(); }

bool task_tree::finished() const { return checked_state()->ready(); }

bool task_tree::accepts_subtasks() const { return checked_state()->accepts_subtasks(); }

const std::exception_ptr& task_tree::get_exception_ptr() const {
    return checked_state()->get_exception_ptr();
}

task_tree_root task_tree::root() const { return task_tree_root(checked_state()->root()); }

task_tree_root::task_tree_root(task_tree_detail::state* st) : base(st) {
    assert(st);
    assert(st->root() == st);
}

future<void> task_tree_root::cancellation_token() const {
    return checked_state()->cancellation_token();
}

namespace {

/// Ties a handle to a future: whatever the future ends with, the node ends with too. The callback
/// keeps the handle alive, so the node counts as running for exactly as long as the future does.
task_tree follow(task_tree_handle handle, const future<void>& f) {
    f.when_ready([handle](const future<void>& ready) mutable noexcept {
        handle.set_succeeded_or_failed(ready.has_exception() ? ready.get_exception_ptr()
                                                             : std::exception_ptr());
    });

    return handle;
}

}  // namespace

task_tree_handle task_tree::create_subtask() const {
    assert(valid());

    task_tree_detail::state* st = checked_state();
    st->increment_subtask_counter();

    return task_tree_handle(new task_tree_detail::state(st));
}

task_tree task_tree::create_subtask(const future<void>& f) const { return follow(create_subtask(), f); }

task_tree_handle task_tree::create(const task_tree& parent) {
    task_tree_detail::state* parent_state = parent.state();

    if (parent_state) parent_state->increment_subtask_counter();

    return task_tree_handle(new task_tree_detail::state(parent_state));
}

task_tree task_tree::create(const future<void>& f) { return follow(create(), f); }

task_tree_handle::task_tree_handle() noexcept : base(nullptr) {}

task_tree_handle::task_tree_handle(task_tree_detail::state* st) : base(st) {
    if (st) st->increment_task_handle_count();
}

task_tree_handle::task_tree_handle(const task_tree_handle& other) : base(other.state()) {
    if (task_tree_detail::state* st = other.state()) st->increment_task_handle_count();
}

task_tree_handle::task_tree_handle(task_tree_handle&& other) noexcept : base(std::move(other)) {}

task_tree_handle::~task_tree_handle() {
    if (task_tree_detail::state* st = task_tree::state()) st->decrement_task_handle_count();
}

void task_tree_handle::set_succeeded() { checked_state()->set_succeeded(); }

void task_tree_handle::set_failed(const std::exception_ptr& error) {
    checked_state()->set_failed(error);
}

task_tree_handle& task_tree_handle::operator=(const task_tree_handle& other) {
    task_tree_detail::state* const my_state = state();
    task_tree_detail::state* const other_state = other.state();

    if (my_state != other_state) {
        if (other_state) other_state->increment_task_handle_count();

        if (my_state) my_state->decrement_task_handle_count();

        base::operator=(other);
    }

    return *this;
}

task_tree_handle& task_tree_handle::operator=(task_tree_handle&& other) {
    if (this != &other) {
        if (task_tree_detail::state* my_state = state()) my_state->decrement_task_handle_count();

        base::operator=(std::move(other));
    }

    return *this;
}

state::state(state* parent)
    : counter_(1),
      parent_(parent),
      // Only a root carries the cancellation channel of the whole hierarchy.
      root_cancellation_promise_(parent ? make_empty_promise<void>() : promise<void>()) {}

state::~state() = default;

namespace {

// The unresolved-task counter and the 'finished' flag share one word so that both change in a
// single atomic step; the flag takes the top bit, which no plausible number of subtasks reaches.
constexpr size_t is_finished_bit = size_t(1) << (std::numeric_limits<size_t>::digits - 1);
constexpr size_t counter_mask = ~is_finished_bit;

constexpr bool is_finished(size_t counter) noexcept { return (counter & is_finished_bit) != 0; }

constexpr size_t counter_value(size_t counter) noexcept { return counter & counter_mask; }

/// Retries `next` against the counter until its compare-exchange takes, and returns the value
/// stored -- or nothing at all, if `next` declined the value it was shown. `next` is called
/// again on every contended attempt, so it has to be a pure function of the value.
///
/// "Nothing" is the all-ones word, which the encoding above already rules out: it would mean
/// finished with `counter_mask` tasks still outstanding, and the flag was put in the top bit
/// precisely because no plausible number of subtasks reaches there.
template <class t_next>
core::nullable<size_t> update(std::atomic<size_t>& counter, t_next next) {
    size_t value = counter.load(std::memory_order_relaxed);

    while (true) {
        const core::nullable<size_t> new_value = next(value);

        if (!new_value) return {};

        if (counter.compare_exchange_weak(value, *new_value, std::memory_order_acq_rel,
                                          std::memory_order_relaxed))
            return new_value;
    }
}

/// Counts one more unresolved task in.
/// \return `false` if the task is already finished.
bool try_increment_counter(std::atomic<size_t>& counter) {
    return update(counter, [](size_t value) -> core::nullable<size_t> {
               if (is_finished(value)) return {};

               assert(counter_value(value) >= 1);

               return counter_value(value) + 1;
           })
        .has_value();
}

/// \return the number of linked shared unresolved tasks left.
size_t decrement_counter(std::atomic<size_t>& counter) {
    return counter_value(*update(counter, [](size_t value) -> core::nullable<size_t> {
        assert(counter_value(value) >= 1);

        return (counter_value(value) - 1) | (value & is_finished_bit);
    }));
}

/// Decrements the counter and raises the 'finished' flag.
/// \return the number of unresolved tasks left, or nothing if the task was already finished.
core::nullable<size_t> finish_counter(std::atomic<size_t>& counter) {
    const core::nullable<size_t> stored = update(counter, [](size_t value) -> core::nullable<size_t> {
        if (is_finished(value)) return {};

        assert(counter_value(value) >= 1);

        return (counter_value(value) - 1) | is_finished_bit;
    });

    return stored ? core::nullable<size_t>(counter_value(*stored)) : core::nullable<size_t>{};
}

}  // namespace

void state::increment_subtask_counter() {
    if (!try_increment_counter(counter_))
        throw std::logic_error(
            "A subtask cannot be added to a task that has already reported its own work.");
}

bool state::accepts_subtasks() const noexcept {
    return !is_finished(counter_.load(std::memory_order_relaxed));
}

void state::decrement_subtask_counter() {
    assert(counter_value(counter_.load(std::memory_order_relaxed)) > 0);

    if (0 == decrement_counter(counter_)) on_finished();
}

void state::decrement_subtask_counter(const std::exception_ptr& result) {
    assert(counter_value(counter_.load(std::memory_order_relaxed)) > 0);

    if (error_in_group_.set()) group_result_ = result;

    if (0 == decrement_counter(counter_)) on_finished();
}

// group_result_ is written before the counter is decremented, and this runs only for whoever
// drove the counter to zero through that same acquire-release exchange -- which is what makes
// the read here safe without a lock of its own.
void state::on_finished() {
    if (error_in_group_.value()) {
        std::exception_ptr error = group_result_;
        assert(error);

        set_exception(error);

        if (state* parent = parent_.get()) parent->decrement_subtask_counter(error);
    } else {
        set_value();

        if (state* parent = parent_.get()) {
            parent->decrement_subtask_counter();
        } else {  // this is a root task
            assert(root_cancellation_promise_.valid());
            assert(!root_cancellation_promise_.get_future().is_ready());

            root_cancellation_promise_.set_value();
        }
    }
}

void state::set_succeeded() {
    const core::nullable<size_t> unresolved = finish_counter(counter_);

    if (!unresolved)
        throw std::logic_error(
            "task_tree_handle::set_succeeded() was called for the already finished task");

    if (*unresolved == 0) {
        core::intrusive_ptr<state> keep_me(this);
        on_finished();
    }
}

void state::set_failed(const std::exception_ptr& result) {
    assert(result);

    if (is_finished(counter_.load(std::memory_order_relaxed)))
        throw std::logic_error(
            "task_tree_handle::set_failed() was called for the already finished task");

    core::intrusive_ptr<state> keep_me(this);

    if (error_in_group_.set()) {
        group_result_ = result;
        root()->root_cancellation_promise_.set_exception(result);
    }

    const core::nullable<size_t> unresolved = finish_counter(counter_);

    if (!unresolved)
        throw std::logic_error(
            "task_tree_handle::set_failed() was called for the already finished task");

    if (*unresolved == 0) on_finished();
}

state* state::root() {
    state* root_state = this;

    while (state* parent = root_state->parent_.get()) root_state = parent;

    return root_state;
}

void state::decrement_task_handle_count() {
    if (0 == decrement_promise_count() &&
        !is_finished(counter_.load(std::memory_order_relaxed)))
        set_failed(std::make_exception_ptr(std::future_error(std::future_errc::broken_promise)));
}

}  // namespace wxl::async

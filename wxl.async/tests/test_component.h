#pragma once



#include "stress_test.h"

import std;
import wxl.core;
import wxl.async;

namespace wxl::async {

// The test components live in wxl::async and reach for wxl.core names (traceable,
// sync_root) as freely as the library itself does.
using namespace wxl::core;

/// Lifecycle methods tracked by test_component.
struct component_method {
    enum enum_ {
        not_set,

        on_starting,
        rollback_start,
        on_started,
        run,
        on_moved_in,
        on_moved_out,
        on_stopping,
        on_stopped,

        method_count,
    };
};

inline std::ostream & operator<<(std::ostream & s, component_method::enum_ value) {
    switch (value) {
    case component_method::on_starting: return s << "on_starting";
    case component_method::rollback_start: return s << "rollback_start";
    case component_method::on_started: return s << "on_started";
    case component_method::run: return s << "run";
    case component_method::on_moved_in: return s << "on_moved_in";
    case component_method::on_moved_out: return s << "on_moved_out";
    case component_method::on_stopping: return s << "on_stopping";
    case component_method::on_stopped: return s << "on_stopped";
    default: return s << "(#error)";
    }
}

/// Order in which the fault is injected relative to the base implementation's own work.
struct method_throw_order {
    enum enum_ { before, after };
};

inline std::ostream & operator<<(std::ostream & s, method_throw_order::enum_ value) {
    return s << (value == method_throw_order::before ? "before" : "after");
}

struct expected {
    enum enum_ { ok, fail };
};

inline std::ostream & operator<<(std::ostream & s, expected::enum_ value) {
    return s << (value == expected::ok ? "OK" : "fail");
}

/// Thread-safe record of which lifecycle methods were called.
class method_calls
{
public:
    bool was_called(component_method::enum_ method) const {
        return flags_[method].load(std::memory_order_relaxed);
    }

    void notify_called(component_method::enum_ method) {
        flags_[method].store(true, std::memory_order_relaxed);
    }

private:
    std::array<std::atomic<bool>, component_method::method_count> flags_{};
};

/// Test mixin tracking lifecycle-method calls and supporting fault injection
/// ("throw before/after method X") plus self-delete/self-stop behaviors.
class test_component
{
public:
    std::atomic<size_t> stop_request_count_{0};
    std::atomic<bool> self_delete_{false};
    std::atomic<bool> self_stop_{false};

    std::atomic<component_method::enum_> throw_before_method_{component_method::not_set};
    std::atomic<component_method::enum_> throw_after_method_{component_method::not_set};
    promise<void> is_deleted_promise_ = make_empty_promise<void>();

    test_component() noexcept = default;

    virtual ~test_component() {
        if (is_deleted_promise_.valid())
            is_deleted_promise_.set_value();
    }

    bool on_started_called() const {
        return was_called(component_method::on_started);
    }

    bool on_stopped_called() const {
        return was_called(component_method::on_stopped);
    }

    bool was_called(component_method::enum_ method) const {
        return method_calls_.was_called(method);
    }

protected:
    void on_before(component_method::enum_ method) {
        method_calls_.notify_called(method);

        if (throw_before_method_.load(std::memory_order_relaxed) == method)
            throw specific_exception();
    }

    void on_after(component_method::enum_ method) const {
        if (throw_after_method_.load(std::memory_order_relaxed) == method)
            throw specific_exception();
    }

private:
    method_calls method_calls_;
};

/// Mixes test_component's tracking/fault-injection into a real component
/// (or derivative) type TBase.
template<class TBase>
class test_component_impl : public TBase, public test_component
{
    typedef TBase base;

public:
    using base::stop_ticket;

    // The start under someone else's task trees is protected in component, because it is
    // the machinery of a composite. The barrier suites drive it directly, so a test
    // component puts it back out in the open.
    using base::start_async;

protected:
    explicit test_component_impl(std::string_view name)
        : base(name) {}

    test_component_impl(std::string_view name, not_null<wxl::core::sync_root> sync_root_arg)
        : base(name, sync_root_arg.get()) {}

    test_component_impl(std::string_view name, thread_group * manager)
        : base(name, manager) {}

    test_component_impl(std::string_view name, thread_group * manager, not_null<wxl::core::sync_root> sync_root_arg)
        : base(name, manager, sync_root_arg.get()) {}

    /// For apartment_pool, whose constructor takes the desired thread count in the middle.
    /// Nothing in this binary uses it any more: apartment_pool and its suites wait in
    /// .claude/wxl.components, whose tests include this header from there.
    test_component_impl(std::string_view name, size_t thread_count, thread_group * manager)
        : base(name, thread_count, manager) {}

    void on_starting() override {
        assert(this->is_synchronized());

        on_before(component_method::on_starting);
        base::on_starting();
        on_after(component_method::on_starting);
    }

    void rollback_start() override {
        assert(this->is_synchronized());

        on_before(component_method::rollback_start);
        base::rollback_start();
        on_after(component_method::rollback_start);
    }

    void on_started() override {
        assert(this->is_synchronized());

        on_before(component_method::on_started);
        base::on_started();
        on_after(component_method::on_started);
    }

    void on_stopping() override {
        assert(this->is_synchronized());

        stop_request_count_.fetch_add(1, std::memory_order_relaxed);

        on_before(component_method::on_stopping);
        base::on_stopping();
        on_after(component_method::on_stopping);
    }

    void on_stopped() override {
        assert(this->is_synchronized());

        on_before(component_method::on_stopped);
        base::on_stopped();
        on_after(component_method::on_stopped);

        if (self_delete_) {
            test_component * self = this;
            this->stop_ticket().when_ready([self]() noexcept { delete self; });
        }
    }
};

}  // namespace wxl::async

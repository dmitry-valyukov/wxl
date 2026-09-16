#pragma once

import std;
import wxl.core;
import wxl.async;


namespace wxl::async {

// The test components live in wxl::async and reach for wxl.core names (traceable,
// sync_root) as freely as the library itself does.
using namespace wxl::core;

/// Synchronously started component.
///
/// Needed to test a situation when start_async() will completely start the component
/// before it returns the corresponding ticket.
class sync_component : public component
{
    typedef component base;

public:
    explicit sync_component(std::string_view name = "Synchronously started component") noexcept
        : base(name)
    {}

protected:
    void on_starting() override {
        base::on_starting();

        if (stop_requested())
            throw operation_canceled_exception("Start procedure was cancelled");

        on_started();
        my_running_subtask_ = running_task().create_subtask();
    }

    void on_stopping() override {
        base::on_stopping();

        ticket stopping_ticket = on_stopping_task().get_future();
        stopping_ticket.when_ready([this, stopping_ticket]() noexcept { set_subtask_ready(stopping_ticket); });
    }

private:
    void set_subtask_ready(const ticket & stopping_ticket) {
        if (stopping_ticket.has_exception())
            my_running_subtask_.set_failed(stopping_ticket.get_exception_ptr());
        else
            my_running_subtask_.set_succeeded();
    }

    task_tree_handle my_running_subtask_;
};

}  // namespace wxl::async

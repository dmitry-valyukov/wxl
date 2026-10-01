#pragma once

// wxl::idle -- `co_await idle()` hands the UI thread back and resumes once it
// has nothing more urgent: input, layout and rendering run first. Long work on
// the UI thread is cut into steps this way, without a second thread:
//
//     async::detached_task index(Model& model) {
//         while (model.step()) co_await idle();
//     }
//
// Going down ends the wait the way it ends an event wait: the coroutine
// resumes with async::operation_canceled_exception.

#include <coroutine>
#include <memory>

#include "impl/event_waits.h"

namespace wxl {

class Idle : impl::waiting_event {
public:
    Idle() = default;
    ~Idle();

    Idle(Idle const&) = delete;
    Idle& operator=(Idle const&) = delete;

    bool await_ready() const noexcept { return impl::events_closing(); }
    bool await_suspend(std::coroutine_handle<> waiter);

    void await_resume() const {
        if (refused_ || ended() || impl::events_closing()) {
            throw_ended();
        }
    }

private:
    // The queued resumption finds the wait through this; a wait ended on the
    // way down is gone by the time the queue gets to it, and leaves it empty.
    std::shared_ptr<Idle*> self_;
    bool refused_ = false;  // the queue takes nothing more
};

[[nodiscard]] inline Idle idle() {
    return {};
}

}  // namespace wxl

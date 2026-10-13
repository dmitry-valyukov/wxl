#pragma once

#include <winrt/Microsoft.UI.Dispatching.h>
#include <winrt/Windows.Foundation.h>

#include "../Operation.h"

// The producer behind the task a WinRT asynchronous operation becomes: an IAsyncOperation<X> or
// an IAsyncAction, with progress or without, goes in, and async::task<R> comes out, R being X in
// wxl's terms. For the generated wrappers, and for any call of wxl.ui that awaits an operation of
// the platform.
//
// It is an async_op, so a task reads it the way it reads every other operation: is it ready, park
// the waiter, take the value or the exception. What is its own:
//
//  * Two threads, and only one of them touches it. The call has started the operation already.
//    Its completion arrives on a thread of the runtime's choice, and all the handler does there is
//    post to the dispatcher queue of the thread that made the call. The producer comes from that
//    thread's pool and its fields are guarded by nothing, so it is touched there alone: the
//    posted call reads the result, converts it into wxl's types, keeps it, and resumes the
//    coroutine waiting for it (come_back()) -- or, with nobody waiting yet, leaves it ready for
//    the co_await still to come, which then does not suspend.
//  * The answer is what happened to the operation. Completed: its result, through `convert`.
//    Canceled: async::operation_canceled_exception, as every cancellation in wxl answers. Error:
//    what GetResults() throws for it -- the projection's exception, which is what every other call
//    of a wrapper fails with as well (failureOf reads it).
//  * It is orphanable: it holds nothing of the coroutine's, so a task let go while the operation
//    is out gives it up without waiting (async_op::abandon), and on_cancel() asks the runtime to
//    stop it -- nothing more. The completion still comes, cancelled or not, and the posted call
//    finds the producer given up: it reads nothing, resumes nobody, and deletes it. Nothing is
//    written into a frame that is gone.
//  * A queue that refuses the posted call -- shut down, its thread on the way out -- leaves the
//    producer as it is: the coroutine waiting for it is not resumed, and one given up is not
//    deleted.
//
// The operation and the queue are template parameters, so that all of this can be tested over a
// fake of each; start_operation and start_action give it the projection's operation and the
// queue of the calling thread.
//
// Private: this header names winrt types, so only wxl's own sources include it.

namespace wxl::impl {

namespace detail {

using winrt::Windows::Foundation::AsyncStatus;

/// One operation of the platform under way, and its answer once it is back. Made, answered and
/// deleted on the thread that made the call.
template <class R, class Op, class Convert>
class producer final : public async::async_op_t<R>
{
public:
    producer(Op operation, Convert convert)
        : async::async_op_t<R>(async::orphanable),
          operation_(std::move(operation)),
          convert_(std::move(convert)) {}

    /// The posted call. Given up, it reads nothing; come_back() then deletes it, and otherwise
    /// marks it ready and resumes the coroutine waiting for it. It may be gone on return.
    void finish(AsyncStatus status) {
        if (!this->abandoned()) {
            if (status == AsyncStatus::Canceled)
                this->answer_canceled();
            else
                read();
        }

        this->come_back();
    }

protected:
    /// Never called: the platform carries the operation out, and it is not sent to a worker.
    bool execute() override { return true; }

    /// Given up while it is out: the runtime is asked to stop it, and its completion still comes.
    /// Called from the task's destructor, so a failure to ask goes no further.
    void on_cancel() noexcept override {
        try {
            operation_.Cancel();
        } catch (...) {
        }
    }

private:
    /// Completed, the result; failed, GetResults() throws what the operation failed with, and that
    /// is the answer -- as is whatever `convert` throws.
    void read() noexcept {
        try {
            if constexpr (std::is_void_v<R>)
                operation_.GetResults();
            else
                this->set_value(convert_(operation_.GetResults()));
        } catch (...) {
            this->set_error(std::current_exception());
        }
    }

    Op operation_;
    Convert convert_;
};

/// The task of an operation the call has started already, answered on the thread of `queue`, the
/// calling thread. The completion handler touches nothing but `queue`, and passes the producer on
/// as a plain pointer: nothing deletes the producer before the posted call has run.
template <class R, class Op, class Queue, class Convert>
async::task<R> start(Op const& operation, Queue const& queue, Convert convert) {
    auto made = std::make_unique<producer<R, Op, Convert>>(operation, std::move(convert));
    producer<R, Op, Convert>* const waited = made.get();

    operation.Completed([waited, queue](auto const&, AsyncStatus status) {
        queue.TryEnqueue([waited, status] { waited->finish(status); });
    });

    return async::task<R>(std::move(made));
}

}  // namespace detail

/// An asynchronous operation with a result: `convert` turns what it returns (a projection value)
/// into `R`, on the calling thread.
template <class R, class Op, class Convert>
async::task<R> start_operation(Op const& operation, Convert convert) {
    return detail::start<R>(
        operation, winrt::Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread(),
        std::move(convert));
}

/// An asynchronous operation without a result (IAsyncAction, with progress or without).
template <class Op>
async::task<> start_action(Op const& operation) {
    return detail::start<void>(
        operation, winrt::Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread(),
        std::identity{});
}

}  // namespace wxl::impl

#pragma once

#include <winrt/Microsoft.UI.Dispatching.h>
#include <winrt/Windows.Foundation.h>

#include "coroutine_checks.h"
#include "dispatcher.h"

#include "../Operation.h"

// The producer behind the task a WinRT asynchronous operation becomes: an IAsyncOperation<X> or
// an IAsyncAction, with progress or without, goes in, and async::task<R> comes out, R being X in
// wxl's terms. For the generated wrappers, and for any call of wxl.ui that awaits an operation of
// the platform.
//
// It is an async_op, so a task reads it the way it reads every other operation: is it ready, suspend
// the waiter, take the value or the exception. What is its own:
//
//  * Two threads, and only one of them touches it. The call has started the operation already.
//    Its completion arrives on a thread of the runtime's choice, and all the handler does there is
//    post to the dispatcher queue of the thread that made the call. The producer comes from that
//    thread's pool and its fields are guarded by nothing, so it is touched there alone: the
//    posted call reads the result, converts it into wxl's types, keeps it, and resumes the
//    coroutine waiting for it (complete()) -- or, with nobody waiting yet, leaves it ready for
//    the co_await still to come, which then does not suspend.
//  * The answer is what happened to the operation. Completed: its result, through `convert`.
//    Canceled: async::operation_canceled_exception, as every cancellation in wxl answers. Error:
//    what GetResults() throws for it -- the projection's exception, which is what every other call
//    of a wrapper fails with as well (failureOf reads it).
//  * It does not wait for the platform to go. The posted calls reach it through a link, a cell
//    the producer clears as it goes, and not through a pointer to it; so the task letting go asks
//    the runtime to stop an operation still out (on_cancel()) and deletes the producer at once,
//    and a completion that comes later finds the link empty and does nothing. Whether the
//    completion ever comes is the platform's business: a ContentDialog asked to stop completes
//    only after its closing transition, and a window on its way out shuts its queue down first.
//    What then stays behind is the platform's own -- its operation, holding the completion
//    handler, and the handler the link and the queue -- and nothing of the producer's: not the
//    operation, the token or the queue. The link is the one thing both threads hold, so its count
//    is refcounted_mt's atomic one, and it comes from the general heap, not the thread's pool: the
//    runtime may let go of the handler on any thread.
//  * A queue that refuses the posted call -- shut down, its thread on the way out -- leaves the
//    coroutine waiting for it as it is: it is not resumed, and the wait is its owner's to end.
//  * Under a token (start_cancelable_winrt_operation, start_cancelable_winrt_action, the form with
//    a token of every generated asynchronous method) it is registered with the token's event from
//    the start of the call until it goes. The token's cancel only posts a turn to the same queue
//    (abort()); asking the runtime to stop, and answering, are that turn's. Asking is not done
//    inside the cancel because the platform answers it with events of the application's,
//    synchronously -- a ContentDialog moves the focus back (GettingFocus, LosingFocus) and, with
//    its popup gone, raises Closed and completes -- and a cancel resumes nobody. Whichever comes
//    first on the queue answers: the completion with what happened to the operation, the turn with
//    the cancellation; the other finds it answered. The turn is what ends the co_await however and
//    whenever the platform ends the operation, and posted from inside the cancel, while the queue
//    still takes work, it runs on the thread's loop or in its final drain. Under a token cancelled
//    already the call is not made at all.
//
// The operation and the dispatcher are template parameters, so that all of this can be tested
// over a fake of each; start_winrt_operation and start_winrt_action give it the projection's
// operation and a dispatcher over the queue of the calling thread.
//
// Private: this header names winrt types, so only wxl's own sources include it.

namespace wxl::impl {

/// One operation of the platform under way, and its answer once it is back. Made, answered and
/// deleted on the thread that made the call. The posted calls find it through its weak_link,
/// which is read on this thread alone; only the link's count crosses threads.
template <class R, class WinrtOp, class Convert>
class winrt_operation_producer
    : public async::async_op_t<R>,
      public core::weak_linked_mt<winrt_operation_producer<R, WinrtOp, Convert>>
{
public:
    winrt_operation_producer(WinrtOp operation, Convert convert)
        : operation_(std::move(operation)), convert_(std::move(convert)) {}

    /// The operation, the producer's own reference to it: the completion is hooked on this one.
    WinrtOp const& operation() const noexcept { return operation_; }

    /// The completion's posted call. Answered already -- by the turn of a cancellation -- it does
    /// nothing; otherwise it answers with what happened to the operation and resumes the coroutine
    /// waiting for it. It may be gone on return.
    void on_completed(winrt::Windows::Foundation::AsyncStatus status) {
        if (this->ready()) return;

        if (status == winrt::Windows::Foundation::AsyncStatus::Canceled)
            this->answer_canceled();
        else
            capture_results();

        this->complete();
    }

    /// The task lets go: an operation still out is asked to stop, and the producer goes at once --
    /// the completion, if it comes, finds the link empty.
    void release() override {
        if (!this->ready()) this->cancel();
        delete this;
    }

protected:
    /// Never called: the platform carries the operation out, and it is not sent to a worker.
    bool execute() override { return true; }

    /// Asks the runtime to stop the operation; once, as async_op::cancel() asks. The operation is
    /// held by a copy across the call: what the platform raises inside it may let the task go, and
    /// the producer with its own reference. Not caught: a refusal leaves the operation going -- a
    /// dialog still open -- while the co_await would end with the cancellation as though it had
    /// stopped, so the refusal stops the process here, through std::terminate, whose handler names
    /// the exception.
    void on_cancel() noexcept override {
        WinrtOp const op = operation_;
        op.Cancel();
    }

    /// Completed, the result; failed, GetResults() throws what the operation failed with, and that
    /// is the answer -- as is whatever `convert` throws.
    void capture_results() noexcept {
        try {
            if constexpr (std::is_void_v<R>)
                operation_.GetResults();
            else
                this->set_value(convert_(operation_.GetResults()));
        } catch (...) {
            this->set_current_exception();
        }
    }

private:
    WinrtOp operation_;
    Convert convert_;
};

/// The producer in its form with a token: it is registered with the token's event while it lives,
/// and the token's cancel posts the turn that asks the runtime to stop and answers the
/// cancellation.
template <class R, class WinrtOp, dispatcher_like Dispatcher, class Convert>
class winrt_cancelable_operation_producer final
    : public async::detail::pooled<
          winrt_cancelable_operation_producer<R, WinrtOp, Dispatcher, Convert>,
          winrt_operation_producer<R, WinrtOp, Convert>>
{
    using base = async::detail::pooled<winrt_cancelable_operation_producer,
                                       winrt_operation_producer<R, WinrtOp, Convert>>;

public:
    /// Made only under a token that has a source and is not cancelled yet.
    winrt_cancelable_operation_producer(WinrtOp operation, Convert convert, Dispatcher dispatcher,
                                        async::cancellation_token token)
        : base(std::move(operation), std::move(convert)),
          dispatcher_(std::move(dispatcher)),
          token_(std::move(token)),
          registration_(*this) {
        coro_check(!async::detail::cancellation_state_of(token_)->canceled(),
                   "winrt_cancelable_operation_producer: made under a token cancelled already",
                   async::detail::site::current());

        registration_.register_with(*async::detail::cancellation_state_of(token_));
    }

    /// Out of the event before the token goes.
    ~winrt_cancelable_operation_producer() override {
        registration_.unregister(*async::detail::cancellation_state_of(token_));
    }

    /// The turn the cancellation posted. Answered already -- the completion came first -- it does
    /// nothing; otherwise it asks the runtime to stop the operation, answers the cancellation and
    /// resumes the coroutine waiting for it. Events the platform raises while it is asked come
    /// here, on the dispatcher's turn. It may be gone on return.
    void abort() {
        if (this->ready()) return;

        // Held across the asking: a handler of the platform's events may let the task go, and the
        // producer with it.
        auto const link = this->weak_link();
        this->cancel();
        if (!link->get() || this->ready()) return;

        this->answer_canceled();
        this->complete();
    }

private:
    /// The token was cancelled: the turn is posted, and nothing else is done here -- cancelling
    /// resumes nobody, and asking the runtime raises events of the application's. A dispatcher
    /// that refuses it is closed, and refuses the completion as well: the wait then never ends, as
    /// it would not without a token. A dispatcher that throws stops the process here, through
    /// std::terminate, rather than leaving a wait nobody will end.
    void on_stopping() noexcept {
        if (this->ready()) return;

        // noexcept: what the turn throws is a terminate the UI thread's handler names, not an
        // HRESULT the projection's delegate hands the queue.
        dispatcher_.schedule([link = this->weak_link()]() noexcept {
            if (link->get())
                static_cast<winrt_cancelable_operation_producer*>(link->get())->abort();
        });
    }

    Dispatcher dispatcher_;
    async::cancellation_token token_;
    /// The producer's place in the token's event, built into it.
    async::cancellation_registration<&winrt_cancelable_operation_producer::on_stopping>
        registration_;
};

/// The completion handler: it touches nothing but the dispatcher and the link, which it hands
/// the turn to.
///
/// It runs on a thread of the runtime's, inside the projection's delegate, which would turn an
/// exception into an HRESULT for the platform and lose the turn without a word. So what the
/// scheduling throws -- the dispatcher's call failing, the delegate it is given not made -- stops
/// the process here, named: terminate_with_reason() describes the exception being handled and
/// aborts, whatever terminate handler the thread has.
///
/// A refusal is not an exception, and it is left as it is: the queue of the calling thread
/// refuses only once it is shut down, with XAML, on the way out. The coroutine waiting stays
/// suspended, and its frame keeps the wait at exit going for it -- a hang, which a Debug
/// build's watch over that wait reports after ten seconds, and not a turn lost in silence.
///
/// The runtime takes the handler once and calls it once, so the handler hands its link on to the
/// turn: the link changes hands, and its count, an atomic one, is not touched.
template <class Producer, class WinrtOp, dispatcher_like Dispatcher>
void post_winrt_completion(WinrtOp const& operation, Dispatcher const& dispatcher,
                           Producer& producer) {
    operation.Completed([link = producer.weak_link(), dispatcher](
                            auto const&, winrt::Windows::Foundation::AsyncStatus status) mutable {
        try {
            // The turn is noexcept for the same reason as abort()'s.
            static_cast<void>(dispatcher.schedule([link = std::move(link), status]() noexcept {
                if (auto* const producer = link->get()) producer->on_completed(status);
            }));
        } catch (...) {
            core::impl::terminate_with_reason();
        }
    });
}

/// The task of an operation the call has started already, answered on the thread of
/// `dispatcher`, the calling thread. The operation has one owner, the producer, and the completion
/// is hooked on its reference, so the hand-over touches no count.
template <class R, class WinrtOp, dispatcher_like Dispatcher, class Convert>
async::task<R> start_winrt_on_dispatcher(WinrtOp operation, Dispatcher const& dispatcher,
                                         Convert convert) {
    auto producer = std::make_unique<winrt_operation_producer<R, WinrtOp, Convert>>(
        std::move(operation), std::move(convert));
    post_winrt_completion(producer->operation(), dispatcher, *producer);
    return async::task<R>(std::move(producer));
}

/// The task of the operation `begin` starts, registered with `token`; under a token cancelled
/// already `begin` is not called, nothing is started, and the task answers the cancellation at
/// once. Under a token nobody can cancel it is start_winrt_on_dispatcher(): there is nothing to
/// register with.
/// The operation `begin` returns goes to the producer, its one owner, and the completion is hooked
/// on the producer's reference: the operation changes hands without its count being touched.
template <class R, class Begin, dispatcher_like Dispatcher, class Convert>
async::task<R> start_cancelable_winrt_on_dispatcher(Begin&& begin, Dispatcher const& dispatcher,
                                                    Convert convert,
                                                    async::cancellation_token token) {
    if (!async::detail::cancellation_state_of(token))
        return start_winrt_on_dispatcher<R>(begin(), dispatcher, std::move(convert));
    if (token.is_canceled()) return async::detail::canceled_task<R>();

    auto operation = begin();
    using producer_t = winrt_cancelable_operation_producer<R, std::decay_t<decltype(operation)>,
                                                           Dispatcher, Convert>;

    auto producer = std::make_unique<producer_t>(std::move(operation), std::move(convert),
                                                 dispatcher, std::move(token));
    post_winrt_completion(producer->operation(), dispatcher, *producer);
    return async::task<R>(std::move(producer));
}

/// An asynchronous operation with a result: `convert` turns what it returns (a projection value)
/// into `R`, on the calling thread.
template <class R, class WinrtOp, class Convert>
async::task<R> start_winrt_operation(WinrtOp operation, Convert convert) {
    return start_winrt_on_dispatcher<R>(
        std::move(operation),
        winrt_dispatcher{winrt::Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread()},
        std::move(convert));
}

/// An asynchronous operation without a result (IAsyncAction, with progress or without).
template <class WinrtOp>
async::task<> start_winrt_action(WinrtOp operation) {
    return start_winrt_on_dispatcher<void>(
        std::move(operation),
        winrt_dispatcher{winrt::Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread()},
        std::identity{});
}

/// start_winrt_operation in the form with a token: `begin` makes the call -- unless the token is
/// cancelled already, and then nothing is started.
template <class R, class Begin, class Convert>
async::task<R> start_cancelable_winrt_operation(Begin&& begin, Convert convert,
                                                async::cancellation_token token) {
    return start_cancelable_winrt_on_dispatcher<R>(
        std::forward<Begin>(begin),
        winrt_dispatcher{winrt::Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread()},
        std::move(convert), std::move(token));
}

/// start_winrt_action in the form with a token.
template <class Begin>
async::task<> start_cancelable_winrt_action(Begin&& begin, async::cancellation_token token) {
    return start_cancelable_winrt_on_dispatcher<void>(
        std::forward<Begin>(begin),
        winrt_dispatcher{winrt::Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread()},
        std::identity{}, std::move(token));
}

}  // namespace wxl::impl

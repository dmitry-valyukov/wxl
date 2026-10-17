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
//  * It does not wait for the platform to go. The posted calls reach it through a link, a cell
//    the producer clears as it goes, and not through a pointer to it; so the task letting go asks
//    the runtime to stop an operation still out (on_cancel()) and deletes the producer at once,
//    and a completion that comes later finds the link empty and does nothing. Whether the
//    completion ever comes is the platform's business: a ContentDialog asked to stop completes
//    only after its closing transition, and a window on its way out shuts its queue down first.
//    What then stays behind is the platform's own -- its operation, holding the completion
//    handler, and the handler the link and the queue -- and nothing of the producer's: not the
//    operation, the token or the queue. The link is the one thing both threads hold, so its count
//    is std::shared_ptr's atomic one, from the general heap: the runtime may let go of the handler
//    on any thread.
//  * A queue that refuses the posted call -- shut down, its thread on the way out -- leaves the
//    coroutine waiting for it as it is: it is not resumed, and the wait is its owner's to end.
//  * Under a token (start_operation_under, start_action_under, the form with a token of every
//    generated asynchronous method) it stands in the token's event from the start of the call
//    until it goes. The token's cancel only posts a turn to the same queue (cut()); asking the
//    runtime to stop, and answering, are that turn's. Asking is not done inside the cancel because
//    the platform answers it with events of the application's, synchronously -- a ContentDialog
//    moves the focus back (GettingFocus, LosingFocus) and, with its popup gone, raises Closed and
//    completes -- and a cancel resumes nobody. Whichever comes first on the queue answers: the
//    completion with what happened to the operation, the turn with the cancellation; the other
//    finds it answered. The turn is what ends the co_await however and whenever the platform ends
//    the operation, and posted from inside the cancel, while the queue still takes work, it runs
//    on the thread's loop or in its final drain. Under a token cancelled already the call is not
//    made at all.
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
class producer : public async::async_op_t<R>
{
public:
    /// Where a posted call finds the producer: the producer while it lives, null once it is gone.
    /// Written and read on the producer's thread alone; only the count crosses threads.
    struct link {
        producer* self;
    };

    producer(Op operation, Convert convert)
        : async::async_op_t<R>(async::orphanable),
          operation_(std::move(operation)),
          convert_(std::move(convert)),
          link_(std::make_shared<link>(link{this})) {}

    ~producer() override { link_->self = nullptr; }

    std::shared_ptr<link> const& reach() const noexcept { return link_; }

    /// The completion's posted call. Answered already -- by the turn of a cancellation -- it does
    /// nothing; otherwise it answers with what happened to the operation and resumes the coroutine
    /// waiting for it. It may be gone on return.
    void finish(AsyncStatus status) {
        if (this->ready()) return;

        if (status == AsyncStatus::Canceled)
            this->answer_canceled();
        else
            read();

        this->come_back();
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
        Op const asked = operation_;
        asked.Cancel();
    }

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

private:
    Op operation_;
    Convert convert_;
    std::shared_ptr<link> link_;
};

/// The producer in its form with a token: it stands in the token's event while it lives, and the
/// token's cancel posts the turn that asks the runtime to stop and answers the cancellation.
template <class R, class Op, class Queue, class Convert>
class producer_under final : public producer<R, Op, Convert>
{
    using base = producer<R, Op, Convert>;
    using cancellation_event = async::detail::cancellation_event;

public:
    /// Made only under a token that has a source and is not cancelled yet.
    producer_under(Op operation, Convert convert, Queue queue, async::cancellation_token stop)
        : base(std::move(operation), std::move(convert)),
          queue_(std::move(queue)),
          stop_(std::move(stop)),
          node_(*this) {
        async::detail::state_of(stop_)->event().add(core::as_not_null<cancellation_event::func_t>(&node_));
    }

    /// Out of the event before the token goes; once the token has been cancelled, the event the
    /// node stood in has been fired and is gone, and remove() finds nothing.
    ~producer_under() override {
        async::detail::state_of(stop_)->event().remove(
            core::cookie_t{static_cast<cancellation_event::func_t*>(&node_)});
    }

    /// The turn the cancellation posted. Answered already -- the completion came first -- it does
    /// nothing; otherwise it asks the runtime to stop the operation, answers the cancellation and
    /// resumes the coroutine waiting for it. Events the platform raises while it is asked come
    /// here, on the queue's turn. It may be gone on return.
    void cut() {
        if (this->ready()) return;

        // Held across the asking: a handler of the platform's events may let the task go, and the
        // producer with it.
        auto const link = this->reach();
        this->cancel();
        if (!link->self || this->ready()) return;

        this->answer_canceled();
        this->come_back();
    }

private:
    /// The token was cancelled: the turn is posted, and nothing else is done here -- cancelling
    /// resumes nobody, and asking the runtime raises events of the application's. A queue that
    /// refuses it is shut down, and refuses the completion as well: the wait then never ends, as it
    /// would not without a token. A queue that throws stops the process here, through
    /// std::terminate, rather than leaving a wait nobody will end.
    void stop_asked() noexcept {
        if (this->ready()) return;

        queue_.TryEnqueue([link = this->reach()] {
            if (link->self) static_cast<producer_under*>(link->self)->cut();
        });
    }

    /// The producer's place in the token's event, built into it.
    class node final : public cancellation_event::func_t
    {
    public:
        explicit node(producer_under& op) noexcept : op_(op) {}

        void operator()() noexcept override { op_.stop_asked(); }

        /// The producer owns its node.
        void release() noexcept override {}

    private:
        producer_under& op_;
    };

    Queue queue_;
    async::cancellation_token stop_;
    node node_;
};

/// The completion handler: it touches nothing but the queue and the link, which it posts on.
///
/// It runs on a thread of the runtime's, inside the projection's delegate, which would turn an
/// exception into an HRESULT for the platform and lose the turn without a word. So what the
/// posting throws -- the queue's call failing, the delegate it is given not made -- stops the
/// process here, named: terminate_with_reason() describes the exception being handled and
/// aborts, whatever terminate handler the thread has.
///
/// A refusal is not an exception, and it is left as it is: the queue of the calling thread
/// refuses only once it is shut down, with XAML, on the way out. The coroutine waiting stays
/// suspended, and a detached frame keeps the wait at exit going for it -- a hang, which a Debug
/// build's watch over that wait reports after ten seconds, and not a turn lost in silence.
template <class Producer, class Op, class Queue>
void post_completion(Op const& operation, Queue const& queue, Producer& made) {
    operation.Completed([link = made.reach(), queue](auto const&, AsyncStatus status) {
        try {
            static_cast<void>(queue.TryEnqueue([link, status] {
                if (link->self) link->self->finish(status);
            }));
        } catch (...) {
            core::impl::terminate_with_reason();
        }
    });
}

/// The task of an operation the call has started already, answered on the thread of `queue`, the
/// calling thread.
template <class R, class Op, class Queue, class Convert>
async::task<R> start(Op const& operation, Queue const& queue, Convert convert) {
    auto made = std::make_unique<producer<R, Op, Convert>>(operation, std::move(convert));
    post_completion(operation, queue, *made);
    return async::task<R>(std::move(made));
}

/// The task of the operation `begin` starts, standing under `stop`; under a token cancelled
/// already `begin` is not called, nothing is started, and the task answers the cancellation at
/// once. Under a token nobody can cancel it is start(): there is nothing to stand under.
template <class R, class Begin, class Queue, class Convert>
async::task<R> start_under(Begin&& begin, Queue const& queue, Convert convert,
                           async::cancellation_token stop) {
    if (!async::detail::state_of(stop)) return start<R>(begin(), queue, std::move(convert));
    if (stop.is_canceled()) return async::detail::canceled<R>();

    auto const operation = begin();
    using made_t = producer_under<R, std::decay_t<decltype(operation)>, Queue, Convert>;

    auto made = std::make_unique<made_t>(operation, std::move(convert), queue, std::move(stop));
    post_completion(operation, queue, *made);
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

/// start_operation in the form with a token: `begin` makes the call -- unless the token is
/// cancelled already, and then nothing is started.
template <class R, class Begin, class Convert>
async::task<R> start_operation_under(Begin&& begin, Convert convert, async::cancellation_token stop) {
    return detail::start_under<R>(
        std::forward<Begin>(begin), winrt::Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread(),
        std::move(convert), std::move(stop));
}

/// start_action in the form with a token.
template <class Begin>
async::task<> start_action_under(Begin&& begin, async::cancellation_token stop) {
    return detail::start_under<void>(
        std::forward<Begin>(begin), winrt::Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread(),
        std::identity{}, std::move(stop));
}

}  // namespace wxl::impl

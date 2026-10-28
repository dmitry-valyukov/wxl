// The task a WinRT asynchronous operation becomes (impl/operation.h): what it answers, where, and
// what letting it go does.
//
// No dispatcher and no runtime. The operation is a fake one with the three calls the producer
// makes -- Completed, GetResults, Cancel -- and the test ends it, on this thread or another. The
// dispatcher is a fake one that keeps what is scheduled on it until the test runs it, on this
// thread, which stands for the thread that made the call. Whether the producer is gone is read off
// the operation: the producer holds a reference to it until it is deleted.
//
// showDialog's two halves (impl/show_dialog.h) are tested here too, over a dialog whose showAsync
// is made of the same fakes the way a generated wrapper makes its form with a token: the call
// that may refuse, and the wait for the answer -- a task whose frame is under way until then.
#include <crtdbg.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <exception>
#include <format>
#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include <winrt/Windows.Foundation.h>

#include "impl/operation.h"
#include "impl/show_dialog.h"
#include "sta_box.h"

namespace {

using winrt::Windows::Foundation::AsyncStatus;
using wxl::async::cancellation_source;
using wxl::async::cancellation_token;
using wxl::async::live_frames;
using wxl::async::operation_canceled_exception;
using wxl::async::task;
using wxl::impl::show_dialog;
using wxl::impl::start_cancelable_winrt_on_dispatcher;
using wxl::impl::start_winrt_on_dispatcher;

/// What the platform throws for an operation that failed: the code, as winrt::hresult_error
/// carries it.
struct platform_failure {
    uint32_t code;
};

/// The code the platform reports for an operation that was cancelled, if its results are asked.
constexpr uint32_t canceled_code = 0x800704C7;

/// The result a fake operation keeps, read only once its status says it completed; an operation
/// without a result keeps none.
template <class T>
struct fake_result {
    T result{};
};

template <>
struct fake_result<void> {
};

/// An operation of the platform: a reference to one object, which its copies share.
template <class T>
class fake_operation
{
public:
    using handler_t = std::function<void(fake_operation const&, AsyncStatus)>;

    /// The handler, as the runtime keeps it: called once, when the operation ends -- at once, on
    /// the thread giving it, for an operation that has ended already.
    void Completed(handler_t handler) const {
        if (state_->status == AsyncStatus::Started) {
            state_->handler = std::move(handler);
            return;
        }
        invoke(handler, state_->status);
    }

    /// The result of one that completed; for one that failed or was cancelled, the platform's
    /// failure, thrown.
    T GetResults() const {
        ++state_->reads;
        if (state_->status == AsyncStatus::Error) throw platform_failure{state_->code};
        if (state_->status != AsyncStatus::Completed) throw platform_failure{canceled_code};
        if constexpr (std::is_void_v<T>)
            return;
        else
            return state_->result;
    }

    /// Counted. What the platform does inside the call happens here: events of the application's
    /// (`when_cancelled`), and an end, for one that ends when asked.
    void Cancel() const {
        ++state_->cancels;
        if (std::function<void()> const hook = std::exchange(state_->when_cancelled, {})) hook();
        if (state_->ends_when_cancelled && state_->status == AsyncStatus::Started)
            end(AsyncStatus::Canceled);
    }

    void ends_when_cancelled() const { state_->ends_when_cancelled = true; }
    void when_cancelled(std::function<void()> hook) const {
        state_->when_cancelled = std::move(hook);
    }

    /// The operation ends: the handler is called on whichever thread calls this.
    void end(AsyncStatus status) const {
        state_->status = status;
        if (handler_t const handler = std::exchange(state_->handler, {})) invoke(handler, status);
    }

    template <class... Value>
    void complete(Value... value) const {
        if constexpr (sizeof...(Value) != 0) state_->result = T{value...};
        end(AsyncStatus::Completed);
    }

    void fail(uint32_t code) const {
        state_->code = code;
        end(AsyncStatus::Error);
    }

    int cancels() const { return state_->cancels; }
    int reads() const { return state_->reads; }

    /// References to the operation: the test's own, and the producer's while it lives.
    long held() const { return static_cast<long>(state_->references()); }

private:
    /// As the projection's delegate calls the handler: what it throws becomes an HRESULT for the
    /// platform, which reads none, so the exception is gone.
    void invoke(handler_t const& handler, AsyncStatus status) const {
        try {
            handler(*this, status);
        } catch (...) {
        }
    }

    /// A copy goes with the handler on whichever thread ends the operation: the count is atomic.
    struct state final : wxl::core::refcounted_mt, fake_result<T> {
        std::size_t references() const noexcept { return ref_count(); }

        AsyncStatus status = AsyncStatus::Started;
        handler_t handler;
        uint32_t code = 0;
        int cancels = 0;
        int reads = 0;
        bool ends_when_cancelled = false;
        std::function<void()> when_cancelled;
    };

    wxl::core::intrusive_ptr<state> state_ = wxl::core::make_refcounted<state>();
};

/// The dispatcher of the thread that made the call: scheduled on from any thread, run by the test
/// on its own.
class fake_dispatcher
{
public:
    template <class Fn>
    bool schedule(Fn&& fn) const {
        std::lock_guard const lock(state_->mutex);
        state_->posted.emplace_back(std::forward<Fn>(fn));
        return true;
    }

    /// Runs what has been posted, in order. \return how many calls ran.
    std::size_t run() const {
        std::vector<std::function<void()>> now;
        {
            std::lock_guard const lock(state_->mutex);
            now.swap(state_->posted);
        }
        for (auto& call : now) call();
        return now.size();
    }

    std::size_t posted() const {
        std::lock_guard const lock(state_->mutex);
        return state_->posted.size();
    }

private:
    /// A copy goes with the completion handler to the thread that ends the operation: the count
    /// is atomic.
    struct state final : wxl::core::refcounted_mt {
        std::mutex mutex;
        std::vector<std::function<void()>> posted;
    };

    wxl::core::intrusive_ptr<state> state_ = wxl::core::make_refcounted<state>();
};

/// What `convert` is for a generated wrapper: the projection's value into wxl's. Counts its calls.
auto counted(int& converted) {
    return [&converted](int value) {
        ++converted;
        return std::to_string(value);
    };
}

/// The call a form with a token makes, as a generated wrapper hands it over: counts how often it
/// is made.
auto begins(fake_operation<int> const& op, int& made) {
    return [&op, &made] {
        ++made;
        return op;
    };
}

using log_t = std::vector<std::string>;

/// Waits for the task and writes down how it ended.
task<> awaits(task<std::string> pending, log_t& log) {
    try {
        log.push_back("value " + co_await std::move(pending));
    } catch (operation_canceled_exception const&) {
        log.push_back("cancelled");
    } catch (platform_failure const& failure) {
        log.push_back(std::format("failed {:#x}", failure.code));
    }
}

task<> awaits(task<> pending, log_t& log) {
    try {
        co_await std::move(pending);
        log.push_back("done");
    } catch (operation_canceled_exception const&) {
        log.push_back("cancelled");
    } catch (platform_failure const& failure) {
        log.push_back(std::format("failed {:#x}", failure.code));
    }
}

TEST(OperationTaskTest, AnswersWithTheResultConvertedOnTheQueue) {
    fake_operation<int> op;
    fake_dispatcher dispatcher;
    int converted = 0;
    log_t log;

    {
        auto const waiting =
            awaits(start_winrt_on_dispatcher<std::string>(op, dispatcher, counted(converted)), log);
        EXPECT_FALSE(waiting.done());

        // The completion only posts: nothing is read or resumed on its thread.
        op.complete(7);
        EXPECT_EQ(dispatcher.posted(), 1u);
        EXPECT_EQ(converted, 0);
        EXPECT_TRUE(log.empty());

        EXPECT_EQ(dispatcher.run(), 1u);
        EXPECT_TRUE(waiting.done());
        EXPECT_EQ(log, (log_t{"value 7"}));
        EXPECT_EQ(converted, 1);
    }

    EXPECT_EQ(op.cancels(), 0);
    EXPECT_EQ(op.held(), 1);
}

TEST(OperationTaskTest, AnActionAnswersWithNothing) {
    fake_operation<void> op;
    fake_dispatcher dispatcher;
    log_t log;

    {
        auto const waiting =
            awaits(start_winrt_on_dispatcher<void>(op, dispatcher, std::identity{}), log);
        op.complete();
        dispatcher.run();
        EXPECT_TRUE(waiting.done());
    }

    EXPECT_EQ(log, (log_t{"done"}));
    EXPECT_EQ(op.reads(), 1);
    EXPECT_EQ(op.held(), 1);
}

TEST(OperationTaskTest, AFailedOperationAnswersWhatGetResultsThrows) {
    fake_operation<int> op;
    fake_dispatcher dispatcher;
    int converted = 0;
    log_t log;

    {
        auto const waiting =
            awaits(start_winrt_on_dispatcher<std::string>(op, dispatcher, counted(converted)), log);
        op.fail(0x80070005);
        dispatcher.run();
        EXPECT_TRUE(waiting.done());
    }

    EXPECT_EQ(log, (log_t{"failed 0x80070005"}));
    EXPECT_EQ(converted, 0);
    EXPECT_EQ(op.held(), 1);
}

TEST(OperationTaskTest, AFailedActionAnswersWhatGetResultsThrows) {
    fake_operation<void> op;
    fake_dispatcher dispatcher;
    log_t log;

    {
        auto const waiting =
            awaits(start_winrt_on_dispatcher<void>(op, dispatcher, std::identity{}), log);
        op.fail(0x80004005);
        dispatcher.run();
        EXPECT_TRUE(waiting.done());
    }

    EXPECT_EQ(log, (log_t{"failed 0x80004005"}));
}

TEST(OperationTaskTest, ACancelledOperationAnswersOperationCanceledException) {
    fake_operation<int> op;
    fake_dispatcher dispatcher;
    int converted = 0;
    log_t log;

    {
        auto const waiting =
            awaits(start_winrt_on_dispatcher<std::string>(op, dispatcher, counted(converted)), log);
        op.end(AsyncStatus::Canceled);
        dispatcher.run();
        EXPECT_TRUE(waiting.done());
    }

    // Answered as every cancellation in wxl is, without asking the platform for its failure.
    EXPECT_EQ(log, (log_t{"cancelled"}));
    EXPECT_EQ(op.reads(), 0);
    EXPECT_EQ(converted, 0);
}

TEST(OperationTaskTest, ACancelledActionAnswersOperationCanceledException) {
    fake_operation<void> op;
    fake_dispatcher dispatcher;
    log_t log;

    {
        auto const waiting =
            awaits(start_winrt_on_dispatcher<void>(op, dispatcher, std::identity{}), log);
        op.end(AsyncStatus::Canceled);
        dispatcher.run();
    }

    EXPECT_EQ(log, (log_t{"cancelled"}));
}

TEST(OperationTaskTest, WhatConvertThrowsIsTheAnswer) {
    fake_operation<int> op;
    fake_dispatcher dispatcher;
    log_t log;

    {
        auto const throws = [](int) -> std::string { throw platform_failure{1}; };
        auto const waiting =
            awaits(start_winrt_on_dispatcher<std::string>(op, dispatcher, throws), log);
        op.complete(7);
        dispatcher.run();
    }

    EXPECT_EQ(log, (log_t{"failed 0x1"}));
}

TEST(OperationTaskTest, AnAnswerHereBeforeTheCoAwaitIsTakenWithoutSuspending) {
    fake_operation<int> op;
    fake_dispatcher dispatcher;
    int converted = 0;
    log_t log;

    auto pending = start_winrt_on_dispatcher<std::string>(op, dispatcher, counted(converted));
    op.complete(7);
    dispatcher.run();
    EXPECT_TRUE(pending.done());

    // Nothing is left to resume the coroutine: one that suspended would not end.
    auto const waiting = awaits(std::move(pending), log);
    EXPECT_TRUE(waiting.done());
    EXPECT_EQ(log, (log_t{"value 7"}));
    EXPECT_EQ(dispatcher.posted(), 0u);
}

TEST(OperationTaskTest, AnOperationOverAlreadyIsAnsweredThroughTheQueueAsWell) {
    fake_operation<int> op;
    fake_dispatcher dispatcher;
    int converted = 0;

    op.complete(7);

    auto pending = start_winrt_on_dispatcher<std::string>(op, dispatcher, counted(converted));
    EXPECT_FALSE(pending.done());
    EXPECT_EQ(dispatcher.posted(), 1u);

    dispatcher.run();
    EXPECT_TRUE(pending.done());
    EXPECT_EQ(pending.result(), "7");
}

TEST(OperationTaskTest, LettingTheTaskGoCancelsTheOperationAndDeletesTheProducerAtOnce) {
    fake_operation<int> op;
    fake_dispatcher dispatcher;
    int converted = 0;
    log_t log;

    {
        auto const waiting =
            awaits(start_winrt_on_dispatcher<std::string>(op, dispatcher, counted(converted)), log);
        EXPECT_EQ(op.cancels(), 0);
    }  // the coroutine's frame goes, and the task it awaits with it

    EXPECT_EQ(op.cancels(), 1);
    EXPECT_EQ(op.held(), 1) << "the producer does not wait for the platform";

    // The completion still comes, and finds nobody: nothing is read, nobody resumed.
    op.end(AsyncStatus::Canceled);
    EXPECT_EQ(dispatcher.run(), 1u);
    EXPECT_EQ(op.held(), 1);
    EXPECT_TRUE(log.empty());
    EXPECT_EQ(converted, 0);
}

TEST(OperationTaskTest, AGivenUpOperationThatCompletedAnywayIsNotRead) {
    fake_operation<int> op;
    fake_dispatcher dispatcher;
    int converted = 0;
    log_t log;

    {
        auto const waiting =
            awaits(start_winrt_on_dispatcher<std::string>(op, dispatcher, counted(converted)), log);
    }

    // Finished before the request to stop reached it: the result is nobody's.
    op.complete(7);
    dispatcher.run();

    EXPECT_EQ(op.held(), 1);
    EXPECT_EQ(op.reads(), 0);
    EXPECT_EQ(converted, 0);
    EXPECT_TRUE(log.empty());
}

TEST(OperationTaskTest, ATaskLetGoWhileItsAnswerIsPostedGoesAndThePostedCallFindsNobody) {
    fake_operation<int> op;
    fake_dispatcher dispatcher;
    int converted = 0;

    auto pending = start_winrt_on_dispatcher<std::string>(op, dispatcher, counted(converted));
    op.complete(7);

    // Not ready until the dispatcher's turn: the task asks the operation to stop, which costs
    // nothing.
    {
        auto const gone = std::move(pending);
    }
    EXPECT_EQ(op.cancels(), 1);
    EXPECT_EQ(op.held(), 1);

    EXPECT_EQ(dispatcher.run(), 1u);
    EXPECT_EQ(op.held(), 1);
    EXPECT_EQ(converted, 0);
}

TEST(OperationTaskTest, ATaskLetGoAfterItsAnswerCancelsNothing) {
    fake_operation<int> op;
    fake_dispatcher dispatcher;
    int converted = 0;

    {
        auto const pending =
            start_winrt_on_dispatcher<std::string>(op, dispatcher, counted(converted));
        op.complete(7);
        dispatcher.run();
        EXPECT_TRUE(pending.done());
    }

    EXPECT_EQ(op.cancels(), 0);
    EXPECT_EQ(op.held(), 1);
}

TEST(OperationTaskTest, TheCompletionOnAnotherThreadOnlyPosts) {
    fake_operation<int> op;
    fake_dispatcher dispatcher;
    int converted = 0;
    log_t log;

    {
        auto const waiting =
            awaits(start_winrt_on_dispatcher<std::string>(op, dispatcher, counted(converted)), log);

        std::thread([op] { op.complete(9); }).join();
        EXPECT_FALSE(waiting.done());
        EXPECT_EQ(converted, 0);

        dispatcher.run();
        EXPECT_TRUE(waiting.done());
    }

    EXPECT_EQ(log, (log_t{"value 9"}));
    EXPECT_EQ(op.held(), 1);
}

TEST(OperationTaskTest, ATaskMoves) {
    fake_operation<int> op;
    fake_dispatcher dispatcher;
    int converted = 0;

    {
        auto first = start_winrt_on_dispatcher<std::string>(op, dispatcher, counted(converted));
        auto second = std::move(first);
        task<std::string> third{std::move(second)};
        first = std::move(third);

        op.complete(7);
        dispatcher.run();
        EXPECT_TRUE(first.done());
        EXPECT_EQ(first.result(), "7");
    }

    // The moved-from tasks hold nothing: no request to stop, one producer, deleted once.
    EXPECT_EQ(op.cancels(), 0);
    EXPECT_EQ(op.held(), 1);
}

// ---- under a token -------------------------------------------------------------------------

TEST(OperationUnderTokenTest, NobodyCancellingItTheAnswerIsTheOperationsOwn) {
    fake_operation<int> op;
    fake_dispatcher dispatcher;
    cancellation_source source;
    int converted = 0;
    int made = 0;
    log_t log;

    {
        auto const waiting =
            awaits(start_cancelable_winrt_on_dispatcher<std::string>(
                       begins(op, made), dispatcher, counted(converted), source.token()),
                   log);
        EXPECT_EQ(made, 1);

        op.complete(7);
        EXPECT_EQ(dispatcher.run(), 1u);
        EXPECT_TRUE(waiting.done());
    }

    EXPECT_EQ(log, (log_t{"value 7"}));
    EXPECT_EQ(op.cancels(), 0);
    EXPECT_EQ(op.held(), 1);

    // Gone, the producer has left the token's event: cancelling reaches nobody.
    source.cancel();
    EXPECT_EQ(dispatcher.posted(), 0u);
    EXPECT_EQ(op.cancels(), 0);
}

TEST(OperationUnderTokenTest, CancellingPostsATurnThatAsksTheOperationToStopAndEndsTheWait) {
    fake_operation<int> op;
    fake_dispatcher dispatcher;
    cancellation_source source;
    int converted = 0;
    int made = 0;
    log_t log;

    {
        auto const waiting =
            awaits(start_cancelable_winrt_on_dispatcher<std::string>(
                       begins(op, made), dispatcher, counted(converted), source.token()),
                   log);

        source.cancel();
        EXPECT_EQ(op.cancels(), 0) << "the runtime is not asked inside the cancel";
        EXPECT_TRUE(log.empty()) << "cancelling resumes nobody";
        EXPECT_EQ(dispatcher.posted(), 1u) << "the turn that asks and answers";

        // The platform has not answered, and need not: the turn ends the wait.
        EXPECT_EQ(dispatcher.run(), 1u);
        EXPECT_EQ(op.cancels(), 1);
        EXPECT_TRUE(waiting.done());
        EXPECT_EQ(log, (log_t{"cancelled"}));
    }  // the task goes, and the producer with it

    EXPECT_EQ(op.cancels(), 1) << "the runtime is asked once";
    EXPECT_EQ(op.held(), 1) << "the producer does not wait for the platform";

    op.end(AsyncStatus::Canceled);
    EXPECT_EQ(dispatcher.run(), 1u);
    EXPECT_EQ(op.held(), 1);
    EXPECT_EQ(op.reads(), 0);
    EXPECT_EQ(converted, 0);
}

TEST(OperationUnderTokenTest, ACompletionPostedBeforeTheCancellationIsTheAnswer) {
    fake_operation<int> op;
    fake_dispatcher dispatcher;
    cancellation_source source;
    int converted = 0;
    int made = 0;
    log_t log;

    {
        auto const waiting =
            awaits(start_cancelable_winrt_on_dispatcher<std::string>(
                       begins(op, made), dispatcher, counted(converted), source.token()),
                   log);

        // Answered on the platform's side, not yet on this thread's.
        op.complete(7);
        source.cancel();
        EXPECT_EQ(dispatcher.posted(), 2u);

        EXPECT_EQ(dispatcher.run(), 2u);
        EXPECT_TRUE(waiting.done());
    }

    // What got there first answers; the turn after it finds it answered and asks nothing.
    EXPECT_EQ(log, (log_t{"value 7"}));
    EXPECT_EQ(converted, 1);
    EXPECT_EQ(op.cancels(), 0);
    EXPECT_EQ(op.held(), 1);
}

TEST(OperationUnderTokenTest, AnOperationThatEndsInsideCancelIsAnsweredOnce) {
    fake_operation<int> op;
    fake_dispatcher dispatcher;
    cancellation_source source;
    int converted = 0;
    int made = 0;
    log_t log;

    op.ends_when_cancelled();

    {
        auto const waiting =
            awaits(start_cancelable_winrt_on_dispatcher<std::string>(
                       begins(op, made), dispatcher, counted(converted), source.token()),
                   log);

        source.cancel();
        EXPECT_EQ(dispatcher.posted(), 1u) << "the turn alone: nothing is asked inside the cancel";

        // The turn asks, the platform ends inside the asking and posts its completion, and the
        // turn answers; the completion then finds it answered.
        EXPECT_EQ(dispatcher.run(), 1u);
        EXPECT_TRUE(waiting.done());
        EXPECT_EQ(dispatcher.run(), 1u);
    }

    EXPECT_EQ(log, (log_t{"cancelled"}));
    EXPECT_EQ(op.cancels(), 1);
    EXPECT_EQ(op.held(), 1);
}

TEST(OperationUnderTokenTest, UnderATokenCancelledAlreadyNothingIsStarted) {
    fake_operation<int> op;
    fake_dispatcher dispatcher;
    cancellation_source source;
    int converted = 0;
    int made = 0;
    log_t log;

    source.cancel();

    auto pending = start_cancelable_winrt_on_dispatcher<std::string>(
        begins(op, made), dispatcher, counted(converted), source.token());
    EXPECT_EQ(made, 0) << "the call is not made";
    EXPECT_TRUE(pending.done());

    auto const waiting = awaits(std::move(pending), log);
    EXPECT_TRUE(waiting.done());
    EXPECT_EQ(log, (log_t{"cancelled"}));
    EXPECT_EQ(dispatcher.posted(), 0u);
    EXPECT_EQ(op.cancels(), 0);
}

TEST(OperationUnderTokenTest, UnderATokenOfNobodyItIsTheFormWithoutOne) {
    fake_operation<int> op;
    fake_dispatcher dispatcher;
    int converted = 0;
    int made = 0;
    log_t log;

    {
        auto const waiting =
            awaits(start_cancelable_winrt_on_dispatcher<std::string>(
                       begins(op, made), dispatcher, counted(converted), cancellation_token{}),
                   log);
        op.complete(7);
        dispatcher.run();
        EXPECT_TRUE(waiting.done());
    }

    EXPECT_EQ(log, (log_t{"value 7"}));
    EXPECT_EQ(made, 1);
    EXPECT_EQ(op.held(), 1);
}

TEST(OperationUnderTokenTest, ATaskLetGoUnderATokenGoesAndTheTokenReachesItNoMore) {
    fake_operation<int> op;
    fake_dispatcher dispatcher;
    cancellation_source source;
    int converted = 0;
    int made = 0;
    log_t log;

    {
        auto const waiting =
            awaits(start_cancelable_winrt_on_dispatcher<std::string>(
                       begins(op, made), dispatcher, counted(converted), source.token()),
                   log);
    }

    EXPECT_EQ(op.cancels(), 1);
    EXPECT_EQ(op.held(), 1) << "let go, the producer goes at once";

    // Gone, it has left the token's event: the cancel posts nothing.
    source.cancel();
    EXPECT_EQ(dispatcher.posted(), 0u);
    EXPECT_EQ(op.cancels(), 1);

    op.end(AsyncStatus::Canceled);
    EXPECT_EQ(dispatcher.run(), 1u);
    EXPECT_EQ(op.held(), 1);
    EXPECT_TRUE(log.empty());
}

TEST(OperationUnderTokenTest, ATaskLetGoWhileTheTurnAsksIsNotTouchedAfterwards) {
    fake_operation<int> op;
    fake_dispatcher dispatcher;
    cancellation_source source;
    int converted = 0;
    int made = 0;

    auto pending = std::make_unique<wxl::test::sta_box<task<std::string>>>(
        start_cancelable_winrt_on_dispatcher<std::string>(begins(op, made), dispatcher,
                                                          counted(converted), source.token()));

    // An event the platform raises while it is asked -- a Closed handler of the application's --
    // lets the task go, and the producer with it, inside the turn.
    op.when_cancelled([&pending] { pending.reset(); });

    source.cancel();
    EXPECT_EQ(dispatcher.run(), 1u);
    EXPECT_EQ(pending, nullptr);
    EXPECT_EQ(op.cancels(), 1);
    EXPECT_EQ(op.held(), 1);
}

// ---- showDialog (impl/show_dialog.h) -------------------------------------------------------

/// What XAML refuses a second dialog over an island with: E_ASYNC_OPERATION_NOT_STARTED.
constexpr uint32_t refused_code = 0x80000019;

/// A ContentDialog as showDialog has it: showAsync in its form with a token, made as a generated
/// wrapper makes it -- the platform's call as a lambda, through
/// start_cancelable_winrt_on_dispatcher -- over the fake operation and the fake dispatcher. One
/// that refuses throws from that call, as ShowAsync does.
struct fake_dialog {
    fake_operation<int> op;
    fake_dispatcher dispatcher;
    bool refuses = false;
    mutable int shown = 0;  ///< calls of the platform's ShowAsync

    task<int> showAsync(cancellation_token token) const {
        return start_cancelable_winrt_on_dispatcher<int>(
            [this] {
                ++shown;
                if (refuses) throw platform_failure{refused_code};
                return op;
            },
            dispatcher, std::identity{}, std::move(token));
    }
};

TEST(ShowDialogTest, TheWaitForTheAnswerIsAFrameUnderWayUntilTheAnswerComes) {
    fake_dialog dialog;
    cancellation_source source;
    [[maybe_unused]] std::size_t before = 0;
    if constexpr (wxl::async::detail::checked) before = live_frames();

    task<> waiting = show_dialog(dialog, source.token());
    EXPECT_EQ(dialog.shown, 1);
    if constexpr (wxl::async::detail::checked) {
        EXPECT_EQ(live_frames(), before + 1) << "the call has returned, and the wait is under way";
    }

    // The reader answers: the platform's completion only posts.
    dialog.op.complete(1);
    if constexpr (wxl::async::detail::checked) {
        EXPECT_EQ(live_frames(), before + 1);
    }

    EXPECT_EQ(dialog.dispatcher.run(), 1u);
    if constexpr (wxl::async::detail::checked) {
        EXPECT_EQ(live_frames(), before);
    }
    EXPECT_TRUE(waiting.done());
    EXPECT_NO_THROW(waiting.result());
    EXPECT_EQ(dialog.op.reads(), 1);
    EXPECT_EQ(dialog.op.held(), 1) << "the producer went with the frame";

    // Gone, the wait is registered with the token no more.
    source.cancel();
    EXPECT_EQ(dialog.dispatcher.posted(), 0u);
    EXPECT_EQ(dialog.op.cancels(), 0);
}

TEST(ShowDialogTest, CancellingTheTokenClosesTheDialogAndTheFrameEndsOnTheNextTurn) {
    fake_dialog dialog;
    cancellation_source source;
    [[maybe_unused]] std::size_t before = 0;
    if constexpr (wxl::async::detail::checked) before = live_frames();

    task<> waiting = show_dialog(dialog, source.token());

    source.cancel();
    EXPECT_EQ(dialog.op.cancels(), 0) << "cancelling only posts the turn";
    if constexpr (wxl::async::detail::checked) {
        EXPECT_EQ(live_frames(), before + 1) << "and resumes nobody";
    }

    // The turn asks the dialog to close and ends the frame: the platform has not answered, and
    // need not.
    EXPECT_EQ(dialog.dispatcher.run(), 1u);
    EXPECT_EQ(dialog.op.cancels(), 1);
    if constexpr (wxl::async::detail::checked) {
        EXPECT_EQ(live_frames(), before);
    }
    EXPECT_EQ(dialog.op.held(), 1) << "the producer went with the frame";
    ASSERT_TRUE(waiting.done());
    EXPECT_THROW(waiting.result(), operation_canceled_exception);

    // The dialog's completion, when it comes after its closing, finds nobody.
    dialog.op.end(AsyncStatus::Canceled);
    EXPECT_EQ(dialog.dispatcher.run(), 1u);
    EXPECT_EQ(dialog.op.reads(), 0);
    EXPECT_EQ(dialog.op.cancels(), 1) << "the runtime is asked once";
}

// Dropping the wait closes the dialog too: the operation still out is asked to stop as its
// producer goes with the frame, and its completion finds nobody.
TEST(ShowDialogTest, DroppingTheWaitClosesTheDialog) {
    fake_dialog dialog;
    cancellation_source source;
    [[maybe_unused]] std::size_t before = 0;
    if constexpr (wxl::async::detail::checked) before = live_frames();

    task<> waiting = show_dialog(dialog, source.token());
    if constexpr (wxl::async::detail::checked) {
        EXPECT_EQ(live_frames(), before + 1);
    }

    waiting = {};
    if constexpr (wxl::async::detail::checked) {
        EXPECT_EQ(live_frames(), before);
    }
    EXPECT_EQ(dialog.op.cancels(), 1) << "the dialog was not asked to close";
    EXPECT_EQ(dialog.op.held(), 1);

    dialog.op.end(AsyncStatus::Canceled);
    EXPECT_EQ(dialog.dispatcher.run(), 1u);
    EXPECT_EQ(dialog.op.reads(), 0);
}

TEST(ShowDialogTest, ADialogThatCompletesInsideItsCancelEndsTheFrameOnce) {
    fake_dialog dialog;
    dialog.op.ends_when_cancelled();  // a ContentDialog whose popup is gone already
    cancellation_source source;
    [[maybe_unused]] std::size_t before = 0;
    if constexpr (wxl::async::detail::checked) before = live_frames();

    task<> waiting = show_dialog(dialog, source.token());
    source.cancel();

    // The turn asks; the completion the asking raises is posted behind it, and finds the frame
    // gone.
    EXPECT_EQ(dialog.dispatcher.run(), 1u);
    if constexpr (wxl::async::detail::checked) {
        EXPECT_EQ(live_frames(), before);
    }
    EXPECT_EQ(dialog.dispatcher.run(), 1u);
    EXPECT_EQ(dialog.op.reads(), 0);
    EXPECT_EQ(dialog.op.held(), 1);
}

TEST(ShowDialogTest, ARefusalIsThrownToTheCallerAndLeavesNothingRunning) {
    fake_dialog dialog;
    dialog.refuses = true;
    cancellation_source source;
    [[maybe_unused]] std::size_t before = 0;
    if constexpr (wxl::async::detail::checked) before = live_frames();

    try {
        task<> waiting = show_dialog(dialog, source.token());
        ADD_FAILURE() << "the refusal did not reach the caller";
    } catch (platform_failure const& refusal) {
        EXPECT_EQ(refusal.code, refused_code);
    }

    EXPECT_EQ(dialog.shown, 1);
    if constexpr (wxl::async::detail::checked) {
        EXPECT_EQ(live_frames(), before) << "no frame is made for a dialog that is not up";
    }
    EXPECT_EQ(dialog.op.held(), 1) << "no producer either";

    // Nothing is registered with the token.
    source.cancel();
    EXPECT_EQ(dialog.dispatcher.posted(), 0u);
    EXPECT_EQ(dialog.op.cancels(), 0);
}

TEST(ShowDialogTest, UnderATokenCancelledAlreadyTheDialogIsNotShownAndNothingIsLeft) {
    fake_dialog dialog;
    cancellation_source source;
    [[maybe_unused]] std::size_t before = 0;
    if constexpr (wxl::async::detail::checked) before = live_frames();

    source.cancel();
    task<> waiting = show_dialog(dialog, source.token());

    EXPECT_EQ(dialog.shown, 0) << "the call is not made";
    if constexpr (wxl::async::detail::checked) {
        EXPECT_EQ(live_frames(), before) << "the frame ended where it began";
    }
    EXPECT_TRUE(waiting.done());
    EXPECT_EQ(dialog.dispatcher.posted(), 0u);
    EXPECT_EQ(dialog.op.held(), 1);
}

/// A report goes to stderr and the process ends without a dialog, which in a test run would be a
/// hang.
void without_a_dialog() { _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT); }

void show_under_a_token_of_nobody(fake_dialog const& dialog) {
    without_a_dialog();
    task<> waiting = show_dialog(dialog, cancellation_token{});
}

TEST(ShowDialogDeathTest, UnderATokenNobodyCanCancelTheCallStops) {
    fake_dialog const dialog;
    EXPECT_DEATH(show_under_a_token_of_nobody(dialog),
                 "wxl: showDialog under a token nobody can cancel");
    EXPECT_EQ(dialog.shown, 0);
}

/// The dispatcher of the calling thread whose call fails: the projection's TryEnqueue throws when
/// the call into the queue fails, or the delegate for it cannot be made.
struct throwing_dispatcher {
    template <class Fn>
    bool schedule(Fn&&) const {
        throw std::runtime_error("the dispatcher failed to take the call");
    }
};

void complete_into_a_throwing_dispatcher() {
    without_a_dialog();
    fake_operation<int> op;
    int converted = 0;
    auto const pending =
        start_winrt_on_dispatcher<std::string>(op, throwing_dispatcher{}, counted(converted));
    std::thread{[&op] { op.complete(7); }}.join();
}

// The completion runs on a thread of the runtime's, inside the projection's delegate, which would
// turn the exception into an HRESULT for the platform and lose the turn: it stops the process
// there instead, and the report names the exception.
TEST(OperationTaskDeathTest, AQueueThatThrowsInTheCompletionStopsTheProcessNamingIt) {
    EXPECT_DEATH(complete_into_a_throwing_dispatcher(),
                 "wxl: unhandled exception: the dispatcher failed to take the call");
}

}  // namespace

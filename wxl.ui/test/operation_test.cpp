// The task a WinRT asynchronous operation becomes (impl/operation.h): what it answers, where, and
// what letting it go does.
//
// No dispatcher and no runtime. The operation is a fake one with the three calls the producer
// makes -- Completed, GetResults, Cancel -- and the test ends it, on this thread or another. The
// queue is a fake one that keeps what is posted to it until the test runs it, on this thread,
// which stands for the thread that made the call. Whether the producer is gone is read off the
// operation: the producer holds a reference to it until it is deleted.
#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <exception>
#include <format>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include <winrt/Windows.Foundation.h>

#include "impl/operation.h"

namespace {

using winrt::Windows::Foundation::AsyncStatus;
using wxl::async::operation_canceled_exception;
using wxl::async::task;
using wxl::impl::operation_detail::start;

/// What the platform throws for an operation that failed: the code, as winrt::hresult_error
/// carries it.
struct platform_failure {
    uint32_t code;
};

/// The code the platform reports for an operation that was cancelled, if its results are asked.
constexpr uint32_t canceled_code = 0x800704C7;

/// An operation of the platform: a reference to one object, which its copies share.
template <class T>
class fake_operation {
public:
    using handler_t = std::function<void(fake_operation const&, AsyncStatus)>;

    /// The handler, as the runtime keeps it: called once, when the operation ends -- at once, on
    /// the thread giving it, for an operation that has ended already.
    void Completed(handler_t handler) const {
        if (state_->status == AsyncStatus::Started) {
            state_->handler = std::move(handler);
            return;
        }
        handler(*this, state_->status);
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
            return *state_->result;
    }

    void Cancel() const { ++state_->cancels; }

    /// The operation ends: the handler is called on whichever thread calls this.
    void end(AsyncStatus status) const {
        state_->status = status;
        if (handler_t const handler = std::exchange(state_->handler, {})) handler(*this, status);
    }

    template <class... Value>
    void complete(Value... value) const {
        if constexpr (sizeof...(Value) != 0) state_->result.emplace(value...);
        end(AsyncStatus::Completed);
    }

    void fail(uint32_t code) const {
        state_->code = code;
        end(AsyncStatus::Error);
    }

    int cancels() const { return state_->cancels; }
    int reads() const { return state_->reads; }

    /// References to the operation: the test's own, and the producer's while it lives.
    long held() const { return state_.use_count(); }

private:
    struct state {
        AsyncStatus status = AsyncStatus::Started;
        handler_t handler;
        std::optional<std::conditional_t<std::is_void_v<T>, int, T>> result;
        uint32_t code = 0;
        int cancels = 0;
        int reads = 0;
    };

    std::shared_ptr<state> state_ = std::make_shared<state>();
};

/// The queue of the thread that made the call: posted to from any thread, run by the test on its
/// own.
class fake_queue {
public:
    template <class Fn>
    bool TryEnqueue(Fn&& fn) const {
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
    struct state {
        std::mutex mutex;
        std::vector<std::function<void()>> posted;
    };

    std::shared_ptr<state> state_ = std::make_shared<state>();
};

/// What `convert` is for a generated wrapper: the projection's value into wxl's. Counts its calls.
auto counted(int& converted) {
    return [&converted](int value) {
        ++converted;
        return std::to_string(value);
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
    fake_queue queue;
    int converted = 0;
    log_t log;

    {
        auto const waiting = awaits(start<std::string>(op, queue, counted(converted)), log);
        EXPECT_FALSE(waiting.done());

        // The completion only posts: nothing is read or resumed on its thread.
        op.complete(7);
        EXPECT_EQ(queue.posted(), 1u);
        EXPECT_EQ(converted, 0);
        EXPECT_TRUE(log.empty());

        EXPECT_EQ(queue.run(), 1u);
        EXPECT_TRUE(waiting.done());
        EXPECT_EQ(log, (log_t{"value 7"}));
        EXPECT_EQ(converted, 1);
    }

    EXPECT_EQ(op.cancels(), 0);
    EXPECT_EQ(op.held(), 1);
}

TEST(OperationTaskTest, AnActionAnswersWithNothing) {
    fake_operation<void> op;
    fake_queue queue;
    log_t log;

    {
        auto const waiting = awaits(start<void>(op, queue, std::identity{}), log);
        op.complete();
        queue.run();
        EXPECT_TRUE(waiting.done());
    }

    EXPECT_EQ(log, (log_t{"done"}));
    EXPECT_EQ(op.reads(), 1);
    EXPECT_EQ(op.held(), 1);
}

TEST(OperationTaskTest, AFailedOperationAnswersWhatGetResultsThrows) {
    fake_operation<int> op;
    fake_queue queue;
    int converted = 0;
    log_t log;

    {
        auto const waiting = awaits(start<std::string>(op, queue, counted(converted)), log);
        op.fail(0x80070005);
        queue.run();
        EXPECT_TRUE(waiting.done());
    }

    EXPECT_EQ(log, (log_t{"failed 0x80070005"}));
    EXPECT_EQ(converted, 0);
    EXPECT_EQ(op.held(), 1);
}

TEST(OperationTaskTest, AFailedActionAnswersWhatGetResultsThrows) {
    fake_operation<void> op;
    fake_queue queue;
    log_t log;

    {
        auto const waiting = awaits(start<void>(op, queue, std::identity{}), log);
        op.fail(0x80004005);
        queue.run();
        EXPECT_TRUE(waiting.done());
    }

    EXPECT_EQ(log, (log_t{"failed 0x80004005"}));
}

TEST(OperationTaskTest, ACancelledOperationAnswersOperationCanceledException) {
    fake_operation<int> op;
    fake_queue queue;
    int converted = 0;
    log_t log;

    {
        auto const waiting = awaits(start<std::string>(op, queue, counted(converted)), log);
        op.end(AsyncStatus::Canceled);
        queue.run();
        EXPECT_TRUE(waiting.done());
    }

    // Answered as every cancellation in wxl is, without asking the platform for its failure.
    EXPECT_EQ(log, (log_t{"cancelled"}));
    EXPECT_EQ(op.reads(), 0);
    EXPECT_EQ(converted, 0);
}

TEST(OperationTaskTest, ACancelledActionAnswersOperationCanceledException) {
    fake_operation<void> op;
    fake_queue queue;
    log_t log;

    {
        auto const waiting = awaits(start<void>(op, queue, std::identity{}), log);
        op.end(AsyncStatus::Canceled);
        queue.run();
    }

    EXPECT_EQ(log, (log_t{"cancelled"}));
}

TEST(OperationTaskTest, WhatConvertThrowsIsTheAnswer) {
    fake_operation<int> op;
    fake_queue queue;
    log_t log;

    {
        auto const throws = [](int) -> std::string { throw platform_failure{1}; };
        auto const waiting = awaits(start<std::string>(op, queue, throws), log);
        op.complete(7);
        queue.run();
    }

    EXPECT_EQ(log, (log_t{"failed 0x1"}));
}

TEST(OperationTaskTest, AnAnswerHereBeforeTheCoAwaitIsTakenWithoutSuspending) {
    fake_operation<int> op;
    fake_queue queue;
    int converted = 0;
    log_t log;

    auto pending = start<std::string>(op, queue, counted(converted));
    op.complete(7);
    queue.run();
    EXPECT_TRUE(pending.done());

    // Nothing is left to resume the coroutine: one that suspended would not end.
    auto const waiting = awaits(std::move(pending), log);
    EXPECT_TRUE(waiting.done());
    EXPECT_EQ(log, (log_t{"value 7"}));
    EXPECT_EQ(queue.posted(), 0u);
}

TEST(OperationTaskTest, AnOperationOverAlreadyIsAnsweredThroughTheQueueAsWell) {
    fake_operation<int> op;
    fake_queue queue;
    int converted = 0;

    op.complete(7);

    auto pending = start<std::string>(op, queue, counted(converted));
    EXPECT_FALSE(pending.done());
    EXPECT_EQ(queue.posted(), 1u);

    queue.run();
    EXPECT_TRUE(pending.done());
    EXPECT_EQ(pending.result(), "7");
}

TEST(OperationTaskTest, LettingTheTaskGoCancelsTheOperationAndItsCompletionDeletesTheProducer) {
    fake_operation<int> op;
    fake_queue queue;
    int converted = 0;
    log_t log;

    {
        auto const waiting = awaits(start<std::string>(op, queue, counted(converted)), log);
        EXPECT_EQ(op.cancels(), 0);
    }  // the coroutine's frame goes, and the task it awaits with it

    EXPECT_EQ(op.cancels(), 1);
    EXPECT_EQ(op.held(), 2) << "given up, the producer waits for the completion";

    op.end(AsyncStatus::Canceled);
    EXPECT_EQ(queue.run(), 1u);

    // Deleted, and nobody resumed: the frame that waited is gone.
    EXPECT_EQ(op.held(), 1);
    EXPECT_TRUE(log.empty());
    EXPECT_EQ(converted, 0);
}

TEST(OperationTaskTest, AGivenUpOperationThatCompletedAnywayIsNotRead) {
    fake_operation<int> op;
    fake_queue queue;
    int converted = 0;
    log_t log;

    {
        auto const waiting = awaits(start<std::string>(op, queue, counted(converted)), log);
    }

    // Finished before the request to stop reached it: the result is nobody's.
    op.complete(7);
    queue.run();

    EXPECT_EQ(op.held(), 1);
    EXPECT_EQ(op.reads(), 0);
    EXPECT_EQ(converted, 0);
    EXPECT_TRUE(log.empty());
}

TEST(OperationTaskTest, ATaskLetGoWhileItsAnswerIsPostedIsDeletedByThePostedCall) {
    fake_operation<int> op;
    fake_queue queue;
    int converted = 0;

    auto pending = start<std::string>(op, queue, counted(converted));
    op.complete(7);

    // Not ready until the queue's turn: the task asks the operation to stop, which costs nothing.
    { auto const gone = std::move(pending); }
    EXPECT_EQ(op.cancels(), 1);
    EXPECT_EQ(op.held(), 2);

    queue.run();
    EXPECT_EQ(op.held(), 1);
    EXPECT_EQ(converted, 0);
}

TEST(OperationTaskTest, ATaskLetGoAfterItsAnswerCancelsNothing) {
    fake_operation<int> op;
    fake_queue queue;
    int converted = 0;

    {
        auto const pending = start<std::string>(op, queue, counted(converted));
        op.complete(7);
        queue.run();
        EXPECT_TRUE(pending.done());
    }

    EXPECT_EQ(op.cancels(), 0);
    EXPECT_EQ(op.held(), 1);
}

TEST(OperationTaskTest, TheCompletionOnAnotherThreadOnlyPosts) {
    fake_operation<int> op;
    fake_queue queue;
    int converted = 0;
    log_t log;

    {
        auto const waiting = awaits(start<std::string>(op, queue, counted(converted)), log);

        std::thread([op] { op.complete(9); }).join();
        EXPECT_FALSE(waiting.done());
        EXPECT_EQ(converted, 0);

        queue.run();
        EXPECT_TRUE(waiting.done());
    }

    EXPECT_EQ(log, (log_t{"value 9"}));
    EXPECT_EQ(op.held(), 1);
}

TEST(OperationTaskTest, ATaskMoves) {
    fake_operation<int> op;
    fake_queue queue;
    int converted = 0;

    {
        auto first = start<std::string>(op, queue, counted(converted));
        auto second = std::move(first);
        task<std::string> third{std::move(second)};
        first = std::move(third);

        op.complete(7);
        queue.run();
        EXPECT_TRUE(first.done());
        EXPECT_EQ(first.result(), "7");
    }

    // The moved-from tasks hold nothing: no request to stop, one producer, deleted once.
    EXPECT_EQ(op.cancels(), 0);
    EXPECT_EQ(op.held(), 1);
}

}  // namespace

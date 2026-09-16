#include <gtest/gtest.h>

import std;
import wxl.core;
import wxl.async;


using namespace wxl::core;
using namespace wxl::async;

TEST(TaskTreeTest, RootSucceedsOnlyAfterAllSubtasksSucceed) {
    task_tree_handle root = task_tree::create();
    future<void> cancellation_token = root.root().cancellation_token();
    future<void> root_future = root.get_future();

    EXPECT_FALSE(cancellation_token.is_ready());
    EXPECT_FALSE(root_future.is_ready());

    task_tree_handle sub1 = root.create_subtask();
    task_tree_handle sub2 = root.create_subtask();

    root.set_succeeded();
    EXPECT_FALSE(root_future.is_ready());

    sub1.set_succeeded();
    EXPECT_FALSE(root_future.is_ready());

    sub2.set_succeeded();
    ASSERT_TRUE(root_future.is_ready());
    EXPECT_FALSE(root_future.has_exception());
    ASSERT_TRUE(cancellation_token.is_ready());
    EXPECT_FALSE(cancellation_token.has_exception());
}

TEST(TaskTreeTest, SubtaskFailurePropagatesToCancellationTokenImmediately) {
    task_tree_handle root = task_tree::create();
    future<void> cancellation_token = root.root().cancellation_token();

    task_tree_handle sub1 = root.create_subtask();
    task_tree_handle sub2 = root.create_subtask();
    root.set_succeeded();

    sub1.set_failed(std::make_exception_ptr(std::runtime_error("boom")));
    ASSERT_TRUE(cancellation_token.is_ready());
    EXPECT_TRUE(cancellation_token.has_exception());

    // Sibling can still resolve normally without crashing anything.
    sub2.set_succeeded();
}

TEST(TaskTreeTest, UnresolvedHandleAutoFailsOnDestruction) {
    task_tree_handle root = task_tree::create();
    future<void> root_future = root.get_future();

    {
        task_tree_handle child = root.create_subtask();
        // child goes out of scope unresolved here.
    }

    root.set_succeeded();

    ASSERT_TRUE(root_future.is_ready());
    EXPECT_TRUE(root_future.has_exception());
}

TEST(TaskTreeTest, CreateSubtaskOnFinishedParentThrows) {
    task_tree_handle root = task_tree::create();
    root.set_succeeded();

    EXPECT_THROW(root.create_subtask(), std::logic_error);
}

TEST(TaskTreeTest, SetSucceededTwiceThrows) {
    task_tree_handle root = task_tree::create();
    root.set_succeeded();

    EXPECT_THROW(root.set_succeeded(), std::logic_error);
}

// All subtasks must be created before the parent is marked succeeded (that's the API
// contract -- the shape of the task tree is established first); once that's done,
// resolving them concurrently is safe and must resolve the root exactly once.
TEST(TaskTreeTest, ConcurrentlyResolvedSubtasksResolveRootExactlyOnce) {
    task_tree_handle root = task_tree::create();
    future<void> root_future = root.get_future();

    constexpr int n = 200;
    std::vector<task_tree_handle> subs;
    subs.reserve(n);
    for (int i = 0; i < n; ++i) subs.push_back(root.create_subtask());

    root.set_succeeded();

    std::vector<std::thread> threads;
    for (int i = 0; i < n; ++i) {
        threads.emplace_back([&subs, i] { subs[i].set_succeeded(); });
    }

    for (auto& t : threads) t.join();

    ASSERT_TRUE(root_future.is_ready());
    EXPECT_FALSE(root_future.has_exception());
}

// task_tree::create(future) / create_subtask(future): the future is what finishes the task, so
// no handle comes back and none has to be kept by the caller.

TEST(TaskTreeTest, CreatedFromFutureSucceedsWithIt) {
    promise<void> p;
    task_tree set = task_tree::create(p.get_future());
    future<void> set_future = set.get_future();

    EXPECT_FALSE(set_future.is_ready());

    p.set_value();

    ASSERT_TRUE(set_future.is_ready());
    EXPECT_FALSE(set_future.has_exception());
}

TEST(TaskTreeTest, CreatedFromFutureFailsWithIt) {
    promise<void> p;
    task_tree set = task_tree::create(p.get_future());
    future<void> set_future = set.get_future();

    p.set_exception(std::make_exception_ptr(std::runtime_error("boom")));

    ASSERT_TRUE(set_future.is_ready());
    EXPECT_TRUE(set_future.has_exception());
}

TEST(TaskTreeTest, CreatedFromReadyFutureIsFinishedAtOnce) {
    ASSERT_TRUE(task_tree::create(make_ready_future()).finished());
}

TEST(TaskTreeTest, SubtaskFromFutureHoldsTheParentUntilItResolves) {
    promise<void> p;
    task_tree_handle root = task_tree::create();
    future<void> root_future = root.get_future();

    task_tree sub = root.create_subtask(p.get_future());
    root.set_succeeded();

    EXPECT_FALSE(root_future.is_ready());
    EXPECT_FALSE(sub.finished());

    p.set_value();

    ASSERT_TRUE(root_future.is_ready());
    EXPECT_FALSE(root_future.has_exception());
    EXPECT_TRUE(sub.succeeded());
}

TEST(TaskTreeTest, SubtaskFromFailedFutureFailsTheWholeSet) {
    promise<void> p;
    task_tree_handle root = task_tree::create();
    future<void> cancellation_token = root.root().cancellation_token();
    future<void> root_future = root.get_future();

    task_tree sub = root.create_subtask(p.get_future());
    root.set_succeeded();

    p.set_exception(std::make_exception_ptr(std::runtime_error("boom")));

    EXPECT_TRUE(cancellation_token.is_ready());
    EXPECT_TRUE(cancellation_token.has_exception());
    ASSERT_TRUE(root_future.is_ready());
    EXPECT_TRUE(root_future.has_exception());
    EXPECT_TRUE(sub.failed());
}

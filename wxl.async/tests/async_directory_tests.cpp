#include <gtest/gtest.h>

#include "sta_pool.h"

import std;
import wxl.core;
import wxl.async;

using namespace wxl::core;
using namespace wxl::async;

namespace {

/// Walks a directory in a coroutine and keeps what it found, sorted.
managed_task list_all(path pattern, std::vector<std::wstring>& out) {
    async_directory listing = co_await async_directory::open(pattern);

    while (const std::optional<async_directory::entry> found = co_await listing.next())
        out.emplace_back(found->name);

    co_await listing.close();

    std::ranges::sort(out);
}

/// Makes a chain of directories, checks it is there, and takes it apart again --
/// the whole of the static half in one coroutine.
managed_task make_check_remove(path deep, bool& existed_after, bool& exists_at_the_end) {
    co_await async_directory::create_all(deep);

    existed_after = co_await async_directory::exists(deep);

    co_await async_directory::remove(deep);

    exists_at_the_end = co_await async_directory::exists(deep);
}

managed_task open_and_catch(path pattern, std::string& message) {
    try {
        co_await async_directory::open(pattern);
        message = "no exception";
    } catch (const system_exception& e) {
        message = e.what();
    }
}

managed_task remove_and_catch(path directory, std::string& message) {
    try {
        co_await async_directory::remove(directory);
        message = "no exception";
    } catch (const system_exception& e) {
        message = e.what();
    }
}

class AsyncDirectoryTest : public ::testing::Test
{
protected:
    void SetUp() override {
        root_ =
            path(std::filesystem::temp_directory_path().wstring()) / L"wxl_async_directory_tests";

        std::filesystem::remove_all(root_.native());

        ASSERT_TRUE(directory::create(root_.c_str()));
    }

    void TearDown() override {
        std::filesystem::remove_all(root_.native());
    }

    void given_a_file(const wchar_t* name) {
        file out = file::create((root_ / name).c_str());

        ASSERT_TRUE(out.opened());
    }

    /// Runs a coroutine to its end on the loop and lets whatever left it out.
    void run(managed_task work) {
        sta_loop::run_until([&] { return work.done(); });

        work.result();
    }

    path root_;
};

}  // namespace

TEST_F(AsyncDirectoryTest, ListsADirectoryInACoroutine) {
    given_a_file(L"one.fb3");
    given_a_file(L"two.fb3");
    given_a_file(L"notes.txt");

    std::vector<std::wstring> found;

    run(list_all(root_ / L"*", found));

    EXPECT_EQ(found, (std::vector<std::wstring>{L"notes.txt", L"one.fb3", L"two.fb3"}));
}

TEST_F(AsyncDirectoryTest, TheMaskGoesToTheSystem) {
    given_a_file(L"one.fb3");
    given_a_file(L"two.fb3");
    given_a_file(L"notes.txt");

    std::vector<std::wstring> found;

    run(list_all(root_ / L"*.fb3", found));

    EXPECT_EQ(found, (std::vector<std::wstring>{L"one.fb3", L"two.fb3"}));
}

TEST_F(AsyncDirectoryTest, AnEmptyDirectoryEndsTheLoopAtOnce) {
    std::vector<std::wstring> found;

    run(list_all(root_ / L"*", found));

    EXPECT_TRUE(found.empty());
}

TEST_F(AsyncDirectoryTest, ListsMoreNamesThanOneTripCanCarry) {
    // Enough of them that the system fills its buffer more than once, so the
    // coroutine goes round the loop many times and every crossing has to be
    // right -- including the one where the listing quietly ends.
    for (int i = 0; i < 200; ++i)
        given_a_file((L"book-" + std::to_wstring(i) + L".fb3").c_str());

    std::vector<std::wstring> found;

    run(list_all(root_ / L"*", found));

    EXPECT_EQ(found.size(), 200u);
    EXPECT_EQ(found.front(), L"book-0.fb3");
}

TEST_F(AsyncDirectoryTest, MakesAChainChecksItAndRemovesIt) {
    bool existed_after = false;
    bool exists_at_the_end = true;

    run(make_check_remove(root_ / L"one" / L"two" / L"three", existed_after,
                          exists_at_the_end));

    EXPECT_TRUE(existed_after);
    EXPECT_FALSE(exists_at_the_end);

    // Only the last level was removed; its parents are still standing.
    EXPECT_TRUE(directory::exists((root_ / L"one" / L"two").c_str()));
}

TEST_F(AsyncDirectoryTest, OpeningWhatIsNotThereThrowsAtTheCoAwait) {
    std::string message;

    run(open_and_catch(root_ / L"no_such_directory" / L"*", message));

    EXPECT_TRUE(message.starts_with("FindFirstFileExW")) << message;
}

TEST_F(AsyncDirectoryTest, RemovingWhatIsNotThereThrowsAtTheCoAwait) {
    std::string message;

    run(remove_and_catch(root_ / L"no_such_directory", message));

    EXPECT_TRUE(message.starts_with("RemoveDirectoryW")) << message;
}

TEST_F(AsyncDirectoryTest, APathGivenToAnOperationNeedNotOutliveTheStatement) {
    // The operation keeps its own copy, so a path that dies with the statement
    // that started the coroutine is still safe. This is the shape that was
    // broken while the path was merely borrowed -- the worker read it after the
    // temporary was gone -- and it is here to stay broken no longer.
    given_a_file(L"one.fb3");

    std::vector<std::wstring> found;

    managed_task work = list_all(path(root_.native()) / L"*", found);

    run(std::move(work));

    EXPECT_EQ(found, (std::vector<std::wstring>{L"one.fb3"}));
}

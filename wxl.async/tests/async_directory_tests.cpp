#include <gtest/gtest.h>

#include "platform.h"

#include "loop_environment.h"
#include "test_directory.h"

import std;
import wxl.core;
import wxl.async;

using namespace wxl::core;
using namespace wxl::async;

namespace {

/// Walks a directory in a coroutine and keeps what it found, sorted.
task<> list_all(path pattern, std::vector<std::wstring>& out) {
    async_directory listing = co_await async_directory::open(pattern);

    while (const std::optional<async_directory::entry> found = co_await listing.next())
        out.emplace_back(found->name);

    co_await listing.close();

    std::ranges::sort(out);
}

/// Makes a chain of directories, checks it is there, and takes it apart again --
/// the whole of the static half in one coroutine.
task<> make_check_remove(path deep, bool& existed_after, bool& exists_at_the_end) {
    co_await async_directory::create_all(deep);

    existed_after = co_await async_directory::exists(deep);

    co_await async_directory::remove(deep);

    exists_at_the_end = co_await async_directory::exists(deep);
}

task<> open_and_catch(path pattern, std::string& message) {
    try {
        co_await async_directory::open(pattern);
        message = "no exception";
    } catch (const last_error& e) {
        message = e.what();
    }
}

task<> remove_and_catch(path directory, std::string& message) {
    try {
        co_await async_directory::remove(directory);
        message = "no exception";
    } catch (const last_error& e) {
        message = e.what();
    }
}

class AsyncDirectoryTest : public ::testing::Test
{
protected:
    void SetUp() override {
        root_ = test_directory();

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

    path root_;
};

}  // namespace

TEST_F(AsyncDirectoryTest, ListsADirectoryInACoroutine) {
    given_a_file(L"one.fb3");
    given_a_file(L"two.fb3");
    given_a_file(L"notes.txt");

    std::vector<std::wstring> found;

    sta_loop::run_until(list_all(root_ / L"*", found));

    EXPECT_EQ(found, (std::vector<std::wstring>{L"notes.txt", L"one.fb3", L"two.fb3"}));
}

TEST_F(AsyncDirectoryTest, TheMaskGoesToTheSystem) {
    given_a_file(L"one.fb3");
    given_a_file(L"two.fb3");
    given_a_file(L"notes.txt");

    std::vector<std::wstring> found;

    sta_loop::run_until(list_all(root_ / L"*.fb3", found));

    EXPECT_EQ(found, (std::vector<std::wstring>{L"one.fb3", L"two.fb3"}));
}

TEST_F(AsyncDirectoryTest, AnEmptyDirectoryEndsTheLoopAtOnce) {
    std::vector<std::wstring> found;

    sta_loop::run_until(list_all(root_ / L"*", found));

    EXPECT_TRUE(found.empty());
}

TEST_F(AsyncDirectoryTest, ListsMoreNamesThanOneTripCanCarry) {
    // Enough of them that the system fills its buffer more than once, so the
    // coroutine goes round the loop many times and every crossing has to be
    // right -- including the one where the listing quietly ends.
    for (int i = 0; i < 200; ++i)
        given_a_file((L"book-" + std::to_wstring(i) + L".fb3").c_str());

    std::vector<std::wstring> found;

    sta_loop::run_until(list_all(root_ / L"*", found));

    EXPECT_EQ(found.size(), 200u);
    EXPECT_EQ(found.front(), L"book-0.fb3");
}

TEST_F(AsyncDirectoryTest, MakesAChainChecksItAndRemovesIt) {
    bool existed_after = false;
    bool exists_at_the_end = true;

    sta_loop::run_until(
        make_check_remove(root_ / L"one" / L"two" / L"three", existed_after, exists_at_the_end));

    EXPECT_TRUE(existed_after);
    EXPECT_FALSE(exists_at_the_end);

    // Only the last level was removed; its parents are still standing.
    EXPECT_TRUE(directory::exists((root_ / L"one" / L"two").c_str()));
}

TEST_F(AsyncDirectoryTest, OpeningWhatIsNotThereThrowsAtTheCoAwait) {
    std::string message;

    sta_loop::run_until(open_and_catch(root_ / L"no_such_directory" / L"*", message));

    EXPECT_TRUE(message.starts_with("FindFirstFileExW")) << message;
}

TEST_F(AsyncDirectoryTest, RemovingWhatIsNotThereThrowsAtTheCoAwait) {
    std::string message;

    sta_loop::run_until(remove_and_catch(root_ / L"no_such_directory", message));

    EXPECT_TRUE(message.starts_with("RemoveDirectoryW")) << message;
}

TEST_F(AsyncDirectoryTest, APathGivenToAnOperationNeedNotOutliveTheStatement) {
    // The operation keeps its own copy, so a path that dies with the statement
    // that started the coroutine is still safe. This is the shape that was
    // broken while the path was merely borrowed -- the worker read it after the
    // temporary was gone -- and it is here to stay broken no longer.
    given_a_file(L"one.fb3");

    std::vector<std::wstring> found;

    task<> work = list_all(path(root_.native()) / L"*", found);

    sta_loop::run_until(work);

    EXPECT_EQ(found, (std::vector<std::wstring>{L"one.fb3"}));
}

// ---- The listing at once: list ----------------------------------------------

namespace {

task<> lists_at_once(path pattern, std::vector<async_directory::listed_entry>& out) {
    out = co_await async_directory::list(pattern);

    std::ranges::sort(out, {}, &async_directory::listed_entry::name);
}

task<> cancelable_lists_at_once(path pattern, cancellation_token token,
                           std::vector<async_directory::listed_entry>& out) {
    out = co_await async_directory::list(pattern, token);

    std::ranges::sort(out, {}, &async_directory::listed_entry::name);
}

task<> lists_and_catches(path pattern, int& code) {
    try {
        co_await async_directory::list(pattern);
        code = 0;
    } catch (const last_error& failure) {
        code = failure.code().value();
    }
}

}  // namespace

TEST_F(AsyncDirectoryTest, ListBringsEveryMatchAtOnce) {
    given_a_file(L"one.fb3");
    given_a_file(L"two.fb3");
    given_a_file(L"notes.txt");

    // A directory whose name matches comes out too, and says what it is.
    ASSERT_TRUE(directory::create((root_ / L"inner.fb3").c_str()));

    std::vector<async_directory::listed_entry> found;

    sta_loop::run_until(lists_at_once(root_ / L"*.fb3", found));

    ASSERT_EQ(found.size(), 3u);
    EXPECT_EQ(found[0].name, L"inner.fb3");
    EXPECT_TRUE(found[0].is_directory);
    EXPECT_EQ(found[1].name, L"one.fb3");
    EXPECT_FALSE(found[1].is_directory);
    EXPECT_EQ(found[1].size, 0u);
    EXPECT_EQ(found[2].name, L"two.fb3");
}

TEST_F(AsyncDirectoryTest, ListBringsWhenEachWasLastWritten) {
    given_a_file(L"one.fb3");

    const std::filesystem::file_time_type written =
        std::filesystem::last_write_time((root_ / L"one.fb3").native());

    std::vector<async_directory::listed_entry> found;

    sta_loop::run_until(lists_at_once(root_ / L"*.fb3", found));

    ASSERT_EQ(found.size(), 1u);
    EXPECT_EQ(found[0].last_write.time_since_epoch().count(), written.time_since_epoch().count());
}

TEST_F(AsyncDirectoryTest, ListUnderATokenBringsWhatThePlainOneDoesUntilAsked) {
    given_a_file(L"one.fb3");
    given_a_file(L"two.fb3");

    cancellation_source source;
    std::vector<async_directory::listed_entry> found;

    sta_loop::run_until(cancelable_lists_at_once(root_ / L"*.fb3", source.token(), found));

    ASSERT_EQ(found.size(), 2u);
    EXPECT_EQ(found[0].name, L"one.fb3");
    EXPECT_EQ(found[1].name, L"two.fb3");

    source.cancel();

    std::vector<async_directory::listed_entry> again;
    task<> asked = cancelable_lists_at_once(root_ / L"*.fb3", source.token(), again);

    EXPECT_TRUE(asked.done()) << "an operation was started under a cancelled token";
    EXPECT_THROW(asked.result(), operation_canceled_exception);
    EXPECT_TRUE(again.empty());
}

TEST_F(AsyncDirectoryTest, ListOfNoMatchIsEmptyAndNotAFailure) {
    given_a_file(L"notes.txt");

    std::vector<async_directory::listed_entry> found{{L"stale"}};

    sta_loop::run_until(lists_at_once(root_ / L"*.fb3", found));

    EXPECT_TRUE(found.empty());
}

TEST_F(AsyncDirectoryTest, ListOfAMissingDirectoryNamesTheSystemsCode) {
    int code = -1;

    sta_loop::run_until(lists_and_catches(root_ / L"nowhere" / L"*", code));

    EXPECT_EQ(code, ERROR_PATH_NOT_FOUND);
}

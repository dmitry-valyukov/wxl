// The load of a window's background on the way (impl/background_load.h): each under a source of
// its own made under its owner's token, cancelled by the next, remembered until the background is
// set another way -- to be asked for again after a replacement of the device -- and the task
// that waits for it, kept with the load: whose body is under way, which the next load or a
// change of the background stops, which does not show what a later change has replaced, which
// ends with a failure of the load, and which a lost device ends quietly.
//
// No window and no Win2D. The load is a task whose coroutine waits at a gate the test opens; what
// is shown is written down. That a real load stands under its token and is cut short by it is
// the operation's own, and operation_test.cpp has it.
#include <crtdbg.h>

#include <gtest/gtest.h>

#include <coroutine>
#include <exception>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "impl/background_load.h"

namespace {

using wxl::DrawingSurface;
using wxl::async::cancellation_source;
using wxl::async::cancellation_token;
using wxl::async::live_frames;
using wxl::async::operation_canceled_exception;
using wxl::async::task;
using loads_t = wxl::impl::background_loads<std::string>;
using wxl::impl::show_when_loaded;

/// Where a load waits until the test lets it on. The wait takes itself off the gate if its frame
/// goes first -- a load stopped with the wait that held it -- so that opening finds nobody.
class gate {
public:
    struct waiter {
        gate& at;

        bool await_ready() const noexcept { return false; }
        void await_suspend(std::coroutine_handle<> waiting) noexcept { at.waiting_ = waiting; }
        void await_resume() const noexcept {}

        ~waiter() { at.waiting_ = {}; }
    };

    waiter operator co_await() noexcept { return waiter{*this}; }

    /// Whether a load stands at the gate.
    bool waited() const noexcept { return static_cast<bool>(waiting_); }

    void open() {
        if (waiting_) std::exchange(waiting_, {}).resume();
    }

private:
    std::coroutine_handle<> waiting_;
};

/// A picture loading: done when the gate opens.
task<std::string> loads(gate& at, std::string picture) {
    co_await at;
    co_return picture;
}

/// Throws, and to the compiler "gives" a T: the body of a coroutine with a value has to end in
/// co_return, and MSVC does not count a throw as its end (C4033).
template <class T, class Failure>
[[noreturn]] T fail_with(Failure failure) {
    throw failure;
}

/// A picture that is not there: the load fails when the gate opens.
task<std::string> fails(gate& at) {
    co_await at;
    co_return fail_with<std::string>(std::runtime_error("no such file"));
}

/// A load the lost device cuts short when the gate opens, as TextureCache::getAsync ends one.
task<std::string> loses_the_device(gate& at) {
    co_await at;
    co_return fail_with<std::string>(DrawingSurface::DeviceLost{});
}

using shown_t = std::vector<std::string>;

auto shows(shown_t& shown) {
    return [&shown](std::string const& picture) { shown.push_back(picture); };
}

/// What result() threw, or nothing.
std::string end_of(task<>& wait) {
    try {
        wait.result();
    } catch (operation_canceled_exception const&) {
        return "cancelled";
    } catch (std::runtime_error const& failure) {
        return failure.what();
    }
    return {};
}

// The wait is a body under way -- its own frame and the load's, which a build that checks
// counts -- until the picture is shown.
TEST(BackgroundLoadTest, TheWaitIsABodyUnderWayUntilItShowsWhatWasLoaded) {
    cancellation_source application;
    loads_t loads_of_window;
    gate at;
    shown_t shown;
    [[maybe_unused]] std::size_t before = 0;
    if constexpr (wxl::async::detail::checked) before = live_frames();

    cancellation_token const load = loads_of_window.next(application.token(), "cover");
    loads_of_window.hold(load, show_when_loaded(loads(at, "cover"), load, shows(shown)));
    if constexpr (wxl::async::detail::checked) {
        EXPECT_EQ(live_frames(), before + 2) << "the call has returned, and the wait is under way";
    }
    EXPECT_TRUE(shown.empty());

    at.open();
    EXPECT_EQ(shown, shown_t{"cover"});
    if constexpr (wxl::async::detail::checked) {
        EXPECT_EQ(live_frames(), before);
    }
}

// A background set while the picture loads keeps the last word: the load under way is cancelled
// and its wait stopped, and nothing is shown.
TEST(BackgroundLoadTest, AnotherChangeCancelsTheLoadAndStopsItsWait) {
    cancellation_source application;
    loads_t loads_of_window;
    gate at;
    shown_t shown;
    [[maybe_unused]] std::size_t before = 0;
    if constexpr (wxl::async::detail::checked) before = live_frames();

    cancellation_token const load = loads_of_window.next(application.token(), "cover");
    loads_of_window.hold(load, show_when_loaded(loads(at, "cover"), load, shows(shown)));

    loads_of_window.cancel();
    EXPECT_TRUE(load.is_canceled());
    EXPECT_FALSE(application.is_canceled()) << "only the load's own source";
    if constexpr (wxl::async::detail::checked) {
        EXPECT_EQ(live_frames(), before) << "the wait was not stopped with its load";
    }
    EXPECT_FALSE(at.waited());

    at.open();
    EXPECT_TRUE(shown.empty());
}

// A load whose answer comes anyway after its cancellation -- it reached the queue first -- is not
// shown by a wait that sees the cancellation.
TEST(BackgroundLoadTest, ALateAnswerAfterTheCancellationIsNotShown) {
    cancellation_source application;
    loads_t loads_of_window;
    gate at;
    shown_t shown;

    cancellation_token const load = loads_of_window.next(application.token(), "cover");
    task<> wait = show_when_loaded(loads(at, "cover"), load, shows(shown));

    loads_of_window.cancel();
    at.open();

    EXPECT_TRUE(shown.empty());
    EXPECT_TRUE(wait.done());
}

// The next load cancels the one before it, and stops its wait; the later picture is the one shown.
TEST(BackgroundLoadTest, TheNextLoadCancelsTheOneBeforeIt) {
    cancellation_source application;
    loads_t loads_of_window;
    gate first_at;
    gate second_at;
    shown_t shown;
    [[maybe_unused]] std::size_t before = 0;
    if constexpr (wxl::async::detail::checked) before = live_frames();

    cancellation_token const first = loads_of_window.next(application.token(), "first");
    loads_of_window.hold(first, show_when_loaded(loads(first_at, "first"), first, shows(shown)));
    cancellation_token const second = loads_of_window.next(application.token(), "second");
    loads_of_window.hold(second, show_when_loaded(loads(second_at, "second"), second, shows(shown)));
    EXPECT_TRUE(first.is_canceled());
    EXPECT_FALSE(second.is_canceled());
    EXPECT_FALSE(first_at.waited()) << "the wait of the first load was not stopped";

    second_at.open();
    first_at.open();
    EXPECT_EQ(shown, shown_t{"second"});
    if constexpr (wxl::async::detail::checked) {
        EXPECT_EQ(live_frames(), before);
    }
}

// The owner's token is the parent of every load: cancelled on the way out, it cancels the one
// under way, whose wait ends without showing it, and a load asked under it afterwards is cancelled
// from birth.
TEST(BackgroundLoadTest, TheOwnersTokenCancelsTheLoad) {
    cancellation_source application;
    loads_t loads_of_window;
    gate at;
    shown_t shown;
    [[maybe_unused]] std::size_t before = 0;
    if constexpr (wxl::async::detail::checked) before = live_frames();

    cancellation_token const load = loads_of_window.next(application.token(), "cover");
    loads_of_window.hold(load, show_when_loaded(loads(at, "cover"), load, shows(shown)));

    application.cancel();
    EXPECT_TRUE(load.is_canceled());
    at.open();
    EXPECT_TRUE(shown.empty());
    if constexpr (wxl::async::detail::checked) {
        EXPECT_EQ(live_frames(), before) << "the wait under the cancelled token did not end";
    }

    EXPECT_TRUE(loads_of_window.next(application.token(), "cover").is_canceled());
}

// A wait whose load was replaced before it is held -- what its start reached asked for the
// picture again -- is nobody's: it goes, and the load that replaced it keeps its own wait.
TEST(BackgroundLoadTest, AWaitWhoseLoadWasReplacedMeanwhileGoes) {
    cancellation_source application;
    loads_t loads_of_window;
    gate first_at;
    gate second_at;
    shown_t shown;
    [[maybe_unused]] std::size_t before = 0;
    if constexpr (wxl::async::detail::checked) before = live_frames();

    cancellation_token const first = loads_of_window.next(application.token(), "first");
    task<> first_wait = show_when_loaded(loads(first_at, "first"), first, shows(shown));
    cancellation_token const second = loads_of_window.next(application.token(), "second");
    loads_of_window.hold(second, show_when_loaded(loads(second_at, "second"), second, shows(shown)));

    loads_of_window.hold(first, std::move(first_wait));
    EXPECT_FALSE(first_at.waited()) << "the wait of the replaced load was kept";
    EXPECT_TRUE(second_at.waited()) << "the wait of the load that replaced it was dropped";
    if constexpr (wxl::async::detail::checked) {
        EXPECT_EQ(live_frames(), before + 2);
    }

    second_at.open();
    EXPECT_EQ(shown, shown_t{"second"});
    if constexpr (wxl::async::detail::checked) {
        EXPECT_EQ(live_frames(), before);
    }
}

// What was asked for is kept, with the token it was asked under, until the background is set
// another way -- the load done or not: a replacement of the device asks for it again.
TEST(BackgroundLoadTest, TheLastLoadIsRememberedUntilTheBackgroundIsSetAnotherWay) {
    cancellation_source application;
    loads_t loads_of_window;
    gate at;
    shown_t shown;
    EXPECT_FALSE(loads_of_window.last());

    cancellation_token const load = loads_of_window.next(application.token(), "cover");
    loads_of_window.hold(load, show_when_loaded(loads(at, "cover"), load, shows(shown)));
    at.open();
    ASSERT_EQ(shown, shown_t{"cover"});

    auto const kept = loads_of_window.last();
    ASSERT_TRUE(kept);
    EXPECT_EQ(kept->request, "cover");
    EXPECT_FALSE(kept->owner.is_canceled());

    // The token kept is the owner's own, not the load's: cancelling the application reaches it.
    application.cancel();
    EXPECT_TRUE(loads_of_window.last()->owner.is_canceled());

    loads_of_window.cancel();
    EXPECT_FALSE(loads_of_window.last()) << "the background was set another way";
}

// The window's answer to a replacement of the device: what was shown is asked for again, under
// the same owner's token, and shown again.
TEST(BackgroundLoadTest, AskedForAgainItIsShownAgain) {
    cancellation_source application;
    loads_t loads_of_window;
    gate first_at;
    gate again_at;
    shown_t shown;
    [[maybe_unused]] std::size_t before = 0;
    if constexpr (wxl::async::detail::checked) before = live_frames();

    cancellation_token const first = loads_of_window.next(application.token(), "cover");
    loads_of_window.hold(first, show_when_loaded(loads(first_at, "cover"), first, shows(shown)));
    first_at.open();

    auto again = loads_of_window.last();
    ASSERT_TRUE(again);
    cancellation_token const reload = loads_of_window.next(std::move(again->owner), std::move(again->request));
    loads_of_window.hold(reload, show_when_loaded(loads(again_at, loads_of_window.last()->request), reload, shows(shown)));
    EXPECT_FALSE(reload.is_canceled());
    again_at.open();

    EXPECT_EQ(shown, (shown_t{"cover", "cover"}));
    if constexpr (wxl::async::detail::checked) {
        EXPECT_EQ(live_frames(), before);
    }

    application.cancel();
    EXPECT_TRUE(reload.is_canceled()) << "the load asked for again is under the owner's token too";
}

// A load that fails ends the wait with its failure, which the wait keeps; nothing is shown.
TEST(BackgroundLoadTest, AFailedLoadEndsTheWaitWithItsFailure) {
    cancellation_source application;
    loads_t loads_of_window;
    gate at;
    shown_t shown;
    [[maybe_unused]] std::size_t before = 0;
    if constexpr (wxl::async::detail::checked) before = live_frames();

    cancellation_token const load = loads_of_window.next(application.token(), "cover");
    task<> wait = show_when_loaded(fails(at), load, shows(shown));
    at.open();

    ASSERT_TRUE(wait.done());
    EXPECT_EQ(end_of(wait), "no such file");
    EXPECT_TRUE(shown.empty());
    if constexpr (wxl::async::detail::checked) {
        EXPECT_EQ(live_frames(), before);
    }
}

// A load the lost device cut short ends the wait without a failure and shows nothing: the
// owner's RenderingDeviceWatch asks for it again once the new device stands.
TEST(BackgroundLoadTest, ALoadTheLostDeviceCutShortEndsTheWaitQuietly) {
    cancellation_source application;
    loads_t loads_of_window;
    gate at;
    shown_t shown;
    [[maybe_unused]] std::size_t before = 0;
    if constexpr (wxl::async::detail::checked) before = live_frames();

    cancellation_token const load = loads_of_window.next(application.token(), "cover");
    task<> wait = show_when_loaded(loses_the_device(at), load, shows(shown));
    if constexpr (wxl::async::detail::checked) {
        EXPECT_EQ(live_frames(), before + 2);
    }
    at.open();

    ASSERT_TRUE(wait.done());
    EXPECT_EQ(end_of(wait), "");
    EXPECT_TRUE(shown.empty());
    if constexpr (wxl::async::detail::checked) {
        EXPECT_EQ(live_frames(), before);
    }
    EXPECT_TRUE(loads_of_window.last()) << "and what was asked for is kept for the replacement";
}

// The cancellation that cuts the load itself short ends the wait with it, and shows nothing.
TEST(BackgroundLoadTest, ACancelledLoadEndsTheWaitWithTheCancellation) {
    cancellation_source application;
    loads_t loads_of_window;
    shown_t shown;
    [[maybe_unused]] std::size_t before = 0;
    if constexpr (wxl::async::detail::checked) before = live_frames();

    application.cancel();
    cancellation_token const load = loads_of_window.next(application.token(), "cover");
    task<> wait = show_when_loaded(wxl::async::detail::canceled_task<std::string>(), load, shows(shown));

    ASSERT_TRUE(wait.done());
    EXPECT_EQ(end_of(wait), "cancelled");
    EXPECT_TRUE(shown.empty());
    if constexpr (wxl::async::detail::checked) {
        EXPECT_EQ(live_frames(), before);
    }
}

}  // namespace

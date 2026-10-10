// A wait on an event asked to end through a cancellation token: cancel() resumes nobody, and the
// thread's queue resumes the coroutine on its next turn -- or the event does, if it comes first --
// where it ends with operation_canceled_exception, or, in the answering form, with an empty
// error. Under a token cancelled already a wait ends at once, every time.
//
// No window, no XAML, no dispatcher. The event is a fake one: a source with the pair of calls a
// wait subscribes through, which keeps the handler and raises it on request with args of its
// own. And the test is the queue: impl::post_canceled_waits() is defined below and counts the
// turns asked for, and a turn is impl::resume_canceled_waits(), called when the test says.
#include <crtdbg.h>

#include <gtest/gtest.h>

#include <cstdint>
#include <format>
#include <optional>
#include <source_location>
#include <string>
#include <vector>

#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.h>

#include "event_awaitable.h"

namespace {

/// Turns of the queue asked for by cancelled waits since the fixture began.
int turns_asked = 0;

}  // namespace

namespace wxl::impl {

/// The queue, as cancelled waits see it: it takes every turn asked for, and the test runs it.
bool post_canceled_waits() noexcept {
    ++turns_asked;
    return true;
}

}  // namespace wxl::impl

namespace {

using wxl::EventArgsBase;
using wxl::EventHandler;
using wxl::EventToken;
using wxl::Object;
using wxl::async::cancellation_source;
using wxl::async::cancellation_token;
using wxl::async::operation_canceled_exception;
using wxl::async::task;

/// What the fake event hands over: args a coroutine answers through, as it answers Handled.
struct key_args : EventArgsBase {
    key_args() noexcept : EventArgsBase(nullptr) {}

    bool handled = false;
};

/// One event: the handler a wait subscribed, and how often it subscribed and left.
struct event_state {
    EventHandler<key_args> handler;
    int subscribed = 0;
    int unsubscribed = 0;
};

/// The source a wait is made over: the pair of calls, as impl::keyed_event has them.
struct fake_event {
    using args_t = key_args;

    event_state* event;

    EventToken add(EventHandler<args_t> const& handler) const {
        event->handler = handler;
        ++event->subscribed;
        return EventToken{1};
    }

    void remove(EventToken) const {
        event->handler = {};
        ++event->unsubscribed;
    }
};

using plain_keys = wxl::event_awaitable<fake_event>;
using keys_under_token = wxl::cancellable_event_awaitable<fake_event>;

/// The sender an event names: an object of its own, cppwinrt's vector, which needs no runtime.
Object a_sender() {
    auto const made = winrt::single_threaded_vector<int32_t>();
    return Object::copy_from_abi(static_cast<::IInspectable*>(winrt::get_abi(made)));
}

/// Raises the event, as the framework would. True when a coroutine took it -- answered Handled
/// from inside the call. The handler is held across the call, as an event source holds its
/// delegate: the coroutine it resumes may end and unsubscribe before the call returns.
bool raise(event_state& event) {
    key_args args;
    EventHandler<key_args> const handler = event.handler;
    handler(a_sender(), args);
    return args.handled;
}

/// Waits for keys under a token until the wait ends, and says how it ended.
task<> takes_keys(event_state& event, cancellation_token stop, int& taken, std::string& ended) {
    keys_under_token keys{fake_event{&event}, std::move(stop)};

    try {
        while (true) {
            key_args& args = co_await keys;
            args.handled = true;
            ++taken;
        }
    } catch (operation_canceled_exception const&) {
        ended = "cancelled";
    }
}

/// The same with the answering form: nothing escapes, and a cancellation is an empty error.
task<> answers_keys(event_state& event, cancellation_token stop, std::vector<std::string>& log) {
    keys_under_token keys{fake_event{&event}, std::move(stop)};

    while (true) {
        auto const got = co_await keys.next();
        if (!got) {
            log.push_back(got.error() ? "failed" : "cancelled");
            co_return;
        }

        got->get().handled = true;
        log.push_back("key");
    }
}

/// Without a token: the wait as it always was.
task<> takes_keys_plainly(event_state& event, int& taken) {
    plain_keys keys{fake_event{&event}};

    while (true) {
        key_args& args = co_await keys;
        args.handled = true;
        ++taken;
    }
}

/// Takes one key, then cancels its own token while it runs -- nobody stands under it then --
/// and waits again.
task<> cancels_between_waits(event_state& event, cancellation_source& stop, std::vector<std::string>& log) {
    keys_under_token keys{fake_event{&event}, stop.token()};

    co_await keys;
    log.push_back("key");

    stop.cancel();

    try {
        co_await keys;
        log.push_back("second key");
    } catch (operation_canceled_exception const&) {
        log.push_back("cancelled at once");
    }
}

/// Every test begins with no wait in either list and no turn in the queue, and leaves it so.
class EventCancelTest : public ::testing::Test
{
protected:
    void SetUp() override {
        turns_asked = 0;
        ASSERT_TRUE(wxl::impl::event_waits_drained());
        ASSERT_FALSE(wxl::impl::canceled_waits_posted());
    }

    void TearDown() override {
        EXPECT_TRUE(wxl::impl::event_waits_drained()) << "a wait was left in a list";
        EXPECT_FALSE(wxl::impl::canceled_waits_posted()) << "a turn was left in the queue";

        wxl::impl::events_closing() = false;
        wxl::impl::canceled_waits_posted() = false;
    }
};

}  // namespace

// Cancelled while it waits: cancel() resumes nobody and asks the queue for one turn, and the turn
// resumes the coroutine with the cancellation.
TEST_F(EventCancelTest, AWaitCancelledWhileSuspendedEndsOnTheQueuesTurn) {
    event_state event;
    cancellation_source stop;
    int taken = 0;
    std::string ended;

    task<> keys = takes_keys(event, stop.token(), taken, ended);

    EXPECT_TRUE(raise(event));
    EXPECT_TRUE(raise(event));
    EXPECT_EQ(taken, 2);

    stop.cancel();

    EXPECT_FALSE(keys.done()) << "cancel() resumed the coroutine itself";
    EXPECT_EQ(turns_asked, 1);
    EXPECT_FALSE(wxl::impl::event_waits_drained())
        << "a cancelled wait counts as drained before it ends";

    wxl::impl::resume_canceled_waits();

    ASSERT_TRUE(keys.done());
    EXPECT_EQ(ended, "cancelled");
    EXPECT_EQ(taken, 2);
    EXPECT_EQ(event.unsubscribed, 1);
}

// The event that comes before the turn resumes the cancelled wait itself -- into the
// cancellation, its args unread and unanswered, so the event travels on -- and the turn then
// finds nobody.
TEST_F(EventCancelTest, AnEventBeforeTheTurnEndsACancelledWaitUnanswered) {
    event_state event;
    cancellation_source stop;
    int taken = 0;
    std::string ended;

    task<> keys = takes_keys(event, stop.token(), taken, ended);

    EXPECT_TRUE(raise(event));
    stop.cancel();
    EXPECT_FALSE(keys.done());

    EXPECT_FALSE(raise(event)) << "a cancelled wait answered the event";

    ASSERT_TRUE(keys.done());
    EXPECT_EQ(ended, "cancelled");
    EXPECT_EQ(taken, 1);
    EXPECT_TRUE(wxl::impl::event_waits_drained());

    wxl::impl::resume_canceled_waits();
}

// Asked before the first wait: by level, the wait ends at once with the cancellation -- it does
// not suspend, and there is nothing for the queue to do.
TEST_F(EventCancelTest, UnderATokenCancelledAlreadyAWaitEndsAtOnce) {
    event_state event;
    cancellation_source stop;
    stop.cancel();

    int taken = 0;
    std::string ended;
    task<> keys = takes_keys(event, stop.token(), taken, ended);

    ASSERT_TRUE(keys.done());
    EXPECT_EQ(ended, "cancelled");
    EXPECT_EQ(taken, 0);
    EXPECT_EQ(turns_asked, 0);
    EXPECT_EQ(event.subscribed, 1);
    EXPECT_EQ(event.unsubscribed, 1);
}

// Asked between two waits, by the coroutine itself: the next wait ends at once.
TEST_F(EventCancelTest, AWaitBegunAfterTheRequestEndsAtOnce) {
    event_state event;
    cancellation_source stop;
    std::vector<std::string> log;

    task<> keys = cancels_between_waits(event, stop, log);

    raise(event);

    ASSERT_TRUE(keys.done());
    EXPECT_EQ(log, (std::vector<std::string>{"key", "cancelled at once"}));
    EXPECT_EQ(turns_asked, 0);
}

// The answering form lets nothing escape under a token either: a cancelled wait answers with an
// empty error, whether it was cancelled while it waited or began under a cancelled token.
TEST_F(EventCancelTest, TheAnsweringFormAnswersTheCancellation) {
    {
        event_state event;
        cancellation_source stop;
        std::vector<std::string> log;

        task<> keys = answers_keys(event, stop.token(), log);

        EXPECT_TRUE(raise(event));
        stop.cancel();
        EXPECT_FALSE(keys.done());

        wxl::impl::resume_canceled_waits();

        ASSERT_TRUE(keys.done());
        EXPECT_NO_THROW(keys.result());
        EXPECT_EQ(log, (std::vector<std::string>{"key", "cancelled"}));
    }

    event_state event;
    cancellation_source stop;
    stop.cancel();

    std::vector<std::string> log;
    task<> keys = answers_keys(event, stop.token(), log);

    ASSERT_TRUE(keys.done());
    EXPECT_EQ(log, (std::vector<std::string>{"cancelled"}));
}

// Events and requests interleaved over three waits: two under one token, one under none. One
// request cancels both of the first two with one turn; the event of one of them ends it before
// the turn, the turn ends the other, and the third goes on taking its events throughout.
TEST_F(EventCancelTest, EventsAndARequestInterleaved) {
    event_state first, second, third;
    cancellation_source stop;
    int taken_first = 0, taken_second = 0, taken_third = 0;
    std::string ended_first, ended_second, ended_third;

    task<> a = takes_keys(first, stop.token(), taken_first, ended_first);
    task<> b = takes_keys(second, stop.token(), taken_second, ended_second);
    task<> c = takes_keys(third, cancellation_token{}, taken_third, ended_third);

    EXPECT_TRUE(raise(first));
    EXPECT_TRUE(raise(third));
    EXPECT_TRUE(raise(second));

    stop.cancel();
    EXPECT_EQ(turns_asked, 1) << "one turn serves every wait cancelled before it";

    EXPECT_FALSE(raise(second));
    EXPECT_TRUE(b.done());
    EXPECT_FALSE(a.done());
    EXPECT_TRUE(raise(third));

    wxl::impl::resume_canceled_waits();

    EXPECT_TRUE(a.done());
    EXPECT_FALSE(c.done());
    EXPECT_EQ(ended_first, "cancelled");
    EXPECT_EQ(ended_second, "cancelled");
    EXPECT_EQ(taken_first, 1);
    EXPECT_EQ(taken_second, 1);

    EXPECT_TRUE(raise(third));
    EXPECT_EQ(taken_third, 3);

    // A turn that finds nobody does nothing.
    wxl::impl::resume_canceled_waits();
    EXPECT_FALSE(c.done());
}

// A frame destroyed while it waits under the token leaves the event's list and the token's: the
// request afterwards reaches nobody and asks for no turn.
TEST_F(EventCancelTest, AFrameDestroyedWhileWaitingLeavesBothLists) {
    event_state event;
    cancellation_source stop;
    int taken = 0;
    std::string ended;

    {
        task<> keys = takes_keys(event, stop.token(), taken, ended);
        EXPECT_TRUE(raise(event));
    }

    EXPECT_TRUE(wxl::impl::event_waits_drained());
    EXPECT_EQ(event.unsubscribed, 1);

    stop.cancel();

    EXPECT_EQ(turns_asked, 0);
    EXPECT_EQ(ended, "");
}

// And one destroyed after it was cancelled, before the queue's turn: it leaves the list of
// cancelled waits, and the turn finds nobody.
TEST_F(EventCancelTest, AFrameDestroyedBeforeTheTurnIsNotResumed) {
    event_state event;
    cancellation_source stop;
    int taken = 0;
    std::string ended;

    std::optional<task<>> keys(std::in_place, takes_keys(event, stop.token(), taken, ended));

    stop.cancel();
    EXPECT_EQ(turns_asked, 1);

    keys.reset();
    EXPECT_TRUE(wxl::impl::event_waits_drained());

    wxl::impl::resume_canceled_waits();
    EXPECT_EQ(ended, "");
}

// The form with a token and the plain one, side by side: without a request the first answers as
// the second does, and a request reaches only the waits that were given its token.
TEST_F(EventCancelTest, TheFormWithATokenAnswersLikeThePlainOne) {
    event_state under, plain;
    cancellation_source stop;
    int taken_under = 0, taken_plain = 0;
    std::string ended;

    task<> a = takes_keys(under, stop.token(), taken_under, ended);
    task<> b = takes_keys_plainly(plain, taken_plain);

    for (int i = 0; i < 3; ++i) {
        EXPECT_TRUE(raise(under));
        EXPECT_TRUE(raise(plain));
    }

    EXPECT_EQ(taken_under, 3);
    EXPECT_EQ(taken_plain, 3);

    stop.cancel();
    wxl::impl::resume_canceled_waits();

    EXPECT_TRUE(a.done());
    EXPECT_FALSE(b.done());
    EXPECT_TRUE(raise(plain));
    EXPECT_EQ(taken_plain, 4);
}

// Going down with a cancelled wait the queue has not resumed yet: phase one ends it with the
// rest, since the queue will not.
TEST_F(EventCancelTest, GoingDownEndsACancelledWaitTheQueueHasNotResumed) {
    event_state canceled, waiting;
    cancellation_source stop;
    int taken_canceled = 0, taken_waiting = 0;
    std::string ended_canceled, ended_waiting;

    task<> a = takes_keys(canceled, stop.token(), taken_canceled, ended_canceled);
    task<> b = takes_keys(waiting, cancellation_token{}, taken_waiting, ended_waiting);

    stop.cancel();
    EXPECT_FALSE(a.done());

    wxl::impl::close_event_waits();

    EXPECT_TRUE(a.done());
    EXPECT_TRUE(b.done());
    EXPECT_EQ(ended_canceled, "cancelled");
    EXPECT_EQ(ended_waiting, "cancelled");
    EXPECT_TRUE(wxl::impl::event_waits_drained());

    // The turn asked for comes after all, and finds nobody.
    wxl::impl::resume_canceled_waits();
}

// A wait made without a token pays nothing for the other: its awaiter is a pointer and two flags,
// as it was a pointer and one, and the proxy has no field it did not have.
static_assert(sizeof(plain_keys::awaiter) == 2 * sizeof(void*));
static_assert(sizeof(keys_under_token) == sizeof(plain_keys) + sizeof(cancellation_token));
static_assert(wxl::async::cancellation_detail::cancellable_awaiter<plain_keys::awaiter>);
static_assert(wxl::async::cancellation_detail::cancellable_awaiter<plain_keys::awaiter_t<false>>);

// ---- The checks of how a wait is used: assert in a Debug build, core::abort under STRICT_CORO --

namespace {

/// A failed check goes to stderr and ends the process without a dialog, so that a death test
/// can read why.
void report_failures_to_stderr() {
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
}

// The co_await a strict build names sits alone on the line below the one that records it.
constexpr std::uint_least32_t line_of_the_wait = std::source_location::current().line() + 1;
task<> waits_on(plain_keys& keys) { co_await keys; }

void two_coroutines_on_one_proxy() {
    report_failures_to_stderr();

    event_state event;
    plain_keys keys{fake_event{&event}};

    task<> first = waits_on(keys);
    task<> second = waits_on(keys);
}

void a_proxy_gone_before_its_waiter() {
    report_failures_to_stderr();

    event_state event;
    auto* const keys = new plain_keys{fake_event{&event}};

    task<> waiting = waits_on(*keys);
    delete keys;
}

class EventWaitDeathTest : public ::testing::Test
{
protected:
    void SetUp() override {
        if constexpr (!wxl::async::coro_detail::checked) GTEST_SKIP() << "built without coroutine checks";
    }
};

class EventWaitStrictDeathTest : public ::testing::Test
{
protected:
    void SetUp() override {
        if constexpr (!wxl::async::coro_detail::strict) GTEST_SKIP() << "built without STRICT_CORO";
    }
};

}  // namespace

TEST_F(EventWaitDeathTest, TwoCoroutinesDoNotWaitOnOneProxy) {
    EXPECT_DEATH(two_coroutines_on_one_proxy(), "two coroutines waiting on one event proxy");
}

TEST_F(EventWaitDeathTest, AProxyDoesNotOutliveItsWaiter) {
    EXPECT_DEATH(a_proxy_gone_before_its_waiter(), "an event proxy outlived the coroutine waiting on it");
}

TEST_F(EventWaitStrictDeathTest, TheReportNamesTheCoAwaitOfTheSecondWait) {
    EXPECT_DEATH(two_coroutines_on_one_proxy(),
                 std::format("event_cancel_test\\.cpp\\({}\\): wxl: two coroutines waiting on one event proxy",
                             line_of_the_wait));
}

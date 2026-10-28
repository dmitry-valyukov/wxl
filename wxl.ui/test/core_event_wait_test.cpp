// A wait on the next invocation of an event of the application's own -- a core::event --
// under a cancellation token. The invocation resumes the coroutine inside the call, with the
// arguments as values -- a reference to non-const alone stays one -- and the wait stands in the
// event only while the coroutine is suspended. cancel() resumes nobody: the thread's queue does on its next turn, or an
// invocation if it comes first. An event cleared or ended under the wait ends it on that turn, as
// one whose event will not come; going down ends it with the rest.
//
// No window, no dispatcher. The queue is the one event_cancel_test stands for: the turn a
// cancelled wait asks for is impl::event_wait::resume_canceled(), called when the test says, and the
// definition of impl::event_wait::post_canceled() is that test's, shared in this binary.
#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include "event_awaitable.h"
#include "event_wait_test.h"

namespace {

using wxl::async::cancellation_source;
using wxl::async::cancellation_token;
using wxl::async::operation_canceled_exception;
using wxl::async::task;
using wxl::core::event;

/// How a wait that did not get its invocation ended: its token was cancelled, or what it
/// waited for will not come.
std::string ending(operation_canceled_exception const& e) {
    return std::string_view{e.what()}.contains("will not come") ? "will not come" : "cancelled";
}

/// Waits for the next count, and logs it or how the wait ended.
task<> takes_count(event<void(int)>& counted, cancellation_token token,
                   std::vector<std::string>& log) {
    try {
        int const count = co_await wxl::on_event(counted, std::move(token));
        log.push_back(std::to_string(count));
    } catch (operation_canceled_exception const& e) {
        log.push_back(ending(e));
    }
}

/// Takes counts until the wait ends. An invocation made between two waits -- from here,
/// once a count is taken -- reaches nobody.
task<> counts_on(event<void(int)>& counted, cancellation_token token, std::vector<int>& seen) {
    while (true) {
        int const count = co_await wxl::on_event(counted, token);
        seen.push_back(count);

        if (count == 1) counted(2);
    }
}

/// Answers through the reference the event passes, inside the call.
task<> answers(event<void(int&) noexcept>& asked, cancellation_token token) {
    int& answer = co_await wxl::on_event(asked, std::move(token));
    answer = 42;
}

/// Several arguments come as a tuple, none as nothing. A reference to const comes as a copy,
/// so what the coroutine keeps lives past its next suspension and the raiser's string.
task<> takes_rename_then_reset(event<void(std::string const&, int)>& renamed, event<void()>& reset,
                               cancellation_token token, std::vector<std::string>& log) {
    auto const [name, times] = co_await wxl::on_event(renamed, token);

    co_await wxl::on_event(reset, token);
    log.push_back(name + " x" + std::to_string(times));
}

/// Counts the copies made of it; a move is not one.
struct copy_count {
    explicit copy_count(int& copies) noexcept : copies(&copies) {}
    copy_count(copy_count const& other) noexcept : copies(other.copies) { ++*copies; }
    copy_count(copy_count&& other) noexcept : copies(other.copies) {}

    int* copies;
};

task<> takes_by_value(event<void(copy_count)>& passed, cancellation_token token) {
    copy_count const taken = co_await wxl::on_event(passed, std::move(token));
    static_cast<void>(taken);
}

/// Kept in a variable, ended by its token; the invocation that comes first resumes it into the
/// cancellation, and its handler ends the event, from inside the event's own call.
task<> holds_the_wait_then_ends_the_event(std::unique_ptr<event<void(int)>>& owner,
                                          cancellation_token token, std::vector<std::string>& log) {
    {
        auto wait = wxl::on_event(*owner, std::move(token));
        try {
            co_await wait;
            log.push_back("value");
        } catch (operation_canceled_exception const& e) {
            log.push_back(ending(e));
            owner.reset();
        }
    }
    log.push_back("after");
}

/// Kept in a variable and ended by going down: it has left the event by the time the coroutine
/// hears so.
task<> holds_the_wait_through_going_down(event<void(int)>& counted, cancellation_token token,
                                         std::vector<std::string>& log) {
    auto wait = wxl::on_event(counted, std::move(token));
    try {
        co_await wait;
    } catch (operation_canceled_exception const& e) {
        log.push_back(ending(e));
        log.push_back(counted ? "still subscribed" : "left");
    }
}

/// The wait kept in a variable and awaited twice. It leaves the event as it resumes; ended by
/// its token, it leaves only with the variable, and an invocation before that resumes nothing.
task<> holds_the_wait(event<void(int)>& counted, cancellation_token token,
                      std::vector<std::string>& log) {
    auto wait = wxl::on_event(counted, std::move(token));

    int const count = co_await wait;
    log.push_back(std::to_string(count));
    log.push_back(counted ? "still subscribed" : "left");

    try {
        co_await wait;
        log.push_back("second");
    } catch (operation_canceled_exception const& e) {
        log.push_back(ending(e));
        counted(2);
    }
}

using token = cancellation_token;

static_assert(
    std::is_same_v<
        decltype(wxl::on_event(std::declval<event<void(int)>&>(), token{}).await_resume()), int>);
static_assert(
    std::is_same_v<decltype(wxl::on_event(std::declval<event<void(int&) noexcept>&>(), token{})
                                .await_resume()),
                   int&>);
static_assert(std::is_same_v<
              decltype(wxl::on_event(std::declval<event<void(std::string const&, int)>&>(), token{})
                           .await_resume()),
              std::tuple<std::string, int>>);
static_assert(
    std::is_same_v<decltype(wxl::on_event(std::declval<event<void(std::string const&)>&>(), token{})
                                .await_resume()),
                   std::string>);
static_assert(
    std::is_same_v<decltype(wxl::on_event(std::declval<event<void(int&, std::string const&)>&>(),
                                          token{})
                                .await_resume()),
                   std::tuple<int&, std::string>>);
static_assert(
    std::is_same_v<decltype(wxl::on_event(std::declval<event<void()>&>(), token{}).await_resume()),
                   void>);
static_assert(
    wxl::async::detail::cancelable_awaiter<wxl::impl::core_event_wait<event<void(int)>, int>>);

/// A reference to const is answered by copy: a type that would be sliced, or cannot be copied, is
/// refused by a static_assert on this concept where the wait is made.
struct polymorphic {
    virtual ~polymorphic() = default;
};

struct uncopyable {
    uncopyable() = default;
    uncopyable(uncopyable const&) = delete;
};

static_assert(wxl::impl::invocation_answerable<std::string const&>);
static_assert(wxl::impl::invocation_answerable<polymorphic&>);
static_assert(!wxl::impl::invocation_answerable<polymorphic const&>);
static_assert(!wxl::impl::invocation_answerable<uncopyable const&>);

/// Every test begins with no wait in either list and no turn in the queue, and leaves it so.
class CoreEventWaitTest : public ::testing::Test
{
protected:
    void SetUp() override {
        ASSERT_TRUE(wxl::impl::event_wait::all_drained());
        ASSERT_FALSE(wxl::impl::event_wait_test::canceled_posted());
    }

    void TearDown() override {
        EXPECT_TRUE(wxl::impl::event_wait::all_drained()) << "a wait was left in a list";
        EXPECT_FALSE(wxl::impl::event_wait_test::canceled_posted())
            << "a turn was left in the queue";

        wxl::impl::event_wait_test::set_closing(false);
        wxl::impl::event_wait_test::set_canceled_posted(false);
    }
};

}  // namespace

// The invocation resumes the coroutine inside the call, and the coroutine runs to its end there:
// the wait, a temporary of its co_await, is gone before the call returns, and the event lets go
// of the subscription after it.
TEST_F(CoreEventWaitTest, TheInvocationResumesTheWaitInsideTheCall) {
    event<void(int)> counted;
    cancellation_source source;
    std::vector<std::string> log;

    task<> waiting = takes_count(counted, source.token(), log);

    EXPECT_FALSE(waiting.done());
    EXPECT_TRUE(static_cast<bool>(counted)) << "the suspended wait is not subscribed";

    counted(7);

    ASSERT_TRUE(waiting.done());
    EXPECT_EQ(log, (std::vector<std::string>{"7"}));
    EXPECT_FALSE(static_cast<bool>(counted)) << "the wait did not leave the event";

    // And it left the token's event: the request reaches nobody.
    source.cancel();
    EXPECT_FALSE(wxl::impl::event_wait_test::canceled_posted());
}

// One argument as a value, a reference to non-const as it is -- it reaches the raiser's object,
// so the coroutine answers inside the call -- several as a tuple, none as nothing; either spelling
// of the signature. A reference to const is a copy, which outlives the raiser's temporary and the
// coroutine's next suspension.
TEST_F(CoreEventWaitTest, TheArgumentsComeAsTheEventPassesThem) {
    cancellation_source source;

    event<void(int&) noexcept> asked;
    task<> answering = answers(asked, source.token());

    int answer = 0;
    asked(answer);
    EXPECT_EQ(answer, 42);
    EXPECT_TRUE(answering.done());

    event<void(std::string const&, int)> renamed;
    event<void()> reset;
    std::vector<std::string> log;
    task<> taking = takes_rename_then_reset(renamed, reset, source.token(), log);

    reset();
    EXPECT_TRUE(log.empty()) << "a wait took an event it did not stand in";

    renamed(std::string(64, 'd'), 3);
    EXPECT_FALSE(static_cast<bool>(renamed));
    EXPECT_TRUE(static_cast<bool>(reset));

    reset();
    ASSERT_TRUE(taking.done());
    EXPECT_NO_THROW(taking.result());
    EXPECT_EQ(log, (std::vector<std::string>{std::string(64, 'd') + " x3"}));
}

// One argument passed by value is the callback's own copy, and the wait moves it out: the one copy
// is the one the event makes for its callback.
TEST_F(CoreEventWaitTest, AnArgumentByValueIsMovedOut) {
    event<void(copy_count)> passed;
    cancellation_source source;
    int copies = 0;

    task<> taking = takes_by_value(passed, source.token());

    passed(copy_count{copies});

    ASSERT_TRUE(taking.done());
    EXPECT_EQ(copies, 1);
}

// Between two waits nothing stands in the event: an invocation then -- here a nested one, from
// inside the coroutine the outer invocation resumed -- is not queued for the next wait.
TEST_F(CoreEventWaitTest, AnInvocationBetweenWaitsReachesNobody) {
    event<void(int)> counted;
    cancellation_source source;
    std::vector<int> seen;

    task<> counting = counts_on(counted, source.token(), seen);

    counted(1);
    counted(3);
    EXPECT_EQ(seen, (std::vector<int>{1, 3}));

    source.cancel();
    wxl::impl::event_wait::resume_canceled();

    ASSERT_TRUE(counting.done());
    EXPECT_THROW(counting.result(), operation_canceled_exception);
    EXPECT_FALSE(static_cast<bool>(counted));
}

// A wait kept in a variable stands in the event again each time it is awaited, and an invocation
// that finds it not suspended -- in the handler of its cancellation, the variable still alive --
// is not its.
TEST_F(CoreEventWaitTest, AWaitKeptInAVariableTakesOnlyTheInvocationsItWaitsFor) {
    event<void(int)> counted;
    cancellation_source source;
    std::vector<std::string> log;

    task<> holding = holds_the_wait(counted, source.token(), log);

    counted(1);
    EXPECT_EQ(log, (std::vector<std::string>{"1", "left"}));
    EXPECT_TRUE(static_cast<bool>(counted)) << "the second wait is not subscribed";

    source.cancel();
    wxl::impl::event_wait::resume_canceled();

    ASSERT_TRUE(holding.done());
    EXPECT_NO_THROW(holding.result());
    EXPECT_EQ(log, (std::vector<std::string>{"1", "left", "cancelled"}));
    EXPECT_FALSE(static_cast<bool>(counted));
}

// The invocation that resumes a cancelled wait kept in a variable takes the subscription out
// before it resumes: the handler of the cancellation ends the event from inside the event's own
// call, and the wait, when its variable goes, has nothing to leave.
TEST_F(CoreEventWaitTest, HeldWaitCancelledThenEventDestroyedInsideItsCall) {
    auto owner = std::make_unique<event<void(int)>>();
    cancellation_source source;
    std::vector<std::string> log;

    task<> holding = holds_the_wait_then_ends_the_event(owner, source.token(), log);

    source.cancel();
    (*owner)(1);

    ASSERT_TRUE(holding.done());
    EXPECT_EQ(log, (std::vector<std::string>{"cancelled", "after"}));

    wxl::impl::event_wait::resume_canceled();
}

// Two waits on one event: one invocation resumes both, in the order they began.
TEST_F(CoreEventWaitTest, OneInvocationResumesEveryWaitStandingInIt) {
    event<void(int)> counted;
    cancellation_source source;
    std::vector<std::string> log;

    task<> first = takes_count(counted, source.token(), log);
    task<> second = takes_count(counted, source.token(), log);

    counted(4);

    EXPECT_TRUE(first.done());
    EXPECT_TRUE(second.done());
    EXPECT_EQ(log, (std::vector<std::string>{"4", "4"}));
    EXPECT_FALSE(static_cast<bool>(counted));
}

// Cancelled while it waits: cancel() resumes nobody and asks the queue for a turn; until the turn
// the wait is still the event's, and the turn ends it with the cancellation.
TEST_F(CoreEventWaitTest, ACancelledWaitEndsOnTheQueuesTurn) {
    event<void(int)> counted;
    cancellation_source source;
    std::vector<std::string> log;

    task<> waiting = takes_count(counted, source.token(), log);

    source.cancel();

    EXPECT_FALSE(waiting.done()) << "cancel() resumed the coroutine itself";
    EXPECT_TRUE(wxl::impl::event_wait_test::canceled_posted());
    EXPECT_TRUE(static_cast<bool>(counted));

    wxl::impl::event_wait::resume_canceled();

    ASSERT_TRUE(waiting.done());
    EXPECT_EQ(log, (std::vector<std::string>{"cancelled"}));
    EXPECT_FALSE(static_cast<bool>(counted));
}

// The invocation that comes before the turn resumes the cancelled wait itself -- into the
// cancellation, its arguments unread -- and the turn then finds nobody.
TEST_F(CoreEventWaitTest, AnInvocationBeforeTheTurnEndsTheCancelledWait) {
    event<void(int)> counted;
    cancellation_source source;
    std::vector<std::string> log;

    task<> waiting = takes_count(counted, source.token(), log);

    source.cancel();
    counted(5);

    ASSERT_TRUE(waiting.done());
    EXPECT_EQ(log, (std::vector<std::string>{"cancelled"}));
    EXPECT_FALSE(static_cast<bool>(counted));
    EXPECT_TRUE(wxl::impl::event_wait::all_drained());

    wxl::impl::event_wait::resume_canceled();
}

// Under a token cancelled already the wait ends at once: it does not suspend, does not
// subscribe, and asks the queue for nothing.
TEST_F(CoreEventWaitTest, UnderATokenCancelledAlreadyTheWaitEndsAtOnce) {
    event<void(int)> counted;
    cancellation_source source;
    source.cancel();

    std::vector<std::string> log;
    task<> waiting = takes_count(counted, source.token(), log);

    ASSERT_TRUE(waiting.done());
    EXPECT_EQ(log, (std::vector<std::string>{"cancelled"}));
    EXPECT_FALSE(wxl::impl::event_wait_test::canceled_posted());

    counted(1);
    EXPECT_EQ(log.size(), 1u);
}

// A frame destroyed while it waits leaves the event and the token: an invocation afterwards
// reaches nobody, and the request asks for no turn.
TEST_F(CoreEventWaitTest, AFrameDestroyedWhileWaitingLeavesTheEventAndTheToken) {
    event<void(int)> counted;
    cancellation_source source;
    std::vector<std::string> log;

    {
        task<> waiting = takes_count(counted, source.token(), log);
        EXPECT_TRUE(static_cast<bool>(counted));
    }

    EXPECT_FALSE(static_cast<bool>(counted));

    counted(1);
    source.cancel();

    EXPECT_TRUE(log.empty());
    EXPECT_FALSE(wxl::impl::event_wait_test::canceled_posted());
}

// An event cleared under the wait lets go of it: nobody is resumed inside clear(), and the queue's
// turn ends the wait as one whose event will not come.
TEST_F(CoreEventWaitTest, AnEventClearedUnderTheWaitEndsItOnTheQueuesTurn) {
    event<void(int)> counted;
    cancellation_source source;
    std::vector<std::string> log;

    task<> waiting = takes_count(counted, source.token(), log);

    counted.clear();

    EXPECT_FALSE(waiting.done()) << "clear() resumed the coroutine itself";
    EXPECT_TRUE(wxl::impl::event_wait_test::canceled_posted());

    wxl::impl::event_wait::resume_canceled();

    ASSERT_TRUE(waiting.done());
    EXPECT_EQ(log, (std::vector<std::string>{"will not come"}));

    // The wait left the token as it ended.
    source.cancel();
    EXPECT_FALSE(wxl::impl::event_wait_test::canceled_posted());
}

// And one that ends: the wait is told the same way, and does not reach for the event on its way
// out.
TEST_F(CoreEventWaitTest, AnEventEndedUnderTheWaitEndsItOnTheQueuesTurn) {
    auto counted = std::make_unique<event<void(int)>>();
    cancellation_source source;
    std::vector<std::string> log;

    task<> waiting = takes_count(*counted, source.token(), log);

    counted.reset();

    EXPECT_FALSE(waiting.done()) << "the event's end resumed the coroutine itself";

    wxl::impl::event_wait::resume_canceled();

    ASSERT_TRUE(waiting.done());
    EXPECT_EQ(log, (std::vector<std::string>{"will not come"}));
}

// The way a notification that happens once is raised -- the subscriptions swapped into a local
// event, and that one called -- reaches the wait as any invocation does; the subscription,
// gone with the local event, reaches nothing after it.
TEST_F(CoreEventWaitTest, AnInvocationOfTheEventSwappedIntoReachesTheWait) {
    event<void(int)> counted;
    cancellation_source source;
    std::vector<std::string> log;

    task<> waiting = takes_count(counted, source.token(), log);

    {
        event<void(int)> once;
        once.swap(counted);
        once(9);
        once(10);
    }

    ASSERT_TRUE(waiting.done());
    EXPECT_EQ(log, (std::vector<std::string>{"9"}));
    EXPECT_FALSE(static_cast<bool>(counted));
}

// Going down ends the wait with the rest, and a wait begun after that ends at once.
TEST_F(CoreEventWaitTest, GoingDownEndsTheWait) {
    event<void(int)> counted;
    cancellation_source source;
    std::vector<std::string> log;

    task<> waiting = takes_count(counted, source.token(), log);

    wxl::impl::event_wait::close_all();

    ASSERT_TRUE(waiting.done());
    EXPECT_FALSE(static_cast<bool>(counted));

    task<> late = takes_count(counted, source.token(), log);

    ASSERT_TRUE(late.done());
    EXPECT_FALSE(static_cast<bool>(counted));
    EXPECT_EQ(log, (std::vector<std::string>{"will not come", "will not come"}));

    std::vector<std::string> held;
    wxl::impl::event_wait_test::set_closing(false);
    task<> holding = holds_the_wait_through_going_down(counted, source.token(), held);
    wxl::impl::event_wait::close_all();

    ASSERT_TRUE(holding.done());
    EXPECT_EQ(held, (std::vector<std::string>{"will not come", "left"}));
}

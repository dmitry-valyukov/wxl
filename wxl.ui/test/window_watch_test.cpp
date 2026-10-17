// A watch on an event of the window: a member of its owner, called each time the event is
// raised while the watch is on the window, and taken off by the watch's end with the
// window's answer checked -- a watch no longer on its window stops the process. The removals
// wxl makes by a token it holds read the same answer.
//
// No window, no XAML. The window is a fake: a handle to the handler lists the real one keeps
// -- impl::handler_list itself -- with the pair of calls per event a watch subscribes
// through, and the test raises the events.
#include <crtdbg.h>

#include <gtest/gtest.h>

#include <memory>
#include <new>
#include <optional>
#include <type_traits>
#include <vector>

// The list first: it reaches the projection, whose standard headers have to come before
// the wxl.core import the watch's header carries.
#include "impl/handler_list.h"
#include "impl/window_watch.h"

namespace {

using wxl::EventArgsBase;
using wxl::EventHandler;
using wxl::EventToken;
using wxl::Object;
using wxl::impl::handler_list;
using wxl::impl::window_watch;

/// What a resize hands over: a plain value, by const reference, as ClientSize is.
struct size_args {
    int width = 0;
};

/// What a request to close hands over: args a listener answers through, as Closing's are.
struct close_args : EventArgsBase {
    close_args() noexcept : EventArgsBase(nullptr) {}

    bool cancel = false;
};

/// The window's side of three events: their handler lists, and the token it gave last, for a
/// test that takes a handler off behind the back of whoever put it there.
struct window_state {
    EventToken lastGiven;

    EventToken give() noexcept { return lastGiven = wxl::impl::next_event_token(); }

    handler_list<Object> moved;
    handler_list<size_args> resized;
    handler_list<close_args> closing;
};

/// A window as a watch has it: a handle to shared state, copied by value, with the pair of
/// calls per event the real one has.
struct fake_window {
    std::shared_ptr<window_state> state = std::make_shared<window_state>();

    EventToken add_onMoved(EventHandler<Object> const& handler) const {
        return state->moved.add(state->give(), handler);
    }
    bool remove_onMoved(EventToken token) const { return state->moved.remove(token); }

    EventToken add_onResized(EventHandler<size_args> const& handler) const {
        return state->resized.add(state->give(), handler);
    }
    bool remove_onResized(EventToken token) const { return state->resized.remove(token); }

    EventToken add_onClosing(EventHandler<close_args> const& handler) const {
        return state->closing.add(state->give(), handler);
    }
    bool remove_onClosing(EventToken token) const { return state->closing.remove(token); }
};

/// The pair of calls for one event, as the generated EventAdder has it -- the members named
/// by pointer here rather than by the event's name in a specialisation.
template <auto Add, auto Remove, class Args>
struct adder {
    template <typename Window, typename Fn>
    static EventToken add(Window const& window, Fn const& handler) {
        return (window.*Add)(handler);
    }

    template <typename Window>
    static auto remove(Window const& window, EventToken token) {
        return (window.*Remove)(token);
    }

    template <typename>
    using args_t = Args;
};

using moved = adder<&fake_window::add_onMoved, &fake_window::remove_onMoved, Object>;
using resized = adder<&fake_window::add_onResized, &fake_window::remove_onResized, size_args>;
using closing = adder<&fake_window::add_onClosing, &fake_window::remove_onClosing, close_args>;

void raise_moved(fake_window const& window) {
    window.state->moved.fire(Object::Impl::empty<Object>());
}

void raise_resized(fake_window const& window, int width) {
    window.state->resized.fire(size_args{width});
}

/// An owner as an application has one: it keeps its watches as members and hears each event
/// through a noexcept member of its own, which takes the args or nothing.
struct Page {
    int moves = 0;
    std::vector<int> widths;
    bool refuses = false;

    void windowMoved() noexcept { ++moves; }
    void sizeChanged(size_args const& size) noexcept { widths.push_back(size.width); }
    void closing(close_args& args) noexcept { args.cancel = refuses; }
};

using move_watch = window_watch<fake_window, moved, &Page::windowMoved>;
using size_watch = window_watch<fake_window, resized, &Page::sizeChanged>;
using close_watch = window_watch<fake_window, closing, &Page::closing>;

// The handler calls the owner the watch was put on with, so there is nowhere to copy or move
// the watch to.
static_assert(!std::is_copy_constructible_v<move_watch>);
static_assert(!std::is_move_constructible_v<move_watch>);
static_assert(!std::is_copy_assignable_v<move_watch>);
static_assert(!std::is_move_assignable_v<move_watch>);

TEST(WindowWatch, calls_its_owner_each_time_the_event_is_raised) {
    fake_window window;
    Page page;
    size_watch watch{window, page};
    EXPECT_TRUE(watch.attached());

    raise_resized(window, 800);
    raise_resized(window, 640);
    EXPECT_EQ(page.widths, (std::vector<int>{800, 640}));
}

// An event whose args are an empty Object says all it has to say by being raised.
TEST(WindowWatch, a_listener_may_take_nothing) {
    fake_window window;
    Page page;
    move_watch watch{window, page};

    raise_moved(window);
    raise_moved(window);
    EXPECT_EQ(page.moves, 2);
}

// Args a handler answers through reach the listener as they reach a handler: by reference.
TEST(WindowWatch, a_listener_answers_through_the_args) {
    fake_window window;
    Page page;
    close_watch watch{window, page};

    close_args asked;
    window.state->closing.fire(asked);
    EXPECT_FALSE(asked.cancel);

    page.refuses = true;
    close_args refused;
    window.state->closing.fire(refused);
    EXPECT_TRUE(refused.cancel);
}

// An empty watch stands nowhere: the event is raised unheard until the watch is attached.
TEST(WindowWatch, starts_empty_and_is_attached_later) {
    fake_window window;
    Page page;
    move_watch watch;
    EXPECT_FALSE(watch.attached());

    raise_moved(window);
    EXPECT_EQ(page.moves, 0);

    watch.attach(window, page);
    EXPECT_TRUE(watch.attached());
    raise_moved(window);
    EXPECT_EQ(page.moves, 1);
}

// An empty watch has nothing to take off.
TEST(WindowWatch, an_empty_watch_goes_quietly) {
    { move_watch watch; }
    SUCCEED();
}

// The watch comes off with its end, and the window, living on, calls the owner no more --
// while the other handlers of the event go on hearing it.
TEST(WindowWatch, comes_off_the_window_when_it_goes) {
    fake_window window;
    Page page;
    int others = 0;
    window.add_onMoved([&others] { ++others; });
    {
        move_watch watch{window, page};
        raise_moved(window);
    }
    raise_moved(window);
    EXPECT_EQ(page.moves, 1);
    EXPECT_EQ(others, 2);
}

// Two watches on one event are two handlers, called in the order they were attached; one
// going leaves the other where it stands.
TEST(WindowWatch, two_watches_on_one_event) {
    fake_window window;
    Page first;
    Page second;
    move_watch firstWatch{window, first};
    {
        move_watch secondWatch{window, second};
        raise_moved(window);
    }
    raise_moved(window);
    EXPECT_EQ(first.moves, 2);
    EXPECT_EQ(second.moves, 1);
}

// The watch holds the window it was given, not the handle it was given it through: the
// window's state lives while the watch is on it, and is let go with the watch.
TEST(WindowWatch, holds_the_window_while_it_is_on_it) {
    fake_window window;
    std::weak_ptr<window_state> const state = window.state;
    Page page;
    {
        move_watch watch{fake_window{window}, page};
        window = fake_window{};
        EXPECT_FALSE(state.expired());
    }
    EXPECT_TRUE(state.expired());
}

// An owner that destroys its watch from inside the call that reached it: the window calls a
// copy of the handler, so the watch goes as it would anywhere else, and the event is not
// heard again. The window is held by someone besides the watch, as a live window's state is
// held by the window itself.
struct SelfRemoving {
    void windowMoved() noexcept {
        ++moves;
        watch.reset();
    }

    int moves = 0;
    std::optional<window_watch<fake_window, moved, &SelfRemoving::windowMoved>> watch;
};

TEST(WindowWatch, goes_from_inside_its_own_call) {
    fake_window window;
    SelfRemoving owner;
    owner.watch.emplace(window, owner);

    raise_moved(window);
    EXPECT_FALSE(owner.watch.has_value());
    raise_moved(window);
    EXPECT_EQ(owner.moves, 1);
}

// A window that cannot take the handler: the attach throws and leaves the watch empty, with
// no copy of the window kept -- a watch that thought itself attached would take off what is
// not there, and the remove below answers so.
struct refusing {
    template <typename Window, typename Fn>
    static EventToken add(Window const&, Fn const&) {
        throw std::bad_alloc{};
    }

    template <typename Window>
    static bool remove(Window const&, EventToken) {
        return false;
    }

    template <typename>
    using args_t = Object;
};

TEST(WindowWatch, a_failed_attach_leaves_it_empty) {
    fake_window window;
    std::weak_ptr<window_state> const state = window.state;
    Page page;
    window_watch<fake_window, refusing, &Page::windowMoved> watch;

    EXPECT_THROW(watch.attach(window, page), std::bad_alloc);
    EXPECT_FALSE(watch.attached());
    window = fake_window{};
    EXPECT_TRUE(state.expired());
}

// The window's answer, which the watch checks: true for a handler that was there, false for
// one taken off already, one never given, and no token at all.
TEST(HandlerList, remove_answers_whether_the_token_was_there) {
    handler_list<size_args> list;
    int heard = 0;
    EventToken const first = list.add(EventToken{1}, [&heard] { ++heard; });
    EventToken const second = list.add(EventToken{2}, [&heard] { ++heard; });

    EXPECT_TRUE(list.remove(first));
    EXPECT_FALSE(list.remove(first));
    EXPECT_FALSE(list.remove(EventToken{second.value + 1}));
    EXPECT_FALSE(list.remove(EventToken{}));

    list.fire(size_args{});
    EXPECT_EQ(heard, 1);
}

// Taken off from inside its own call, a handler answers once: its place, kept until the call
// is over, keeps no token -- not even the empty one.
TEST(HandlerList, a_handler_taken_off_inside_a_call_answers_once) {
    handler_list<size_args> list;
    std::vector<bool> answers;
    EventToken self;
    self = list.add(EventToken{1}, [&] {
        answers.push_back(list.remove(self));
        answers.push_back(list.remove(self));
        answers.push_back(list.remove(EventToken{}));
    });

    list.fire(size_args{});
    list.fire(size_args{});
    EXPECT_EQ(answers, (std::vector<bool>{true, false, false}));
}

// The tokens are not the list's: a token one event gave names nothing in another, which keeps
// its handler.
TEST(HandlerList, a_token_of_another_event_names_nothing) {
    fake_window window;
    int moves = 0;
    int resizes = 0;
    EventToken const moved = window.add_onMoved([&moves] { ++moves; });
    EventToken const resized = window.add_onResized([&resizes] { ++resizes; });

    EXPECT_FALSE(window.remove_onMoved(resized));
    EXPECT_FALSE(window.remove_onResized(moved));
    raise_moved(window);
    raise_resized(window, 1);
    EXPECT_EQ(moves, 1);
    EXPECT_EQ(resizes, 1);
}

// The counter is the process's: a token one window gave names nothing in another, which keeps
// its handler.
TEST(HandlerList, a_token_of_another_window_names_nothing) {
    fake_window first;
    fake_window second;
    int firstMoves = 0;
    int secondMoves = 0;
    EventToken const token = first.add_onMoved([&firstMoves] { ++firstMoves; });
    second.add_onMoved([&secondMoves] { ++secondMoves; });

    EXPECT_FALSE(second.remove_onMoved(token));
    raise_moved(first);
    raise_moved(second);
    EXPECT_EQ(firstMoves, 1);
    EXPECT_EQ(secondMoves, 1);
}

// The removals wxl makes by a token it holds -- `-=` in the braces, the end of a wait on an
// event -- read the window's answer, as the watch does; a remove that answers nothing, a
// generated wrapper's, is taken at its word.
TEST(CheckedRemove, reads_the_answer_when_there_is_one) {
    fake_window window;
    int moves = 0;
    EventToken const token = window.add_onMoved([&moves] { ++moves; });
    wxl::impl::checked_remove([&] { return window.remove_onMoved(token); });
    raise_moved(window);
    EXPECT_EQ(moves, 0);

    int removes = 0;
    wxl::impl::checked_remove([&removes] { ++removes; });
    EXPECT_EQ(removes, 1);
}

// Each broken rule stops the process: what would follow is a window calling an owner that
// did not ask, or a watch that cannot say whether its handler is still there. The report
// goes to stderr rather than to a dialog that would hang the run.
void without_a_dialog() {
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
}

void attach_twice() {
    without_a_dialog();
    fake_window first;
    fake_window second;
    Page page;
    move_watch watch{first, page};
    watch.attach(second, page);
}

TEST(WindowWatchDeathTest, attaching_twice_stops) {
    EXPECT_DEATH(attach_twice(), "a WindowWatch is attached twice");
}

// The token is the watch's: whoever takes the handler off by it takes away what the watch's
// destructor counts on.
void take_off_by_someone_else() {
    without_a_dialog();
    fake_window window;
    Page page;
    move_watch watch{window, page};
    static_cast<void>(window.remove_onMoved(window.state->lastGiven));
}

TEST(WindowWatchDeathTest, taken_off_by_someone_else_stops) {
    EXPECT_DEATH(take_off_by_someone_else(), "a WindowWatch is no longer on its window");
}

// A token taken off twice: the second removal finds nothing, and says so.
void remove_twice() {
    without_a_dialog();
    fake_window window;
    EventToken const token = window.add_onMoved([] {});
    wxl::impl::checked_remove([&] { return window.remove_onMoved(token); });
    wxl::impl::checked_remove([&] { return window.remove_onMoved(token); });
}

TEST(CheckedRemoveDeathTest, a_token_that_names_nothing_stops) {
    EXPECT_DEATH(remove_twice(), "a handler taken off by its token was not there");
}

}  // namespace

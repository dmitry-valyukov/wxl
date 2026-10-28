// The token of a subscription made in the description: `onX[token] += handler` writes it, in a
// control's braces and in Apply, and `Apply {control, onX -= token}` takes the subscription back
// and empties the token -- a second removal through it stops the process.
//
// No window, no XAML. The control is a fake: an Object with the pair of calls a generated wrapper
// has for one event, over impl::handler_list -- the list CompositionWindow keeps its handlers in --
// and the setter-pack constructor a generated wrapper has, which applies each argument the way
// the generated one does (impl::apply_argument). The event is keyed by a value of EventKey no
// generated event takes, so its event_adder here is the only one, written the way the generator
// writes it. The test raises the event.
#include <crtdbg.h>

#include <gtest/gtest.h>

#include <memory>
#include <type_traits>
#include <utility>

// The list first: it reaches the projection, whose standard headers have to come before the
// wxl.core import the DSL's header carries.
#include "impl/handler_list.h"
#include "impl/member.h"

namespace {

using wxl::Apply;
using wxl::EventHandler;
using wxl::EventToken;
using wxl::Object;
using wxl::impl::handler_list;

// An event no generated wrapper has: the last value of the key's type.
constexpr auto test_event = static_cast<wxl::EventKey>(-1);

/// A control as the DSL has it: an Object, copied by value, its handlers shared by the copies, with
/// the pair of calls a generated wrapper declares for the event and its setter-pack constructor.
class FakeControl : public Object
{
public:
    FakeControl() : Object(Object::Impl::empty<Object>()) {}

    template <typename... Setters>
        requires(sizeof...(Setters) > 0 &&
                 !(std::same_as<std::remove_cvref_t<Setters>, FakeControl> || ...))
    explicit FakeControl(Setters&&... setters) : FakeControl() {
        (wxl::impl::apply_argument(*this, std::forward<Setters>(setters)), ...);
    }

    EventToken add_onTest(EventHandler<Object> const& handler) const {
        return handlers_->add(wxl::impl::next_event_token(), handler);
    }

    bool remove_onTest(EventToken& token) const { return handlers_->remove(token); }

    void raise() const { handlers_->raise(Object::Impl::empty<Object>()); }

private:
    std::shared_ptr<handler_list<Object>> handlers_ = std::make_shared<handler_list<Object>>();
};

}  // namespace

// What the generator writes for an event, for the test's own.
template <>
struct wxl::impl::event_adder<test_event> {
    template <typename Obj, typename Fn>
    static EventToken add(Obj const& object, Fn const& handler) {
        return object.add_onTest(handler);
    }

    template <typename Obj>
    static auto remove(Obj const& object, EventToken& token) {
        return object.remove_onTest(token);
    }

    template <typename Obj>
    using args_t = Object;
};

namespace {

inline constexpr wxl::impl::event_tag<test_event> onTest;

// The token is the one handle of its subscription: not copied, and moving it empties the source.
static_assert(!std::is_copy_constructible_v<EventToken>);
static_assert(!std::is_copy_assignable_v<EventToken>);

TEST(EventToken, moving_a_token_empties_the_source) {
    FakeControl control;
    EventToken first = control.add_onTest([] {});
    ASSERT_TRUE(static_cast<bool>(first));

    EventToken moved = std::move(first);
    EXPECT_FALSE(static_cast<bool>(first));
    EXPECT_TRUE(static_cast<bool>(moved));

    EventToken assigned;
    assigned = std::move(moved);
    EXPECT_FALSE(static_cast<bool>(moved));
    EXPECT_TRUE(control.remove_onTest(assigned));
    EXPECT_FALSE(static_cast<bool>(assigned));
}

TEST(EventTokenForm, in_the_braces_the_token_is_written_and_the_handler_called) {
    EventToken token;
    int heard = 0;
    FakeControl const control{onTest[token] += [&heard] { ++heard; }};

    EXPECT_TRUE(static_cast<bool>(token));
    control.raise();
    EXPECT_EQ(heard, 1);
}

TEST(EventTokenForm, in_apply_the_token_is_written_and_the_handler_called) {
    FakeControl const control;
    EventToken token;
    int heard = 0;
    Apply{control, onTest[token] += [&heard] { ++heard; }};

    EXPECT_TRUE(static_cast<bool>(token));
    control.raise();
    EXPECT_EQ(heard, 1);
}

TEST(EventTokenForm, apply_with_minus_takes_the_handler_off_and_empties_the_token) {
    EventToken token;
    int heard = 0;
    FakeControl const control{onTest[token] += [&heard] { ++heard; }};
    control.raise();
    ASSERT_EQ(heard, 1);

    Apply{control, onTest -= token};

    EXPECT_FALSE(static_cast<bool>(token));
    control.raise();
    EXPECT_EQ(heard, 1) << "the handler was called after it was taken off";
}

// A second removal through the same variable: the token is empty, and the process stops rather
// than take off whatever subscription got the number next. The report goes to stderr rather than
// to a dialog that would hang the run.
void remove_twice() {
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    EventToken token;
    FakeControl const control{onTest[token] += [] {}};
    Apply{control, onTest -= token};
    Apply{control, onTest -= token};
}

TEST(EventTokenFormDeathTest, a_second_removal_through_one_token_stops) {
    EXPECT_DEATH(remove_twice(), "empty token");
}

// Neither form with a token goes in a Preset or a Template: a description is applied many times
// and later, and the token would be overwritten or written through a dangling pointer. That is a
// static_assert in their constructors, which a requires-expression cannot see, so it is checked by
// hand: uncomment one line, and the build stops naming borrows_token.
//
//     EventToken token;
//     wxl::Preset const look{onTest[token] += [] {}};
//     wxl::Preset const off{onTest -= token};

}  // namespace

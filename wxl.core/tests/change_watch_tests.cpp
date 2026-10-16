#include <gtest/gtest.h>

#include <crtdbg.h>

import std;
import wxl.core;

using namespace wxl::core;

namespace {

// An owner as an application has one: it keeps its watches as members and hears every
// change through a member of its own.
struct Listener {
    std::vector<int> heard;
    std::vector<list_change> changes;

    void valueChanged(int const& value) noexcept { heard.push_back(value); }
    void listChanged(list_change const& change) noexcept { changes.push_back(change); }
};

using value_watch = change_watch<observable<int const>, &Listener::valueChanged>;
using list_watch = change_watch<observable_list<int const>, &Listener::listChanged>;

// The node stands in the field's list by its address, so there is nothing to copy or move
// it into.
static_assert(!std::is_copy_constructible_v<value_watch>);
static_assert(!std::is_move_constructible_v<value_watch>);
static_assert(!std::is_copy_assignable_v<value_watch>);
static_assert(!std::is_move_assignable_v<value_watch>);

TEST(ChangeWatch, calls_its_owner_on_every_change) {
    observable<int> value{0};
    Listener owner;
    value_watch watch{value, owner};
    EXPECT_TRUE(watch.attached());

    value.set(1);
    value.set(1);  // no change, nothing to hear
    value.set(2);
    EXPECT_EQ(owner.heard, (std::vector<int>{1, 2}));
}

// An empty watch stands nowhere: the field changes unheard until the watch is attached.
TEST(ChangeWatch, starts_empty_and_is_attached_later) {
    observable<int> value{0};
    Listener owner;
    value_watch watch;
    EXPECT_FALSE(watch.attached());

    value.set(1);
    EXPECT_TRUE(owner.heard.empty());

    watch.attach(value, owner);
    EXPECT_TRUE(watch.attached());
    value.set(2);
    EXPECT_EQ(owner.heard, (std::vector<int>{2}));
}

// An empty watch has nothing to take off.
TEST(ChangeWatch, an_empty_watch_goes_quietly) {
    { value_watch watch; }
    SUCCEED();
}

// The watch comes off with its end, and the field, living on, calls the owner no more --
// while the other watchers of the field go on hearing it.
TEST(ChangeWatch, comes_off_the_field_when_it_goes) {
    observable<int> value{0};
    Listener owner;
    int others = 0;
    value.on_change([&others](int const&) noexcept { ++others; });
    {
        value_watch watch{value, owner};
        value.set(1);
    }
    value.set(2);
    EXPECT_EQ(owner.heard, (std::vector<int>{1}));
    EXPECT_EQ(others, 2);
}

// A model hands out its field read-only, and the watch is on the field behind it.
TEST(ChangeWatch, watches_a_field_seen_read_only) {
    observable<int> field{0};
    observable<int const>& readonly = field;
    Listener owner;
    value_watch watch{readonly, owner};

    field.set(5);
    EXPECT_EQ(owner.heard, (std::vector<int>{5}));
}

// Two watches on one field are two nodes, called in the order they were attached; one going
// leaves the other where it stands.
TEST(ChangeWatch, two_watches_on_one_field) {
    observable<int> value{0};
    Listener first;
    Listener second;
    value_watch firstWatch{value, first};
    {
        value_watch secondWatch{value, second};
        value.set(1);
    }
    value.set(2);
    EXPECT_EQ(first.heard, (std::vector<int>{1, 2}));
    EXPECT_EQ(second.heard, (std::vector<int>{1}));
}

// On a list the watch hears what changed, exactly as the list says it: a reset after an
// assign, with the new size.
TEST(ChangeWatch, watches_a_list) {
    observable_list<int> list;
    Listener owner;
    {
        list_watch watch{list, owner};
        list.push_back(1);
        list.assign({4, 5, 6});
        list.clear();
    }
    list.push_back(2);
    EXPECT_EQ(owner.changes, (std::vector<list_change>{
                                 {list_change::inserted, 0, 1},
                                 {list_change::reset, 0, 3},
                                 {list_change::reset, 0, 0},
                             }));
}

// Each broken rule stops the process: what would follow is an event holding a node that
// is gone, or a watch calling an owner that did not ask. The report goes to stderr rather
// than to a dialog that would hang the run.
void without_a_dialog() {
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
}

void attach_twice() {
    without_a_dialog();
    observable<int> first{0};
    observable<int> second{0};
    Listener owner;
    value_watch watch{first, owner};
    watch.attach(second, owner);
}

TEST(ChangeWatchDeathTest, attaching_twice_stops) {
    EXPECT_DEATH(attach_twice(), "a change_watch is attached twice");
}

// Declared before the field it watches, a watch outlives it: the field lets go of the node
// as it ends, and the watch would take itself off a field that is gone.
void outlive_the_field() {
    without_a_dialog();
    Listener owner;
    value_watch watch;
    {
        observable<int> value{0};
        watch.attach(value, owner);
    }
}

TEST(ChangeWatchDeathTest, outliving_its_field_stops) {
    EXPECT_DEATH(outlive_the_field(), "a change_watch outlived its field");
}

// The node is the watch's: whoever takes it off the field by its address takes away what
// the watch's destructor counts on.
void take_off_by_someone_else() {
    without_a_dialog();
    observable<int> value{0};
    Listener owner;
    value_watch watch{value, owner};
    using node_t = observable<int const>::changed_event::func_t;
    static_cast<void>(value.remove_change(cookie_t{static_cast<node_t*>(&watch)}));
}

TEST(ChangeWatchDeathTest, taken_off_by_someone_else_stops) {
    EXPECT_DEATH(take_off_by_someone_else(), "or was taken off it by someone else");
}

// An owner that destroys its watch from inside the call that reached it: the event would
// let go of the node only after the call returns, into memory that is gone.
struct SelfDestroying {
    void valueChanged(int const&) noexcept { watch.reset(); }

    std::optional<change_watch<observable<int const>, &SelfDestroying::valueChanged>> watch;
};

void destroy_from_inside_its_call() {
    without_a_dialog();
    observable<int> value{0};
    SelfDestroying owner;
    owner.watch.emplace(value, owner);
    value.set(1);
}

TEST(ChangeWatchDeathTest, destroyed_from_inside_its_own_call_stops) {
    EXPECT_DEATH(destroy_from_inside_its_call(),
                 "a change_watch is destroyed from inside its own call");
}

}  // namespace

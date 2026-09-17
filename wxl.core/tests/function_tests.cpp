#include <gtest/gtest.h>

#include <crtdbg.h>
#include <stdlib.h>

import std;
import wxl.core;



// The node itself: one virtual call over a capture, owned by whoever asked for
// it. `function` next door is one way to own it; an event, which links it into
// a list and deletes it by cookie, is the other.
namespace {

using wxl::core::impl::func_body;

TEST(FuncBodyTest, InvokesTheWrappedCallable) {
    const auto fn = func_body<int(int, int)>::create([](int a, int b) noexcept { return a + b; });

    EXPECT_EQ((*fn)(2, 3), 5);

    delete fn.get();
}

TEST(FuncBodyTest, SupportsVoidReturnAndSideEffects) {
    int calls = 0;
    const auto fn = func_body<void(int)>::create([&calls](int n) noexcept { calls += n; });

    (*fn)(4);
    (*fn)(5);

    EXPECT_EQ(calls, 9);

    delete fn.get();
}

TEST(FuncBodyTest, CapturesStateByValue) {
    int captured = 41;
    const auto fn = func_body<int()>::create([captured]() noexcept { return captured + 1; });

    captured = 0;  // create() copies/moves the lambda, so this must not affect fn

    EXPECT_EQ((*fn)(), 42);

    delete fn.get();
}

TEST(FuncBodyTest, ForwardsArgumentsByReference) {
    const auto fn =
        func_body<void(std::string&)>::create([](std::string& s) noexcept { s += "!"; });

    std::string s = "hi";
    (*fn)(s);

    EXPECT_EQ(s, "hi!");

    delete fn.get();
}

TEST(FuncBodyTest, WorksWithMoveOnlyCallables) {
    auto owned = std::make_unique<int>(7);
    const auto fn = func_body<int()>::create([owned = std::move(owned)]() noexcept { return *owned; });

    EXPECT_EQ((*fn)(), 7);

    delete fn.get();
}

TEST(FuncBodyTest, IsAccessedThroughItsPolymorphicBase) {
    using Fn = func_body<int(int)>;
    const auto base = Fn::create([](int n) noexcept { return n * n; });

    EXPECT_EQ((*base)(6), 36);

    delete base.get();
}

// noexcept follows the signature: the plain spelling holds callables that may
// throw and hands their exceptions to the caller, the noexcept spelling
// promises a call that cannot -- and the promise sits on operator() itself.
TEST(FuncBodyTest, PlainSignatureLetsAnExceptionThrough) {
    const auto fn = func_body<void(int)>::create([](int n) {
        if (n < 0) throw std::invalid_argument{"negative"};
    });

    EXPECT_NO_THROW((*fn)(1));
    EXPECT_THROW((*fn)(-1), std::invalid_argument);

    delete fn.get();
}

static_assert(noexcept(std::declval<func_body<void() noexcept>&>()()));
static_assert(!noexcept(std::declval<func_body<void()>&>()()));

// The two spellings are two distinct types, not one type with a flag.
static_assert(!std::is_same_v<func_body<void()>, func_body<void() noexcept>>);

}  // namespace

// The value form: the same node, held by reference count, which is what makes
// handing one around cost nothing.
namespace {

using wxl::core::function;
using wxl::core::nullable;

// A payload that counts every copy of itself, so that a test can say what
// copying the function did and did not do.
struct tally {
    int* copies;

    explicit tally(int* counter) noexcept : copies(counter) {}
    tally(tally const& other) noexcept : copies(other.copies) { ++*copies; }
    tally(tally&&) noexcept = default;
};

// A payload that says how many of it are alive, for the question of who owns
// the body and when it goes.
struct live_count {
    int* live;

    explicit live_count(int* counter) noexcept : live(counter) { ++*live; }
    live_count(live_count const& other) noexcept : live(other.live) { ++*live; }
    live_count(live_count&& other) noexcept : live(other.live) { ++*live; }
    ~live_count() { --*live; }
};

TEST(FunctionTest, HoldsAndCallsTheCallable) {
    function<int(int, int)> const add = [](int a, int b) { return a + b; };

    EXPECT_EQ(add(2, 3), 5);
}

// There is no empty function: a keeper that starts without one says so in its
// own type. And says it for free -- the absent function is the null body the
// present one already has room for.
TEST(FunctionTest, EmptinessBelongsToTheHolderNotToTheFunction) {
    nullable<function<void()>> fn;

    EXPECT_FALSE(fn.has_value());

    int calls = 0;
    fn = [&calls] { ++calls; };
    ASSERT_TRUE(fn.has_value());

    (*fn)();
    EXPECT_EQ(calls, 1);

    fn.reset();
    EXPECT_FALSE(fn.has_value());
}

static_assert(sizeof(nullable<function<void()>>) == sizeof(function<void()>));

// And the function is the body pointer and nothing else -- in size and in
// alignment. That is the whole of the trade func_sentinel makes when it reads
// an empty function out of a null pointer instead of constructing one, and it
// is asserted here rather than beside the cast: the module is compiled into
// every translation unit that imports it, a test is compiled once.
static_assert(sizeof(function<void()>) == sizeof(void*));
static_assert(alignof(function<void()>) == alignof(void*));
static_assert(sizeof(function<int(std::string&) noexcept>) == sizeof(void*));
static_assert(alignof(function<int(std::string&) noexcept>) == alignof(void*));

// Every way to an empty one is closed, including the one the sentinel used to
// have: there is no default constructor, no spelling from nullptr, and the
// body is held in a pointer whose type says it is not null.
static_assert(!std::is_default_constructible_v<function<void()>>);
static_assert(!std::is_constructible_v<function<void()>, std::nullptr_t>);

// A copy is still a copy, and an assignment still an assignment: the holder
// needs both to move its value about.
static_assert(std::is_copy_constructible_v<function<void()>>);
static_assert(std::is_copy_assignable_v<function<void()>>);

// The whole point of the type: a closure written once and handed to a dozen
// handlers, where copying the lambda itself would copy the capture a dozen
// times.
TEST(FunctionTest, ACopyIsAReferenceCountNotASecondCapture) {
    int copies = 0;
    int calls = 0;

    function<void()> const type = [payload = tally{&copies}, &calls] { ++calls; };

    ASSERT_EQ(copies, 0);

    auto const twin = type;  // what a handler capturing it by value does
    twin();
    type();

    EXPECT_EQ(copies, 0);
    EXPECT_EQ(calls, 2);
}

TEST(FunctionTest, TheBodyGoesWithTheLastCopy) {
    int live = 0;
    {
        nullable<function<void()>> fn = [payload = live_count{&live}] {};
        ASSERT_EQ(live, 1);

        auto copy = *fn;
        EXPECT_EQ(live, 1);

        fn.reset();
        EXPECT_EQ(live, 1);  // the copy still names it
    }

    EXPECT_EQ(live, 0);
}

// The body comes from the STA pool rather than from the ordinary heap, which
// is what lets a preset or a handler be made by the hundred while an interface
// is built. The pool of this binary is the one sta_allocator_tests puts up for
// every test in it.
//
// The debug CRT heap is what the probe reads, and it is only there in a debug
// build -- the same probe, and the same pair of tests, as the coroutine
// frame's in wxl.async.
#ifdef _DEBUG

TEST(FunctionTest, TakesItsBodyFromTheStaPool) {
    _CrtMemState before{}, after{}, difference{};

    // The size class is warmed first: the pool takes a page from the ordinary
    // heap when it has no room left, and that is the page's cost rather than
    // the body's.
    function<void()> const warm = [payload = std::array<char, 64>{}] { (void)payload; };

    _CrtMemCheckpoint(&before);
    function<void()> const held = [payload = std::array<char, 64>{}] { (void)payload; };
    _CrtMemCheckpoint(&after);

    EXPECT_EQ(0, _CrtMemDifference(&difference, &before, &after))
        << "the body came from the CRT heap rather than from sta_memory_pool";
}

// And the probe would have noticed. The same body under another base -- the
// one an event links into its list -- stays on the ordinary heap on purpose:
// an event is fired from whatever thread the program has, and the pool belongs
// to one.
TEST(FunctionTest, TheProbeNoticesABodyFromTheOrdinaryHeap) {
    _CrtMemState before{}, after{}, difference{};

    _CrtMemCheckpoint(&before);
    auto const node = wxl::core::impl::func_body<void()>::create(
        [payload = std::array<char, 64>{}] { (void)payload; });
    _CrtMemCheckpoint(&after);

    EXPECT_NE(0, _CrtMemDifference(&difference, &before, &after));

    delete node.get();
}

#endif

// The handle is shared, so const on one copy could not say anything about a
// body the others also name. Which is why operator() is const -- and why a
// mutable lambda still keeps its own state across calls.
TEST(FunctionTest, CallsAMutableCallableThroughAConstHandle) {
    function<int()> const next = [n = 0]() mutable { return ++n; };

    EXPECT_EQ(next(), 1);
    EXPECT_EQ(next(), 2);
}

TEST(FunctionTest, ForwardsArgumentsByReference) {
    function<void(std::string&)> const shout = [](std::string& s) { s += "!"; };

    std::string s = "hi";
    shout(s);

    EXPECT_EQ(s, "hi!");
}

TEST(FunctionTest, PlainSignatureLetsAnExceptionThrough) {
    function<void(int)> const check = [](int n) {
        if (n < 0) throw std::invalid_argument{"negative"};
    };

    EXPECT_NO_THROW(check(1));
    EXPECT_THROW(check(-1), std::invalid_argument);
}

// The signature is read off the callable, `noexcept` and result included.
void plain_function(int);
void nothrow_function(int) noexcept;

static_assert(std::is_same_v<decltype(function{[](int) {}}), function<void(int)>>);
static_assert(std::is_same_v<decltype(function{[](int) noexcept {}}), function<void(int) noexcept>>);
static_assert(std::is_same_v<decltype(function{[](int) { return 1L; }}), function<long(int)>>);
static_assert(std::is_same_v<decltype(function{[n = 0]() mutable { return ++n; }}), function<int()>>);
static_assert(std::is_same_v<decltype(function{&plain_function}), function<void(int)>>);
static_assert(std::is_same_v<decltype(function{&nothrow_function}), function<void(int) noexcept>>);

// Copying one deduces what it already is, rather than reading the signature
// off `function`'s own call operator.
static_assert(
    std::is_same_v<decltype(function{std::declval<function<void(int)> const&>()}), function<void(int)>>);

// The two spellings stay two types here as well.
static_assert(!std::is_same_v<function<void()>, function<void() noexcept>>);

// A callable of the wrong shape is refused at the point where it is handed
// over, by the same concept that guards create().
static_assert(std::is_constructible_v<function<void(int)>, decltype([](int) {})>);
static_assert(!std::is_constructible_v<function<void(int)>, decltype([](std::string) {})>);
static_assert(!std::is_constructible_v<function<void(int) noexcept>, decltype([](int) {})>);

// A result that can be empty takes a callable that returns nothing; one that
// cannot be empty does not.
static_assert(std::is_constructible_v<function<std::optional<int>(int)>, decltype([](int) {})>);
static_assert(std::is_constructible_v<function<nullable<int>(int)>, decltype([](int) {})>);
static_assert(std::is_constructible_v<function<std::optional<int>(int) noexcept>,
                                      decltype([](int) noexcept {})>);
static_assert(!std::is_constructible_v<function<std::optional<int>(int) noexcept>,
                                       decltype([](int) {})>);
static_assert(!std::is_constructible_v<function<int(int)>, decltype([](int) {})>);

TEST(FunctionTest, AnOptionalResultTakesAValueAnOptionalOrNothing) {
    function<std::optional<int>(int)> const value = [](int n) { return n * 2; };
    function<std::optional<int>(int)> const maybe = [](int n) -> std::optional<int> {
        if (n < 0) return std::nullopt;
        return n;
    };
    int calls = 0;
    function<std::optional<int>(int)> const nothing = [&calls](int) { ++calls; };
    function<nullable<int>(int)> const nothing_nullable = [](int) {};

    EXPECT_EQ(value(21), 42);
    EXPECT_EQ(maybe(-1), std::nullopt);
    EXPECT_EQ(maybe(7), 7);
    EXPECT_EQ(nothing(1), std::nullopt);
    EXPECT_EQ(calls, 1);
    EXPECT_FALSE(nothing_nullable(1).has_value());
}

// The one way left to call nothing: dereference a nullable that is empty
// without having asked. That is a mistake in the program, not a case to handle
// -- there is no result to invent and nothing sensible to do -- so it names the
// reason and aborts, in every build. See not_null's death test for the CRT
// settings.
void call_an_empty_one() {
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);

    nullable<function<void()>> const nothing;

    (*nothing)();
}

TEST(FunctionDeathTest, AnEmptyFunctionTakesTheProcessDown) {
    EXPECT_DEATH(call_an_empty_one(), "");
}

}  // namespace

// The concept behind the constraint on create(). Static assertions rather than tests: what
// is checked is that a callable of the wrong shape is refused, and a refusal is not
// something a test can call.
namespace {
using wxl::core::invocable;

auto takes_nothing = []() noexcept {};
auto takes_int = [](int) noexcept {};
auto returns_int = []() noexcept { return 1; };
auto returns_int_from_int = [](int) noexcept { return 1; };
auto may_throw = [] {};

static_assert(invocable<decltype(takes_nothing), void()>);
static_assert(invocable<decltype(takes_int), void(int)>);

// Arity and argument types are the obvious half.
static_assert(!invocable<decltype(takes_int), void()>);
static_assert(!invocable<decltype(takes_nothing), void(int)>);

// The half is_invocable_r would have let through: to it, a void signature means "the result
// is discardable", while func_body::create() wraps the call in `return fn_(...)`.
static_assert(!invocable<decltype(returns_int), void()>);
static_assert(std::is_invocable_r_v<void, decltype(returns_int)&>);

// A non-void signature takes anything convertible, as usual.
static_assert(invocable<decltype(returns_int_from_int), int(int)>);
static_assert(invocable<decltype(returns_int_from_int), long(int)>);
static_assert(!invocable<decltype(takes_int), int(int)>);

// Reference and const-reference callables are decayed first, so the same object passes
// however it is spelled at the call.
static_assert(invocable<decltype(takes_nothing)&, void()>);
static_assert(invocable<const decltype(takes_nothing)&, void()>);

// noexcept is part of a function type, so the two signatures are two different types and
// ask for different things. create() asks for whichever its own signature spells: an
// event's function is spelled noexcept and refuses a callable that may throw, while the
// plain spelling takes it and hands its exceptions to the caller.
static_assert(invocable<decltype(takes_nothing), void() noexcept>);
static_assert(!invocable<decltype(may_throw), void() noexcept>);
static_assert(invocable<decltype(may_throw), void()>);
static_assert(invocable<decltype(takes_int), void(int) noexcept>);
static_assert(!invocable<decltype(takes_nothing), void(int) noexcept>);
static_assert(!invocable<decltype(returns_int), void() noexcept>);
}  // namespace

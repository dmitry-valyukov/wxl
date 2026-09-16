module;

#include "abi.h"

export module wxl.core:function;

import :checks;
import :compressed_optional;
import :intrusive_ptr;
import :not_null;
import :refcounted;
import :sta_allocator;
import std;

export namespace wxl::core {

namespace impl {
template <class D>
struct Dummy {
};

// The signature-shaped half of invocable. A concept cannot be specialized -- neither
// explicitly nor partially, the standard forbids both -- so the pattern matching on
// `R(Args...)` is done by a class template, which can, and the concept is a one-line
// front for it.
template <class F, class Signature>
struct invocable_t : std::false_type {
};

// Calling `F&` with `Args...` gives nothing at all. A requires-expression rather than
// invoke_result_t, which is ill-formed -- not merely false -- when the call does not
// compile, and both halves of the && below are instantiated regardless.
template <class F, class... Args>
concept void_result = requires {
    { std::declval<F&>()(std::declval<Args>()...) } -> std::same_as<void>;
};

// `noexcept` is part of a function type, so `R(Args...)` and `R(Args...) noexcept` are two
// different types and each needs its own specialization. The second is the one an event
// asks for: its func_body::operator() is noexcept, and this is what makes the compiler
// check that promise at the subscription instead of leaving it to std::terminate.
template <class F, class R, class... Args>
struct invocable_t<F, R(Args...) noexcept>
    : std::bool_constant<
          std::is_nothrow_invocable_r_v<R, F&, Args...> &&
          (!std::is_void_v<R> || void_result<F, Args...>)> {
};

template <class F, class R, class... Args>
struct invocable_t<F, R(Args...)>
    : std::bool_constant<
          std::is_invocable_r_v<R, F&, Args...> &&
          // is_invocable_r<void, ...> is happy with any result -- to it, void means "the
          // result is discardable". func_body is stricter: it wraps the call in
          // `return fn_(...)`, and a void function cannot return a value. So a void
          // signature here asks for a void result.
          (!std::is_void_v<R> || void_result<F, Args...>)> {
};

}  // namespace impl

/**
 * `F` can be called the way `Signature` says, and its result fits.
 *
 * Written as the signature, not as an argument list, because that is how the thing being
 * subscribed to is declared anyway -- `event<void(int)>`, `function<void(int)>` -- and
 * because an argument list says nothing about the result. At the point of use it reads
 * `template <invocable<void(int)> F>`.
 *
 * The value of it is where the error appears: a callable of the wrong shape handed to
 * func_body::create() breaks inside the wrapper it generates, several frames away from the
 * mistake, and with a constraint it is refused at the call.
 */
template <class F, class Signature>
concept invocable = impl::invocable_t<std::decay_t<F>, Signature>::value;

namespace impl {

// The signature, assembled back from its pieces. Two concrete spellings
// rather than one `R(T...) noexcept(NX)`, and not for style: MSVC matches a
// dependent noexcept(NX) correctly but *re-forms* the type without it -- a
// class deriving from Base<func_body<R(T...) noexcept(NX), Base>> ended up
// based on the throwing spelling of itself while NX said otherwise. The
// trait keeps every re-formed mention of the signature concrete.
template <bool NX, class R, class... T>
struct sig {
    using type = R(T...);
};

template <class R, class... T>
struct sig<true, R, T...> {
    using type = R(T...) noexcept;
};

template <bool NX, class R, class... T>
using sig_t = typename sig<NX, R, T...>::type;

template <class Signature, template <class> class Base = Dummy>
class func_body;

// One body for both spellings: `noexcept(NX)` in a partial specialization is
// deduced, so `func_body<void(int)>` arrives with NX = false and
// `func_body<void(int) noexcept>` with NX = true, and the primary template is
// never instantiated.
//
// This is the callable itself, one virtual call over a capture laid out
// inside the node. It is spelled out here rather than hidden because the two
// things built on it need different halves: `function` next door needs a
// value with a reference count, an `event` needs a node it can link into a
// list and delete by cookie. `Base` is where they differ and all they differ
// in.
template <typename R, typename... T, bool NX, template <class> class Base>
class func_body<R(T...) noexcept(NX), Base>
    : public Base<func_body<sig_t<NX, R, T...>, Base>>
{
    using body_type = func_body<sig_t<NX, R, T...>, Base>;

public:
    /// Whether calling this may throw is the signature's own promise, checked where the
    /// callable is handed in rather than left to std::terminate. An event spells its
    /// signature `noexcept` and gets callbacks that cannot throw -- called out of a list,
    /// from a fire() with no result and no business abandoning the rest halfway, they have
    /// nobody to throw to. A callable whose work can fail -- a preset's setters walking
    /// into WinRT, say -- is held by the plain spelling, and its exceptions are the
    /// caller's to handle.
    virtual R operator()(T... args) noexcept(NX) = 0;
    virtual ~func_body() = default;

    /// Makes a callback out of `fn` and hands it over: the caller owns it and deletes it,
    /// or gives it to something that does -- an event, for one, and `function` for
    /// another.
    template <invocable<sig_t<NX, R, T...>> F>
    [[nodiscard]] static not_null<body_type> create(F&& fn) {
        using Fn = std::decay_t<F>;
        struct impl : public body_type {
            Fn fn_;
            explicit impl(Fn value) : fn_(std::move(value)) {}
            R operator()(T... args) noexcept(NX) override {
                return fn_(std::forward<T>(args)...);
            }
        };

        return as_not_null<body_type>(new impl(std::forward<F>(fn)));
    }
};

// What a value function's body stands on: the reference count, and the STA
// pool under the allocation -- sta_refcounted is both, and it sits in the
// `Base` hook because that is where the hook is, and because an event fills
// the same hook with its list node and keeps the allocation it has.
//
// The pool's terms come with it, and they are the holder's to meet: a body is
// made and released on the pool's thread, and inside the pool's life. So no
// function at namespace scope, where the constructor would run before the
// pool and the destructor after it -- one that must be written once and
// reused is written as a function returning it -- and a holder that outlives
// the pool gives its function back before the end, the way wWinMain does with
// its teardown handler and sta_loop::stop() with its wake-up.
template <class>
using counted = sta_refcounted;

// The signature a pointer names: a pointer to an `operator()`, with the object
// and its const-ness dropped, or a plain function pointer as it is. The
// primary is defined and empty on purpose -- a type that names no signature
// has to be an ordinary substitution failure, so that the deduction guide at
// the end is passed over rather than hard-erroring on, say, an int.
template <class M>
struct signature_from {};

template <class R, class C, class... A, bool NX>
struct signature_from<R (C::*)(A...) noexcept(NX)> {
    using type = sig_t<NX, R, A...>;
};

template <class R, class C, class... A, bool NX>
struct signature_from<R (C::*)(A...) const noexcept(NX)> {
    using type = sig_t<NX, R, A...>;
};

template <class R, class... A, bool NX>
struct signature_from<R (*)(A...) noexcept(NX)> {
    using type = sig_t<NX, R, A...>;
};

// The signature a callable declares. A lambda declares it on its call
// operator; a function pointer is one already. A callable whose `operator()`
// is overloaded or a template -- a generic lambda, for one -- declares no
// single signature and lands on the empty primary above: that one has to be
// given its signature in writing.
//
// `noexcept` is carried across, so a callable that promised it gets a
// `function` that promises it too. The two spellings are different types, and
// deducing the weaker one would quietly drop a promise the author wrote down.
template <class F, class = void>
struct declared_signature : signature_from<F> {};

template <class F>
struct declared_signature<F, std::void_t<decltype(&F::operator())>>
    : signature_from<decltype(&F::operator())> {};

// Defined below the class it is about: the one thing that may make a function
// with nothing in it, and the one thing that can tell.
template <class Signature>
struct func_sentinel;

}  // namespace impl

/**
 * A callable of one signature, held by value: the house's erased callback.
 *
 * ```
 * core::function<void(wchar_t)> const type = [display, calc](wchar_t key) { ... };
 * ```
 *
 * From there it is copied into whatever needs it, and a copy is a reference count
 * rather than a second capture. That is what it is for: a closure over a few
 * wrappers, written once and handed to a dozen handlers, where copying the lambda
 * itself would copy the capture a dozen times.
 *
 * The signature may be left to the compiler -- `core::function const type =
 * [display, calc](wchar_t key) { ... };` reads it off the lambda, `noexcept` and
 * all. It cannot be read off a generic lambda, which declares no single signature;
 * that one is written out.
 *
 * The body is made once, on the way in, and shared from then on -- which is also
 * the rule for where copies may go: the count is not atomic, as nothing here is,
 * so a function belongs to one thread. Calling it from another is fine; copying it
 * there is not.
 *
 * There is no empty function. It has no default constructor and no way to be
 * emptied, so holding one is holding a callable, and the caller that has one never
 * asks whether there is anything in it. A keeper that really starts without a
 * handler -- a teardown, a wake-up, a link click -- says so in its own type, with
 * `nullable<function<...>>`: the empty state costs nothing there, because the
 * absent function *is* the null body pointer, and the question is asked where the
 * answer can still be no.
 *
 * Not std::function, for two reasons that outlast this class: the capture lives in
 * the node itself rather than in storage of its own, and a header that mentions
 * this one has not thereby pulled in the textual std library -- which matters
 * wherever `import std` and cppwinrt meet.
 */
template <class Signature>
class function;

// Both spellings, one body, the way func_body above does it: `function<void()>`
// and `function<void() noexcept>` stay two distinct types.
template <class R, class... T, bool NX>
class function<R(T...) noexcept(NX)>
{
    using body_t = impl::func_body<impl::sig_t<NX, R, T...>, impl::counted>;

    friend struct impl::func_sentinel<impl::sig_t<NX, R, T...>>;

public:
    using result_type = R;

    /// From any callable the signature fits, checked here rather than inside the
    /// wrapper, where it would fail several frames away from the mistake. The body
    /// is allocated once, right here, and every copy from now on shares it.
    template <invocable<impl::sig_t<NX, R, T...>> F>
        requires(!std::same_as<std::decay_t<F>, function>)
    //
    // Adopting, not adding: the pointer overload of not_null takes over the
    // reference an object deriving from refcounted is born with, which is the
    // one create() hands over.
    function(F&& fn) : body_{body_t::create(std::forward<F>(fn)).get()} {}

    /// Calls it.
    ///
    /// Const, and not as a courtesy: every copy names the same body, so const on
    /// one of them could not say anything about that body anyway -- and a function
    /// captured by a lambda is const inside it, which is where most of them are
    /// called from.
    R operator()(T... args) const noexcept(NX) {
        return (*body_.get())(std::forward<T>(args)...);
    }

private:
    not_null<intrusive_ptr<body_t>> body_;
};

/// The signature, read off the callable itself: `core::function type = [](wchar_t key)
/// { ... };`.
template <class F>
function(F) -> function<typename impl::declared_signature<F>::type>;

namespace impl {

// What makes `nullable<function<...>>` cost nothing over a function: the
// absent one is the null body -- a bit pattern the type has room for and no
// constructor for.
//
// No constructor for it on purpose, and that is what this has to work around.
// An empty function is not a state the type offers: `function{nullptr}` does
// not compile, the body is held in a pointer that says in its own type that it
// is not null, and there is no spelling anywhere that makes one without a
// callable. So the empty one is not constructed here -- a null body pointer is
// read as the function it is the whole of, which is the same cast the WinRT
// projections make between a wrapper and the ABI pointer underneath it.
//
// A pointer variable is the storage, and no buffer or alignment dance is
// wanted: the terms of the trade are that the function *is* one pointer -- in
// size and in alignment -- so a pointer is exactly the right shape to read it
// out of. Those terms are what function_tests.cpp asserts, once, rather than
// here, where every translation unit seeing this module would pay for them.
// Everything that then touches the sentinel copies or destroys a null pointer,
// and intrusive_ptr does both without reading through it.
template <class Signature>
struct func_sentinel {
    static function<Signature> sentinel() noexcept {
        void* const empty = nullptr;
        return *reinterpret_cast<const function<Signature>*>(&empty);
    }

    // Read back the way sentinel() wrote it, and for the same reason: the
    // function is one pointer, and the empty one is that pointer holding null.
    // Nothing here asks the wrapper -- by its type it has no such state, and
    // every accessor it offers says so to the optimizer.
    static bool is_sentinel(function<Signature> const& fn) noexcept {
        return *reinterpret_cast<void* const*>(&fn) == nullptr;
    }
};

}  // namespace impl

// The extension point compressed_optional.ixx documents, keyed on every
// function at once. Its effect is the whole reason the empty function could be
// taken away: what used to be a state of the type is now a state of the holder
// that admits to having one, and it still fits in the same pointer.
template <class Signature>
struct optional_selector<function<Signature>> {
    using nullable = compressed_optional<function<Signature>, impl::func_sentinel<Signature>>;
};

}  // export namespace wxl::core

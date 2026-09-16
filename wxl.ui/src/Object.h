#pragma once

// wxl::Object / wxl::DependencyObject -- the two levels "given from above"
// hand-written, never generated, and the root every
// generated wrapper's inheritance chain ends at.
//
// wxl::Statics lives here too, and is the other half of the same statement:
// what a generated class derives from when it is not a wrapper at all.
//
// core.h is where the wxl.core import lives, for every wxl header alike.
#include "core.h"

struct IInspectable;

namespace wxl {

// The base of a WinRT class that is nothing but its static members.
//
// It carries no state, no Impl and no behaviour -- everything it has is
// deleted. What it carries is a name: `class X final : public Statics` says
// at a glance that X is a place to hang functions, where a deleted default
// constructor alone would only have said that X cannot be default-built.
//
// WinRT gives such a class System.Object as its base like every other, but
// there is nothing to inherit for: an instance cannot be made, so an Object
// wrapper for one would be a smart pointer that is always null over an Impl
// field that is never filled.
//
// Not to be confused with impl::Statics<I>, the cached activation-factory
// proxy a static member's body calls through. That one is private, is a
// template, and is about reaching the factory; this one is about saying
// what the class is.
class Statics {
public:
    Statics() = delete;
    ~Statics() = delete;
    Statics(Statics const&) = delete;
    Statics(Statics&&) = delete;
    Statics& operator=(Statics const&) = delete;
    Statics& operator=(Statics&&) = delete;
};

// Extracts the (Impl level, interface) pair out of a pointer-to-data-member
// used as a non-type template argument: &Button::Impl::button_ has type
// `winrt::...::IButton Button::Impl::*`, so impl_t = Button::Impl and
// interface_t = winrt::...::IButton. The specialization re-declares
// `member` at the specific type `T C::*` purely so its C/T become nameable
// -- the standard "recover the pieces of a pointer-to-member NTTP" idiom.
template <auto member>
struct ImplTraits;

template <typename C, typename T, T C::*member>
struct ImplTraits<member> {
    using class_t = C;
    using member_t = T;
};

template <auto member>
using class_type = typename ImplTraits<member>::class_t;

template <auto member>
using member_type = typename ImplTraits<member>::member_t;

// The root of the public wrapper hierarchy. Holds exactly one data member,
// the intrusive pointer to the Impl chain -- and it is the *only* level in
// the whole hierarchy that holds anything at all, which is what makes
// public inheritance across the chain safe (slicing can lose derived-only
// methods, never state).
//
// `Impl` is deliberately only declared here: it is defined in Object.impl.h,
// which is the private, winrt-facing side. That is the module-visibility
// boundary wxl's compile-time goal rests on -- no consumer of this header
// parses a cppwinrt projection header.
class Object {
public:
    // Declared, never defined in a public header. Publishing the name gives
    // a consumer nothing -- every use of it goes through a
    // pointer-to-member that cannot be formed without the definition, which
    // lives on the private side. What it does give is the one thing the
    // wrappers need from each other: a member of one class naming the Impl
    // field of another, to hand that object to WinRT with no QueryInterface.
    //
    // It is also the single door into every wrapper's private side, and the
    // one name each of them declares a friend. Generated member bodies need
    // what no public interface grants -- the wrapped WinRT object of an
    // argument they were handed, and a way to build a wrapper around a
    // WinRT object a call returned -- and those are operations *between*
    // wrappers, which ordinary protected access cannot express, reaching as
    // it does only into one's own base. Object's own privates it reaches
    // without being told: a nested class is a member.
    class Impl;

    Object(Object const&) noexcept;
    Object(Object&&) noexcept;
    Object& operator=(Object const&) noexcept;
    Object& operator=(Object&&) noexcept;
    ~Object();

    // Lazy per-object interface cache, keyed by a pointer-to-data-member
    // naming which Impl level owns the interface:
    // get<&Button::Impl::button_>(). The first call does the one real
    // QueryInterface this object will ever do for that interface and stores
    // the result; every later call reads the field back. Defined in
    // Object.impl.h, so only the private side can instantiate it.
    //
    // Public because a generated member reaches it on the object it was
    // *handed*, not only on itself -- and protected access in C++ never
    // reaches into another object.
    //
    // Const because every member of every wrapper is: a wrapper is a smart
    // pointer to its Impl chain, so its own constness says nothing about
    // the object behind it.
    template <auto member>
    const member_type<member>& get() const;

    // This wrapper as another wrapper type, when the object really is one: a
    // QueryInterface, wrapped. An event hands its handler the sender as a bare
    // Object; try_as recovers the control that fired -- `try_as<ToggleSwitch>()`
    // -- so the handler reads it from the sender instead of capturing it, which
    // a wxl handler must not do (a control is a temporary in the description
    // tree; a captured wrapper would keep a dead one alive, a captured pointer
    // would dangle).
    //
    // On a miss the result is an empty wrapper -- `operator bool` false -- and a
    // debug build asserts first: for a control's own event the sender is that
    // control, so a miss is a wiring mistake, not a case to handle at runtime.
    // A caller still checks, and an empty wrapper read as one is the silent,
    // do-nothing outcome a release build is left with.
    //
    // Defined on the private side, like get<>: it does the QueryInterface.
    template <typename T>
    T try_as() const;

    // Whether this wrapper holds an object at all: a smart pointer's own
    // question. A moved-from wrapper and the empty result of a try_as miss hold
    // none; every freshly built or handed-over one does.
    explicit operator bool() const noexcept { return impl_ != nullptr; }

    // ---- The way out of wxl, and back in ----
    //
    // Everything above is built for the one STA thread wxl is: the Impl
    // chain comes from a pool that belongs to it, and the reference count
    // is not interlocked. A WinRT component with a thread of its own --
    // Win2D's CanvasAnimatedControl runs a game loop and raises its events
    // there -- cannot be met on those terms, and wxl does not pretend
    // otherwise. It hands the object over instead: the application includes
    // the cppwinrt projection for that component and goes on under WinRT's
    // rules, where the apartment question already has an answer.
    //
    // ::IInspectable is only forward-declared here, so this pair costs a
    // consumer nothing -- naming it needs no winrt header, and only code
    // that actually crosses over includes one.

    // The wrapped object as the ABI pointer, borrowed: this wrapper goes on
    // owning it, so a caller keeping the pointer takes a reference of its
    // own -- winrt::copy_from_abi does exactly that.
    ::IInspectable* get_abi() const noexcept;

    // A wrapper around an object the caller holds, with a reference taken
    // here; the caller keeps its own. On the STA thread, like every other
    // wrapper: the Impl it allocates comes from the pool.
    //
    // What comes back is a plain Object, which is what a raw IInspectable
    // says. It is enough to hand the object to any wxl member that takes one
    // -- and every member taking a WinRT interface takes one.
    static Object copy_from_abi(::IInspectable* object) noexcept;

    // The same, taking the caller's reference over rather than adding one:
    // for a pointer that arrived already addrefed and would otherwise have
    // to be released by hand.
    static Object attach_abi(::IInspectable* object) noexcept;

protected:
    explicit Object(Impl* impl) noexcept;

    // Where the activation factory writes: the raw slot of the root's
    // wrapped IInspectable, so a generated constructor activates straight
    // into it with no temporary smart pointer and no extra AddRef.
    ::IInspectable** put_abi() noexcept;

    Impl* impl() const noexcept { return impl_.get(); }

private:
    core::intrusive_ptr<Impl> impl_;
};

// The second given-from-above level. Every generated wrapper that wraps a
// real DependencyObject-derived WinUI3 class inherits (directly or not)
// from this one.
class DependencyObject : public Object {
    using base_t = Object;

public:
    class Impl;

protected:
    explicit DependencyObject(Impl* impl) noexcept;

    friend class Object::Impl;
};

}  // namespace wxl

namespace wxl::core {

// A wrapper that may be empty, for every wrapper at once: `nullable<Style>`
// means a style or nothing, and costs exactly the pointer a Style already is.
//
// Every wxl wrapper is a smart pointer over an Impl chain, so the empty state
// exists in the representation -- but no wrapper offers it, deliberately: a
// Style that might be null would have to be tested at every use, and the one
// place that genuinely needs it would have made it everyone's problem. This
// puts it in a type of its own, and the type is `core::nullable`, the same one
// everything else optional in wxl is written with.
//
// The sentinel has to *build* an empty wrapper, and building one reaches a
// wrapper's protected constructor. `maker` is how, and it is nested inside the
// sentinel rather than offered anywhere: a class derived from T may pass T's
// protected constructor its own base, and nobody outside can name this one to
// do the same. So the empty wrapper stays as unreachable as it was when only
// Object::Impl could make one -- and, unlike then, this header is public, so
// an application can hold a nullable<Border> of its own.
template <typename T>
    requires std::derived_from<T, wxl::Object>
struct optional_selector<T> {
    struct wrapper_sentinel {
        // An empty wrapper is a null thing, and that is how it has always been
        // written: `nullable<Border> root = nullptr`. See compressed_optional.
        static constexpr bool empty_is_nullptr = true;

        static T sentinel() noexcept {
            struct maker : T {
                maker() noexcept : T{static_cast<typename T::Impl*>(nullptr)} {}
            };

            return maker{};
        }

        static bool is_sentinel(T const& value) noexcept { return !value; }
    };

    using nullable = compressed_optional<T, wrapper_sentinel>;
};

}  // namespace wxl::core

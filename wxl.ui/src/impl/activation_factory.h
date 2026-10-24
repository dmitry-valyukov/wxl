#pragma once

// winrt/base.h is the same narrow exception activation_factory.h makes: only
// winrt::impl::guid_v<I> is wanted here, as the address of an IID the slot
// keeps -- never a projection header.
#include <winrt/base.h>

#include "activation_factory.h"
#include "runtime_class_name.h"

namespace wxl::impl {

// One interface's entire statics cost, and all of it data: three words, no
// code generated per interface. Resolving is written once, in statics.cpp,
// and shared by every I<Name>Statics and I<Name>Factory interface the
// activation-factory object of a class implements.
//
// The dispatch is `fn_(slot)`, and what it returns is the address of arg1_:
// the runtime class name going in, the resolved interface coming out. Once
// resolved, fn_ is one shared function returning that address, so every
// access after the first is a single indirect call -- no guard variable, no
// test -- and nothing of it is instantiated per interface.
//
// The interface is kept for the whole process and never released: the factory
// lives as long as the runtime does, and an owning wrapper in a static would
// release it at process exit, after COM is already gone.
struct statics_slot {
    using resolve_fn = void* (*)(statics_slot*);

    // The slot's first fn_: resolves the factory as the interface arg2_ names,
    // stores it in arg1_ and patches fn_ to the settled function. Throws on
    // failure and leaves the slot alone, so the next access starts over --
    // safe here, unlike the noexcept ABI-boundary style of the activation
    // slot, because statics are only ever reached from wxl's own generated or
    // user-facing code, never from inside a callback the runtime makes.
    static void* init(statics_slot* slot);

    resolve_fn fn_;
    void* arg1_;
    void* arg2_;
};

// Usage: Statics<IOverlappedPresenterStatics>->Create() -- `Statics<I>` names
// a global, stateless, zero-size instance (a variable template, not the class
// template itself: operator-> cannot be static, so this has to be an *object*
// for that call syntax to work at all).
template <typename I>
class StaticsProxy {
public:
    // The projection type is one pointer and nothing else, so the raw pointer
    // in the slot *is* the object: viewing it as one hands generated code the
    // ordinary projection call syntax without an owning wrapper.
    I const* operator->() const {
        static_assert(sizeof(I) == sizeof(void*));
        return static_cast<I const*>(slot_.fn_(&slot_));
    }

private:
    // constinit, because the slot must be ready bytes, never startup code: an
    // initializer that stops being constant fails the build with C2127 instead
    // of quietly growing a startup function per interface.
    static statics_slot slot_;
};

// The IID is read from winrt::impl::guid_v directly, which is the variable
// winrt::guid_of<I>() would return anyway: MSVC does not accept that call as a
// constant expression, and the constinit above is what says why it matters.
template <typename I>
inline constinit statics_slot StaticsProxy<I>::slot_{
    &statics_slot::init,
    const_cast<wchar_t*>(runtime_class_name_of<I>::value),
    const_cast<winrt::guid*>(&winrt::impl::guid_v<I>)};

template <typename I>
inline constexpr StaticsProxy<I> Statics{};

} // namespace wxl::impl

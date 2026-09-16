#pragma once

#include <inspectable.h>
#include <roapi.h>
#include <unknwn.h>
#include <winstring.h>

// winrt/base.h is a narrow, deliberate exception to wxl::impl's rule of having
// no winrt/cppwinrt dependency: winrt::param::hstring and winrt::guid are tiny
// internal details here, never stored or exposed. What that rule exists to
// keep out is the heavy per-namespace projection headers, not base.h.
#include <winrt/base.h>

#include "hresult.h"
#include "runtime_class_name.h"

namespace wxl::impl {

// The activation factory of a runtime class, as the interface `iid` names --
// IActivationFactory for an activation, the class's own factory or statics
// interface for anything else. Shared by everything below and by the statics
// proxy, because the answer is the same for all of them, and so is the
// awkward case: a component shipped beside the application rather than in a
// package, which nothing has registered. See activation_factory.cpp.
HRESULT resolve_activation_factory(wchar_t const* class_name, GUID const& iid,
                                   void** factory) noexcept;

// Whether the XAML application is up, which is the only time a WinRT object
// can be made. launch.cpp sets it for the application's life, and
// resolve_activation_factory asserts on it: every first creation of a class
// passes through there, so a wxl object built at static initialization -- a
// namespace-scope object, most likely -- is caught with its cause named
// instead of failing as an unregistered class.
void set_application_launched(bool launched) noexcept;

// One class's entire activation cost, and all of it data: three words, no code
// generated per class. Everything that fills a slot in is written once, in
// activation_factory.cpp, and shared by every class.
//
// Two rules hold this design up. Both have been paid for; neither is free to
// trade away in a later change.
//
// THE DISPATCH IS `fn_(fac_, result)`, AND STAYS THAT WAY. fac_ is nothing but
// fn_'s first argument, and it changes exactly once: it is the slot itself
// until the class is resolved -- which is how init finds its own data -- and
// the factory object afterwards. That is what makes the settled state a direct
// call to ActivateInstance, one indirect call into the ABI with nothing of
// ours in between. Passing the slot instead would be tidier and would cost
// every class a thunk that only the composable ones need.
//
// WHAT IS RESOLVED ONCE IS CACHED IN THE SLOT. Walking a vtable belongs to
// init and to nowhere else. Re-reading an entry point per call turns a cached
// indirect call back into a virtual one and throws the whole scheme away, for
// the eight bytes the cached pointer occupies.
//
// arg1_ is the slot's scratch: the runtime class name going in, and on the
// composition path the class's factory interface coming out. It is a void*
// because of that second life.
struct activation_slot {
    // IActivationFactory::ActivateInstance(&instance) -- the activation ABI,
    // and the shape the dispatch keeps in every state it passes through.
    using activate_fn = HRESULT(__stdcall*)(void*, ::IInspectable**) noexcept;

    // The slot's first fn_: resolves the factory, takes ActivateInstance out
    // of its vtable and patches the slot to it. On failure it releases the
    // factory and leaves the slot alone, so the next activation starts over --
    // the fields only ever change together, and only in here.
    static HRESULT __stdcall init(activation_slot* slot, ::IInspectable** result) noexcept;

    void* fac_;
    activate_fn fn_;
    void* arg1_;
};

// The same, for a class WinRT lets you *compose* rather than activate.
//
// Which of the two a class needs cannot be read off the metadata: nearly every
// XAML class carries ComposableAttribute and nearly all of them implement
// ActivateInstance anyway -- but not all. RadialGradientBrush answers
// E_NOTIMPL, and an instance of it comes from the class's own factory
// interface, CreateInstance with no outer object, exactly as the projection
// does it. So the choice is made once, at the first activation, by trying, and
// E_NOTIMPL is the ONLY answer that means "wrong path" -- any other failure is
// an ordinary failure on the right one.
//
// A class that does answer ActivateInstance -- most of them, even here -- ends
// up in exactly the plain slot's settled state and pays nothing for having
// been declared composable. Only one that does not keeps fac_ pointing at the
// slot and dispatches through the thunk, which is the single place a hop is
// unavoidable: CreateInstance takes three parameters and returns the instance
// in the last of them.
//
// arg2_ carries the factory interface's IID going in and its cached
// CreateInstance coming out. Both halves matter: the IID is read once, and the
// entry point is kept so the thunk never touches a vtable again.
struct composition_slot : activation_slot {
    static HRESULT __stdcall init(composition_slot* slot, ::IInspectable** result) noexcept;

    void* arg2_;
};

// Activates instances of Obj, handing back the raw IInspectable* the
// activation ABI produces without adapting it to any particular interface: a
// generated constructor writes it straight into its own object's IInspectable
// field, with no temporary smart pointer and no extra AddRef/Release on the
// way, and queries every other interface lazily off that field later.
//
// wxl activates through this rather than cppwinrt's winrt::make<T>() for the
// same reason it avoids winrt::implements<>: a single-threaded, STA-tuned path
// written once here, without the generality -- and the atomics -- the
// general-purpose machinery pays for.
template <typename Obj>
class ActivationFactory {
public:
    // Not noexcept: a class that cannot be activated is an ordinary error, and
    // check_hresult throws so its HRESULT -- the one thing that says why --
    // reaches the caller instead of terminating the process.
    static void activate(::IInspectable** result) {
        check_hresult(slot_.fn_(slot_.fac_, result));
    }

private:
    // Out of class, because the initializer names the object itself, and
    // constinit there because the slot must be ready bytes, never startup code.
    static activation_slot slot_;
};

template <typename Obj>
inline constinit activation_slot ActivationFactory<Obj>::slot_{
    &ActivationFactory<Obj>::slot_,
    reinterpret_cast<activation_slot::activate_fn>(&activation_slot::init),
    const_cast<wchar_t*>(runtime_class_name_of<Obj>::value)};

// `Factory` is the projection's own factory interface, and the only thing wanted
// from it is its IID -- read from winrt::impl::guid_v directly, which is the
// variable winrt::guid_of<Factory>() would return anyway. MSVC does not accept
// that call as a constant expression here, and the constinit below is what says
// why it matters: a slot that is not constant-initialized becomes a startup
// function and a .CRT$XCU entry per class instead of ready bytes in .data.
template <typename Obj, typename Factory>
class ComposableFactory {
public:
    // Not noexcept, for the same reason as above.
    static void activate(::IInspectable** result) {
        check_hresult(slot_.fn_(slot_.fac_, result));
    }

private:
    // Out of class, because the initializer names the object itself, and
    // constinit there because the slot must be ready bytes, never startup code.
    static composition_slot slot_;
};

template <typename Obj, typename Factory>
inline constinit composition_slot ComposableFactory<Obj, Factory>::slot_{
    {&ComposableFactory<Obj, Factory>::slot_,
     reinterpret_cast<activation_slot::activate_fn>(&composition_slot::init),
     const_cast<wchar_t*>(runtime_class_name_of<Obj>::value)},
    const_cast<winrt::guid*>(&winrt::impl::guid_v<Factory>)};

} // namespace wxl::impl

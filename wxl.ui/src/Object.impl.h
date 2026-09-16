#pragma once

// The private side of the two given-from-above levels: the actual `Impl`
// definitions, the interface-cache machinery, and nothing a consumer of
// wxl ever includes. Generated <Class>.impl.h files include this (directly
// or through their base's impl header) and nothing else reaches it.
//
// This is the one side of wxl where winrt:: names are allowed to appear:
// the real cppwinrt projection types are used directly here, and the cost
// of parsing their headers is paid inside wxl's own build only.

#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Windows.Foundation.h>

#include <cassert>
#include <cstddef>
#include <cstdint>

#include "Object.h"

namespace wxl {

// Reference-counted without interlocked operations and allocated from the
// STA pool -- which is what core::sta_refcounted is, and both halves of it
// rest on the same single-main-STA-thread guarantee the rest of wxl does.
class Object::Impl : public core::sta_refcounted {
public:
    // What this wrapper stands for on the WinRT side. Every level restates
    // it, so anything needing the real type of a wxl type asks the wrapper
    // instead of keeping a table of its own.
    using winrt_t = winrt::Windows::Foundation::IInspectable;

    explicit Impl(winrt::Windows::Foundation::IInspectable inspectable) noexcept
        : inspectable_(std::move(inspectable)) {}

    Impl() noexcept = default;

    // The wrapped WinRT object itself. Every interface field further down
    // the chain is a QueryInterface of exactly this.
    winrt::Windows::Foundation::IInspectable inspectable_;

    // Hands this object to a call as the interface that field holds. Every
    // level declares one per field of its own, so a caller with a wrapper in
    // hand converts by passing it -- naming no field and knowing no level.
    operator winrt::Windows::Foundation::IInspectable const&() { return inspectable_; }

    // Lazy per-object interface cache, keyed by a pointer-to-data-member
    // naming which level owns the interface: get<&Button::Impl::button_>().
    // The first call does the one real QueryInterface this object will ever
    // do for that interface and stores the result; every later call reads
    // the field back.
    template <auto member>
    member_type<member> const& get();

    // The Impl chain of another wrapper, at the level that wrapper stands
    // for. Reaching a wrapper's own Impl is what the conversion operators
    // above are then applied to, and reaching it is exactly what only this
    // class may do.
    template <typename T>
    static typename T::Impl* get_typed(T const& wrapper) {
        return static_cast<typename T::Impl*>(static_cast<Object const&>(wrapper).impl_.get());
    }

    // ---- What one wrapper may do to another ----
    //
    // A generated member body is the only caller of these. All are static
    // and none adds anything a wrapper could not do to itself; the point is
    // purely that one wrapper may do it to *another* -- Panel reaching into
    // a UIElement it was handed -- which ordinary protected access cannot
    // express, since that only ever reaches into one's own base. Every
    // wrapper names this class as its friend, and it is the only name it
    // has to name.

    // Builds a wrapper of type `T` around a WinRT object a call returned.
    //
    // The object arrives as a temporary holding one reference, and it is
    // moved rather than copied: the Impl takes that reference over instead
    // of adding a second one only for the temporary to drop it again. What
    // the Impl fills is its own default-interface field together with the
    // root's inspectable_ -- both without a QueryInterface, since the two
    // hold the same pointer. Every other level's field stays empty for the
    // lazy cache, which is the point of it.
    // Nothing wrapped is the empty wrapper, not an Impl holding null. A
    // runtime that passes no object -- an event whose args carry nothing, a
    // property with no value set -- would otherwise cost a pool allocation
    // for an object that is not there, and hand back a wrapper whose
    // `operator bool` says it is. DispatcherQueueTimer::Tick is the one that
    // shows it: its args are always null, and it fires on a timer.
    template <typename T, typename WinRT>
    static T wrap(WinRT&& object) {
        if (!object) return empty<T>();
        return T{new typename T::Impl{std::forward<WinRT>(object)}};
    }

    // An empty wrapper of type T: no object behind it, `operator bool` false.
    // try_as returns it on a miss. Here rather than at the call because building
    // a wrapper reaches its protected constructor, which only this class -- the
    // friend every wrapper names -- may do.
    template <typename T>
    static T empty() {
        return T{static_cast<typename T::Impl*>(nullptr)};
    }

    // The wrapped object as the WinRT type a call expects. Only a collection
    // needs this: every other wrapper converts by being passed, while a
    // Collection<T> stands for whichever concrete collection the signature
    // happens to name -- so which interface to ask for is known at the call,
    // not at the wrapper.
    template <typename WinRT, typename T>
    static WinRT as(T const& wrapper) {
        typename T::Impl::winrt_t const& self = *get_typed<T>(wrapper);
        return self.template as<WinRT>();
    }

    // The args view a handler is called with, over the ABI pointer the
    // runtime passed. Returned by value although the type is non-copyable:
    // the result is a prvalue, so it is materialised straight into the
    // caller's variable and no copy or move is involved.
    template <typename Args>
    static Args make_args(void* abi) noexcept {
        return Args{abi};
    }

    // The WinRT type a wrapper stands for: a template like Collection<T> can
    // ask T what it is instead of being told separately.
    template <typename T>
    using winrt_type = typename T::Impl::winrt_t;
};

template <auto member>
member_type<member> const& Object::Impl::get() {
    auto& field = static_cast<class_type<member>&>(*this).*member;
    if (!field) {
        // First use of this interface on this object: one real
        // QueryInterface, cached in the field for the object's lifetime.
        // Non-atomic on purpose -- single main STA thread.
        //
        // The check stays even for a level's own default interface: a member
        // is emitted at the level that declares it, and the object it runs
        // on is routinely more derived than that level -- UIElement::
        // visibility() on a Button reads UIElement::Impl's field, which
        // Button's constructor never filled. Filling every level eagerly
        // would be one QueryInterface per level per construction, which is
        // what this cache exists to avoid.
        field = inspectable_.as<member_type<member>>();
    }
    return field;
}

template <auto member>
const member_type<member>& Object::get() const {
    return impl_->get<member>();
}

template <typename T>
T Object::try_as() const {
    if (impl_) {
        // winrt's own try_as: the QueryInterface, null on a miss rather than a
        // throw. T::Impl::winrt_t is the WinRT type T stands for -- the same one
        // wrap() would fill T's Impl with.
        if (auto native = impl_->inspectable_.try_as<typename T::Impl::winrt_t>()) {
            return Impl::wrap<T>(std::move(native));
        }
    }
    assert(false && "wxl: try_as to a type this object does not hold");
    return Impl::empty<T>();
}

inline ::IInspectable** Object::put_abi() noexcept {
    // The same reinterpretation winrt::put_abi performs: a winrt smart
    // pointer is one raw pointer, so the address of the slot is the address
    // of the member.
    return reinterpret_cast<::IInspectable**>(&impl_->inspectable_);
}

class DependencyObject::Impl : public Object::Impl {
public:
    using Object::Impl::Impl;

    using winrt_t = winrt::Microsoft::UI::Xaml::DependencyObject;

    // Explicitly empty: a projection *class* has no default constructor,
    // only the one from nullptr -- an interface would default to empty on
    // its own.
    winrt::Microsoft::UI::Xaml::DependencyObject dependencyObject_{nullptr};

    explicit Impl(winrt::Microsoft::UI::Xaml::DependencyObject&& object) noexcept
        : Object::Impl(object), dependencyObject_(std::move(object)) {}

    operator winrt::Microsoft::UI::Xaml::DependencyObject const&() {
        return get<&Impl::dependencyObject_>();
    }
};

}  // namespace wxl

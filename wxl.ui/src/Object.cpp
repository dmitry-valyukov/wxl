#include "Object.impl.h"

// Out-of-line on purpose: Object::Impl is incomplete in Object.h, so the
// copy/move/destroy of the intrusive_ptr member -- which has to touch the
// reference count, and therefore the type -- cannot be generated there.
// This is the whole cost of keeping every winrt:: name out of the public
// header, and it is paid once, here, rather than by each consumer.

namespace wxl {

Object::Object(Impl* impl) noexcept : impl_(impl, /*add_ref=*/false) {}

// refcounted objects start life at a count of 1, which the constructor
// above adopts; copies genuinely share that one Impl chain, which is what
// gives wxl::X reference semantics (needed to capture a wrapper by value
// in an event handler).
Object::Object(Object const&) noexcept = default;
Object::Object(Object&&) noexcept = default;
Object& Object::operator=(Object const&) noexcept = default;
Object& Object::operator=(Object&&) noexcept = default;
Object::~Object() = default;

// The boundary with the projection, in both directions. Out of line for the
// same reason everything else here is: the winrt type of the field lives on
// the private side.
//
// Both casts are the reinterpretation winrt::get_abi performs -- a winrt
// smart pointer is one raw pointer and nothing else -- and both are why this
// pair costs the public header only a forward declaration.
::IInspectable* Object::get_abi() const noexcept {
    return reinterpret_cast<::IInspectable*>(winrt::get_abi(impl_->inspectable_));
}

// A null pointer is no object, and the wrapper for it is the empty one --
// the same answer wrap() and a try_as miss give. Allocating an Impl to hold
// nothing would cost the pool a block and would leave `operator bool` saying
// there is something behind it.
Object Object::copy_from_abi(::IInspectable* object) noexcept {
    if (!object) return Impl::empty<Object>();
    auto impl = new Impl{};
    winrt::copy_from_abi(impl->inspectable_, object);
    return Object{impl};
}

Object Object::attach_abi(::IInspectable* object) noexcept {
    if (!object) return Impl::empty<Object>();
    auto impl = new Impl{};
    winrt::attach_abi(impl->inspectable_, object);
    return Object{impl};
}

DependencyObject::DependencyObject(Impl* impl) noexcept : base_t(impl) {}

}  // namespace wxl

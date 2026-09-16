#pragma once

namespace wxl::impl {

// Customization point the generator specializes per WinRT-backed
// wxl::impl::X (for ActivationFactory<Obj>, see activation_factory.h) or
// per I<Name>Statics interface (for Statics<I>, see statics.h) --
// deliberately no primary definition, so using either of those templates
// for a T nobody specialized this for is a compile error, not a silently
// wrong runtime lookup.
//
//   runtime_class_name_of<T>::value -- T's real WinRT runtime class name
//     (e.g. L"Microsoft.UI.Xaml.Controls.Grid"), used to resolve the
//     class's activation-factory object via RoGetActivationFactory.
//
// Shared, single declaration for both consumers -- ActivationFactory<Obj>
// specializes this on an Obj marker (the class being activated);
// Statics<I> specializes it directly on the statics interface I itself
// (which already uniquely identifies its class, so no separate marker
// type is needed there).
template <typename T>
struct runtime_class_name_of;

} // namespace wxl::impl

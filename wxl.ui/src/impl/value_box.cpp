#include "value_box.h"

namespace wxl::impl {
namespace {

using winrt::Windows::Foundation::IReference;
using winrt::Windows::Foundation::PropertyType;

// False, as WinRT wants to be handed it. No field: the value is a constant in
// the two methods that report it -- the one XAML reads through (GetBoolean,
// after asking get_Type) and the one the value comes back through when the
// box reaches us rather than leaves us (get_Value).
struct winrt_false : eternal_value<IReference<bool>> {
    int32_t __stdcall get_Type(int32_t* type) noexcept override {
        *type = static_cast<int32_t>(PropertyType::Boolean);
        return 0;
    }

    int32_t __stdcall GetBoolean(bool* value) noexcept override {
        *value = false;
        return 0;
    }

    int32_t __stdcall get_Value(bool* value) noexcept override {
        *value = false;
        return 0;
    }
};

// And true, which differs from false in exactly the two answers -- so it says
// only those, and inherits the rest of a boolean box from the boolean box
// that is already here.
struct winrt_true final : winrt_false {
    int32_t __stdcall GetBoolean(bool* value) noexcept override {
        *value = true;
        return 0;
    }

    int32_t __stdcall get_Value(bool* value) noexcept override {
        *value = true;
        return 0;
    }
};

// The objects themselves: two vtable pointers apiece and not a byte of state,
// in the program's own storage. Nothing constructs them and nothing can
// destroy them.
winrt_false false_value;
winrt_true true_value;

// The interface pointers, made once from those. "Taking ownership" of an
// object that counts no references costs nothing and gives nothing away: the
// Release these will perform at the end of the program does not run, and
// would do nothing if it did.
IReference<bool> const false_reference{
    static_cast<winrt::impl::abi_t<IReference<bool>>*>(&false_value),
    winrt::take_ownership_from_abi};

IReference<bool> const true_reference{
    static_cast<winrt::impl::abi_t<IReference<bool>>*>(&true_value),
    winrt::take_ownership_from_abi};

}  // namespace

IReference<bool> const& bool_reference(bool value) { return value ? true_reference : false_reference; }

}  // namespace wxl::impl

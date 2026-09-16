#pragma once

// The box a nullable property is set with -- and, for a boolean, the two of
// them the program ever needs, made by the linker rather than by anyone.
//
// WinRT spells "a value or nothing" as IReference<T>: a COM object around the
// value, or a null pointer. Handing one over is the only way to set such a
// property, and cppwinrt's own IReference<T>{value} allocates a fresh object
// off the CRT heap for every call. Two measurements decided what wxl does
// instead of that:
//
//   * XAML does not keep the box. It unboxes inside the setter and lets go
//     before returning -- the property store holds a bool, and reading the
//     property back hands out a *different*, freshly made box.
//
//   * It unboxes through IPropertyValue, not through IReference: Type() and
//     then the getter of that type. A box implementing only IReference<bool>
//     is refused outright, with E_FAIL.
//
// Together those say that a box is a temporary nobody remembers -- so for a
// type with few values there is no reason to make one per call, nor to count
// references to it. A boolean has two values, so wxl has two objects: they
// live in the program's own storage, they are never made and never unmade,
// and their AddRef and Release do nothing at all. There is no heap here, no
// interlocked increment, and no `this` to free -- the entire cost of setting
// a nullable bool is a pointer.
//
// Neither object carries a field. `winrt_false` answers with `false` from
// constants written into its methods; `winrt_true` derives from it and
// overrides the two that say which value it is. What is left of an instance
// is its vtable pointers, and nothing else exists to initialise.
//
// `eternal_value` is what makes writing such a box small. IPropertyValue asks
// about every type a property value can be -- some thirty-five getters -- and
// exactly two of them mean anything for any one box. The base refuses all of
// them and answers the whole of IUnknown and IInspectable; a box says what it
// is by defining get_Type() and its own getter, and nothing else.
//
// The interfaces are written at the ABI, in cppwinrt's own vtable structs,
// because the ordinary route -- winrt::implements plus winrt::make -- is the
// heap allocation and the reference count this file exists to avoid.

#include <winrt/Windows.Foundation.h>

namespace wxl::impl {

/// A property value that is never made and never counted: one object per
/// value, placed in the program's storage.
///
/// Derive with the interface the box really is, and define `get_Type()` and
/// the one getter that matches it, plus the interface's own accessor.
///
/// `get_Type()` is deliberately left pure here: a box that forgets to say
/// what it holds does not compile.
// cppwinrt calls every one of its ABI vtables `type` -- abi<X>::type -- so
// deriving from two of them at once leaves a class with two base classes of
// the same name, and MSVC refuses it. One named layer each is enough to tell
// them apart; nothing else is added, and neither has a member of its own.
template <typename Interface>
struct interface_vtable : winrt::impl::abi_t<Interface> {};

struct property_value_vtable : winrt::impl::abi_t<winrt::Windows::Foundation::IPropertyValue> {};

template <typename Interface>
struct eternal_value : interface_vtable<Interface>, property_value_vtable {
    // One definition covers both bases: a member with a matching signature
    // overrides the virtual function in every base subobject that declares
    // it, and the compiler puts an adjustor thunk in the second vtable.
    int32_t __stdcall QueryInterface(winrt::guid const& id, void** object) noexcept final {
        if (id == winrt::guid_of<Interface>() ||
            id == winrt::guid_of<winrt::Windows::Foundation::IInspectable>() ||
            id == winrt::guid_of<winrt::Windows::Foundation::IUnknown>() ||
            // Truthfully agile: there is no state to race over and no thread
            // this object belongs to.
            id == winrt::impl::guid_v<winrt::impl::IAgileObject>) {
            *object = static_cast<winrt::impl::abi_t<Interface>*>(this);
            return 0;
        }

        if (id == winrt::guid_of<winrt::Windows::Foundation::IPropertyValue>()) {
            *object = static_cast<winrt::impl::abi_t<winrt::Windows::Foundation::IPropertyValue>*>(
                this);
            return 0;
        }

        *object = nullptr;
        return no_interface;
    }

    // The whole of the lifetime. Two is what a live object with one more
    // outstanding reference would answer, and one is what a Release that
    // freed nothing answers; both are constants because both are true
    // forever.
    uint32_t __stdcall AddRef() noexcept final { return 2; }
    uint32_t __stdcall Release() noexcept final { return 1; }

    int32_t __stdcall GetIids(uint32_t* count, winrt::guid** ids) noexcept final try {
        winrt::com_array<winrt::guid> implemented{
            winrt::guid_of<Interface>(),
            winrt::guid_of<winrt::Windows::Foundation::IPropertyValue>()};

        std::tie(*count, *ids) = winrt::detach_abi(implemented);
        return 0;
    } catch (...) {
        return winrt::to_hresult();
    }

    /// Diagnostics only, and the one place here that allocates -- which is
    /// why it allocates when asked and not before.
    int32_t __stdcall GetRuntimeClassName(void** name) noexcept final try {
        *name = winrt::detach_abi(winrt::hstring{winrt::name_of<Interface>()});
        return 0;
    } catch (...) {
        return winrt::to_hresult();
    }

    int32_t __stdcall GetTrustLevel(winrt::Windows::Foundation::TrustLevel* level) noexcept final {
        *level = winrt::Windows::Foundation::TrustLevel::BaseTrust;
        return 0;
    }

    // Everything a property value can be asked and is not.
    int32_t __stdcall get_IsNumericScalar(bool* value) noexcept override {
        *value = false;
        return 0;
    }

    int32_t __stdcall GetUInt8(uint8_t*) noexcept override { return not_implemented; }
    int32_t __stdcall GetInt16(int16_t*) noexcept override { return not_implemented; }
    int32_t __stdcall GetUInt16(uint16_t*) noexcept override { return not_implemented; }
    int32_t __stdcall GetInt32(int32_t*) noexcept override { return not_implemented; }
    int32_t __stdcall GetUInt32(uint32_t*) noexcept override { return not_implemented; }
    int32_t __stdcall GetInt64(int64_t*) noexcept override { return not_implemented; }
    int32_t __stdcall GetUInt64(uint64_t*) noexcept override { return not_implemented; }
    int32_t __stdcall GetSingle(float*) noexcept override { return not_implemented; }
    int32_t __stdcall GetDouble(double*) noexcept override { return not_implemented; }
    int32_t __stdcall GetChar16(char16_t*) noexcept override { return not_implemented; }
    int32_t __stdcall GetBoolean(bool*) noexcept override { return not_implemented; }
    int32_t __stdcall GetString(void**) noexcept override { return not_implemented; }
    int32_t __stdcall GetGuid(winrt::guid*) noexcept override { return not_implemented; }
    int32_t __stdcall GetDateTime(int64_t*) noexcept override { return not_implemented; }
    int32_t __stdcall GetTimeSpan(int64_t*) noexcept override { return not_implemented; }
    int32_t __stdcall GetPoint(winrt::Windows::Foundation::Point*) noexcept override {
        return not_implemented;
    }
    int32_t __stdcall GetSize(winrt::Windows::Foundation::Size*) noexcept override {
        return not_implemented;
    }
    int32_t __stdcall GetRect(winrt::Windows::Foundation::Rect*) noexcept override {
        return not_implemented;
    }

    int32_t __stdcall GetUInt8Array(uint32_t*, uint8_t**) noexcept override {
        return not_implemented;
    }
    int32_t __stdcall GetInt16Array(uint32_t*, int16_t**) noexcept override {
        return not_implemented;
    }
    int32_t __stdcall GetUInt16Array(uint32_t*, uint16_t**) noexcept override {
        return not_implemented;
    }
    int32_t __stdcall GetInt32Array(uint32_t*, int32_t**) noexcept override {
        return not_implemented;
    }
    int32_t __stdcall GetUInt32Array(uint32_t*, uint32_t**) noexcept override {
        return not_implemented;
    }
    int32_t __stdcall GetInt64Array(uint32_t*, int64_t**) noexcept override {
        return not_implemented;
    }
    int32_t __stdcall GetUInt64Array(uint32_t*, uint64_t**) noexcept override {
        return not_implemented;
    }
    int32_t __stdcall GetSingleArray(uint32_t*, float**) noexcept override {
        return not_implemented;
    }
    int32_t __stdcall GetDoubleArray(uint32_t*, double**) noexcept override {
        return not_implemented;
    }
    int32_t __stdcall GetChar16Array(uint32_t*, char16_t**) noexcept override {
        return not_implemented;
    }
    int32_t __stdcall GetBooleanArray(uint32_t*, bool**) noexcept override {
        return not_implemented;
    }
    int32_t __stdcall GetStringArray(uint32_t*, void***) noexcept override {
        return not_implemented;
    }
    int32_t __stdcall GetInspectableArray(uint32_t*, void***) noexcept override {
        return not_implemented;
    }
    int32_t __stdcall GetGuidArray(uint32_t*, winrt::guid**) noexcept override {
        return not_implemented;
    }
    int32_t __stdcall GetDateTimeArray(uint32_t*, int64_t**) noexcept override {
        return not_implemented;
    }
    int32_t __stdcall GetTimeSpanArray(uint32_t*, int64_t**) noexcept override {
        return not_implemented;
    }
    int32_t __stdcall GetPointArray(uint32_t*,
                                    winrt::Windows::Foundation::Point**) noexcept override {
        return not_implemented;
    }
    int32_t __stdcall GetSizeArray(uint32_t*,
                                   winrt::Windows::Foundation::Size**) noexcept override {
        return not_implemented;
    }
    int32_t __stdcall GetRectArray(uint32_t*,
                                   winrt::Windows::Foundation::Rect**) noexcept override {
        return not_implemented;
    }

protected:
    static constexpr int32_t no_interface = static_cast<int32_t>(0x80004002);   // E_NOINTERFACE
    static constexpr int32_t not_implemented = static_cast<int32_t>(0x80004001);  // E_NOTIMPL
};

/// One of the two booleans WinRT is ever handed, and the only two that exist.
/// Safe to share precisely because the measurement showed XAML does not keep
/// the box -- and safe even if it did, since neither of these ever goes away.
winrt::Windows::Foundation::IReference<bool> const& bool_reference(bool value);

}  // namespace wxl::impl

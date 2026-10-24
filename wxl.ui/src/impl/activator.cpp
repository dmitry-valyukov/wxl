#include "activation_factory.h"

#include <cassert>

// The branch hints below are part of the tuning, not decoration: they say
// which way each test goes for the overwhelming majority of classes. Carry
// them across any rewrite of these two functions.

namespace wxl::impl {
namespace {

bool s_application_launched = false;

// IActivationFactory and every I<Class>Factory alike declare their one extra
// member directly after IInspectable, so the entry point is always vtable slot
// 6: three IUnknown slots, three IInspectable slots, then it. The read is
// unconditionally safe -- an interface without that member is not what
// QueryInterface handed back.
//
// This is init's tool and only init's: what it returns is meant to be stored
// in the slot, never fetched again per call.
template <typename Fn>
Fn abi_entry_point(void* itf) noexcept {
    return reinterpret_cast<Fn>((*static_cast<void***>(itf))[6]);
}

::IInspectable* as_inspectable(void* itf) noexcept {
    return static_cast<::IInspectable*>(itf);
}

using dll_get_activation_factory_fn = HRESULT(__stdcall*)(HSTRING, void**) noexcept;

// A WinRT component that ships beside the application rather than in a
// package: nothing registered it, so RoGetActivationFactory has nowhere to
// look and answers REGDB_E_CLASSNOTREG.
//
// The convention every unpackaged host falls back to -- cppwinrt's own
// get_activation_factory included -- is the class name itself: trim it from
// the right, one dotted piece at a time, and ask the first DLL of that name
// that loads for the factory. Win2D's Microsoft.Graphics.Canvas.CanvasDevice
// is found in Microsoft.Graphics.Canvas.dll on the third try.
//
// The module is deliberately never freed. A factory handed out of it is
// alive for as long as anything holds an object, and the slot that cached it
// is a static that outlives every such object anyway.
HRESULT resolve_factory_from_dll(wchar_t const* name, GUID const& iid, void** factory) noexcept {
    std::wstring candidate{name};

    for (size_t cut = candidate.rfind(L'.'); cut != std::wstring::npos;
         cut = candidate.rfind(L'.')) {
        candidate.resize(cut);

        HMODULE const module = ::LoadLibraryW((candidate + L".dll").c_str());
        if (!module) {
            continue;
        }

        auto const entry = reinterpret_cast<dll_get_activation_factory_fn>(
            ::GetProcAddress(module, "DllGetActivationFactory"));
        if (!entry) {
            continue;
        }

        winrt::param::hstring const text{name};
        ::IActivationFactory* activation = nullptr;
        HRESULT hr = entry(static_cast<HSTRING>(get_abi(text)),
                           reinterpret_cast<void**>(&activation));
        if (FAILED(hr)) {
            continue;
        }

        // DllGetActivationFactory hands out IActivationFactory and nothing
        // else, so a caller after the class's own factory or statics
        // interface asks the object for it here -- the same object answers
        // both, which is what makes one entry point enough.
        if (iid == __uuidof(::IActivationFactory)) {
            *factory = activation;
            return hr;
        }

        hr = activation->QueryInterface(iid, factory);
        activation->Release();
        return hr;
    }

    return REGDB_E_CLASSNOTREG;
}

} // namespace

void set_application_launched(bool launched) noexcept {
    s_application_launched = launched;
}

HRESULT resolve_activation_factory(wchar_t const* class_name, GUID const& iid,
                                   void** factory) noexcept {
    assert(s_application_launched &&
           "wxl: a WinRT object is being created before the application has launched -- most "
           "likely a namespace-scope or static object. Nothing of WinRT exists before "
           "wxl_launched(): describe it with Template<T> there, and it will be built where it "
           "is used.");

    winrt::param::hstring const text{class_name};
    HRESULT const hr = RoGetActivationFactory(static_cast<HSTRING>(get_abi(text)), iid, factory);

    // Only "nobody registered this" is worth a second look; any other failure
    // is a real one on the right path.
    if (hr == REGDB_E_CLASSNOTREG) [[unlikely]] {
        return resolve_factory_from_dll(class_name, iid, factory);
    }
    return hr;
}

namespace {

HRESULT resolve_factory(void const* name, void** factory) noexcept {
    return resolve_activation_factory(static_cast<wchar_t const*>(name),
                                      __uuidof(::IActivationFactory), factory);
}

using create_fn = HRESULT(__stdcall*)(void*, ::IInspectable*, ::IInspectable**,
                                      ::IInspectable**) noexcept;

// The settled state of a class that has to be composed, and the only hop in
// the whole scheme: CreateInstance takes three parameters and returns the
// instance in the last of them, so the slot's own shape cannot carry it. Both
// operands are read straight out of the slot -- arg2_ is the entry point init
// already resolved, so nothing here walks a vtable.
//
// `inner` is the composed object seen without its delegation, and with no
// outer object it is the same object as the instance, so the reference it
// arrives with is released here -- otherwise every composable object would be
// born with one too many. The projection's own constructor does the same, by
// letting a local go out of scope.
HRESULT __stdcall thunk(composition_slot* slot, ::IInspectable** result) noexcept {
    auto const create = reinterpret_cast<create_fn>(slot->arg2_);
    ::IInspectable* inner = nullptr;
    HRESULT const hr = create(slot->arg1_, nullptr, &inner, result);
    if (inner) {
        inner->Release();
    }
    return hr;
}

} // namespace

HRESULT __stdcall activation_slot::init(activation_slot* slot,
                                        ::IInspectable** result) noexcept {
    void* factory = nullptr;
    HRESULT hr = resolve_factory(slot->arg1_, &factory);
    if (FAILED(hr)) {
        return hr;
    }

    auto activate = abi_entry_point<activation_slot::activate_fn>(factory);
    hr = activate(factory, result);

    // Patched only on success, and both fields together: from here on the
    // dispatch is ActivateInstance called directly on the factory.
    if (SUCCEEDED(hr)) [[likely]] {
        slot->fac_ = factory;
        slot->fn_ = activate;
        return hr;
    }

    as_inspectable(factory)->Release();
    return hr;
}

HRESULT __stdcall composition_slot::init(composition_slot* slot,
                                         ::IInspectable** result) noexcept {
    void* factory = nullptr;
    HRESULT hr = resolve_factory(slot->arg1_, &factory);
    if (FAILED(hr)) {
        return hr;
    }

    auto const activate = abi_entry_point<activation_slot::activate_fn>(factory);
    hr = activate(factory, result);

    // The class activates like any other, which is the common case even among
    // composable ones: it settles into the plain slot's state and never sees
    // the thunk.
    if (SUCCEEDED(hr)) [[likely]] {
        slot->fac_ = factory;
        slot->fn_ = activate;
        return hr;
    }

    // Only E_NOTIMPL means the path is wrong; anything else is a real failure
    // on the right one.
    if (hr != winrt::impl::error_not_implemented) [[unlikely]] {
        as_inspectable(factory)->Release();
        return hr;
    }

    // The same factory object also implements the class's own factory
    // interface, so this is the one QueryInterface the whole scheme ever does.
    // That interface holds the object up on its own, which is why the
    // activation factory's reference is handed straight back.
    void* composable_fac = nullptr;
    hr = as_inspectable(factory)->QueryInterface(
        *static_cast<winrt::guid const*>(slot->arg2_), &composable_fac);
    as_inspectable(factory)->Release();

    if (FAILED(hr)) {
        return hr;
    }

    ::IInspectable* inner = nullptr;
    auto const create = abi_entry_point<create_fn>(composable_fac);
    hr = create(composable_fac, nullptr, &inner, result);

    if (FAILED(hr)) {
        as_inspectable(composable_fac)->Release();
        return hr;
    }

    // fac_ stays the slot -- that is what the thunk runs on -- and CreateInstance
    // is kept in arg2_ so the thunk never resolves it again.
    slot->arg1_ = composable_fac;
    slot->arg2_ = reinterpret_cast<void*>(create);
    slot->fn_ = reinterpret_cast<activation_slot::activate_fn>(&thunk);
    return hr;
}

} // namespace wxl::impl

// Continuation of framework_element_activation.cpp's probe: manual
// RoGetActivationFactory + CreateInstance from a bare console-style main()
// got as far as RPC_E_WRONG_THREAD even with a real outer identity and a
// manually-created DispatcherQueueController. Modeled after
// M:\source\WxlApp1\WxlApp1\main.cpp (an existing, working unpackaged
// WinUI3 C++/WinRT app) -- that project's OnLaunched is where real WinUI3
// object creation actually happens, driven by Application::Start itself
// (which sets up whatever thread registration a manually-created
// DispatcherQueueController alone didn't). This file borrows that
// bootstrap sequence (winrt::init_apartment, MddBootstrapInitialize,
// Application::Start), but keeps it otherwise minimal -- no Window, no
// controls, no XamlControlsResources -- just enough App/OnLaunched to run
// wxl's own composable-activation probe from inside a genuine XAML
// UI-thread context, then exit.
//
// Uses real cppwinrt (winrt::) for the Application/OnLaunched scaffolding
// only -- that's test/host scaffolding, not wxl's own production code
// (wxl's own impl:: types never depend on cppwinrt). The actual
// thing under test -- IFrameworkElementFactory::CreateInstance and the
// blind-aggregation identity check -- is the same raw WinRT COM style as
// framework_element_activation.cpp, no cppwinrt involved in that part.
#include <windows.h>
#include <unknwn.h>
#include <inspectable.h>
#include <restrictederrorinfo.h>
#include <hstring.h>
#include <roapi.h>
#include <winstring.h>

// Prevent conflict with Storyboard::GetCurrentTime, same as WxlApp1/pch.h.
#undef GetCurrentTime

#include <winrt/Windows.Foundation.h>
#include <winrt/Microsoft.UI.Xaml.h>

#include <wxl/core/bootstrap.h>
#include <wxl/core/hresult.h>

#include <cstdio>

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;

namespace {

// Same bare ABI declaration and throwaway outer identity as
// framework_element_activation.cpp -- duplicated rather than shared, since
// both files are disposable probing code, not wxl itself (see that file's
// own header comment).
struct __declspec(uuid("BD3F2272-3EFA-5F92-B759-90B1CC3E784C"))
    __declspec(novtable) IFrameworkElementFactory : ::IInspectable {
    virtual HRESULT __stdcall CreateInstance(
        ::IInspectable* baseInterface,
        ::IInspectable** innerInterface,
        ::IInspectable** value) noexcept = 0;
};

struct TestOuter : ::IInspectable {
    HRESULT __stdcall QueryInterface(REFIID riid, void** result) noexcept override {
        if (riid == __uuidof(::IUnknown) || riid == __uuidof(::IInspectable)) {
            *result = static_cast<::IInspectable*>(this);
            AddRef();
            return S_OK;
        }
        *result = nullptr;
        return E_NOINTERFACE;
    }
    ULONG __stdcall AddRef() noexcept override { return ++refs_; }
    ULONG __stdcall Release() noexcept override { return --refs_; }
    HRESULT __stdcall GetIids(ULONG* c, IID**) noexcept override { if (c) *c = 0; return S_OK; }
    HRESULT __stdcall GetRuntimeClassName(HSTRING* n) noexcept override { if (n) *n = nullptr; return S_OK; }
    HRESULT __stdcall GetTrustLevel(TrustLevel* t) noexcept override { if (t) *t = BaseTrust; return S_OK; }

private:
    ULONG refs_ = 1;
};

// Real GUID, verified against generated/cppwinrt earlier in this session
// (winrt/impl/Microsoft.UI.Xaml.0.h): E7BEAEE7-160E-50F7-8789-D63463F979FA.
constexpr GUID IID_IDependencyObject = {
    0xE7BEAEE7, 0x160E, 0x50F7, {0x87, 0x89, 0xD6, 0x34, 0x63, 0xF9, 0x79, 0xFA}
};

// The actual experiment: real IFrameworkElementFactory::CreateInstance
// aggregated under `outer`, then two identity checks against COM's
// aggregation rules --
//   1. querying the returned inner for IUnknown must equal `outer`
//      (first-hop identity).
//   2. querying inner for some *other* real interface it implements
//      (IDependencyObject -- FrameworkElement genuinely implements it,
//      unlike the mocks in overrides_demo.cpp), then querying *that*
//      pointer again for IUnknown, must *still* equal `outer`
//      (second-hop identity).
void run_activation_probe() {
    HSTRING className{};
    HRESULT hrClassName = WindowsCreateString(L"Microsoft.UI.Xaml.FrameworkElement", 34, &className);
    if (FAILED(hrClassName)) {
        std::printf("WindowsCreateString failed: 0x%08X\n", static_cast<unsigned>(hrClassName));
        return;
    }

    IFrameworkElementFactory* factory{};
    HRESULT hr = RoGetActivationFactory(className, __uuidof(IFrameworkElementFactory), reinterpret_cast<void**>(&factory));
    WindowsDeleteString(className);
    if (FAILED(hr)) {
        std::printf("RoGetActivationFactory(IFrameworkElementFactory) failed: 0x%08X\n", static_cast<unsigned>(hr));
        return;
    }
    std::printf("RoGetActivationFactory(IFrameworkElementFactory) succeeded.\n");

    TestOuter outer;
    ::IInspectable* inner{};
    ::IInspectable* value{};
    hr = factory->CreateInstance(&outer, &inner, &value);
    factory->Release();
    if (FAILED(hr)) {
        std::printf("CreateInstance(&outer, &inner, &value) failed: 0x%08X\n", static_cast<unsigned>(hr));
        return;
    }
    std::printf("CreateInstance succeeded -- got a real, aggregated FrameworkElement.\n");

    // `inner` is documented as the raw, *non-delegating*
    // identity -- meant for our own internal use, precisely so we can reach
    // the inner's stock default behavior without recursing back into
    // ourselves. So inner->QueryInterface(IUnknown) is *expected* to answer
    // with inner's own identity, not ours -- check that expectation, and
    // separately check `value` (the properly-delegating, externally-facing
    // identity CreateInstance also returns), which per the aggregation
    // contract *should* match `outer`.
    void* innerUnknown{};
    HRESULT const hrInnerUnknown = inner->QueryInterface(__uuidof(::IUnknown), &innerUnknown);
    bool const innerIsNonDelegating = hrInnerUnknown == S_OK && innerUnknown != static_cast<void*>(static_cast<::IInspectable*>(&outer));
    std::printf("  inner->QueryInterface(IUnknown) is inner's own (non-delegating) identity, not ours: %s\n", innerIsNonDelegating ? "yes" : "no");
    if (innerUnknown) {
        static_cast<::IUnknown*>(innerUnknown)->Release();
    }

    void* valueUnknown{};
    HRESULT const hrValueUnknown = value->QueryInterface(__uuidof(::IUnknown), &valueUnknown);
    bool const valueMatchesOuter = hrValueUnknown == S_OK && valueUnknown == static_cast<void*>(static_cast<::IInspectable*>(&outer));
    std::printf("  value->QueryInterface(IUnknown) matches our outer: %s\n", valueMatchesOuter ? "yes" : "no");
    if (valueUnknown) {
        static_cast<::IUnknown*>(valueUnknown)->Release();
    }

    void* dependencyObjectPtr{};
    HRESULT const hrDependencyObject = inner->QueryInterface(IID_IDependencyObject, &dependencyObjectPtr);
    if (FAILED(hrDependencyObject) || !dependencyObjectPtr) {
        std::printf("  inner->QueryInterface(IDependencyObject) failed: 0x%08X\n", static_cast<unsigned>(hrDependencyObject));
    } else {
        void* secondHop{};
        HRESULT const hrSecondHop = static_cast<::IUnknown*>(dependencyObjectPtr)->QueryInterface(__uuidof(::IUnknown), &secondHop);
        bool const secondHopOk = hrSecondHop == S_OK && secondHop == static_cast<void*>(static_cast<::IInspectable*>(&outer));
        std::printf("  second hop: (inner->QI(IDependencyObject))->QI(IUnknown) matches our outer: %s\n", secondHopOk ? "yes" : "no");
        if (secondHop) {
            static_cast<::IUnknown*>(secondHop)->Release();
        }
        static_cast<::IUnknown*>(dependencyObjectPtr)->Release();
    }

    inner->Release();
    value->Release();
}

struct App : ApplicationT<App> {
    void OnLaunched(LaunchActivatedEventArgs const&) {
        run_activation_probe();
        Exit();
    }
};

} // namespace

int main() {
    init_apartment();

    try {
        wxl::impl::ensure_windows_app_runtime_initialized();
    } catch (wxl::hresult_error const& e) {
        std::printf("ensure_windows_app_runtime_initialized failed: 0x%08X\n", static_cast<unsigned>(e.code()));
        return 1;
    }
    std::printf("Windows App Runtime bootstrap succeeded.\n");

    Application::Start([](auto&&) { make<App>(); });
    return 0;
}

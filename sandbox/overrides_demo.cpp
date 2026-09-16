// Demonstrates Overrides authoring the way .claude/design.md settles on: the
// hook (OnDisconnectVisualChildren) lives directly on wxl::impl::UIElement
// as `protected virtual` (matching the real projection shape, C#:
// `protected virtual extern void IUIElementOverrides.OnDisconnectVisualChildren();`)
// -- no separate hooks/mixin type. impl::UIElement *is* the outer COM
// aggregate identity now too (an earlier, separate UIElementIdentity class
// was merged into it -- see .claude/design.md): a class authoring a composable
// element derives from wxl::impl::UIElement directly and overrides the
// hook, ordinary inheritance, no CRTP.
//
// This exercises UIElement's own QueryInterface -- the actual "outer
// identity" mechanism described in .claude/design.md -- rather than reaching
// the Overrides shim through an internal accessor directly. QueryInterface
// for IUIElementOverrides is answered with this element's own shim;
// anything else is forwarded to the wrapped/composed object.
//
// wxl::impl::UIElement's constructor QueryInterfaces for IUIElement/
// IUIElementProtected/IUIElementOverrides for real, so exercising it here
// (rather than just compiling it, like uielement_demo.cpp) needs *something*
// answering those calls -- there's no live XAML app in this sandbox. The
// mocks below are test-only stand-ins: minimal, non-realistic COM objects
// (their sub-interfaces don't share one COM identity the way a real
// aggregate must) that exist purely to make wxl's own construction/dispatch
// code run without a live WinUI3 runtime.
#include <iostream>

#include <wxl/impl/UIElement.h>

namespace {

using namespace wxl::abi::Microsoft::UI::Xaml;

// A properly-aggregated inner object's *own* interfaces (as opposed to the
// raw, non-delegating pointer the aggregator holds internally as
// inspectable()) are contractually required to delegate their entire
// IUnknown/IInspectable identity to the outer/controlling object -- that's
// what "blind aggregation" means, and it's exactly the property the user's
// question is probing: if you take a pointer that Object::QueryInterface
// forwarded from inspectable_, and query *that* pointer again, COM's
// identity rules require the answer to come back through the outer, not
// straight from the inner. `outer` is wired up post-construction (see
// main()), mirroring how a real CreateInstance(outer, &inner) call wires an
// inner object to the outer identity it was aggregated under.
template <typename Interface>
struct DelegatingMock : Interface {
    ::IInspectable* outer{};

    HRESULT __stdcall QueryInterface(REFIID riid, void** result) noexcept override {
        return outer->QueryInterface(riid, result);
    }
    ULONG __stdcall AddRef() noexcept override { return outer->AddRef(); }
    ULONG __stdcall Release() noexcept override { return outer->Release(); }
    HRESULT __stdcall GetIids(ULONG* iidCount, IID** iids) noexcept override { return outer->GetIids(iidCount, iids); }
    HRESULT __stdcall GetRuntimeClassName(HSTRING* className) noexcept override { return outer->GetRuntimeClassName(className); }
    HRESULT __stdcall GetTrustLevel(TrustLevel* trustLevel) noexcept override { return outer->GetTrustLevel(trustLevel); }
};

struct MockUIElement final : DelegatingMock<IUIElement> {
    std::int32_t __stdcall get_DesiredSize(void*) noexcept override { return 0; }
    std::int32_t __stdcall get_AllowDrop(bool* v) noexcept override { if (v) *v = false; return 0; }
    std::int32_t __stdcall put_AllowDrop(bool) noexcept override { return 0; }
    std::int32_t __stdcall get_Opacity(double* v) noexcept override { if (v) *v = 1.0; return 0; }
    std::int32_t __stdcall put_Opacity(double) noexcept override { return 0; }
    std::int32_t __stdcall get_Clip(void** v) noexcept override { if (v) *v = nullptr; return 0; }
    std::int32_t __stdcall put_Clip(void*) noexcept override { return 0; }
    std::int32_t __stdcall get_RenderTransform(void** v) noexcept override { if (v) *v = nullptr; return 0; }
    std::int32_t __stdcall put_RenderTransform(void*) noexcept override { return 0; }
    std::int32_t __stdcall get_Projection(void** v) noexcept override { if (v) *v = nullptr; return 0; }
    std::int32_t __stdcall put_Projection(void*) noexcept override { return 0; }
    std::int32_t __stdcall get_Transform3D(void** v) noexcept override { if (v) *v = nullptr; return 0; }
    std::int32_t __stdcall put_Transform3D(void*) noexcept override { return 0; }
    std::int32_t __stdcall get_RenderTransformOrigin(void*) noexcept override { return 0; }
    std::int32_t __stdcall put_RenderTransformOrigin(void*) noexcept override { return 0; }
    std::int32_t __stdcall get_IsHitTestVisible(bool* v) noexcept override { if (v) *v = true; return 0; }
    std::int32_t __stdcall put_IsHitTestVisible(bool) noexcept override { return 0; }
    std::int32_t __stdcall get_Visibility(Visibility* v) noexcept override { if (v) *v = Visibility::Visible; return 0; }
    std::int32_t __stdcall put_Visibility(Visibility) noexcept override { return 0; }
};

struct MockUIElementProtected final : DelegatingMock<IUIElementProtected> {
    std::int32_t __stdcall get_ProtectedCursor(void** v) noexcept override { if (v) *v = nullptr; return 0; }
    std::int32_t __stdcall put_ProtectedCursor(void*) noexcept override { return 0; }
};

struct MockUIElementOverrides final : DelegatingMock<IUIElementOverrides> {
    std::int32_t __stdcall OnCreateAutomationPeer(void** v) noexcept override { if (v) *v = nullptr; return 0; }
    std::int32_t __stdcall OnDisconnectVisualChildren() noexcept override {
        std::cout << "  (stock/default IUIElementOverrides::OnDisconnectVisualChildren)\n";
        return 0;
    }
};

// Routes QueryInterface to the three mocks above. Represents the raw,
// non-delegating inner pointer an aggregator holds internally (that's what
// wxl::impl::Object::inspectable_ points at here) -- so *this* object's own
// IUnknown/IInspectable answers itself, which is correct: our own
// Object::QueryInterface never forwards IUnknown/IInspectable queries to it
// in the first place (it answers those itself). Its three sub-mocks
// (element/protected_/overrides), however, are what gets handed back to
// *external* callers via forwarding -- those must delegate to the outer
// identity per the "blind aggregation" contract (see DelegatingMock above),
// wired up in main() once the outer identity exists.
struct MockIdentity final : ::IInspectable {
    MockUIElement element;
    MockUIElementProtected protected_;
    MockUIElementOverrides overrides;

    HRESULT __stdcall QueryInterface(REFIID riid, void** result) noexcept override {
        if (riid == __uuidof(::IUnknown) || riid == __uuidof(::IInspectable)) {
            *result = static_cast<::IInspectable*>(this);
        } else if (riid == __uuidof(IUIElement)) {
            *result = &element;
        } else if (riid == __uuidof(IUIElementProtected)) {
            *result = &protected_;
        } else if (riid == __uuidof(IUIElementOverrides)) {
            *result = &overrides;
        } else {
            *result = nullptr;
            return E_NOINTERFACE;
        }
        return S_OK;
    }

    ULONG __stdcall AddRef() noexcept override { return 1; }
    ULONG __stdcall Release() noexcept override { return 1; }
    HRESULT __stdcall GetIids(ULONG* c, IID**) noexcept override { if (c) *c = 0; return S_OK; }
    HRESULT __stdcall GetRuntimeClassName(HSTRING* n) noexcept override { if (n) *n = nullptr; return S_OK; }
    HRESULT __stdcall GetTrustLevel(TrustLevel* t) noexcept override { if (t) *t = BaseTrust; return S_OK; }
};

// Derives from wxl::impl::UIElement directly -- it's a real, working outer
// identity on its own now, so MyElement only needs to override the one
// hook it wants to customize.
class MyElement : public wxl::impl::UIElement {
public:
    using UIElement::UIElement;

    void OnDisconnectVisualChildren() override {
        std::cout << "MyElement::OnDisconnectVisualChildren (custom override, plain inheritance, no CRTP)\n";
    }
};

} // namespace

int main() {
    MockIdentity identity;
    MyElement element{wxl::release_only_ptr<::IInspectable>(&identity)};

    // Wires identity's three sub-mocks to delegate to `element` -- this is
    // the test-only stand-in for what a real CreateInstance(outer, &inner)
    // call does as part of aggregation activation (see .claude/design.md,
    // "Still deferred: composable activation proper"). Must happen after
    // `element` exists (its address is what's being delegated to), but the
    // queries element's own constructor already made above aren't affected:
    // those were answered directly by MockIdentity::QueryInterface handing
    // out &identity.element/&identity.protected_/&identity.overrides, never
    // through the sub-mocks' own (until-now-unreachable) QueryInterface.
    identity.element.outer = &element;
    identity.protected_.outer = &element;
    identity.overrides.outer = &element;

    std::cout << "-- overridden element, reached via the outer identity's own QueryInterface --\n";
    void* overridesPtr{};
    element.QueryInterface(__uuidof(IUIElementOverrides), &overridesPtr);
    static_cast<IUIElementOverrides*>(overridesPtr)->OnDisconnectVisualChildren();

    std::cout << "-- forwarding: querying IUIElement through the outer identity reaches the wrapped/inner object --\n";
    void* forwardedPtr{};
    HRESULT const hr = element.QueryInterface(__uuidof(IUIElement), &forwardedPtr);
    std::cout << ((hr == S_OK && forwardedPtr == &identity.element) ? "forwarded correctly\n" : "FAILED\n");

    // Neither UIElement nor DependencyObject nor Object checks for
    // IUIElementProtected explicitly -- this exercises the *whole* 3-hop
    // chain (UIElement -> DependencyObject -> Object) reaching Object's
    // fallback and forwarding to the wrapped/inner object, same mechanism
    // that answers e.g. IDependencyObject for real (see .claude/design.md).
    std::cout << "-- 3-hop fallback: IUIElementProtected isn't checked at any level, still reaches the inner object --\n";
    void* protectedPtr{};
    HRESULT const hrProtected = element.QueryInterface(__uuidof(IUIElementProtected), &protectedPtr);
    std::cout << ((hrProtected == S_OK && protectedPtr == &identity.protected_) ? "forwarded correctly\n" : "FAILED\n");

    // The question this test answers: take a pointer that was *forwarded*
    // from the inner object (forwardedPtr, an IUIElement*, from the test
    // above) and query *that* pointer again for a different interface
    // (IUIElementOverrides). Per COM's identity rules, this second hop must
    // come back through *our* outer identity -- proven here by checking
    // that we get back element's own shim (overrides_abi()), not
    // &identity.overrides (the inner's stock default) -- both would return
    // S_OK, so only comparing *which* pointer came back actually proves the
    // round-trip went through us. This is the guarantee the mocks could NOT
    // demonstrate before DelegatingMock existed (every sub-mock used to
    // hardcode `return E_NOINTERFACE;`, which is not just "not tested" but
    // actively the wrong answer for a properly-aggregated inner object).
    std::cout << "-- second hop: querying again on a forwarded pointer must route back to OUR identity, not the inner's own --\n";
    auto* forwardedUIElement = static_cast<IUIElement*>(forwardedPtr);
    void* secondHopPtr{};
    HRESULT const hrSecondHop = forwardedUIElement->QueryInterface(__uuidof(IUIElementOverrides), &secondHopPtr);
    std::cout << ((hrSecondHop == S_OK && secondHopPtr == element.overrides_abi())
        ? "round-tripped back to our identity (got our shim, not the inner's stock default)\n"
        : "FAILED\n");

    std::cout << "-- default (stock) behavior: a plain UIElement, no override --\n";
    wxl::impl::UIElement plainElement{wxl::release_only_ptr<::IInspectable>(&identity)};
    void* plainOverridesPtr{};
    plainElement.QueryInterface(__uuidof(IUIElementOverrides), &plainOverridesPtr);
    static_cast<IUIElementOverrides*>(plainOverridesPtr)->OnDisconnectVisualChildren();

    return 0;
}

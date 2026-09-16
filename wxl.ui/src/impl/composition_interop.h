#pragma once

// The three COM interfaces the composition surface chain is made of.
//
// They belong to Microsoft.UI.Composition.Interop.h in the WindowsAppSDK's
// InteractiveExperiences package, and that header cannot be included: its
// first line pulls in the ABI header Microsoft.ui.composition.h, which the
// package does not carry (the same trap Microsoft.UI.Interop.h sets, see
// WindowPlacement.cpp). Everything it would give us is three vtables and
// three IIDs, so they are restated here, copied from that header.
//
// Two of the types those signatures name are left as raw IInspectable**:
// ICompositionGraphicsDevice and ICompositionSurface are WinRT interfaces
// whose declarations live in the missing ABI header, and a pointer is a
// pointer -- the vtable slot is the same width either way, and what comes
// back is handed straight to cppwinrt.

#include <unknwn.h>
#include <windows.h>

namespace wxl::impl {

// {FAB19398-6D19-4D8A-B752-8F096C396069}
MIDL_INTERFACE("FAB19398-6D19-4D8A-B752-8F096C396069")
ICompositorInterop : public ::IUnknown {
public:
    virtual HRESULT STDMETHODCALLTYPE CreateGraphicsDevice(::IUnknown* renderingDevice,
                                                           ::IInspectable** result) = 0;
};

// {2D6355C2-AD57-4EAE-92E4-4C3EFF65D578}
MIDL_INTERFACE("2D6355C2-AD57-4EAE-92E4-4C3EFF65D578")
ICompositionDrawingSurfaceInterop : public ::IUnknown {
public:
    // The IID is asked for rather than assumed: the surface will hand out an
    // ID2D1DeviceContext, an IDXGISurface or anything else its device can
    // speak, and which one is the caller's choice.
    virtual HRESULT STDMETHODCALLTYPE BeginDraw(RECT const* updateRect, REFIID iid,
                                                void** updateObject, POINT* updateOffset) = 0;
    virtual HRESULT STDMETHODCALLTYPE EndDraw() = 0;
    virtual HRESULT STDMETHODCALLTYPE Resize(SIZE sizePixels) = 0;
    virtual HRESULT STDMETHODCALLTYPE Scroll(RECT const* scrollRect, RECT const* clipRect,
                                             int offsetX, int offsetY) = 0;
    virtual HRESULT STDMETHODCALLTYPE ResumeDraw() = 0;
    virtual HRESULT STDMETHODCALLTYPE SuspendDraw() = 0;
};

// {4AFA8030-BC70-4B0C-B1C7-6E69F933DC83}
MIDL_INTERFACE("4AFA8030-BC70-4B0C-B1C7-6E69F933DC83")
ICompositionGraphicsDeviceInterop : public ::IUnknown {
public:
    virtual HRESULT STDMETHODCALLTYPE GetRenderingDevice(::IUnknown** value) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetRenderingDevice(::IUnknown* value) = 0;
};

}  // namespace wxl::impl

// The four links between an application's Direct2D and the compositor.
//
// The projection and the Windows headers come first, and with them every
// standard header they need: wxl's own headers carry the wxl.core import,
// and a standard header included after that import is one the compiler has
// already seen through the std module.
#include <winrt/Microsoft.Graphics.DirectX.h>
#include <winrt/Microsoft.UI.Composition.h>

#include <d2d1_1.h>
#include <d3d11.h>
#include <dxgi.h>
#include <inspectable.h>

#include "DrawingSurface.h"
#include "Object.impl.h"
#include "generated/Microsoft.UI.Composition.impl.h"
#include "impl/composition_interop.h"

namespace wxl {
namespace {

namespace composition = winrt::Microsoft::UI::Composition;
namespace directx = winrt::Microsoft::Graphics::DirectX;

/// The graphics device every drawing surface of this process comes from.
///
/// One, and made once. WinUI gives a thread a single compositor and wxl runs
/// a single main STA thread, so a second device could only ever be a second
/// copy of this one -- and each costs a D3D11 device, which is the most
/// expensive object in the chain.
///
/// Kept for the whole process, the way impl::StaticsProxy keeps an activation
/// factory.
composition::CompositionGraphicsDevice graphics_device(composition::Compositor const& compositor) {
    // One reference, held raw and on purpose. A cppwinrt object in a static
    // would be released at exit, in an order nothing here controls and
    // possibly after the apartment it belongs to is gone; a raw pointer that
    // is never released cannot be.
    static ::IInspectable* kept = nullptr;
    if (kept) {
        composition::CompositionGraphicsDevice device{nullptr};
        winrt::copy_from_abi(device, kept);
        return device;
    }

    // BGRA support is not optional: it is what a D2D device requires of the
    // D3D device underneath it.
    winrt::com_ptr<ID3D11Device> d3d;
    winrt::check_hresult(::D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
                                             D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0,
                                             D3D11_SDK_VERSION, d3d.put(), nullptr, nullptr));

    // Single-threaded on purpose, and safe for the same reason the rest of
    // wxl is: everything that touches this device runs on the main STA
    // thread. The compositor's own work happens elsewhere, but it does not
    // reach through this device -- it reads the surfaces afterwards.
    D2D1_CREATION_PROPERTIES const properties{D2D1_THREADING_MODE_SINGLE_THREADED,
                                              D2D1_DEBUG_LEVEL_NONE,
                                              D2D1_DEVICE_CONTEXT_OPTIONS_NONE};
    winrt::com_ptr<ID2D1Device> d2d;
    winrt::check_hresult(
        ::D2D1CreateDevice(d3d.as<IDXGIDevice>().get(), &properties, d2d.put()));

    auto const interop = compositor.as<impl::ICompositorInterop>();
    winrt::com_ptr<::IInspectable> made;
    winrt::check_hresult(interop->CreateGraphicsDevice(d2d.as<::IUnknown>().get(), made.put()));

    auto device = made.as<composition::CompositionGraphicsDevice>();
    kept = made.detach();
    return device;
}

/// Allocates the surface itself.
///
/// Premultiplied alpha, because that is what the compositor blends in, and
/// B8G8R8A8 because that is what D2D draws into.
composition::CompositionDrawingSurface allocate(composition::Compositor const& compositor,
                                                SizeInt32 sizePixels) {
    return graphics_device(compositor).CreateDrawingSurface2(
        winrt::Windows::Graphics::SizeInt32{std::max(sizePixels.width, 0),
                                           std::max(sizePixels.height, 0)},
        directx::DirectXPixelFormat::B8G8R8A8UIntNormalized,
        directx::DirectXAlphaMode::Premultiplied);
}

}  // namespace

DrawingSurface::DrawingSurface(Compositor const& compositor, SizeInt32 sizePixels)
    : compositor_(compositor),
      surface_(Object::Impl::wrap<CompositionDrawingSurface>(
          allocate(*Object::Impl::get_typed<Compositor>(compositor), sizePixels))) {}

SizeInt32 DrawingSurface::size() const {
    return surface_.sizeInt32();
}

void DrawingSurface::resize(SizeInt32 sizePixels) {
    surface_.resize({std::max(sizePixels.width, 0), std::max(sizePixels.height, 0)});
}

void DrawingSurface::draw(std::function<void(ID2D1DeviceContext*)> const& paint) const {
    SizeInt32 const pixels = size();
    if (!paint || pixels.width <= 0 || pixels.height <= 0) {
        return;  // nothing to draw on: an element that has not been measured
    }

    composition::CompositionDrawingSurface const& surface =
        *Object::Impl::get_typed<CompositionDrawingSurface>(surface_);
    auto const interop = surface.as<impl::ICompositionDrawingSurfaceInterop>();

    winrt::com_ptr<ID2D1DeviceContext> context;
    POINT offset{};
    // nullptr: the whole surface. A partial update would be an update
    // rectangle here, and the day something wants one it is one overload.
    winrt::check_hresult(interop->BeginDraw(nullptr, winrt::guid_of<ID2D1DeviceContext>(),
                                            context.put_void(), &offset));

    // Surfaces share one texture atlas, so the origin of this one is
    // wherever the atlas put it. Drawing without this transform lands on
    // whatever else is in the same texture -- which looks like another
    // surface's contents leaking into this one.
    context->SetTransform(D2D1::Matrix3x2F::Translation(static_cast<float>(offset.x),
                                                        static_cast<float>(offset.y)));
    context->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));

    // EndDraw runs even if the drawing threw: a surface left open is a
    // surface no later draw can begin on.
    try {
        paint(context.get());
    } catch (...) {
        interop->EndDraw();
        throw;
    }
    winrt::check_hresult(interop->EndDraw());
}

CompositionSurfaceBrush DrawingSurface::brush() const {
    composition::Compositor const& compositor = *Object::Impl::get_typed<Compositor>(compositor_);

    // Named, not nested: the surface converts to the projection type through
    // its Impl and the projection type converts to ICompositionSurface, and
    // one implicit conversion is all an argument gets.
    composition::CompositionDrawingSurface const& drawn =
        *Object::Impl::get_typed<CompositionDrawingSurface>(surface_);
    return Object::Impl::wrap<CompositionSurfaceBrush>(compositor.CreateSurfaceBrush(drawn));
}

}  // namespace wxl

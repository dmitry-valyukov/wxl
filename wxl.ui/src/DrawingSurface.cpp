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

#include <vector>

#include "DrawingSurface.h"
#include "Object.impl.h"
#include "generated/Microsoft.UI.Composition.impl.h"
#include "impl/composition_interop.h"

namespace wxl {
namespace {

namespace composition = winrt::Microsoft::UI::Composition;
namespace directx = winrt::Microsoft::Graphics::DirectX;

/// The Direct2D device every drawing surface of this process draws through.
///
/// One, and made once: a D3D11 device is the most expensive object in the
/// chain, and nothing about it belongs to a compositor -- several graphics
/// devices sit on this one happily.
///
/// Kept for the whole process, the way impl::StaticsProxy keeps an activation
/// factory. The reference is held raw and on purpose: a cppwinrt object in a
/// static would be released at exit, in an order nothing here controls and
/// possibly after the apartment it belongs to is gone; a raw pointer that is
/// never released cannot be.
ID2D1Device* d2d_device() {
    static ID2D1Device* kept = nullptr;
    if (kept) return kept;

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

    kept = d2d.detach();
    return kept;
}

/// The graphics device *of this compositor*, made once per compositor.
///
/// One per compositor, and that is the whole point of the lookup. A graphics
/// device is made **by** a compositor, and every surface it allocates belongs
/// to that compositor; hand such a surface to another compositor's
/// CreateSurfaceBrush and composition refuses it with E_ACCESSDENIED -- which
/// arrives as a fail-fast, not as an exception a caller could see.
///
/// This is not a theoretical case. A wxl::CompositionWindow has two
/// compositors: the one whose pixels DWM shows for the window itself, on
/// which the application draws its own scene, and the XAML island's, which
/// owns every visual a control hangs off ElementCompositionPreview. An
/// application that draws on both -- a page on the scene, an overlay inside
/// the island -- comes here twice, and before this each call after the first
/// got a device belonging to whoever asked first.
///
/// The table is tiny by nature: two entries for a window, four for two
/// windows. Both pointers in it are leaked deliberately, for the reason
/// d2d_device() gives; the compositor's is kept only as an identity to
/// compare against, is never called through, and being immortal cannot be a
/// stale address a later compositor happens to reuse.
composition::CompositionGraphicsDevice graphics_device(composition::Compositor const& compositor) {
    struct entry {
        ::IUnknown* owner;
        ::IInspectable* device;
    };
    static std::vector<entry> kept;

    // COM identity, not the interface pointer at hand: two references to one
    // compositor through different interfaces are different addresses, and
    // only IUnknown answers the same one every time.
    winrt::com_ptr<::IUnknown> identity = compositor.as<::IUnknown>();

    for (entry const& known : kept) {
        if (known.owner != identity.get()) continue;

        composition::CompositionGraphicsDevice device{nullptr};
        winrt::copy_from_abi(device, known.device);
        return device;
    }

    auto const interop = compositor.as<impl::ICompositorInterop>();
    winrt::com_ptr<::IInspectable> made;
    winrt::check_hresult(interop->CreateGraphicsDevice(d2d_device(), made.put()));

    auto device = made.as<composition::CompositionGraphicsDevice>();
    kept.push_back({identity.detach(), made.detach()});
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

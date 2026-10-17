// The four links between an application's Direct2D and the compositor -- WinUI's for every
// DrawingSurface, and the system's for WindowShade (impl/system_drawing.h).
//
// The projection and the Windows headers come first, and with them every
// standard header they need: wxl's own headers carry the wxl.core import,
// and a standard header included after that import is one the compiler has
// already seen through the std module.
#include "platform.h"

#include <winrt/Microsoft.Graphics.DirectX.h>
#include <winrt/Microsoft.UI.Composition.h>
#include <winrt/Microsoft.UI.Dispatching.h>
#include <winrt/Windows.Graphics.h>

#include <d2d1_1.h>
#include <d3d11_4.h>
#include <dxgi.h>
#include <inspectable.h>
#include <windows.ui.composition.interop.h>   // the system compositor's interop

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <span>
#include <vector>

#include "DrawingSurface.h"
#include "Object.impl.h"
#include <wxl/Microsoft.UI.Composition.impl.h>
#include "impl/composition_interop.h"
#include "impl/device_limits.h"
#include "impl/device_loss.h"
#include "impl/device_replacement.h"
#include "impl/handler_list.h"
#include "impl/system_drawing.h"

namespace wxl {
namespace {

namespace composition = winrt::Microsoft::UI::Composition;
namespace directx = winrt::Microsoft::Graphics::DirectX;
namespace dispatching = winrt::Microsoft::UI::Dispatching;
namespace system_interop = ABI::Windows::UI::Composition;

/// The Direct2D device every drawing surface of this process draws through, and what
/// watches it for being lost.
///
/// One at a time, made on first use and again after each loss: a D3D11 device is the most
/// expensive object in the chain, and nothing about it belongs to a compositor -- several
/// graphics devices sit on this one happily, of either composition API.
///
/// Every reference here is held raw and on purpose, the way impl::StaticsProxy keeps an
/// activation factory: a cppwinrt object in a static would be released at exit, in an order
/// nothing here controls and possibly after the apartment it belongs to is gone; a raw
/// pointer that is never released cannot be. A replaced device is released by the
/// replacement, on the UI thread, while everything is alive.
struct rendering_device {
    ID2D1Device* d2d = nullptr;
    ID3D11Device4* d3d = nullptr;   // the one that says it was removed
    HANDLE removed = nullptr;       // set by the D3D device when it is removed
    DWORD cookie = 0;               // its registration, to take back from the old device
    HANDLE wait = nullptr;          // the system pool's wait on `removed`
    std::int32_t max_update_side = 0;   // of `d3d`, by its feature level
};

/// What impl::rendering_devices makes and moves: the device above, and graphics devices and
/// compositors as the raw interfaces they are. A graphics device is leaked deliberately, for the
/// reason rendering_device gives. A compositor's identity is held while its entry stands -- only
/// compared, and held, it cannot be a stale address a later compositor happens to reuse -- and let
/// go with the entry, when the compositor's last watch comes off and no surface was made on it:
/// at the moment that watch lets its own reference go. An entry that keeps a graphics device
/// stands for good, and its identity with it.
struct device_backend {
    using owner_t = ::IUnknown*;
    using device_t = rendering_device;
    using graphics_t = ::IInspectable*;

    // The UI thread's queue, taken when the first device is made -- on the UI thread, as
    // every surface is. The pool hands the news of a loss over through it; it is agile, and
    // only ever read after that first device, so the pool reads it without a lock.
    ::IInspectable* queue = nullptr;

    rendering_device make(std::uint64_t generation);
    bool lost(rendering_device const& device) noexcept { return device.d3d->GetDeviceRemovedReason() != S_OK; }
    void unwatch(rendering_device& device) noexcept;
    void release(rendering_device& device) noexcept;
    void move(::IInspectable* graphics, impl::composition_api api, rendering_device const& device);
    void post() noexcept;
    void keep(::IUnknown* owner) noexcept { owner->AddRef(); }
    void forget(::IUnknown* owner) noexcept { owner->Release(); }
};

/// The device, the graphics devices and the watches of the process. Leaked, as everything in it
/// is: a watch held by an object of static duration is taken off in that object's destructor,
/// after any function-local static would be gone.
///
/// Between a loss and its replacement every draw() skips, and so it does between a replacement
/// graphics_of ran and the turn that calls the holders. The watches are called by the
/// replacement and not from the graphics devices' own RenderingDeviceReplaced: whether that
/// event is raised inside SetRenderingDevice or on a later turn the documentation does not say,
/// and if later, a drawing between the two would draw what the owner made from the old device
/// into a context of the new one -- D2DERR_WRONG_RESOURCE_DOMAIN at EndDraw, which is a failure.
/// The graphics devices are wxl's own and nobody else replaces their device, so the event says
/// nothing wxl does not know here, and nothing listens to it.
impl::rendering_devices<device_backend>& devices() {
    static impl::rendering_devices<device_backend>& kept = *new impl::rendering_devices<device_backend>{};
    return kept;
}

/// Stops with what failed and its code: a failure here leaves nothing to draw with, and
/// there is nobody to answer it.
[[noreturn]] void stop_on(char const* what, std::uint32_t code) noexcept {
    char text[192];
    std::snprintf(text, sizeof text, "%s, 0x%08X", what, static_cast<unsigned>(code));
    core::abort(text);
}

/// Whether `result` is the answer of a lost device: a code that names one (the list is
/// impl::device_loss_code) from the device that says it was removed. A code that names one
/// from a device that is fine stops: taken for a loss, it would skip every drawing for ever,
/// and no replacement would come to end it.
bool confirmed_loss(HRESULT result) noexcept {
    if (!impl::device_loss_code(result)) return false;
    rendering_device const* const device = devices().current();
    if (device && device->d3d->GetDeviceRemovedReason() != S_OK) return true;
    stop_on("wxl: a drawing call answered that the device is lost, and the device is not",
            static_cast<std::uint32_t>(result));
}

/// The pool's half of a loss: hand the news to the UI thread, which owns the device and
/// every surface -- the replacement of the device the wait was put on for, whose generation
/// is the context of the wait. Nothing else happens on the pool's thread. A queue that takes
/// nothing more is going down, and then there is nothing left to draw. noexcept: the pool has
/// nobody to hand an exception to.
///
/// What makes it safe to run after its watch is taken back (unwatch does not wait for it): it
/// touches devices(), which is never let go, the queue in backend().queue, written once on the UI
/// thread before the first wait is registered and never again, and the generation, by value --
/// nothing unwatch frees, neither the device nor the event nor the wait. The turn it posts, coming
/// late, replaces nothing: after the close (closed) or after a replacement (another generation).
void CALLBACK device_removed(void* context, BOOLEAN) noexcept {
    // MSVC keeps a terminate handler for each thread, so the one that names the exception is
    // set here too: an exception out of the handing over -- the projection's TryEnqueue, the
    // delegate it is given -- ends in std::terminate. A thread of the pool is the system's and
    // runs other callbacks after this one, so it gets its own handler back. The handler and
    // not a catch, as post_completion has: it names a terminate a noexcept call inside raises too.
    const std::terminate_handler previous = core::impl::set_terminate_with_reason();

    std::uint64_t const generation = reinterpret_cast<std::uintptr_t>(context);
    dispatching::DispatcherQueue queue{nullptr};
    winrt::copy_from_abi(queue, devices().backend().queue);
    queue.TryEnqueue([generation]() noexcept { devices().replace(generation); });

    std::set_terminate(previous);
}

/// A D3D11 device on the first adapter of a fresh DXGI factory that makes one, WARP last.
///
/// Fresh, because a factory made before a driver update lists the adapters of the old
/// driver, and RegisterDeviceRemovedEvent's documentation names that very case; in order,
/// because the first adapter is the one with the desktop. One pass, not a retry: nothing
/// made at all -- not even WARP -- leaves nothing to draw with, and stops.
winrt::com_ptr<ID3D11Device> new_d3d_device() {
    // BGRA support is not optional: it is what a D2D device requires of the D3D device
    // underneath it.
    UINT constexpr flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;

    winrt::com_ptr<IDXGIFactory1> factory;
    HRESULT const made = ::CreateDXGIFactory1(winrt::guid_of<IDXGIFactory1>(), factory.put_void());
    if (FAILED(made)) {
        stop_on("wxl: no DXGI factory to find a rendering device in", static_cast<std::uint32_t>(made));
    }

    winrt::com_ptr<ID3D11Device> d3d;
    HRESULT last = S_OK;
    for (UINT at = 0; !d3d; ++at) {
        winrt::com_ptr<IDXGIAdapter1> adapter;
        HRESULT const found = factory->EnumAdapters1(at, adapter.put());
        if (found == DXGI_ERROR_NOT_FOUND) break;
        if (FAILED(found)) {
            stop_on("wxl: the adapters of a DXGI factory could not be listed",
                    static_cast<std::uint32_t>(found));
        }
        // An adapter of its own means the driver type is UNKNOWN: the adapter says it.
        last = ::D3D11CreateDevice(adapter.get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr, flags,
                                   nullptr, 0, D3D11_SDK_VERSION, d3d.put(), nullptr, nullptr);
    }
    if (!d3d) {
        last = ::D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, flags, nullptr, 0,
                                   D3D11_SDK_VERSION, d3d.put(), nullptr, nullptr);
    }
    if (!d3d) {
        stop_on("wxl: no adapter and not WARP made a Direct3D device; the last said",
                static_cast<std::uint32_t>(last));
    }
    return d3d;
}

/// Makes the D3D11 device, the D2D device on it, and the watch for its removal: the device
/// sets an event when it is removed, and a wait of the system pool -- no thread of ours,
/// nothing that looks again and again -- hears it once, and asks for the replacement of
/// `generation`.
///
/// With the first device the UI thread's queue is taken, and the replacement is closed when the
/// queue begins to shut down: ShutdownStarting is the first of its shutdown events, raised before
/// FrameworkShutdownStarting, in which XAML shuts down on the thread and closes its compositor
/// (FrameworkApplication::StartOnCurrentThreadImpl, DCompTreeHost::ReleaseResources in the
/// sources of WinUI 3) -- and SetRenderingDevice on a graphics device of a closed compositor
/// fails. The handler does not throw: close() stops on a failure of its own.
rendering_device device_backend::make(std::uint64_t generation) {
    if (!queue) {
        auto current = dispatching::DispatcherQueue::GetForCurrentThread();
        if (!current) core::abort("wxl: a drawing surface is made off the UI thread");
        // Never taken off: the queue goes with it. close() does not wait for a removal callback under
        // way, which posts to this queue from the pool: unwatch takes the wait back without waiting
        // (device_removed says why it may run late), so the handler blocks on nothing the queue holds.
        static_cast<void>(current.ShutdownStarting([](auto&&, auto&&) noexcept { devices().close(); }));
        queue = static_cast<::IInspectable*>(winrt::detach_abi(current));
    }

    winrt::com_ptr<ID3D11Device> const d3d = new_d3d_device();

    // Single-threaded on purpose, and safe for the same reason the rest of wxl is:
    // everything that touches this device runs on the main STA thread -- the replacement
    // too, which the pool only asks for. The compositor's own work happens elsewhere, but
    // it does not reach through this device -- it reads the surfaces afterwards.
    D2D1_CREATION_PROPERTIES const properties{D2D1_THREADING_MODE_SINGLE_THREADED,
                                              D2D1_DEBUG_LEVEL_NONE,
                                              D2D1_DEVICE_CONTEXT_OPTIONS_NONE};
    winrt::com_ptr<ID2D1Device> d2d;
    winrt::check_hresult(
        ::D2D1CreateDevice(d3d.as<IDXGIDevice>().get(), &properties, d2d.put()));

    // ID3D11Device4 is Windows 10's; the Windows App SDK runs on nothing older.
    // Not const: its reference goes to the device made below, which owns it raw (detach).
    auto removable = d3d.as<ID3D11Device4>();
    HANDLE const removed = ::CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!removed) winrt::throw_last_error();
    DWORD cookie = 0;
    winrt::check_hresult(removable->RegisterDeviceRemovedEvent(removed, &cookie));
    HANDLE wait = nullptr;
    if (!::RegisterWaitForSingleObject(&wait, removed, &device_removed,
                                       reinterpret_cast<void*>(static_cast<std::uintptr_t>(generation)),
                                       INFINITE, WT_EXECUTEONLYONCE)) {
        winrt::throw_last_error();
    }

    return rendering_device{d2d.detach(), removable.detach(), removed, cookie, wait,
                            impl::max_update_side(d3d->GetFeatureLevel())};
}

/// The watch taken back: from a replacement -- the wait fired once (WT_EXECUTEONLYONCE) and its
/// callback asked for the turn, or the UI thread found the device lost first -- or from the
/// close. A callback under way is not waited for: with no completion event UnregisterWaitEx takes
/// the wait back and returns at once, answering FALSE with ERROR_IO_PENDING when a callback is
/// still outstanding, queued or running -- which is success here, since the callback touches
/// nothing freed below (device_removed), and the turn it posts finds its device replaced or the
/// replacement closed.
void device_backend::unwatch(rendering_device& device) noexcept {
    if (!::UnregisterWaitEx(device.wait, nullptr) && ::GetLastError() != ERROR_IO_PENDING) {
        stop_on("wxl: the wait on the rendering device did not come off, Win32 error",
                ::GetLastError());
    }
    device.d3d->UnregisterDeviceRemoved(device.cookie);
    if (!::CloseHandle(device.removed)) {
        stop_on("wxl: the removal event of the rendering device did not close, Win32 error",
                ::GetLastError());
    }
    device.wait = nullptr;
    device.removed = nullptr;
}

/// Nothing of wxl's draws through the old device any more; what the owners made from it
/// holds it until they let go, which is what they are told next.
void device_backend::release(rendering_device& device) noexcept {
    device.d3d->Release();
    device.d2d->Release();
    device = rendering_device{};
}

/// The same call under the IID of the graphics device's own API. A graphics device of a closed
/// compositor refuses with RO_E_CLOSED: the compositor was closed before the UI thread's queue began
/// to shut down, against what DrawingSurface.h asks, and the process stops saying so.
template <class Interop>
void set_rendering_device(::IInspectable* graphics, ID2D1Device* d2d) {
    winrt::com_ptr<Interop> interop;
    winrt::check_hresult(graphics->QueryInterface(winrt::guid_of<Interop>(), interop.put_void()));
    HRESULT const set = interop->SetRenderingDevice(d2d);
    if (set == RO_E_CLOSED) {
        core::abort("wxl: a compositor a drawing surface was made on is closed before the UI thread's queue began "
                    "to shut down, and its graphics device cannot take the new rendering device (RO_E_CLOSED)");
    }
    winrt::check_hresult(set);
}

void device_backend::move(::IInspectable* graphics, impl::composition_api api,
                          rendering_device const& device) {
    if (api == impl::composition_api::winui)
        set_rendering_device<impl::ICompositionGraphicsDeviceInterop>(graphics, device.d2d);
    else
        set_rendering_device<system_interop::ICompositionGraphicsDeviceInterop>(graphics, device.d2d);
}

/// The turn that calls the holders after a replacement graphics_of ran, on the queue taken with the
/// first device. The queue refuses only once it is shut down, and the replacement is closed before
/// that (ShutdownStarting) and posts nothing: a refusal here is not a way out, and stops. noexcept:
/// by now everything has moved, and a TryEnqueue that throws leaves the holders uncalled and every
/// draw() answering false until the next loss -- so the exception stops the process here, named by
/// the UI thread's terminate handler, instead of leaving graphics_of for the application's code.
void device_backend::post() noexcept {
    dispatching::DispatcherQueue current{nullptr};
    winrt::copy_from_abi(current, queue);
    if (!current.TryEnqueue([]() noexcept { devices().notify(); })) {
        core::abort("wxl: the UI thread's queue refused the turn that calls the holders of a new rendering device");
    }
}

/// The identity a compositor is named by: COM's, not the interface pointer at hand -- two
/// references to one compositor through different interfaces are different addresses, and only
/// IUnknown answers the same one every time.
winrt::com_ptr<::IUnknown> identity_of(Compositor const& compositor) {
    return Object::Impl::get_typed<Compositor>(compositor)->as<::IUnknown>();
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
composition::CompositionGraphicsDevice graphics_device(composition::Compositor const& compositor) {
    winrt::com_ptr<::IUnknown> const identity = compositor.as<::IUnknown>();
    ::IInspectable* const made = devices().graphics_of(
        identity.get(), impl::composition_api::winui, [&](rendering_device const& device) {
            auto const interop = compositor.as<impl::ICompositorInterop>();
            winrt::com_ptr<::IInspectable> graphics;
            winrt::check_hresult(interop->CreateGraphicsDevice(device.d2d, graphics.put()));
            return graphics.detach();
        });

    composition::CompositionGraphicsDevice device{nullptr};
    winrt::copy_from_abi(device, made);
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

/// BeginDraw, the paint, EndDraw -- what every drawing of a surface shares, with what a lost
/// device means at each step, through the drawing interop of either composition API: the same
/// calls under IIDs of their own. `update` is the part to draw, in the surface's pixels, or null
/// for the whole surface.
///
/// The transform puts the surface's origin where the paint draws it: BeginDraw's offset is
/// where the top left corner of the update landed in the texture handed out (surfaces share
/// a texture atlas), so the origin is that offset less the update's corner. A part is also
/// clipped: BeginDraw's documentation asks the application not to draw outside the update
/// rectangle, and a paint drawing in the whole surface's coordinates reaches past any part
/// of it. Clear honours the clip, so it clears that part alone.
template <class Interop>
bool draw_on(Interop& interop, RECT const* update,
             std::function<void(ID2D1DeviceContext*)> const& paint) {
    // The device is new and its holders have not been called yet: what they made from the old one
    // would go into a context of this one. Skipped; they draw again from their call.
    if (!devices().holders_told()) return false;

    winrt::com_ptr<ID2D1DeviceContext> context;
    POINT offset{};
    HRESULT const begun = interop.BeginDraw(update, winrt::guid_of<ID2D1DeviceContext>(),
                                            context.put_void(), &offset);
    // A lost device is the one failure of BeginDraw that is not a bug: skipped, and drawn
    // again once the new device stands (RenderingDeviceWatch). A begin that failed is not
    // ended.
    if (confirmed_loss(begun)) return false;
    winrt::check_hresult(begun);

    POINT const corner = update ? POINT{update->left, update->top} : POINT{};
    context->SetTransform(D2D1::Matrix3x2F::Translation(static_cast<float>(offset.x - corner.x),
                                                        static_cast<float>(offset.y - corner.y)));
    if (update) {
        context->PushAxisAlignedClip(
            D2D1::RectF(static_cast<float>(update->left), static_cast<float>(update->top),
                        static_cast<float>(update->right), static_cast<float>(update->bottom)),
            D2D1_ANTIALIAS_MODE_ALIASED);
    }
    context->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));

    // EndDraw runs even if the drawing threw: a surface left open is a surface no later
    // draw can begin on. The clip comes off first, on every way out -- a push without its
    // pop leaves the context in error. A paint ended by a lost device (throwIfDeviceLost)
    // is a drawing skipped: whatever EndDraw says of the same device then, nothing was
    // drawn.
    auto const end = [&]() {
        if (update) context->PopAxisAlignedClip();
        return interop.EndDraw();
    };
    try {
        paint(context.get());
    } catch (DrawingSurface::DeviceLost const&) {
        HRESULT const ended = end();
        if (!confirmed_loss(ended)) winrt::check_hresult(ended);
        return false;
    } catch (...) {
        end();
        throw;
    }
    // The interop documentation says EndDraw does not report a lost device; whoever says
    // it, a lost device is that and nothing else, and gets the answer BeginDraw's does.
    HRESULT const ended = end();
    if (confirmed_loss(ended)) return false;
    winrt::check_hresult(ended);
    return true;
}

/// The drawing interop of a WinUI surface, fixed or virtual: the virtual one is held as the
/// drawing surface it derives from.
winrt::com_ptr<impl::ICompositionDrawingSurfaceInterop> interop_of(
    CompositionDrawingSurface const& surface) {
    composition::CompositionDrawingSurface const& held =
        *Object::Impl::get_typed<CompositionDrawingSurface>(surface);
    return held.as<impl::ICompositionDrawingSurfaceInterop>();
}

/// The size of a virtual surface: a side of zero or less is zero, as with a fixed one, and a
/// side over VirtualDrawingSurface::maxSide stops -- it is past what the platform documents
/// as its largest, and a caller who asks for one has a length it did not expect.
SizeInt32 virtual_size(SizeInt32 sizePixels) {
    if (sizePixels.width > VirtualDrawingSurface::maxSide ||
        sizePixels.height > VirtualDrawingSurface::maxSide) {
        char text[160];
        std::snprintf(text, sizeof text,
                      "wxl: a VirtualDrawingSurface of %dx%d pixels, over %d a side",
                      static_cast<int>(sizePixels.width), static_cast<int>(sizePixels.height),
                      static_cast<int>(VirtualDrawingSurface::maxSide));
        core::abort(text);
    }
    return {std::max(sizePixels.width, 0), std::max(sizePixels.height, 0)};
}

/// Allocates a virtual surface, in the format of a fixed one, on the same graphics device of
/// the compositor -- moved with the rest on a replacement -- and held as the drawing surface it
/// derives from: size, resize and the interop of BeginDraw are that class's, and only trim
/// asks for the virtual one.
composition::CompositionDrawingSurface allocate_virtual(composition::Compositor const& compositor,
                                                        SizeInt32 sizePixels) {
    return graphics_device(compositor).CreateVirtualDrawingSurface(
        winrt::Windows::Graphics::SizeInt32{sizePixels.width, sizePixels.height},
        directx::DirectXPixelFormat::B8G8R8A8UIntNormalized,
        directx::DirectXAlphaMode::Premultiplied);
}

/// A brush showing a surface of either kind. Named, not nested: the surface converts to the
/// projection type through its Impl and the projection type converts to ICompositionSurface,
/// and one implicit conversion is all an argument gets.
CompositionSurfaceBrush brush_of(Compositor const& owner, CompositionDrawingSurface const& shown) {
    composition::Compositor const& compositor = *Object::Impl::get_typed<Compositor>(owner);
    composition::CompositionDrawingSurface const& drawn =
        *Object::Impl::get_typed<CompositionDrawingSurface>(shown);
    return Object::Impl::wrap<CompositionSurfaceBrush>(compositor.CreateSurfaceBrush(drawn));
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

bool DrawingSurface::draw(std::function<void(ID2D1DeviceContext*)> const& paint) const {
    SizeInt32 const pixels = size();
    if (!paint || pixels.width <= 0 || pixels.height <= 0) {
        return false;  // nothing to draw on: an element that has not been measured
    }

    // Null: the whole surface. Surfaces share one texture atlas, so the origin of this one
    // is wherever the atlas put it; drawing without draw_on's transform lands on whatever
    // else is in the same texture -- which looks like another surface's contents leaking
    // into this one.
    return draw_on(*interop_of(surface_), nullptr, paint);
}

void DrawingSurface::throwIfDeviceLost(std::int32_t result) {
    if (confirmed_loss(result)) throw DeviceLost{};
}

CompositionSurfaceBrush DrawingSurface::brush() const {
    return brush_of(compositor_, surface_);
}

VirtualDrawingSurface::VirtualDrawingSurface(Compositor const& compositor, SizeInt32 sizePixels)
    : compositor_(compositor),
      surface_(Object::Impl::wrap<CompositionDrawingSurface>(allocate_virtual(
          *Object::Impl::get_typed<Compositor>(compositor), virtual_size(sizePixels)))) {}

SizeInt32 VirtualDrawingSurface::size() const {
    return surface_.sizeInt32();
}

void VirtualDrawingSurface::resize(SizeInt32 sizePixels) {
    surface_.resize(virtual_size(sizePixels));
}

std::int32_t VirtualDrawingSurface::maxUpdateSide() const {
    // The surface's graphics device was made when the surface was, and with it the rendering
    // device; a replacement makes the new one before anybody is told.
    return devices().device().max_update_side;
}

bool VirtualDrawingSurface::draw(RectInt32 updatePixels,
                                 std::function<void(ID2D1DeviceContext*)> const& paint) const {
    // Cut to the boundaries in 64 bits: x + width of two int32_t may not fit in one.
    SizeInt32 const pixels = size();
    std::int64_t const left = std::max<std::int64_t>(updatePixels.x, 0);
    std::int64_t const top = std::max<std::int64_t>(updatePixels.y, 0);
    std::int64_t const right =
        std::min<std::int64_t>(std::int64_t{updatePixels.x} + updatePixels.width, pixels.width);
    std::int64_t const bottom =
        std::min<std::int64_t>(std::int64_t{updatePixels.y} + updatePixels.height, pixels.height);
    if (!paint || left >= right || top >= bottom) return false;

    // Before the limit: a replacement graphics_of ran has the new device's limit in place
    // already, and an owner not told yet draws its parts by the old one -- a part it may no
    // longer draw at once, which is not its mistake. Skipped, as draw_on would skip it; the
    // owner cuts its parts again from its call.
    if (!devices().holders_told()) return false;

    std::int32_t const most = maxUpdateSide();
    if (right - left > most || bottom - top > most) {
        char text[192];
        std::snprintf(
            text, sizeof text,
            "wxl: a VirtualDrawingSurface update of %lldx%lld pixels, over maxUpdateSide %d",
            static_cast<long long>(right - left), static_cast<long long>(bottom - top),
            static_cast<int>(most));
        core::abort(text);
    }

    RECT const update{static_cast<LONG>(left), static_cast<LONG>(top), static_cast<LONG>(right),
                      static_cast<LONG>(bottom)};
    return draw_on(*interop_of(surface_), &update, paint);
}

// The rectangles are what stays, and no rectangles at all is nothing kept: so
// IDCompositionVirtualSurface::Trim documents them, and Microsoft's VirtualSurfaces sample
// relies on it -- TileDrawingManager::Trim hands over the tiles it still needs and counts
// them as the ones drawn.
void VirtualDrawingSurface::trim(std::span<RectInt32 const> keepPixels) const {
    std::vector<winrt::Windows::Graphics::RectInt32> kept;
    kept.reserve(keepPixels.size());
    for (RectInt32 const& part : keepPixels) {
        kept.push_back({part.x, part.y, part.width, part.height});
    }
    composition::CompositionDrawingSurface const& surface =
        *Object::Impl::get_typed<CompositionDrawingSurface>(surface_);
    surface.as<composition::CompositionVirtualDrawingSurface>().Trim(kept);
}

CompositionSurfaceBrush VirtualDrawingSurface::brush() const {
    return brush_of(compositor_, surface_);
}

namespace impl {

// The handlers of the watches on a compositor live in its entry, on wxl's own list, whose
// removal answers; the replacement calls them. A watch put on before any surface makes the
// entry without a graphics device, and a watch taken off finds the entry its attach made -- and
// the last one off lets that entry go, if no surface was made on the compositor.

EventToken rendering_device_replaced::add(Compositor const& compositor, EventHandler<Object> handler) {
    return devices()
        .handlers_of(identity_of(compositor).get(), composition_api::winui)
        .add(next_event_token(), std::move(handler));
}

bool rendering_device_replaced::remove(Compositor const& compositor, EventToken token) {
    return devices().remove(identity_of(compositor).get(), token);
}

::IInspectable* system_graphics_device(system_compositor compositor) {
    return devices().graphics_of(compositor.identity, composition_api::system,
                                 [&](rendering_device const& device) {
                                     winrt::com_ptr<system_interop::ICompositorInterop> interop;
                                     winrt::check_hresult(compositor.identity->QueryInterface(
                                         winrt::guid_of<system_interop::ICompositorInterop>(),
                                         interop.put_void()));
                                     winrt::com_ptr<system_interop::ICompositionGraphicsDevice> graphics;
                                     winrt::check_hresult(
                                         interop->CreateGraphicsDevice(device.d2d, graphics.put()));
                                     return static_cast<::IInspectable*>(graphics.detach());
                                 });
}

bool draw_system_surface(::IUnknown* surface, std::function<void(ID2D1DeviceContext*)> const& paint) {
    if (!paint) return false;

    winrt::com_ptr<system_interop::ICompositionDrawingSurfaceInterop> interop;
    winrt::check_hresult(surface->QueryInterface(
        winrt::guid_of<system_interop::ICompositionDrawingSurfaceInterop>(), interop.put_void()));
    return draw_on(*interop, nullptr, paint);
}

EventToken system_rendering_device_replaced::add(system_compositor compositor, EventHandler<Object> handler) {
    return devices()
        .handlers_of(compositor.identity, composition_api::system)
        .add(next_event_token(), std::move(handler));
}

bool system_rendering_device_replaced::remove(system_compositor compositor, EventToken token) {
    return devices().remove(compositor.identity, token);
}

std::uint64_t rendering_device_replacements() noexcept {
    return devices().replacements();
}

void ensure_rendering_device() {
    static_cast<void>(devices().device());
}

void signal_rendering_device_removed() {
    // The event the D3D device sets when it is removed is ours, made in device_backend::make:
    // setting it is what the device does, and the wait cannot tell the two apart. Once the
    // queue has begun to shut down there is no event: nothing is replaced any more.
    rendering_device const* const device = devices().current();
    if (!device) core::abort("wxl: no rendering device to lose -- no drawing surface yet");
    if (devices().closed()) return;
    if (!::SetEvent(device->removed)) winrt::throw_last_error();
}

}  // namespace impl

}  // namespace wxl

// The four links between an application's Direct2D and the compositor.
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
#include "impl/handler_list.h"

namespace wxl {
namespace {

namespace composition = winrt::Microsoft::UI::Composition;
namespace directx = winrt::Microsoft::Graphics::DirectX;
namespace dispatching = winrt::Microsoft::UI::Dispatching;

/// The Direct2D device every drawing surface of this process draws through, and what
/// watches it for being lost.
///
/// One at a time, made on first use and again after each loss: a D3D11 device is the most
/// expensive object in the chain, and nothing about it belongs to a compositor -- several
/// graphics devices sit on this one happily.
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
    // The UI thread's queue, taken when the first device is made -- on the UI thread, as
    // every surface is. The pool hands the news of a loss over through it; it is agile, and
    // only ever read after that first device, so the pool reads it without a lock.
    ::IInspectable* queue = nullptr;
};

rendering_device& current_device() {
    static rendering_device kept;
    return kept;
}

/// The graphics device of one compositor and the handlers of RenderingDeviceWatch on it.
/// All three pointers are leaked deliberately, for the reason rendering_device gives; the
/// compositor's is kept only as an identity to compare against, is never called through,
/// and being immortal cannot be a stale address a later compositor happens to reuse.
struct graphics_entry {
    ::IUnknown* owner;
    ::IInspectable* device;
    impl::handler_list<Object>* replaced;
};

/// Every graphics device made, one per compositor. Tiny by nature: two entries for a
/// window, four for two windows.
std::vector<graphics_entry>& graphics_entries() {
    static std::vector<graphics_entry> kept;
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
    ID3D11Device4* const d3d = current_device().d3d;
    if (d3d && d3d->GetDeviceRemovedReason() != S_OK) return true;
    stop_on("wxl: a drawing call answered that the device is lost, and the device is not",
            static_cast<std::uint32_t>(result));
}

void replace_device() noexcept;

/// The pool's half of a loss: hand the news to the UI thread, which owns the device and
/// every surface. Nothing else happens on the pool's thread. A queue that takes nothing
/// more is going down, and then there is nothing left to draw. noexcept: the pool has
/// nobody to hand an exception to.
void CALLBACK device_removed(void*, BOOLEAN) noexcept {
    // MSVC keeps a terminate handler for each thread, so the one that names the exception is
    // set here too: an exception out of the handing over -- the projection's TryEnqueue, the
    // delegate it is given -- ends in std::terminate. A thread of the pool is the system's and
    // runs other callbacks after this one, so it gets its own handler back. The handler and
    // not a catch, as post_completion has: it names a terminate a noexcept call inside raises too.
    const std::terminate_handler previous = core::impl::set_terminate_with_reason();

    dispatching::DispatcherQueue queue{nullptr};
    winrt::copy_from_abi(queue, current_device().queue);
    queue.TryEnqueue([]() noexcept { replace_device(); });

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
/// nothing that looks again and again -- hears it once.
void make_device(rendering_device& device) {
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
    // Not const: its reference goes to device.d3d, which owns it raw (detach below).
    auto removable = d3d.as<ID3D11Device4>();
    HANDLE const removed = ::CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!removed) winrt::throw_last_error();
    DWORD cookie = 0;
    winrt::check_hresult(removable->RegisterDeviceRemovedEvent(removed, &cookie));
    HANDLE wait = nullptr;
    if (!::RegisterWaitForSingleObject(&wait, removed, &device_removed, nullptr, INFINITE,
                                       WT_EXECUTEONLYONCE)) {
        winrt::throw_last_error();
    }

    device.max_update_side = impl::max_update_side(d3d->GetFeatureLevel());
    device.d2d = d2d.detach();
    device.d3d = removable.detach();
    device.removed = removed;
    device.cookie = cookie;
    device.wait = wait;
}

ID2D1Device* d2d_device() {
    rendering_device& device = current_device();
    if (device.d2d) return device.d2d;

    if (!device.queue) {
        auto queue = dispatching::DispatcherQueue::GetForCurrentThread();
        if (!queue) core::abort("wxl: a DrawingSurface is made off the UI thread");
        device.queue = static_cast<::IInspectable*>(winrt::detach_abi(queue));
    }
    make_device(device);
    return device.d2d;
}

/// The UI thread's half of a loss: the old device is let go, a new one made, every graphics
/// device moved onto it, and then -- only then, when no surface is left on the old one --
/// every RenderingDeviceWatch is called. Between the loss and here every draw() skips.
///
/// The watches are called from here and not from the graphics devices' own
/// RenderingDeviceReplaced: whether that event is raised inside SetRenderingDevice or on a
/// later turn the documentation does not say, and if later, a drawing between the two would
/// draw what the owner made from the old device into a context of the new one --
/// D2DERR_WRONG_RESOURCE_DOMAIN at EndDraw, which is a failure. The graphics devices are
/// wxl's own and nobody else replaces their device, so the event says nothing wxl does not
/// know here, and nothing listens to it.
///
/// noexcept: this runs from a turn of the queue, and a graphics device that refuses the new
/// device leaves nothing to draw with. The exception names itself on the way out through
/// wxl's terminate handler.
void replace_device() noexcept {
    rendering_device& device = current_device();

    // The wait fired once (WT_EXECUTEONLYONCE) and its callback asked for this turn; it is
    // waited out here, nowhere near the callback itself, before the wait is let go.
    if (!::UnregisterWaitEx(device.wait, INVALID_HANDLE_VALUE)) {
        stop_on("wxl: the wait on the lost device did not come off, Win32 error",
                ::GetLastError());
    }
    device.d3d->UnregisterDeviceRemoved(device.cookie);
    if (!::CloseHandle(device.removed)) {
        stop_on("wxl: the removal event of the lost device did not close, Win32 error",
                ::GetLastError());
    }
    device.d3d->Release();
    ID2D1Device* const lost = device.d2d;
    device = rendering_device{.queue = device.queue};

    make_device(device);

    // By index, and the count taken first: a watch drawing again may be the first to draw
    // on another compositor and add an entry -- made on the new device already, so it has
    // nothing to be moved or told.
    std::vector<graphics_entry> const& entries = graphics_entries();
    std::size_t const known = entries.size();
    for (std::size_t at = 0; at < known; ++at) {
        winrt::com_ptr<impl::ICompositionGraphicsDeviceInterop> interop;
        winrt::check_hresult(entries[at].device->QueryInterface(
            winrt::guid_of<impl::ICompositionGraphicsDeviceInterop>(), interop.put_void()));
        winrt::check_hresult(interop->SetRenderingDevice(device.d2d));
    }

    // Nothing of wxl's draws through the old device any more; what the owners made from it
    // holds it until they let go, which is what they are told now.
    lost->Release();
    for (std::size_t at = 0; at < known; ++at) {
        entries[at].replaced->fire(Object::Impl::empty<Object>());
    }
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
/// The entry keeps the handlers of the watches on this compositor, on wxl's own list,
/// whose removal answers; replace_device calls them.
graphics_entry graphics_entry_of(composition::Compositor const& compositor) {
    // COM identity, not the interface pointer at hand: two references to one
    // compositor through different interfaces are different addresses, and
    // only IUnknown answers the same one every time.
    winrt::com_ptr<::IUnknown> identity = compositor.as<::IUnknown>();

    for (graphics_entry const& known : graphics_entries()) {
        if (known.owner == identity.get()) return known;
    }

    auto const interop = compositor.as<impl::ICompositorInterop>();
    winrt::com_ptr<::IInspectable> made;
    winrt::check_hresult(interop->CreateGraphicsDevice(d2d_device(), made.put()));

    graphics_entry const entry{identity.detach(), made.detach(), new impl::handler_list<Object>{}};
    graphics_entries().push_back(entry);
    return entry;
}

composition::CompositionGraphicsDevice graphics_device(composition::Compositor const& compositor) {
    composition::CompositionGraphicsDevice device{nullptr};
    winrt::copy_from_abi(device, graphics_entry_of(compositor).device);
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

/// Allocates a virtual surface, in the format of a fixed one, held as the drawing surface it
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

/// BeginDraw, the paint, EndDraw -- what every drawing of a surface shares, with what a lost
/// device means at each step. `update` is the part to draw, in the surface's pixels, or null
/// for the whole surface.
///
/// The transform puts the surface's origin where the paint draws it: BeginDraw's offset is
/// where the top left corner of the update landed in the texture handed out (surfaces share
/// a texture atlas), so the origin is that offset less the update's corner. A part is also
/// clipped: BeginDraw's documentation asks the application not to draw outside the update
/// rectangle, and a paint drawing in the whole surface's coordinates reaches past any part
/// of it. Clear honours the clip, so it clears that part alone.
bool draw_on(composition::CompositionDrawingSurface const& surface, RECT const* update,
             std::function<void(ID2D1DeviceContext*)> const& paint) {
    auto const interop = surface.as<impl::ICompositionDrawingSurfaceInterop>();

    winrt::com_ptr<ID2D1DeviceContext> context;
    POINT offset{};
    HRESULT const begun = interop->BeginDraw(update, winrt::guid_of<ID2D1DeviceContext>(),
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
        return interop->EndDraw();
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
    return draw_on(*Object::Impl::get_typed<CompositionDrawingSurface>(surface_), nullptr, paint);
}

char const* DrawingSurface::DeviceLost::what() const noexcept {
    return "wxl: the rendering device is lost";
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
    return current_device().max_update_side;
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
    return draw_on(*Object::Impl::get_typed<CompositionDrawingSurface>(surface_), &update, paint);
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

EventToken rendering_device_replaced::add(Compositor const& compositor, EventHandler<Object> handler) {
    graphics_entry const entry = graphics_entry_of(*Object::Impl::get_typed<Compositor>(compositor));
    return entry.replaced->add(next_event_token(), std::move(handler));
}

bool rendering_device_replaced::remove(Compositor const& compositor, EventToken token) {
    graphics_entry const entry = graphics_entry_of(*Object::Impl::get_typed<Compositor>(compositor));
    return entry.replaced->remove(token);
}

void signal_rendering_device_removed() {
    // The event the D3D device sets when it is removed is ours, made in make_device:
    // setting it is what the device does, and the wait cannot tell the two apart.
    HANDLE const removed = current_device().removed;
    if (!removed) core::abort("wxl: no rendering device to lose -- no DrawingSurface yet");
    if (!::SetEvent(removed)) winrt::throw_last_error();
}

}  // namespace impl

}  // namespace wxl

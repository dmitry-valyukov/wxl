// The overlay, the snapshot and the flight -- a window animated to a point on
// the system compositor.
//
// The winrt and Windows headers come first, and with them the standard
// library they pull in textually: WindowShade.h reaches wxl.core through
// core.h, and a standard header included after that import is one MSVC has
// already seen through the std module. WindowShade.h is last for the same
// reason.
//
// The system compositor (Windows.UI.Composition), not WinUI's -- because the
// target is a bare HWND of our own and the flight goes outside any window,
// which is exactly what a DesktopWindowTarget is for and what a XAML island's
// compositor is not. The four links from Direct2D to a composition surface
// are the same ones DrawingSurface uses, here against this compositor.
//
// GDI is needed for the window snapshot (PrintWindow into a DIB), so NOGDI is
// lifted for this one file, the way WindowBackdrop.cpp does. It is first,
// before the winrt headers pull windows.h in with NOGDI still in force.
// NODRAWTEXT stays: without it a DrawText macro would rewrite the DrawText
// method d2d1 declares on ID2D1DeviceContext.
#undef NOGDI

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Numerics.h>
#include <winrt/Windows.Graphics.DirectX.h>
#include <winrt/Windows.System.h>
#include <winrt/Windows.UI.Composition.Desktop.h>
#include <winrt/Windows.UI.Composition.h>

#include <windows.h>

#include <d2d1_1.h>
#include <d3d11.h>
#include <dispatcherqueue.h>
#include <dxgi.h>
#include <windows.ui.composition.interop.h>

#include <chrono>
#include <vector>

#include "WindowShade.h"

namespace wxl {
namespace {

namespace comp = winrt::Windows::UI::Composition;
namespace desktop = winrt::Windows::UI::Composition::Desktop;
namespace numerics = winrt::Windows::Foundation::Numerics;
namespace directx = winrt::Windows::Graphics::DirectX;

wchar_t const kOverlayClass[] = L"wxl.WindowShade.Overlay";

/// The overlay is composed by DWM, never drawn into by GDI, and never taken
/// as input: it only carries the flying snapshot above everything else.
ATOM overlayClass() {
    static ATOM const registered = [] {
        WNDCLASSEXW description{};
        description.cbSize = sizeof description;
        description.lpfnWndProc = &::DefWindowProcW;
        description.hInstance = ::GetModuleHandleW(nullptr);
        description.lpszClassName = kOverlayClass;
        return ::RegisterClassExW(&description);
    }();
    return registered;
}

struct Snapshot {
    std::vector<std::uint32_t> pixels;  // BGRA, top-down
    int width = 0;
    int height = 0;
    RECT rect{};  // where the window was, in screen pixels
};

/// The window's pixels, now, as an opaque bitmap. PrintWindow draws through
/// GDI, which leaves the alpha channel at zero; a window snapshot is opaque,
/// so the alpha is forced to full -- without it the compositor would blend
/// the whole thing away to nothing.
Snapshot capture(HWND window) {
    RECT rect{};
    if (!::GetWindowRect(window, &rect)) return {};

    int const width = rect.right - rect.left;
    int const height = rect.bottom - rect.top;
    if (width <= 0 || height <= 0) return {};

    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof info.bmiHeader;
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;  // top-down, the way the surface reads
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;

    HDC const screen = ::GetDC(nullptr);
    HDC const memory = ::CreateCompatibleDC(screen);
    void* bits = nullptr;
    HBITMAP const bitmap = ::CreateDIBSection(memory, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    HGDIOBJ const previous = ::SelectObject(memory, bitmap);

    Snapshot shot;
    if (bits && ::PrintWindow(window, memory, PW_RENDERFULLCONTENT)) {
        shot.width = width;
        shot.height = height;
        shot.rect = rect;
        shot.pixels.assign(static_cast<std::uint32_t*>(bits),
                           static_cast<std::uint32_t*>(bits) + std::size_t(width) * height);
        for (std::uint32_t& pixel : shot.pixels) pixel |= 0xFF000000u;
    }

    ::SelectObject(memory, previous);
    ::DeleteObject(bitmap);
    ::DeleteDC(memory);
    ::ReleaseDC(nullptr, screen);
    return shot;
}

/// The rectangle the flight needs: the window's place and the point it goes
/// to, with room around the point for the shrunk snapshot.
///
/// Deliberately not the whole screen. A topmost window that covers the entire
/// monitor trips Windows' "an app went full-screen" rule and turns Focus
/// Assist on for the blink the animation lasts -- the do-not-disturb icon
/// flickering in the corner. Kept a pixel short of the virtual screen on every
/// side, it never is that window, whatever the window's size.
RECT flightBounds(RECT windowRect, Point target) {
    long const margin = 48;
    RECT bounds{
        std::min(windowRect.left, long(target.x) - margin),
        std::min(windowRect.top, long(target.y) - margin),
        std::max(windowRect.right, long(target.x) + margin),
        std::max(windowRect.bottom, long(target.y) + margin),
    };

    long const left = ::GetSystemMetrics(SM_XVIRTUALSCREEN);
    long const top = ::GetSystemMetrics(SM_YVIRTUALSCREEN);
    long const right = left + ::GetSystemMetrics(SM_CXVIRTUALSCREEN);
    long const bottom = top + ::GetSystemMetrics(SM_CYVIRTUALSCREEN);
    bounds.left = std::max(bounds.left, left + 1);
    bounds.top = std::max(bounds.top, top + 1);
    bounds.right = std::min(bounds.right, right - 1);
    bounds.bottom = std::min(bounds.bottom, bottom - 1);
    return bounds;
}

constexpr std::chrono::milliseconds kFlight{240};

/// How small the snapshot shrinks to at the point: about a tray icon's worth
/// of pixels, so it does not vanish to nothing before it arrives.
constexpr float kLandedPixels = 24.0f;

}  // namespace

struct WindowShade::Impl {
    HWND window = nullptr;
    HWND overlay = nullptr;
    bool ready = false;

    comp::Compositor compositor{nullptr};
    winrt::Windows::System::DispatcherQueueController queue{nullptr};
    desktop::DesktopWindowTarget target{nullptr};
    comp::CompositionGraphicsDevice graphics{nullptr};
    comp::ContainerVisual root{nullptr};
    comp::SpriteVisual sprite{nullptr};
    comp::CompositionScopedBatch batch{nullptr};

    // The snapshot the flight carries, kept between collapse and expand: the
    // window is hidden by the time it expands, so a fresh capture then would
    // be blank.
    RECT windowRect{};
    bool hasSnapshot = false;
    POINT overlayOrigin{};

    explicit Impl(HWND w) : window(w) { build(); }

    ~Impl() {
        if (overlay) ::DestroyWindow(overlay);
    }

    void build() try {
        if (!overlayClass()) return;

        // Created tiny and moved to the flight's rectangle before each show
        // (placeOverlay): it is never a full-screen window, so it never trips
        // Focus Assist -- see flightBounds.
        overlay = ::CreateWindowExW(WS_EX_NOREDIRECTIONBITMAP | WS_EX_TRANSPARENT |
                                        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
                                    kOverlayClass, L"", WS_POPUP, 0, 0, 1, 1, nullptr, nullptr,
                                    ::GetModuleHandleW(nullptr), nullptr);
        if (!overlay) return;

        // The compositor needs a Windows.System dispatcher on the thread; the
        // WinUI one is a different queue, so make ours where there is none.
        if (!winrt::Windows::System::DispatcherQueue::GetForCurrentThread()) {
            DispatcherQueueOptions options{sizeof(DispatcherQueueOptions), DQTYPE_THREAD_CURRENT,
                                           DQTAT_COM_STA};
            ABI::Windows::System::IDispatcherQueueController* controller = nullptr;
            winrt::check_hresult(::CreateDispatcherQueueController(options, &controller));
            winrt::attach_abi(queue, controller);
        }

        compositor = comp::Compositor{};
        graphics = makeGraphicsDevice();

        auto const interop = compositor.as<ABI::Windows::UI::Composition::Desktop::
                                               ICompositorDesktopInterop>();
        ABI::Windows::UI::Composition::Desktop::IDesktopWindowTarget* raw = nullptr;
        winrt::check_hresult(interop->CreateDesktopWindowTarget(overlay, true, &raw));
        winrt::attach_abi(target, raw);

        root = compositor.CreateContainerVisual();
        target.Root(root);
        sprite = compositor.CreateSpriteVisual();
        root.Children().InsertAtTop(sprite);

        ready = true;
    } catch (...) {
        // No compositor: the shade still hides and shows the window, only
        // without the flight, so a caller never has to check.
        ready = false;
    }

    comp::CompositionGraphicsDevice makeGraphicsDevice() {
        winrt::com_ptr<ID3D11Device> d3d;
        winrt::check_hresult(::D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
                                                 D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0,
                                                 D3D11_SDK_VERSION, d3d.put(), nullptr, nullptr));

        D2D1_CREATION_PROPERTIES const properties{D2D1_THREADING_MODE_SINGLE_THREADED,
                                                  D2D1_DEBUG_LEVEL_NONE,
                                                  D2D1_DEVICE_CONTEXT_OPTIONS_NONE};
        winrt::com_ptr<ID2D1Device> d2d;
        winrt::check_hresult(
            ::D2D1CreateDevice(d3d.as<IDXGIDevice>().get(), &properties, d2d.put()));

        auto const interop = compositor.as<ABI::Windows::UI::Composition::ICompositorInterop>();
        ABI::Windows::UI::Composition::ICompositionGraphicsDevice* raw = nullptr;
        winrt::check_hresult(interop->CreateGraphicsDevice(d2d.as<::IUnknown>().get(), &raw));
        comp::CompositionGraphicsDevice device{nullptr};
        winrt::attach_abi(device, raw);
        return device;
    }

    /// Draws the snapshot into a surface and wears it on the sprite, sized to
    /// the window.
    void wear(Snapshot const& shot) {
        comp::CompositionDrawingSurface surface = graphics.CreateDrawingSurface(
            {float(shot.width), float(shot.height)},
            directx::DirectXPixelFormat::B8G8R8A8UIntNormalized,
            directx::DirectXAlphaMode::Premultiplied);

        auto const interop =
            surface.as<ABI::Windows::UI::Composition::ICompositionDrawingSurfaceInterop>();
        winrt::com_ptr<ID2D1DeviceContext> context;
        POINT offset{};
        winrt::check_hresult(interop->BeginDraw(nullptr, winrt::guid_of<ID2D1DeviceContext>(),
                                                context.put_void(), &offset));
        // The surface lives in a shared atlas, so its origin is wherever the
        // atlas put it -- drawing without this transform lands on a neighbour.
        context->SetTransform(D2D1::Matrix3x2F::Translation(float(offset.x), float(offset.y)));
        context->Clear(D2D1::ColorF(0, 0, 0, 0));

        D2D1_BITMAP_PROPERTIES const props{
            {DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED}, 96, 96};
        winrt::com_ptr<ID2D1Bitmap> bitmap;
        context->CreateBitmap(D2D1_SIZE_U{UINT32(shot.width), UINT32(shot.height)},
                              shot.pixels.data(), UINT32(shot.width) * 4, props, bitmap.put());
        if (bitmap) {
            context->DrawBitmap(bitmap.get(),
                                D2D1_RECT_F{0, 0, float(shot.width), float(shot.height)});
        }
        winrt::check_hresult(interop->EndDraw());

        auto brush = compositor.CreateSurfaceBrush(surface);
        brush.Stretch(comp::CompositionStretch::Fill);
        sprite.Brush(brush);
        sprite.Size({float(shot.width), float(shot.height)});
    }

    /// Moves the overlay to where the flight needs it, and remembers its
    /// origin so screen points map into its composition coordinates.
    void placeOverlay(RECT bounds) {
        ::MoveWindow(overlay, bounds.left, bounds.top, bounds.right - bounds.left,
                     bounds.bottom - bounds.top, FALSE);
        overlayOrigin = {bounds.left, bounds.top};
    }

    /// A screen point in the overlay's own composition coordinates.
    numerics::float3 overlayPoint(float screenX, float screenY) const {
        return {screenX - overlayOrigin.x, screenY - overlayOrigin.y, 0};
    }

    /// Flies the sprite from one placement to another with a fade, hiding the
    /// overlay when it lands and running `whenDone` there.
    void fly(numerics::float3 fromOffset, numerics::float3 fromScale, float fromOpacity,
             numerics::float3 toOffset, numerics::float3 toScale, float toOpacity,
             std::function<void()> whenDone) {
        sprite.Offset(fromOffset);
        sprite.Scale(fromScale);
        sprite.Opacity(fromOpacity);

        auto const easing = compositor.CreateCubicBezierEasingFunction({0.3f, 0.0f}, {0.2f, 1.0f});
        auto const span = std::chrono::duration_cast<winrt::Windows::Foundation::TimeSpan>(kFlight);

        auto offset = compositor.CreateVector3KeyFrameAnimation();
        offset.InsertKeyFrame(1.0f, toOffset, easing);
        offset.Duration(span);
        auto scale = compositor.CreateVector3KeyFrameAnimation();
        scale.InsertKeyFrame(1.0f, toScale, easing);
        scale.Duration(span);
        auto opacity = compositor.CreateScalarKeyFrameAnimation();
        opacity.InsertKeyFrame(1.0f, toOpacity, easing);
        opacity.Duration(span);

        batch = compositor.CreateScopedBatch(comp::CompositionBatchTypes::Animation);
        sprite.StartAnimation(L"Offset", offset);
        sprite.StartAnimation(L"Scale", scale);
        sprite.StartAnimation(L"Opacity", opacity);
        batch.End();

        batch.Completed([this, done = std::move(whenDone)](auto&&, auto&&) {
            if (done) done();
            ::ShowWindow(overlay, SW_HIDE);
        });
    }
};

WindowShade::WindowShade(HWND__* window)
    : impl_(std::make_unique<Impl>(reinterpret_cast<HWND>(window))) {}

WindowShade::~WindowShade() = default;
WindowShade::WindowShade(WindowShade&&) noexcept = default;
WindowShade& WindowShade::operator=(WindowShade&&) noexcept = default;

void WindowShade::collapseTo(Point screenPoint, std::function<void()> whenDone) const {
    Impl& shade = *impl_;

    Snapshot const shot = shade.ready ? capture(shade.window) : Snapshot{};
    if (!shade.ready || shot.width <= 0) {
        // No compositor, or nothing to snapshot: hide and be done.
        ::ShowWindow(shade.window, SW_HIDE);
        if (whenDone) whenDone();
        return;
    }

    shade.wear(shot);
    shade.windowRect = shot.rect;
    shade.hasSnapshot = true;

    // The overlay over just the path, then up with the snapshot exactly over
    // the window, then the window hidden behind it -- no gap where neither
    // shows.
    shade.placeOverlay(flightBounds(shot.rect, screenPoint));
    numerics::float3 const from = shade.overlayPoint(float(shot.rect.left), float(shot.rect.top));
    ::ShowWindow(shade.overlay, SW_SHOWNOACTIVATE);
    ::ShowWindow(shade.window, SW_HIDE);

    float const scale = shot.width > 0 ? kLandedPixels / shot.width : 0.1f;
    numerics::float3 const to = shade.overlayPoint(screenPoint.x, screenPoint.y);
    shade.fly(from, {1, 1, 1}, 1.0f, to, {scale, scale, 1}, 0.0f, std::move(whenDone));
}

void WindowShade::expandFrom(Point screenPoint, std::function<void()> whenDone) const {
    Impl& shade = *impl_;

    if (!shade.ready || !shade.hasSnapshot) {
        ::ShowWindow(shade.window, SW_SHOW);
        if (whenDone) whenDone();
        return;
    }

    RECT const& rect = shade.windowRect;
    float const width = float(rect.right - rect.left);
    float const scale = width > 0 ? kLandedPixels / width : 0.1f;

    shade.placeOverlay(flightBounds(rect, screenPoint));
    numerics::float3 const from = shade.overlayPoint(screenPoint.x, screenPoint.y);
    numerics::float3 const to = shade.overlayPoint(float(rect.left), float(rect.top));

    ::ShowWindow(shade.overlay, SW_SHOWNOACTIVATE);
    // The window is shown under the snapshot only once the flight lands, so it
    // does not flash at full size before the animation has grown to it.
    shade.fly(from, {scale, scale, 1}, 0.0f, to, {1, 1, 1}, 1.0f,
              [&shade, done = std::move(whenDone)] {
                  ::ShowWindow(shade.window, SW_SHOW);
                  if (done) done();
              });
}

}  // namespace wxl

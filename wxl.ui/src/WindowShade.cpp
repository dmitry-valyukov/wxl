// The overlay, the snapshot and the flight -- a window animated to a point on
// the system compositor.
//
// The winrt and Windows headers come first, and with them the standard
// library they pull in textually: WindowShade.h reaches wxl.core through
// core.h, and a standard header included after that import is one MSVC has
// already seen through the std module. The wxl headers are last for the same
// reason.
//
// The system compositor (Windows.UI.Composition), not WinUI's -- because the
// target is a bare HWND of our own and the flight goes outside any window,
// which is exactly what a DesktopWindowTarget is for and what a XAML island's
// compositor is not. Its graphics device sits on the rendering device every
// DrawingSurface draws through, and is moved with them when that device is
// lost and replaced (impl/system_drawing.h); the snapshot is drawn again then.
//
// GDI is needed for the window snapshot (PrintWindow into a DIB), so NOGDI is
// lifted for this one file, the way WindowBackdrop.cpp does. It is first,
// before platform.h brings windows.h in with NOGDI still in force.
// NODRAWTEXT stays: without it a DrawText macro would rewrite the DrawText
// method d2d1 declares on ID2D1DeviceContext.
#undef NOGDI
#include "platform.h"

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Numerics.h>
#include <winrt/Windows.Graphics.DirectX.h>
#include <winrt/Windows.UI.Composition.Desktop.h>
#include <winrt/Windows.UI.Composition.h>

#include <d2d1_1.h>
#include <windows.ui.composition.interop.h>

#include <chrono>
#include <cstdint>
#include <memory>
#include <vector>

#include "WindowShade.h"
#include "DrawingSurface.h"
#include "impl/system_drawing.h"

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

    comp::Compositor compositor{nullptr};
    desktop::DesktopWindowTarget target{nullptr};
    comp::CompositionGraphicsDevice graphics{nullptr};
    comp::ContainerVisual root{nullptr};
    comp::SpriteVisual sprite{nullptr};
    comp::CompositionScopedBatch batch{nullptr};

    // The snapshot the flight carries, kept between collapse and expand: the
    // window is hidden by the time it expands, so a fresh capture then would
    // be blank -- and kept whole, pixels too, so that a surface the lost
    // device took with it can be drawn again. Let go once the expand lands.
    Snapshot shot;
    comp::CompositionDrawingSurface surface{nullptr};
    bool hasSnapshot = false;
    // Commands so far, collapse and expand alike: which one a flight's
    // end is of. A flight that a later command cut short ends doing
    // nothing -- the later command's own end, or the command itself if it
    // has no flight, decides what is shown.
    std::uint64_t command = 0;

    // Where the end of a flight finds the shade: the shade while it lives,
    // null once it is gone. The batch's Completed outlives the shade -- the
    // compositor raises it when the animation ends or is cut, and a shade
    // destroyed mid-flight is gone by then -- so the delegate holds the link,
    // as the completion of an operation holds its producer's.
    struct link {
        Impl* self;
    };
    std::shared_ptr<link> const reach = std::make_shared<link>(link{this});
    POINT overlayOrigin{};

    // A failure to build is thrown on, with the overlay made before it taken
    // down: the destructor of an Impl that was never made does not run.
    explicit Impl(HWND w) : window(w) {
        try {
            build();
        } catch (...) {
            if (overlay) ::DestroyWindow(overlay);
            throw;
        }
    }

    ~Impl() {
        reach->self = nullptr;
        if (overlay) ::DestroyWindow(overlay);
    }

    void build() {
        if (!overlayClass()) winrt::throw_last_error();

        // Created tiny and moved to the flight's rectangle before each show
        // (placeOverlay): it is never a full-screen window, so it never trips
        // Focus Assist -- see flightBounds.
        overlay = ::CreateWindowExW(WS_EX_NOREDIRECTIONBITMAP | WS_EX_TRANSPARENT |
                                        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
                                    kOverlayClass, L"", WS_POPUP, 0, 0, 1, 1, nullptr, nullptr,
                                    ::GetModuleHandleW(nullptr), nullptr);
        if (!overlay) winrt::throw_last_error();

        // The compositor needs a Windows.System dispatcher on the thread, and the
        // thread has one: the I/O loop started in launch.cpp made it.
        compositor = comp::Compositor{};

        auto const interop = compositor.as<ABI::Windows::UI::Composition::Desktop::
                                               ICompositorDesktopInterop>();
        ABI::Windows::UI::Composition::Desktop::IDesktopWindowTarget* raw = nullptr;
        winrt::check_hresult(interop->CreateDesktopWindowTarget(overlay, true, &raw));
        winrt::attach_abi(target, raw);

        root = compositor.CreateContainerVisual();
        target.Root(root);
        sprite = compositor.CreateSpriteVisual();
        root.Children().InsertAtTop(sprite);

        deviceWatch.attach(identity(), *this);
    }

    impl::system_compositor identity() const {
        return {compositor.as<::IUnknown>().get()};
    }

    /// The graphics device, made with the first snapshot: a shade that never
    /// flies makes none.
    comp::CompositionGraphicsDevice const& graphicsDevice() {
        if (!graphics) winrt::copy_from_abi(graphics, impl::system_graphics_device(identity()));
        return graphics;
    }

    /// Draws the snapshot into a surface of its size and wears it on the
    /// sprite, sized to the window.
    void wear(Snapshot taken) {
        comp::CompositionGraphicsDevice const& device = graphicsDevice();
        shot = std::move(taken);
        surface = device.CreateDrawingSurface(
            {float(shot.width), float(shot.height)},
            directx::DirectXPixelFormat::B8G8R8A8UIntNormalized,
            directx::DirectXAlphaMode::Premultiplied);
        draw();

        auto brush = compositor.CreateSurfaceBrush(surface);
        brush.Stretch(comp::CompositionStretch::Fill);
        sprite.Brush(brush);
        sprite.Size({float(shot.width), float(shot.height)});
    }

    /// The snapshot's pixels into the surface. Not drawn on a lost device: the
    /// replacement calls deviceReplaced, and the snapshot is drawn then.
    void draw() {
        impl::draw_system_surface(surface.as<::IUnknown>().get(), [this](ID2D1DeviceContext* context) {
            D2D1_BITMAP_PROPERTIES const props{
                {DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED}, 96, 96};
            winrt::com_ptr<ID2D1Bitmap> bitmap;
            HRESULT const made = context->CreateBitmap(
                D2D1_SIZE_U{UINT32(shot.width), UINT32(shot.height)}, shot.pixels.data(),
                UINT32(shot.width) * 4, props, bitmap.put());
            DrawingSurface::throwIfDeviceLost(made);
            winrt::check_hresult(made);
            context->DrawBitmap(bitmap.get(),
                                D2D1_RECT_F{0, 0, float(shot.width), float(shot.height)});
        });
    }

    /// The flight back is over and the window shows: the snapshot goes -- its
    /// pixels and its surface, the next collapse takes a fresh one -- and a
    /// replacement of the device has nothing to draw again.
    void land() {
        hasSnapshot = false;
        sprite.Brush(nullptr);
        surface = nullptr;
        shot = Snapshot{};
    }

    /// The device under the surface was replaced, and the surface is empty:
    /// the snapshot is drawn again, if there is one to draw. noexcept, as the
    /// watch asks: a failure here stops the process, named.
    void deviceReplaced() noexcept {
        if (hasSnapshot) draw();
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

    /// Whether a collapse flies the snapshot worn rather than a fresh
    /// capture: the window is hidden already -- by the flight back this
    /// collapse cuts short, or by a collapse before it -- and PrintWindow on a
    /// hidden window fails or answers with black or stale pixels, which would
    /// take the place of the good snapshot the next expand shows.
    bool keepsSnapshot() const { return hasSnapshot && !::IsWindowVisible(window); }

    /// The end of the flight of command `given`: unless a later command has
    /// come since, `done` runs and the overlay is hidden. A later command's
    /// StartAnimation on the same properties ends this flight's batch at once,
    /// and its end then would show or hide what the later command does not.
    /// Asked again after `done`: a command given from it is the later one.
    void flown(std::uint64_t given, std::function<void()> const& done) {
        if (given != command) return;
        if (done) done();
        if (given != command) return;
        ::ShowWindow(overlay, SW_HIDE);
    }

    /// Flies the sprite from one placement to another with a fade, for command
    /// `given`: at its end, flown().
    void fly(numerics::float3 fromOffset, numerics::float3 fromScale, float fromOpacity,
             numerics::float3 toOffset, numerics::float3 toScale, float toOpacity,
             std::uint64_t given, std::function<void()> whenDone) {
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

        // The projection's delegate would turn an exception into an HRESULT for
        // the compositor and lose it: whatever `whenDone` throws stops the
        // process here, named.
        batch.Completed([reach = reach, given, done = std::move(whenDone)](auto&&, auto&&) {
            try {
                if (Impl* const self = reach->self) self->flown(given, done);
            } catch (...) {
                core::impl::terminate_with_reason();
            }
        });
    }

    // Last: after what its listener touches -- the snapshot and its surface --
    // and after the listener itself, which a member pointer has to name.
    impl::system_device_watch<&Impl::deviceReplaced> deviceWatch;
};

WindowShade::WindowShade(HWND__* window)
    : impl_(std::make_unique<Impl>(reinterpret_cast<HWND>(window))) {}

WindowShade::~WindowShade() = default;
WindowShade::WindowShade(WindowShade&&) noexcept = default;
WindowShade& WindowShade::operator=(WindowShade&&) noexcept = default;

void WindowShade::collapseTo(Point screenPoint, std::function<void()> whenDone) const {
    Impl& shade = *impl_;
    std::uint64_t const given = ++shade.command;

    if (!shade.keepsSnapshot()) {
        Snapshot shot = capture(shade.window);
        if (shot.width <= 0) {
            // Nothing to snapshot: hide and be done -- the overlay too, which
            // a flight this command cut short leaves up.
            ::ShowWindow(shade.overlay, SW_HIDE);
            ::ShowWindow(shade.window, SW_HIDE);
            if (whenDone) whenDone();
            return;
        }
        shade.wear(std::move(shot));
        shade.hasSnapshot = true;
    }

    RECT const rect = shade.shot.rect;
    int const width = shade.shot.width;

    // The overlay over just the path, then up with the snapshot exactly over
    // the window, then the window hidden behind it -- no gap where neither
    // shows.
    shade.placeOverlay(flightBounds(rect, screenPoint));
    numerics::float3 const from = shade.overlayPoint(float(rect.left), float(rect.top));
    ::ShowWindow(shade.overlay, SW_SHOWNOACTIVATE);
    ::ShowWindow(shade.window, SW_HIDE);

    float const scale = kLandedPixels / width;
    numerics::float3 const to = shade.overlayPoint(screenPoint.x, screenPoint.y);
    shade.fly(from, {1, 1, 1}, 1.0f, to, {scale, scale, 1}, 0.0f, given, std::move(whenDone));
}

void WindowShade::expandFrom(Point screenPoint, std::function<void()> whenDone) const {
    Impl& shade = *impl_;
    std::uint64_t const given = ++shade.command;

    if (!shade.hasSnapshot) {
        ::ShowWindow(shade.overlay, SW_HIDE);
        ::ShowWindow(shade.window, SW_SHOW);
        if (whenDone) whenDone();
        return;
    }

    RECT const& rect = shade.shot.rect;
    float const width = float(rect.right - rect.left);
    float const scale = width > 0 ? kLandedPixels / width : 0.1f;

    shade.placeOverlay(flightBounds(rect, screenPoint));
    numerics::float3 const from = shade.overlayPoint(screenPoint.x, screenPoint.y);
    numerics::float3 const to = shade.overlayPoint(float(rect.left), float(rect.top));

    ::ShowWindow(shade.overlay, SW_SHOWNOACTIVATE);
    // The window is shown under the snapshot only once the flight lands, so it
    // does not flash at full size before the animation has grown to it.
    // Asked again after SW_SHOW: the messages it sends may give a command,
    // and the snapshot that one wears is not this landing's to let go.
    shade.fly(from, {scale, scale, 1}, 0.0f, to, {1, 1, 1}, 1.0f, given,
              [&shade, given, done = std::move(whenDone)] {
                  ::ShowWindow(shade.window, SW_SHOW);
                  if (given != shade.command) return;
                  shade.land();
                  if (done) done();
              });
}

}  // namespace wxl

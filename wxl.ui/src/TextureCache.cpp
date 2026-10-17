// Реализация wxl::TextureCache. Внутри -- сырой winrt (Win2D в профиль
// генератора не входит, потребляется проекцией), на границе заголовка -- мост в
// обёртки wxl через Object::Impl, ровно как у DrawingSurface.
//
// Проекция и её стандартные заголовки -- первыми: заголовки wxl несут импорт
// wxl.core, после которого стандартный заголовок MSVC уже видел через модуль
// std.
#include <winrt/Microsoft.Graphics.Canvas.UI.Composition.h>
#include <winrt/Microsoft.Graphics.Canvas.h>
#include <winrt/Microsoft.Graphics.DirectX.h>
#include <winrt/Microsoft.UI.Composition.h>
#include <winrt/Microsoft.UI.Dispatching.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Imaging.h>   // BitmapSize у CanvasBitmap.SizeInPixels()
#include <winrt/Windows.Graphics.h>

#include <string>
#include <unordered_map>

#include "TextureCache.h"
#include "impl/operation.h"
#include "Object.impl.h"
#include <wxl/Microsoft.UI.Composition.impl.h>

namespace wxl {

namespace {

namespace muc = winrt::Microsoft::UI::Composition;
namespace canvas = winrt::Microsoft::Graphics::Canvas;
namespace canvasui = winrt::Microsoft::Graphics::Canvas::UI::Composition;
namespace directx = winrt::Microsoft::Graphics::DirectX;
namespace wf = winrt::Windows::Foundation;

}  // namespace

// Общее состояние кэша: устройство Win2D, композиторное графическое устройство
// поверх нашего компоновщика и карта путь -> поверхность.
struct TextureCache::Impl {
    muc::Compositor compositor{nullptr};
    canvas::CanvasDevice device{nullptr};
    muc::CompositionGraphicsDevice graphicsDevice{nullptr};
    std::unordered_map<std::wstring, muc::CompositionDrawingSurface> surfaces;

    // Битмап (уже в видеопамяти) -> композиторная поверхность его размера.
    // Только на UI-потоке: трогает компоновщик. Кэшируется по ключу.
    muc::CompositionDrawingSurface surfaceFor(std::wstring const& key,
                                              canvas::CanvasBitmap const& bitmap) {
        auto const size = bitmap.SizeInPixels();   // Windows::Graphics::Imaging::BitmapSize
        muc::CompositionDrawingSurface surface = graphicsDevice.CreateDrawingSurface2(
            winrt::Windows::Graphics::SizeInt32{static_cast<int32_t>(size.Width),
                                                static_cast<int32_t>(size.Height)},
            directx::DirectXPixelFormat::B8G8R8A8UIntNormalized,
            directx::DirectXAlphaMode::Premultiplied);

        // Рисуем битмап во всю поверхность и закрываем сессию -- закрытие сбрасывает
        // рисунок в поверхность.
        {
            canvas::CanvasDrawingSession session =
                canvasui::CanvasComposition::CreateDrawingSession(surface);
            session.DrawImage(bitmap, wf::Rect{0.0f, 0.0f, static_cast<float>(size.Width),
                                               static_cast<float>(size.Height)});
            session.Close();
        }

        surfaces.emplace(key, surface);
        return surface;
    }
};

// ---- TextureCache --------------------------------------------------------

TextureCache::TextureCache(Compositor const& compositor) : impl_(std::make_shared<Impl>()) {
    impl_->compositor = *Object::Impl::get_typed<Compositor>(compositor);

    // Общее устройство Win2D на процесс и композиторное графическое устройство
    // поверх нашего компоновщика: в его поверхностях и живут текстуры.
    impl_->device = canvas::CanvasDevice::GetSharedDevice();
    impl_->graphicsDevice =
        canvasui::CanvasComposition::CreateCompositionGraphicsDevice(impl_->compositor, impl_->device);
}

TextureCache::~TextureCache() = default;
TextureCache::TextureCache(TextureCache&&) noexcept = default;
TextureCache& TextureCache::operator=(TextureCache&&) noexcept = default;

async::task<CompositionDrawingSurface> TextureCache::getAsync(std::filesystem::path const& image,
                                                             async::cancellation_token stop) {
    std::wstring const key = image.wstring();

    // Уже в кэше -- задача готова сразу, и co_await её не приостанавливается.
    if (auto const it = impl_->surfaces.find(key); it != impl_->surfaces.end()) {
        return async::sta_loop::call_here(
            [surface = Object::Impl::wrap<CompositionDrawingSurface>(it->second)] { return surface; });
    }

    // Загрузка запускается сразу -- декод пойдёт на потоках WinRT, -- а поверхность
    // из битмапа делается по её завершении, уже на этом (UI-) потоке, и кэшируется.
    // Загрузка держит кэш: устройство и карта переживают выброшенную обёртку.
    return impl::start_operation_under<CompositionDrawingSurface>(
        [this, &key] { return canvas::CanvasBitmap::LoadAsync(impl_->device, key); },
        [cache = impl_, key](canvas::CanvasBitmap const& bitmap) {
            return Object::Impl::wrap<CompositionDrawingSurface>(cache->surfaceFor(key, bitmap));
        },
        std::move(stop));
}

void TextureCache::clear() { impl_->surfaces.clear(); }

}  // namespace wxl

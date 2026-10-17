// Реализация wxl::TextureCache. Внутри -- сырой winrt (Win2D в профиль
// генератора не входит, потребляется проекцией), на границе заголовка -- мост в
// обёртки wxl через Object::Impl, ровно как у DrawingSurface.
//
// Заголовки Windows, проекция и их стандартные заголовки -- первыми: заголовки
// wxl несут импорт wxl.core, после которого стандартный заголовок MSVC уже видел
// через модуль std.
#include "platform.h"

#include <winrt/Microsoft.Graphics.Canvas.UI.Composition.h>
#include <winrt/Microsoft.Graphics.Canvas.h>
#include <winrt/Microsoft.Graphics.DirectX.h>
#include <winrt/Microsoft.UI.Composition.h>
#include <winrt/Microsoft.UI.Dispatching.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Imaging.h>   // BitmapSize у CanvasBitmap.SizeInPixels()
#include <winrt/Windows.Graphics.h>

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>

#include "DrawingSurface.h"
#include "TextureCache.h"
#include "impl/device_loss.h"
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

    // Сколько раз заменено устройство wxl, когда кэш смотрел последний раз.
    std::uint64_t followed{impl::rendering_device_replacements()};

    // Устройство wxl заменено с тех пор, как кэш смотрел. Потеряно ли с ним и
    // устройство Win2D, кэш не спрашивает -- при потере адаптера на нём оба, -- а
    // переходит на новое: карта забывается, устройство Win2D -- новое, а не
    // общее (общее Win2D отдаёт прежним, пока само не скажет, что потеряно), и
    // графическое устройство переходит на него.
    void follow() {
        std::uint64_t const now = impl::rendering_device_replacements();
        if (now == followed) return;
        surfaces.clear();
        device = canvas::CanvasDevice{};
        canvasui::CanvasComposition::SetCanvasDevice(graphicsDevice, device);
        followed = now;
    }

    // Отказ Win2D, который устройство Win2D подтверждает потерей: его wxl не
    // слышит -- кэш ставит событие замены сам (signal_rendering_device_removed), и
    // замена идёт как после потери устройства wxl: новое устройство, держатели,
    // а кэш при следующем getAsync -- на новое Win2D. Загрузка кончается
    // DrawingSurface::DeviceLost. Иной отказ возвращается и бросается дальше.
    void throwIfLost(winrt::hresult const code) {
        if (!device.IsDeviceLost(code)) return;
        impl::signal_rendering_device_removed();
        throw DrawingSurface::DeviceLost{};
    }

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
    // До первого co_await: путь -- ссылка вызывающего. Кадр держит кэш: устройство
    // и карта переживают выброшенную обёртку.
    std::wstring const key = image.wstring();
    std::shared_ptr<Impl> const cache = impl_;
    impl::ensure_rendering_device();

    for (;;) {
        cache->follow();

        // Уже в кэше -- задача готова сразу, и co_await её не приостанавливается.
        if (auto const it = cache->surfaces.find(key); it != cache->surfaces.end()) {
            co_return Object::Impl::wrap<CompositionDrawingSurface>(it->second);
        }

        // Загрузка запускается сразу -- декод пойдёт на потоках WinRT, -- а поверхность
        // из битмапа делается по её завершении, уже на этом (UI-) потоке, и кэшируется.
        std::uint64_t const replacements = impl::rendering_device_replacements();
        canvas::CanvasBitmap bitmap{nullptr};
        try {
            bitmap = co_await impl::start_operation_under<canvas::CanvasBitmap>(
                [&] { return canvas::CanvasBitmap::LoadAsync(cache->device, key); }, std::identity{},
                stop);
        } catch (winrt::hresult_error const& failure) {
            cache->throwIfLost(failure.code());
            throw;
        }

        // Устройство заменили, пока файл грузился: битмап -- на устройстве, которое
        // кэш покинул, -- грузится снова, уже на новом.
        if (impl::rendering_device_replacements() != replacements) continue;

        try {
            co_return Object::Impl::wrap<CompositionDrawingSurface>(cache->surfaceFor(key, bitmap));
        } catch (winrt::hresult_error const& failure) {
            cache->throwIfLost(failure.code());
            throw;
        }
    }
}

void TextureCache::clear() { impl_->surfaces.clear(); }

}  // namespace wxl

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
#include "impl/rendering_device.h"
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
struct TextureCache::Impl : core::sta_refcounted {
    // Общее устройство Win2D на процесс и композиторное графическое устройство
    // поверх нашего компоновщика: в его поверхностях и живут текстуры.
    explicit Impl(muc::Compositor const& compositor)
        : compositor{compositor},
          device{canvas::CanvasDevice::GetSharedDevice()},
          graphics_device{canvasui::CanvasComposition::CreateCompositionGraphicsDevice(compositor, device)} {}

    muc::Compositor const compositor;
    // Меняет follow(): после замены устройства wxl кэш переходит на новое.
    canvas::CanvasDevice device;
    muc::CompositionGraphicsDevice const graphics_device;
    std::unordered_map<std::wstring, muc::CompositionDrawingSurface> surfaces;

    // Сколько раз заменено устройство wxl, когда кэш смотрел последний раз.
    std::uint64_t followed{impl::rendering_device::replacements()};

    // Устройство wxl заменено с тех пор, как кэш смотрел. Потеряно ли с ним и
    // устройство Win2D, кэш не спрашивает -- адаптер тому выбирал Win2D, и он
    // может быть другим, -- а переходит на новое: карта забывается, устройство
    // Win2D -- новое, а не общее (общее Win2D отдаёт прежним, пока само не скажет,
    // что потеряно), и графическое устройство переходит на него.
    void follow() {
        std::uint64_t const now = impl::rendering_device::replacements();
        if (now == followed) return;
        surfaces.clear();
        device = canvas::CanvasDevice{};
        canvasui::CanvasComposition::SetCanvasDevice(graphics_device, device);
        followed = now;
    }

    // Отказ Win2D, который устройство Win2D подтверждает потерей: его wxl не
    // слышит -- кэш ставит событие замены сам (rendering_device::signal_removed), и
    // замена идёт как после потери устройства wxl: новое устройство, держатели,
    // а кэш при следующем getAsync -- на новое Win2D. Загрузка кончается
    // DrawingSurface::DeviceLost. Иной отказ возвращается и бросается дальше.
    void throw_if_lost(winrt::hresult const code) {
        if (!device.IsDeviceLost(code)) return;
        impl::rendering_device::signal_removed();
        throw DrawingSurface::DeviceLost{};
    }

    // Битмап (уже в видеопамяти) -> композиторная поверхность его размера.
    // Только на UI-потоке: трогает компоновщик. Кэшируется по ключу.
    muc::CompositionDrawingSurface surface_for(std::wstring const& key,
                                               canvas::CanvasBitmap const& bitmap) {
        auto const size = bitmap.SizeInPixels();   // Windows::Graphics::Imaging::BitmapSize
        muc::CompositionDrawingSurface surface = graphics_device.CreateDrawingSurface2(
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

TextureCache::TextureCache(Compositor const& compositor)
    : impl_(core::make_refcounted<Impl>(*Object::Impl::get_typed<Compositor>(compositor))) {}

TextureCache::~TextureCache() = default;

async::task<CompositionDrawingSurface> TextureCache::getAsync(std::filesystem::path const& image,
                                                             async::cancellation_token token) {
    // До первого co_await: путь -- ссылка вызывающего. Кадр держит кэш: устройство
    // и карта переживают выброшенную обёртку.
    std::wstring const key = image.wstring();
    core::intrusive_ptr<Impl> const impl = impl_;

    for (;;) {
        // Под отменённым токеном ничего не начинается: ни устройство wxl, ни переход
        // на новое устройство Win2D, ни загрузка, ни её повтор.
        token.throw_if_canceled();
        impl::rendering_device::ensure();
        impl->follow();

        // Уже в кэше -- задача готова сразу, и co_await её не приостанавливается.
        if (auto const it = impl->surfaces.find(key); it != impl->surfaces.end()) {
            co_return Object::Impl::wrap<CompositionDrawingSurface>(it->second);
        }

        // Загрузка запускается сразу -- декод пойдёт на потоках WinRT, -- а поверхность
        // из битмапа делается по её завершении, уже на этом (UI-) потоке, и кэшируется.
        std::uint64_t const replacements = impl::rendering_device::replacements();
        canvas::CanvasBitmap bitmap{nullptr};
        try {
            bitmap = co_await impl::start_cancelable_winrt_operation<canvas::CanvasBitmap>(
                [&] { return canvas::CanvasBitmap::LoadAsync(impl->device, key); },
                std::identity{}, token);
        } catch (winrt::hresult_error const& failure) {
            // Устройство заменили, пока файл грузился: отказ мог прийти от устройства
            // Win2D, которое кэш покинул (сброс GPU, и ответ загрузки встал в очередь
            // за ходом замены), а о потере спросили бы новое -- оно цело, ответ «нет»,
            // и законный сброс остановил бы процесс. Файл грузится снова, уже на
            // новом; иной отказ повторится там и выйдет отсюда.
            if (impl::rendering_device::replacements() != replacements) continue;
            impl->throw_if_lost(failure.code());
            throw;
        }

        // Устройство заменили, пока файл грузился: битмап -- на устройстве, которое
        // кэш покинул, -- грузится снова, уже на новом.
        if (impl::rendering_device::replacements() != replacements) continue;

        try {
            co_return Object::Impl::wrap<CompositionDrawingSurface>(impl->surface_for(key, bitmap));
        } catch (winrt::hresult_error const& failure) {
            impl->throw_if_lost(failure.code());
            throw;
        }
    }
}

void TextureCache::clear() { impl_->surfaces.clear(); }

}  // namespace wxl

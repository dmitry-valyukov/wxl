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
#include "Object.impl.h"
#include "generated/Microsoft.UI.Composition.impl.h"

namespace wxl {

namespace {

namespace muc = winrt::Microsoft::UI::Composition;
namespace mud = winrt::Microsoft::UI::Dispatching;
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

// Состояние одного запроса. Держит shared-ссылку на кэш: незавершённый запрос
// переживает выброшенную кэш-обёртку.
struct TextureRequest::Impl {
    std::shared_ptr<TextureCache::Impl> cache;
    std::wstring key;

    muc::CompositionDrawingSurface cached{nullptr};   // не пусто -> уже в кэше
    wf::IAsyncOperation<canvas::CanvasBitmap> op{nullptr};
    mud::DispatcherQueue ui{nullptr};
    canvas::CanvasBitmap loaded{nullptr};
    std::exception_ptr error;
};

// ---- TextureRequest ------------------------------------------------------

TextureRequest::TextureRequest(std::unique_ptr<Impl> impl) noexcept : impl_(std::move(impl)) {}
TextureRequest::TextureRequest(TextureRequest&&) noexcept = default;
TextureRequest& TextureRequest::operator=(TextureRequest&&) noexcept = default;
TextureRequest::~TextureRequest() = default;

bool TextureRequest::await_ready() const noexcept { return static_cast<bool>(impl_->cached); }

void TextureRequest::await_suspend(std::coroutine_handle<> resume) {
    // Загрузка уже идёт (запущена в getAsync), декод -- на потоках WinRT. Ждём
    // её завершения; обработчик приходит на потоке пула, и всё, что он делает,
    // -- перекладывает возобновление корутины на UI-поток нашим диспетчером.
    // self валиден до конца co_await: awaiter живёт в кадре корутины, а тот --
    // до самого возобновления.
    Impl* const self = impl_.get();
    mud::DispatcherQueue const ui = self->ui;
    self->op.Completed([self, resume, ui](wf::IAsyncOperation<canvas::CanvasBitmap> const& op,
                                          wf::AsyncStatus) {
        try {
            self->loaded = op.GetResults();
        } catch (...) {
            self->error = std::current_exception();
        }
        ui.TryEnqueue([resume]() { resume.resume(); });
    });
}

CompositionDrawingSurface TextureRequest::await_resume() {
    Impl* const self = impl_.get();

    if (self->error) std::rethrow_exception(self->error);

    // Из кэша -- отдаём как есть. Иначе рисуем поверхность из загруженного
    // битмапа (мы уже на UI-потоке) и кэшируем.
    muc::CompositionDrawingSurface const surface =
        self->cached ? self->cached : self->cache->surfaceFor(self->key, self->loaded);

    return Object::Impl::wrap<CompositionDrawingSurface>(surface);
}

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

TextureRequest TextureCache::getAsync(std::filesystem::path const& image) {
    auto request = std::make_unique<TextureRequest::Impl>();
    request->cache = impl_;
    request->key = image.wstring();

    if (auto const it = impl_->surfaces.find(request->key); it != impl_->surfaces.end()) {
        request->cached = it->second;   // готово без подвеса
        return TextureRequest{std::move(request)};
    }

    // Запускаем загрузку сразу -- декод пойдёт на потоках WinRT, -- и запоминаем
    // диспетчер UI-потока (мы на нём), чтобы вернуться на него по завершении.
    request->ui = mud::DispatcherQueue::GetForCurrentThread();
    request->op = canvas::CanvasBitmap::LoadAsync(impl_->device, request->key);

    return TextureRequest{std::move(request)};
}

void TextureCache::clear() { impl_->surfaces.clear(); }

}  // namespace wxl

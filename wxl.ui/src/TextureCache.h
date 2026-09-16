#pragma once

#include "generated/Microsoft.UI.Composition.h"

#include <coroutine>
#include <filesystem>
#include <memory>

// wxl::TextureCache -- картинки и текстуры в видеопамяти, загружаемые
// асинхронно и удерживаемые по пути файла.
//
// Стоит на Win2D (Microsoft.Graphics.Canvas): декод и заливку в видеопамять
// делает CanvasBitmap.LoadAsync -- на своих потоках, не на нашем, -- а
// рисование готового битмапа в композиторную поверхность -- CanvasComposition
// поверх нашего же Microsoft.UI-композитора. Так задник, обложка и (позже)
// буквица оказываются текстурой в видеопамяти, которую композитор двигает и
// растягивает как любой визуал. Страница книги при этом остаётся на сыром
// Direct2D через DrawingSurface -- это её другой путь.
//
// Кэш -- одна поверхность на путь: один и тот же файл декодится и заливается
// раз, а дальше отдаётся готовым.
//
// Асинхронность -- через собственный awaitable, а не через ожидание чужой
// winrt-корутины в теле нашей: getAsync() возвращает объект, который корутина
// wxl co_await-ит. Пока картинки нет в кэше, await_suspend вешает продолжение
// на CanvasBitmap.LoadAsync и по завершении (на потоке пула WinRT) кладёт
// возобновление обратно в UI-поток нашим диспетчером; await_resume -- уже на
// UI-потоке -- создаёт поверхность, рисует в неё битмап, кэширует и отдаёт. Так
// winrt наружу не течёт, а возврат на UI-поток держим сами.
//
// Обёртка, не владелец видеопамяти: копия -- второй handle на тот же кэш; сам
// кэш живёт, пока жив любой его handle или незавершённый запрос к нему.

namespace wxl {

class TextureCache;

/// Незавершённая загрузка одной текстуры. Живёт в кадре корутины, которая её
/// ждёт: `CompositionDrawingSurface s = co_await cache.getAsync(path);`.
///
/// await_* объявлены здесь, а определены в TextureCache.cpp -- корутинной
/// машине незачем видеть их телами, а заголовку незачем видеть winrt.
class TextureRequest {
public:
    TextureRequest(TextureRequest&&) noexcept;
    TextureRequest& operator=(TextureRequest&&) noexcept;
    TextureRequest(TextureRequest const&) = delete;
    TextureRequest& operator=(TextureRequest const&) = delete;
    ~TextureRequest();

    /// Готово без подвеса, если поверхность уже в кэше.
    bool await_ready() const noexcept;

    /// Вешает продолжение на завершение загрузки; резюмирует его на UI-потоке.
    void await_suspend(std::coroutine_handle<> resume);

    /// Уже на UI-потоке: создаёт поверхность из битмапа, кэширует, отдаёт (или
    /// бросает, если загрузка не удалась).
    CompositionDrawingSurface await_resume();

private:
    friend class TextureCache;
    struct Impl;
    explicit TextureRequest(std::unique_ptr<Impl> impl) noexcept;
    std::unique_ptr<Impl> impl_;
};

class TextureCache {
public:
    /// Кэш поверх этого композитора: в его поверхности рисуются текстуры, его
    /// же кистями они попадают на визуалы. Микрософт-UI-композитор -- тот
    /// самый, что несёт сцену окна.
    explicit TextureCache(Compositor const& compositor);
    ~TextureCache();

    TextureCache(TextureCache&&) noexcept;
    TextureCache& operator=(TextureCache&&) noexcept;
    TextureCache(TextureCache const&) = delete;
    TextureCache& operator=(TextureCache const&) = delete;

    /// Поверхность с картинкой из файла: из кэша, если уже грузили, иначе
    /// загрузкой в видеопамять. Путь абсолютный. co_await-ится корутиной wxl.
    [[nodiscard]] TextureRequest getAsync(std::filesystem::path const& image);

    /// Забыть всё: следующий getAsync грузит заново. Видеопамять освобождается,
    /// когда её перестают держать и кэш, и кисти на визуалах.
    void clear();

private:
    friend class TextureRequest;
    // shared: незавершённый TextureRequest держит его тоже, чтобы устройство и
    // карта пережили запрос, даже если сам кэш-обёртку выронили.
    struct Impl;
    std::shared_ptr<Impl> impl_;
};

}  // namespace wxl

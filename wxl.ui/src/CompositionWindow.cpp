// Реализация wxl::CompositionWindow. Кишки подняты из sandbox/AttachedProbe:
// окно создаётся внутри уже поднятого wxl (Application::Start в launch.cpp), а
// значит без своего загрузчика и цикла сообщений. Внутри держим сырой winrt
// (острова своего окна в профиль генератора не входят), а на границе заголовка
// мостим в обёртки wxl через Object::Impl -- ровно как DrawingSurface.
//
// Проекция и заголовки Windows -- первыми, и с ними стандартные, что они тянут:
// заголовки wxl несут импорт wxl.core, после которого текстовый заголовок MSVC
// уже видел бы через модуль std. windows.h после winrt: его GetCurrentTime
// иначе подставился бы в одноимённый метод проекции.
#include <winrt/Microsoft.UI.Composition.h>
#include <winrt/Microsoft.UI.Content.h>
#include <winrt/Microsoft.UI.Dispatching.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>   // Grid -- одноразовый элемент под chromeCompositor()
#include <winrt/Microsoft.UI.Xaml.Hosting.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.Input.h>
#include <winrt/Microsoft.UI.h>
#include <winrt/Windows.UI.h>

#include <windows.h>

#include <d2d1_1.h>     // ID2D1DeviceContext, DrawBitmap -- фон-картинка рисуется через DrawingSurface
#include <wincodec.h>   // WIC: декод картинки заднего фона в поверхность
#include <shellapi.h>   // HDROP, DragAcceptFiles/DragQueryFileW/DragFinish -- WIN32_LEAN_AND_MEAN их прячет

#include "CompositionWindow.h"
#include "TextureCache.h"
// Перед Object.impl.h: тот тянет import wxl.core (модульный std), а
// window_placement.h несёт обычный <optional> -- он должен встретиться до
// импорта, иначе MSVC не примет стандартный заголовок после него.
#include "impl/window_placement.h"
#include "Object.impl.h"
#include "generated/Microsoft.UI.Composition.impl.h"
#include "generated/Microsoft.UI.Dispatching.impl.h"
#include "generated/Microsoft.UI.Input.impl.h"
#include "generated/Microsoft.UI.Xaml.impl.h"

// Импорт последним: он несёт модульный std, а обычный заголовок после него
// MSVC уже не примет. Нужен для wxl::core::append_number -- сборки строки
// запомненного места окна.
import wxl.core;

namespace wxl {

namespace {

namespace muc = winrt::Microsoft::UI::Composition;
namespace content = winrt::Microsoft::UI::Content;
namespace input = winrt::Microsoft::UI::Input;
namespace mud = winrt::Microsoft::UI::Dispatching;
namespace xaml = winrt::Microsoft::UI::Xaml;

winrt::Windows::UI::Color asWinrt(Color c) noexcept { return {c.A, c.R, c.G, c.B}; }

winrt::Microsoft::UI::WindowId windowIdOf(HWND hwnd) noexcept {
    return {reinterpret_cast<uint64_t>(hwnd)};
}

// Класс окна -- один на процесс. Без кисти фона и с WM_ERASEBKGND->1:
// поверхности перенаправления нет, белому взяться неоткуда.
constexpr wchar_t kClassName[] = L"wxl.CompositionWindow";

}  // namespace

// Общее состояние окна: сырой winrt внутри, обёртки -- только на границе.
struct WindowState {
    HWND hwnd{nullptr};
    muc::Compositor compositor{nullptr};
    // Один визуал на всю сцену: он и корень острова, и задний фон (картинка или
    // цвет). Страница висит на нём ребёнком -- контейнер над контейнером не нужен.
    muc::SpriteVisual backdrop{nullptr};
    content::DesktopAttachedSiteBridge sceneBridge{nullptr};
    xaml::Hosting::DesktopWindowXamlSource chrome{nullptr};   // оснастка островом
    std::function<void()> onClosed;
    std::function<void(std::filesystem::path)> onFileDrop;

    SizeInt32 minSize{};                // нижний предел клиента (WM_GETMINMAXINFO)
    WINDOWPLACEMENT savedPlacement{};   // место до полноэкранного -- куда вернуться
    LONG_PTR savedStyle{};              // стиль окна до полноэкранного
    bool fullScreen{false};

    // Кэш текстур поверх compositor -- под асинхронные задники (Win2D). Заводится
    // в конструкторе, когда компоновщик готов.
    std::optional<TextureCache> textureCache;

    // Ввод со сцены (режим чтения без острова): источники ввода её ContentIsland
    // и обработчики читалки. Источники держим живыми -- на них висят подписки.
    content::ContentIsland sceneIsland{nullptr};
    input::InputPointerSource pointerInput{nullptr};   // указатель сцены; клавиатура -- через WndProc
    std::function<void(VirtualKey)> onKeyDown;
    std::function<void(PointerPoint const&)> onPointerPressed;
    std::function<void(PointerPoint const&)> onPointerMoved;
    std::function<void(PointerPoint const&)> onPointerReleased;
    std::function<void(PointerPoint const&)> onPointerWheel;
    std::function<void()> onGeometryChanged;
    std::function<void(SizeInt32, float)> onClientSizeChanged;

    void resize(float width, float height) {
        winrt::Windows::Foundation::Numerics::float2 const wh{width, height};
        // Один визуал сцены -- задний. Он корень острова и картинка разом, а
        // страница висит на нём ребёнком и меряет себя сама, так что под окно
        // подгоняем только его.
        if (backdrop) backdrop.Size(wh);
        if (chrome) {
            chrome.SiteBridge().MoveAndResize(
                {0, 0, static_cast<int32_t>(width), static_cast<int32_t>(height)});
        }
    }
};

namespace {

WindowState* stateOf(HWND hwnd) noexcept {
    return reinterpret_cast<WindowState*>(::GetWindowLongPtrW(hwnd, GWLP_USERDATA));
}

LRESULT CALLBACK windowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    WindowState* state = stateOf(hwnd);
    switch (message) {
        case WM_ERASEBKGND:
            return 1;

        case WM_GETMINMAXINFO:
            // Нижний предел -- в размерах клиента; переводим в размеры окна
            // текущим стилем. Сообщение приходит и при создании, до установки
            // USERDATA: тогда state == nullptr, предел не наложен -- окну и так
            // задан CW_USEDEFAULT.
            if (state && (state->minSize.width > 0 || state->minSize.height > 0)) {
                RECT frame{0, 0, state->minSize.width, state->minSize.height};
                ::AdjustWindowRectEx(&frame,
                                     static_cast<DWORD>(::GetWindowLongPtrW(hwnd, GWL_STYLE)), FALSE,
                                     static_cast<DWORD>(::GetWindowLongPtrW(hwnd, GWL_EXSTYLE)));
                auto* const bounds = reinterpret_cast<MINMAXINFO*>(lparam);
                bounds->ptMinTrackSize.x = frame.right - frame.left;
                bounds->ptMinTrackSize.y = frame.bottom - frame.top;
                return 0;
            }
            break;

        case WM_SIZE:
            if (state && wparam != SIZE_MINIMIZED) {
                state->resize(static_cast<float>(LOWORD(lparam)),
                              static_cast<float>(HIWORD(lparam)));

                // Приложению -- новая клиентская ширина, ещё до того как XAML
                // возьмётся за вёрстку: перекладывать дерево можно здесь, а из
                // SizeChanged самого XAML -- уже нельзя, оно приходит изнутри
                // прохода вёрстки.
                if (state->onClientSizeChanged) {
                    state->onClientSizeChanged(
                        {static_cast<int32_t>(LOWORD(lparam)),
                         static_cast<int32_t>(HIWORD(lparam))},
                        static_cast<float>(::GetDpiForWindow(hwnd)) / 96.0f);
                }
                // Максимизация и восстановление меняют место без перетаскивания
                // (WM_EXITSIZEMOVE не придёт) -- отмечаем их здесь. Растяжка за
                // рамку тоже сюда попадает, но сохранение места отложено, так что
                // лишние отметки во время тяги схлопнутся в одну запись.
                if ((wparam == SIZE_MAXIMIZED || wparam == SIZE_RESTORED) &&
                    state->onGeometryChanged) {
                    state->onGeometryChanged();
                }
            }
            break;

        case WM_EXITSIZEMOVE:
            // Конец перетаскивания рамки: и сдвиг, и растяжка кончаются здесь.
            if (state && state->onGeometryChanged) state->onGeometryChanged();
            return 0;

        case WM_KEYDOWN:
            // Клавиатура у чистой сцены доходит сюда (в отличие от указателя,
            // которого перехватывает InputSite острова). Отдаём код клавиши
            // обёрткой wxl -- тем же VirtualKey, что раздаёт XAML. Не return 0:
            // пусть DefWindowProc довершит своё (WM_CHAR, системные клавиши).
            if (state && state->onKeyDown) {
                state->onKeyDown(static_cast<VirtualKey>(static_cast<int32_t>(wparam)));
            }
            break;

        case WM_DROPFILES:
            if (state && state->onFileDrop) {
                HDROP drop = reinterpret_cast<HDROP>(wparam);
                wchar_t path[MAX_PATH] = {};
                if (::DragQueryFileW(drop, 0, path, MAX_PATH)) {
                    state->onFileDrop(std::filesystem::path{path});
                }
                ::DragFinish(drop);
            }
            return 0;

        case WM_CLOSE:
            // Приложению есть что сохранить по дороге -- место чтения, геометрию.
            if (state && state->onClosed) state->onClosed();
            ::DestroyWindow(hwnd);
            return 0;

        case WM_DESTROY:
            // Своё окно закрылось -- гасим приложение; после этого
            // Application::Start возвращается и отрабатывает Teardown.
            xaml::Application::Current().Exit();
            return 0;
    }
    return ::DefWindowProcW(hwnd, message, wparam, lparam);
}

void ensureClass() {
    static bool const registered = [] {
        WNDCLASSEXW wc{sizeof(WNDCLASSEXW)};
        wc.lpfnWndProc = windowProc;
        wc.hInstance = ::GetModuleHandleW(nullptr);
        wc.lpszClassName = kClassName;
        // IDC_ARROW без UNICODE -- LPSTR, а LoadCursorW хочет LPCWSTR; значение
        // (номер ресурса в указателе) для A/W одно.
        wc.hCursor = ::LoadCursorW(nullptr, reinterpret_cast<LPCWSTR>(IDC_ARROW));
        wc.hbrBackground = nullptr;   // без классовой кисти -- белому взяться неоткуда
        ::RegisterClassExW(&wc);
        return true;
    }();
    (void)registered;
}

// Корутина «запусти и забудь» для смены задника на UI-потоке: стартует сразу
// (initial_suspend never), подвисает на co_await, пока Win2D декодит,
// возвращается на UI-поток и завершается, уничтожая свой же кадр (final_suspend
// never). Владельца ей не надо: кадр жив всё ожидание, а обработчик завершения
// возобновляет именно его. Кадр -- из STA-пула: живёт и умирает на UI-потоке.
struct fire_and_forget {
    struct promise_type {
        fire_and_forget get_return_object() const noexcept { return {}; }
        std::suspend_never initial_suspend() const noexcept { return {}; }
        std::suspend_never final_suspend() const noexcept { return {}; }
        void return_void() const noexcept {}
        void unhandled_exception() const noexcept {}   // задник не сменился -- не падаем

        static void* operator new(std::size_t size) {
            return core::sta_memory_pool::alloc(static_cast<uint32_t>(size));
        }
        static void operator delete(void* mem, std::size_t size) noexcept {
            core::sta_memory_pool::free(mem, static_cast<uint32_t>(size));
        }
    };
};

// Асинхронная смена задника: ждёт текстуру из кэша (декод на потоках WinRT) и,
// вернувшись на UI-поток, ставит её кистью UniformToFill на единственный задний
// визуал. Путь по значению -- корутина владеет им через ожидание.
fire_and_forget swapBackground(WindowState* state, std::filesystem::path image) {
    CompositionDrawingSurface const surface = co_await state->textureCache->getAsync(image);

    // Именованные промежутки: обёртка -> сырая поверхность (одно преобразование),
    // сырая поверхность -> ICompositionSurface у CreateSurfaceBrush (второе); в
    // одном выражении их было бы два подряд, чего аргументу не дают.
    muc::CompositionDrawingSurface const& raw =
        *Object::Impl::get_typed<CompositionDrawingSurface>(surface);
    muc::CompositionSurfaceBrush const brush = state->compositor.CreateSurfaceBrush(raw);
    brush.Stretch(muc::CompositionStretch::UniformToFill);
    brush.HorizontalAlignmentRatio(0.5f);
    brush.VerticalAlignmentRatio(0.5f);
    state->backdrop.Brush(brush);
}

}  // namespace

// ---- Сцена: задний фон и страница ------------------------------------------

Compositor CompositionWindow::compositor() const {
    return Object::Impl::wrap<Compositor>(state_->compositor);
}

ContainerVisual CompositionWindow::contentVisual() const {
    return Object::Impl::wrap<ContainerVisual>(state_->backdrop);
}

SizeInt32 CompositionWindow::clientSize() const {
    RECT client{};
    ::GetClientRect(state_->hwnd, &client);
    return {client.right, client.bottom};
}

float CompositionWindow::rasterizationScale() const {
    // DPI окна, а не системного: окно может стоять на мониторе с другим
    // масштабом, и физические пиксели поверхности считаются по нему.
    return static_cast<float>(::GetDpiForWindow(state_->hwnd)) / 96.0f;
}

void CompositionWindow::background(Color color) {
    state_->backdrop.Brush(state_->compositor.CreateColorBrush(asWinrt(color)));
}

void CompositionWindow::clearBackground() {
    // Спрайт без кисти ничего не рисует, но остаётся корнем сцены и
    // контейнером визуалов приложения -- дерево не меняется, меняется только
    // то, что у него нет своей заливки.
    state_->backdrop.Brush(nullptr);
}

void CompositionWindow::background(std::filesystem::path const& image) {
    // Синхронный декод: первый (стартовый) экран показывается только уже
    // загруженным -- окно до того скрыто. Смены фона на ходу пойдут асинхронно
    // через Win2D/TextureCache, отдельным шагом; здесь путь под первую картинку.
    winrt::com_ptr<IWICImagingFactory> wic;
    winrt::check_hresult(::CoCreateInstance(CLSID_WICImagingFactory, nullptr,
                                            CLSCTX_INPROC_SERVER, __uuidof(IWICImagingFactory),
                                            wic.put_void()));
    winrt::com_ptr<IWICBitmapDecoder> decoder;
    winrt::check_hresult(wic->CreateDecoderFromFilename(image.c_str(), nullptr, GENERIC_READ,
                                                        WICDecodeMetadataCacheOnLoad, decoder.put()));
    winrt::com_ptr<IWICBitmapFrameDecode> frame;
    winrt::check_hresult(decoder->GetFrame(0, frame.put()));

    // Premultiplied BGRA -- то, во что рисует D2D и что смешивает композитор
    // (тот же формат, что у поверхности DrawingSurface).
    winrt::com_ptr<IWICFormatConverter> converter;
    winrt::check_hresult(wic->CreateFormatConverter(converter.put()));
    winrt::check_hresult(converter->Initialize(frame.get(), GUID_WICPixelFormat32bppPBGRA,
                                               WICBitmapDitherTypeNone, nullptr, 0.0,
                                               WICBitmapPaletteTypeMedianCut));

    UINT width = 0;
    UINT height = 0;
    winrt::check_hresult(frame->GetSize(&width, &height));

    DrawingSurface surface{compositor(),
                           SizeInt32{static_cast<int32_t>(width), static_cast<int32_t>(height)}};
    IWICBitmapSource* const source = converter.get();
    surface.draw([source, width, height](ID2D1DeviceContext* context) {
        winrt::com_ptr<ID2D1Bitmap> bitmap;
        winrt::check_hresult(context->CreateBitmapFromWicBitmap(source, nullptr, bitmap.put()));
        context->DrawBitmap(bitmap.get(), D2D1::RectF(0.0f, 0.0f, static_cast<float>(width),
                                                      static_cast<float>(height)));
    });

    // Кисть тянет картинку фиксированного размера на задник во всё окно --
    // UniformToFill по центру, ровно как обложка на стартовом экране: обрезается
    // по краю, но не искажается. Обёртку держим именованной: get_typed отдаёт
    // сырой winrt внутри неё, а не копию, и временная обёртка умерла бы прежде,
    // чем кисть попадёт на визуал.
    CompositionSurfaceBrush wrapped = surface.brush();
    muc::CompositionSurfaceBrush const& brush =
        *Object::Impl::get_typed<CompositionSurfaceBrush>(wrapped);
    brush.Stretch(muc::CompositionStretch::UniformToFill);
    brush.HorizontalAlignmentRatio(0.5f);
    brush.VerticalAlignmentRatio(0.5f);
    state_->backdrop.Brush(brush);
}

void CompositionWindow::background(DrawingSurface const& surface) {
    // Обёртку держим именованной: get_typed отдаёт сырой winrt внутри неё, а не
    // копию, а временная обёртка surface.brush() умерла бы концом выражения --
    // и ссылка повисла бы. (Цвет и картинку выше это правило тоже касается.)
    CompositionSurfaceBrush wrapped = surface.brush();
    muc::CompositionSurfaceBrush const& brush =
        *Object::Impl::get_typed<CompositionSurfaceBrush>(wrapped);
    // UniformToFill: поверхность-задник тянется под окно, как картинка-заставка,
    // и в просвете быстрой растяжки кроет окно, а не сквозит.
    brush.Stretch(muc::CompositionStretch::UniformToFill);
    state_->backdrop.Brush(brush);
}

void CompositionWindow::backgroundAsync(std::filesystem::path const& image) {
    // Запускаем и забываем: swapBackground сам доведёт себя до конца на UI-потоке
    // и уберёт свой кадр. Кэш держит текстуру, так что тот же путь во второй раз
    // сменит задник уже без загрузки.
    swapBackground(state_.get(), image);
}

// ---- CompositionWindow ----------------------------------------------------

CompositionWindow::CompositionWindow(std::wstring_view title, SizeInt32 minSize)
    : state_{std::make_unique<WindowState>()} {
    state_->minSize = minSize;

    ensureClass();

    std::wstring const caption{title};
    state_->hwnd = ::CreateWindowExW(WS_EX_NOREDIRECTIONBITMAP, kClassName, caption.c_str(),
                                     WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                                     CW_USEDEFAULT, CW_USEDEFAULT, nullptr, nullptr,
                                     ::GetModuleHandleW(nullptr), nullptr);
    ::SetWindowLongPtrW(state_->hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state_.get()));

    // Сцена: свой Microsoft.UI-композитор и один задний визуал -- он же корень
    // острова, он же фон. Подключаем его к нашему HWND attached-мостом -- без
    // дочернего окна рисования (мост InputSiteWindowClass -- только под ввод).
    state_->compositor = muc::Compositor{};
    state_->backdrop = state_->compositor.CreateSpriteVisual();

    // Кэш текстур на том же компоновщике -- под асинхронные задники.
    state_->textureCache.emplace(Object::Impl::wrap<Compositor>(state_->compositor));

    state_->sceneIsland = content::ContentIsland::Create(state_->backdrop);
    state_->sceneBridge = content::DesktopAttachedSiteBridge::CreateFromWindowId(
        mud::DispatcherQueue::GetForCurrentThread(), windowIdOf(state_->hwnd));
    state_->sceneBridge.Connect(state_->sceneIsland);

    // Ввод у чистой сцены расщеплён. Клавиатура доходит до верхнего WndProc
    // (WM_KEYDOWN -- проверено пробой), а указатель перехватывает InputSite
    // острова, и его берём через InputPointerSource острова. InputKeyboardSource
    // не заводим: его GetForIsland на attached-острове падал AV, а клавиатуре он
    // и не нужен. Подписки простые, без auto_revoke: источник живёт в state_
    // столько же, сколько окно, а окно -- до выхода приложения; st валиден, пока
    // источник не отпущен.
    WindowState* const st = state_.get();
    st->pointerInput = input::InputPointerSource::GetForIsland(st->sceneIsland);
    st->pointerInput.PointerPressed(
        [st](input::InputPointerSource const&, input::PointerEventArgs const& args) {
            if (st->onPointerPressed)
                st->onPointerPressed(Object::Impl::wrap<PointerPoint>(args.CurrentPoint()));
        });
    st->pointerInput.PointerMoved(
        [st](input::InputPointerSource const&, input::PointerEventArgs const& args) {
            if (st->onPointerMoved)
                st->onPointerMoved(Object::Impl::wrap<PointerPoint>(args.CurrentPoint()));
        });
    st->pointerInput.PointerReleased(
        [st](input::InputPointerSource const&, input::PointerEventArgs const& args) {
            if (st->onPointerReleased)
                st->onPointerReleased(Object::Impl::wrap<PointerPoint>(args.CurrentPoint()));
        });
    st->pointerInput.PointerWheelChanged(
        [st](input::InputPointerSource const&, input::PointerEventArgs const& args) {
            if (st->onPointerWheel)
                st->onPointerWheel(Object::Impl::wrap<PointerPoint>(args.CurrentPoint()));
        });

    RECT client{};
    ::GetClientRect(state_->hwnd, &client);
    state_->resize(static_cast<float>(client.right), static_cast<float>(client.bottom));
}

CompositionWindow::~CompositionWindow() = default;
CompositionWindow::CompositionWindow(CompositionWindow&&) noexcept = default;
CompositionWindow& CompositionWindow::operator=(CompositionWindow&&) noexcept = default;

void CompositionWindow::content(UIElement const& chrome) {
    xaml::UIElement const& raw = *Object::Impl::get_typed<UIElement>(chrome);
    if (!state_->chrome) {
        state_->chrome = xaml::Hosting::DesktopWindowXamlSource{};
        state_->chrome.Initialize(windowIdOf(state_->hwnd));
    }
    state_->chrome.Content(raw);
    RECT client{};
    ::GetClientRect(state_->hwnd, &client);
    state_->chrome.SiteBridge().MoveAndResize({0, 0, client.right, client.bottom});
    state_->chrome.SiteBridge().Show();
}

UIElement CompositionWindow::content() const {
    xaml::UIElement current{nullptr};
    if (state_->chrome) current = state_->chrome.Content();
    return Object::Impl::wrap<UIElement>(current);
}

Compositor CompositionWindow::chromeCompositor() const {
    // Композитор XAML-потока: его отдаёт хэндофф-визуал любого элемента. На потоке
    // XAML он один -- тот же, на котором окажется содержимое, показанное через
    // content(). Берём у одноразового Grid.
    xaml::Controls::Grid const probe;
    return Object::Impl::wrap<Compositor>(
        xaml::Hosting::ElementCompositionPreview::GetElementVisual(probe).Compositor());
}

void CompositionWindow::hideContent() {
    // Прячем остров и снимаем с него содержимое: в режиме чтения ввод должен
    // идти сцене, а пустой, но показанный остров перехватывал бы его над собой.
    if (state_->chrome) {
        state_->chrome.Content(nullptr);
        state_->chrome.SiteBridge().Hide();
    }
}

void CompositionWindow::activate() {
    ::ShowWindow(state_->hwnd, SW_SHOW);
    ::UpdateWindow(state_->hwnd);
}

void CompositionWindow::resize(SizeInt32 size) {
    ::SetWindowPos(state_->hwnd, nullptr, 0, 0, size.width, size.height,
                   SWP_NOMOVE | SWP_NOZORDER | SWP_NOOWNERZORDER);
}

namespace {

// Монитор, на котором окно стоит сейчас, за вычетом панели задач.
RECT workAreaOf(HWND hwnd) noexcept {
    MONITORINFO monitor{};
    monitor.cbSize = sizeof(monitor);
    ::GetMonitorInfoW(::MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &monitor);
    return monitor.rcWork;
}

// Клиентский прямоугольник, обросший рамкой. Стиль берётся у САМОГО окна, а не
// из констант создания: в полном экране он другой, и посчитанная по константам
// рамка обманула бы. DPI -- монитора, на котором окно сейчас: у окна
// PerMonitorV2 рамка на разных экранах разной толщины.
RECT withFrame(HWND hwnd, RECT client) noexcept {
    ::AdjustWindowRectExForDpi(&client, static_cast<DWORD>(::GetWindowLongPtrW(hwnd, GWL_STYLE)),
                               FALSE,
                               static_cast<DWORD>(::GetWindowLongPtrW(hwnd, GWL_EXSTYLE)),
                               ::GetDpiForWindow(hwnd));
    return client;
}

}  // namespace

void CompositionWindow::centreWithClientSize(SizeInt32 client) {
    RECT const frame = withFrame(state_->hwnd, {0, 0, client.width, client.height});

    int const width = frame.right - frame.left;
    int const height = frame.bottom - frame.top;

    RECT const work = workAreaOf(state_->hwnd);

    ::SetWindowPos(state_->hwnd, nullptr, work.left + ((work.right - work.left) - width) / 2,
                   work.top + ((work.bottom - work.top) - height) / 2, width, height,
                   SWP_NOZORDER | SWP_NOOWNERZORDER);
}

SizeInt32 CompositionWindow::maxClientSize() const {
    RECT const work = workAreaOf(state_->hwnd);

    // Пустой клиент, обросший рамкой, -- это и есть одна рамка: слева-справа и
    // сверху-снизу вместе с заголовком.
    RECT const frame = withFrame(state_->hwnd, {0, 0, 0, 0});

    return {(work.right - work.left) - (frame.right - frame.left),
            (work.bottom - work.top) - (frame.bottom - frame.top)};
}

HWND__* CompositionWindow::handle() const { return state_->hwnd; }

DispatcherQueue CompositionWindow::dispatcherQueue() const {
    return Object::Impl::wrap<DispatcherQueue>(mud::DispatcherQueue::GetForCurrentThread());
}

void CompositionWindow::placement(std::wstring_view saved) {
    // Та же строка, что у генерируемого Window; разбор формата и подгонку к
    // сегодняшним мониторам берём общими -- impl::parse_placement и
    // fit_placement_to_displays из WindowPlacement.cpp.
    auto const wanted = impl::parse_placement(saved);
    if (!wanted) {
        return;  // пусто или мусор: окно остаётся там, где создано
    }

    WINDOWPLACEMENT placement{};
    placement.length = sizeof(placement);
    if (!::GetWindowPlacement(state_->hwnd, &placement)) {
        return;
    }
    placement.rcNormalPosition = impl::fit_placement_to_displays(wanted->restore);
    placement.showCmd =
        wanted->state == impl::placement_state::maximized ? SW_SHOWMAXIMIZED : SW_SHOWNORMAL;
    ::SetWindowPlacement(state_->hwnd, &placement);

    // Полный экран после восстановления места, не вместо: fullScreen(true)
    // запомнит только что выставленное место как то, куда возвращаться, и
    // накроет монитор.
    if (wanted->state == impl::placement_state::full_screen) {
        fullScreen(true);
    }
}

std::wstring CompositionWindow::placement() const {
    WINDOWPLACEMENT placement{};
    placement.length = sizeof(placement);
    if (!::GetWindowPlacement(state_->hwnd, &placement)) {
        return {};
    }

    // Под нашим Win32-накрытием rcNormalPosition уже равно прямоугольнику во
    // весь монитор (SetWindowPos сдвинул нормальное место), поэтому в полном
    // экране прямоугольник возврата берём из сохранённого на входе; в прочих
    // состояниях верим окну.
    RECT const rc = state_->fullScreen ? state_->savedPlacement.rcNormalPosition
                                       : placement.rcNormalPosition;
    impl::placement_state const state =
        state_->fullScreen                      ? impl::placement_state::full_screen
        : placement.showCmd == SW_SHOWMAXIMIZED ? impl::placement_state::maximized
                                                : impl::placement_state::normal;

    std::wstring text{impl::name_of(state)};
    for (long const value : {rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top}) {
        text += L' ';
        wxl::core::append_number(text, value);
    }
    return text;
}

bool CompositionWindow::fullScreen() const { return state_->fullScreen; }

void CompositionWindow::fullScreen(bool on) {
    if (on == state_->fullScreen) {
        return;
    }
    HWND const hwnd = state_->hwnd;
    if (on) {
        // Приём Реймонда Чена: запомнить место и стиль, снять рамку и накрыть
        // монитор целиком. WM_SIZE от SetWindowPos тянет за собой сцену и
        // остров синхронно -- отставать нечему.
        state_->savedPlacement.length = sizeof(state_->savedPlacement);
        ::GetWindowPlacement(hwnd, &state_->savedPlacement);
        state_->savedStyle = ::GetWindowLongPtrW(hwnd, GWL_STYLE);

        MONITORINFO monitor{sizeof(monitor)};
        ::GetMonitorInfoW(::MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY), &monitor);
        ::SetWindowLongPtrW(hwnd, GWL_STYLE,
                            state_->savedStyle & ~static_cast<LONG_PTR>(WS_OVERLAPPEDWINDOW));
        ::SetWindowPos(hwnd, HWND_TOP, monitor.rcMonitor.left, monitor.rcMonitor.top,
                       monitor.rcMonitor.right - monitor.rcMonitor.left,
                       monitor.rcMonitor.bottom - monitor.rcMonitor.top,
                       SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    } else {
        ::SetWindowLongPtrW(hwnd, GWL_STYLE, state_->savedStyle);
        ::SetWindowPlacement(hwnd, &state_->savedPlacement);
        ::SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                       SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    }
    state_->fullScreen = on;
}

void CompositionWindow::close() {
    // PostMessage, а не Send: закрытие встаёт в очередь, как системный крестик,
    // и разбирается тем же WM_CLOSE (onClosed -> DestroyWindow -> WM_DESTROY).
    ::PostMessageW(state_->hwnd, WM_CLOSE, 0, 0);
}

void CompositionWindow::onClosed(std::function<void()> handler) {
    state_->onClosed = std::move(handler);
}

void CompositionWindow::acceptFileDrops(std::function<void(std::filesystem::path)> handler) {
    state_->onFileDrop = std::move(handler);
    ::DragAcceptFiles(state_->hwnd, TRUE);
}

void CompositionWindow::onKeyDown(std::function<void(VirtualKey)> handler) {
    state_->onKeyDown = std::move(handler);
}
void CompositionWindow::onPointerPressed(std::function<void(PointerPoint const&)> handler) {
    state_->onPointerPressed = std::move(handler);
}
void CompositionWindow::onPointerMoved(std::function<void(PointerPoint const&)> handler) {
    state_->onPointerMoved = std::move(handler);
}
void CompositionWindow::onPointerReleased(std::function<void(PointerPoint const&)> handler) {
    state_->onPointerReleased = std::move(handler);
}
void CompositionWindow::onPointerWheel(std::function<void(PointerPoint const&)> handler) {
    state_->onPointerWheel = std::move(handler);
}

void CompositionWindow::onGeometryChanged(std::function<void()> handler) {
    state_->onGeometryChanged = std::move(handler);
}

void CompositionWindow::onClientSizeChanged(std::function<void(SizeInt32, float)> handler) {
    state_->onClientSizeChanged = std::move(handler);
}

}  // namespace wxl

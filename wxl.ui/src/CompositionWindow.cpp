// Реализация wxl::CompositionWindow. Кишки подняты из sandbox/AttachedProbe:
// окно создаётся внутри уже поднятого wxl (Application::Start в launch.cpp), а
// значит без своего загрузчика и цикла сообщений. Внутри держим сырой winrt
// (острова своего окна в профиль генератора не входят), а на границе заголовка
// мостим в обёртки wxl через Object::Impl -- ровно как DrawingSurface.
//
// Проекция и заголовки Windows -- первыми, и с ними стандартные, что они тянут:
// заголовки wxl несут импорт wxl.core, после которого текстовый заголовок MSVC
// уже видел бы через модуль std.
#include "platform.h"

#include <winrt/Microsoft.Graphics.Canvas.Effects.h>   // Border и Composite -- фон плиткой и цвет под ним
#include <winrt/Microsoft.UI.Composition.h>
#include <winrt/Microsoft.UI.Content.h>
#include <winrt/Microsoft.UI.Dispatching.h>
#include <winrt/Microsoft.UI.Input.h>
#include <winrt/Microsoft.UI.Windowing.h>
#include <winrt/Microsoft.UI.Xaml.Automation.h>
#include <winrt/Microsoft.UI.Xaml.Controls.Primitives.h>   // ButtonBase::Click -- команда кнопки окна для UI Automation
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Hosting.h>
#include <winrt/Microsoft.UI.Xaml.Input.h>
#include <winrt/Microsoft.UI.Xaml.Markup.h>
#include <winrt/Microsoft.UI.Xaml.Media.Animation.h>   // PopupThemeTransition -- въезд индикатора масштаба
#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.UI.Xaml.Interop.h>    // xaml_typename -- геометрия значка кнопки окна из строки
#include <winrt/Windows.UI.h>

#include <cmath>
#include <limits>
#include <utility>
#include <vector>

#include <d2d1_1.h>     // ID2D1DeviceContext, DrawBitmap -- фон-картинка рисуется через DrawingSurface
#include <wincodec.h>   // WIC: декод картинки заднего фона в поверхность
#include <shellapi.h>   // HDROP, DragAcceptFiles/DragQueryFileW/DragFinish -- WIN32_LEAN_AND_MEAN их прячет

#include "CompositionWindow.h"
#include "TextureCache.h"
// Перед Object.impl.h: тот тянет import wxl.core (модульный std), а
// window_helpers.h несёт обычный <optional> -- он должен встретиться до
// импорта, иначе MSVC не примет стандартный заголовок после него.
#include "impl/effect_helpers.h"
#include "impl/window_helpers.h"
#include "Application.h"
#include "impl/background_load.h"
#include "Object.impl.h"
#include "impl/handler_list.h"
#include <wxl/Microsoft.UI.Composition.impl.h>
#include <wxl/Microsoft.UI.Dispatching.impl.h>
#include <wxl/Microsoft.UI.Input.impl.h>
#include <wxl/Microsoft.UI.Windowing.impl.h>
#include <wxl/Microsoft.UI.Xaml.Media.impl.h>
#include <wxl/Microsoft.UI.Xaml.impl.h>
#include "AppZoom.h"

// Импорт последним: он несёт модульный std, а обычный заголовок после него
// MSVC уже не примет. Нужен для wxl::core::append_number -- сборки строки
// запомненного места окна.
import wxl.async;
import wxl.core;

namespace wxl {

namespace {

namespace canvas = winrt::Microsoft::Graphics::Canvas;
namespace effects = winrt::Microsoft::Graphics::Canvas::Effects;
namespace muc = winrt::Microsoft::UI::Composition;
namespace content = winrt::Microsoft::UI::Content;
namespace controls = winrt::Microsoft::UI::Xaml::Controls;
namespace graphics = winrt::Windows::Graphics;
namespace input = winrt::Microsoft::UI::Input;
namespace mud = winrt::Microsoft::UI::Dispatching;
namespace windowing = winrt::Microsoft::UI::Windowing;
namespace xaml = winrt::Microsoft::UI::Xaml;

winrt::Windows::UI::Color as_winrt(Color c) noexcept { return {c.A, c.R, c.G, c.B}; }

winrt::Microsoft::UI::WindowId window_id_of(HWND hwnd) noexcept {
    return {reinterpret_cast<uint64_t>(hwnd)};
}

bool same_rect(graphics::RectInt32 const& a, graphics::RectInt32 const& b) noexcept {
    return a.X == b.X && a.Y == b.Y && a.Width == b.Width && a.Height == b.Height;
}

// Класс окна -- один на процесс. Без кисти фона и с WM_ERASEBKGND->1:
// поверхности перенаправления нет, белому взяться неоткуда.
constexpr wchar_t class_name[] = L"wxl.CompositionWindow";

// Строка системного меню окна -- «&Свернуть», «&Закрыть\tAlt+F4» -- без
// ускорителя и клавиш: ею кнопки окна называют себя для UI Automation на языке
// Windows, не заводя своих строк.
std::wstring system_menu_text(HWND hwnd, UINT command) {
    wchar_t text[128] = {};
    int const length =
        ::GetMenuStringW(::GetSystemMenu(hwnd, FALSE), command, text, 128, MF_BYCOMMAND);
    std::wstring name;
    for (int at = 0; at < length && text[at] != L'\t'; ++at) {
        if (text[at] != L'&') name += text[at];
    }
    return name;
}

// Где у элемента, видимого поверх фона, свойство фона: у контрола, панели и
// рамки оно своё. Под кнопками окна -- тот же фон, что у заголовка рядом.
xaml::DependencyProperty background_property_of(xaml::FrameworkElement const& element) {
    if (element.try_as<controls::Control>()) return controls::Control::BackgroundProperty();
    if (element.try_as<controls::Panel>()) return controls::Panel::BackgroundProperty();
    if (element.try_as<controls::Border>()) return controls::Border::BackgroundProperty();
    return nullptr;
}

}  // namespace

// Общее состояние окна: сырой winrt внутри, обёртки -- только на границе.
// Узел списка живых окон (live_windows): в нём состояние, пока у него есть окно, --
// от создания до WM_NCDESTROY.
struct CompositionWindow::Impl : core::refcounted, core::intrusive_list_node<Impl> {
    HWND hwnd{nullptr};
    muc::Compositor compositor{nullptr};
    // Один визуал на всю сцену: он и корень острова, и задний фон (картинка или
    // цвет). Его же отдаёт contentVisual(): собственные визуалы приложения висят
    // на нём детьми -- контейнер над контейнером не нужен.
    muc::SpriteVisual backdrop{nullptr};
    content::DesktopAttachedSiteBridge scene_bridge{nullptr};
    xaml::Hosting::DesktopWindowXamlSource chrome{nullptr};   // оснастка островом
    xaml::Media::SystemBackdrop system_backdrop{nullptr};      // материал острова, если задан до него
    std::function<void(std::filesystem::path)> on_file_drop;

    SizeInt32 min_size{};                // нижний предел клиента (WM_GETMINMAXINFO)
    WINDOWPLACEMENT saved_placement{};   // место до полноэкранного -- куда вернуться
    LONG_PTR saved_style{};              // стиль окна до полноэкранного
    bool full_screen{false};

    // Кэш текстур поверх compositor -- под асинхронные задники (Win2D). Заводится
    // в конструкторе, когда компоновщик готов.
    std::optional<TextureCache> texture_cache;

    // Картинка заднего фона в свой размер и то, как она кроет окно. Кисть
    // задника из неё зависит от DPI экрана и собирается заново, когда окно
    // переезжает на другой экран.
    muc::CompositionSurfaceBrush picture{nullptr};
    BackgroundFill picture_fill{};
    Color picture_color{};

    // Асинхронная загрузка фона -- идущая или кончившаяся, пока фон не сменили
    // иначе: под своим источником, дочерним от токена, с которым её попросили,
    // и что просили. Всякая смена фона её отменяет -- фон, поставленный позже,
    // остаётся; замена устройства грузит её снова (device_replaced).
    impl::background_load<BackgroundImage> background_load;

    // Ввод со сцены, когда остров спрятан или его ещё нет: источники ввода её
    // ContentIsland. Источники держим живыми -- на них висят подписки.
    content::ContentIsland scene_island{nullptr};
    input::InputPointerSource pointer_input{nullptr};   // указатель сцены; клавиатура -- через WndProc

    // Подписчики событий окна; токены им даёт impl::next_event_token.
    impl::handler_list<WindowClosingEventArgs> closing;
    impl::handler_list<Object> closed;
    impl::handler_list<Object> geometry_changed;
    impl::handler_list<ClientSize> client_size_changed;
    impl::handler_list<VirtualKey> key_down;
    impl::handler_list<PointerPoint> pointer_pressed;
    impl::handler_list<PointerPoint> pointer_moved;
    impl::handler_list<PointerPoint> pointer_released;
    impl::handler_list<PointerPoint> pointer_wheel_changed;

    // Увеличение острова и то, как его понимает OverrideScale моста:
    // документация называет его и масштабом вместо масштаба окна, и множителем к
    // нему, так что это меряется на деле (apply_zoom) и запоминается.
    double zoom_factor{1.0};
    bool zoom_applied{false};

    // Масштаб, который присоединил ZoomEffect: клавиши и колесо острова двигают
    // модель, остров следует за её масштабом. Колесо копится до щелчка
    // (WHEEL_DELTA): точный тачпад шлёт его мелкими долями. Подписка на модель
    // хранится своим токеном: пустой -- подписки нет.
    core::intrusive_ptr<AppZoom> app_zoom;
    core::event_token zoom_factor_token;
    bool zoom_handlers_added{false};
    int32_t accumulated_wheel_delta{0};

    // Датчик жеста масштабирования: ScrollViewer над корнем острова со своим
    // масштабом и без прокрутки. Жест со вложенных списков доходит до него по
    // цепочке масштабирования; пока пальцы движутся, он увеличивает остров как
    // картинку, а в конце модель встаёт на ближайшую ступень, и он -- обратно к
    // 1 (проба sandbox/ZoomGestureProbe). Ctrl+колесо до него не доходит:
    // обработчик колеса у корня помечает его обработанным, и шаги остаются за
    // моделью.
    controls::ScrollViewer zoom_sensor{nullptr};

    // Индикатор текущего масштаба: появляется, пока масштаб меняется, и сам
    // прячется через zoomIndicatorDelay после последнего изменения. Простой
    // Popup, а не Flyout: у Flyout лёгкое закрытие, и открытый, он закрывался
    // на каждой смене масштаба острова -- она меняет размер XamlRoot, а на это
    // Popup с лёгким закрытием закрывается сам (Popup::OnXamlRootChanged в
    // исходниках WinUI), -- и ещё при уходе окна из активных и растяжке.
    // Popup без лёгкого закрытия остаётся открытым и только меняет текст
    // (проба sandbox/KeyboardZoomProbe --flyout-scale --popup). Внутри --
    // FlyoutPresenter: вид тот же, что у Flyout. Фокуса Popup не берёт:
    // следующее Ctrl+«+» идёт окну.
    controls::Primitives::Popup zoom_indicator{nullptr};
    controls::TextBlock zoom_indicator_text{nullptr};
    // Без строки заголовка индикатор встаёт под этот пустой элемент во всю
    // ширину верха корня.
    controls::Border zoom_indicator_anchor{nullptr};
    winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer zoom_indicator_timer{nullptr};
    static constexpr std::chrono::milliseconds zoom_indicator_delay{1500};

    // Раскладка острова: корень в две строки -- заголовок с кнопками окна и под
    // ним содержимое приложения. Заводится вместе с островом.
    controls::Grid root{nullptr};
    controls::Grid bar{nullptr};                 // строка заголовка: элемент | кнопки окна
    controls::StackPanel caption_buttons{nullptr};
    controls::Button minimize_button{nullptr};
    controls::Button maximize_button{nullptr};
    controls::Button close_button{nullptr};
    xaml::UIElement page{nullptr};               // содержимое приложения

    // Фокус клавиатуры -- как у Microsoft.UI.Xaml.Window (DesktopWindowImpl в
    // исходниках WinUI 3). Пока остров показан, WM_ACTIVATE не доходит до
    // DefWindowProc: тот ставит фокус на само окно, и клавиши идут в WndProc
    // мимо XAML. Окно отдаёт фокус острову и возвращает его элементу, у
    // которого он был; уходя, запоминает дочернее окно с фокусом. Снаружи
    // остров виден только через XamlRoot содержимого, а он появляется не
    // сразу: пришла активация раньше него -- передача фокуса ждёт загрузки
    // (проба sandbox/KeyboardZoomProbe).
    HWND last_focused_child{nullptr};
    bool initial_activation{true};
    bool island_focus_pending{false};
    winrt::guid last_take_focus_correlation{};
    // Остров показан (show_page) и не спрятан (hideContent). Не
    // IsVisible моста: тот ложен и у показанного острова, пока не видно само
    // окно, -- а первый WM_ACTIVATE приходит изнутри ShowWindow.
    bool content_shown{false};

    // Заголовок окна -- то, что у Microsoft.UI.Xaml.Window держит WindowChrome.
    // AppWindow и источник неклиентского ввода заводятся при первой нужде: окну,
    // которое заголовок не трогает, они ни к чему.
    windowing::AppWindow app_window{nullptr};
    // Подписки на неклиентский ввод -- токенами, а не auto_revoke: отзыватель
    // берёт у источника слабую ссылку, а InputNonClientPointerSource слабых
    // ссылок не даёт, и подписка с ним падает нарушением доступа.
    input::InputNonClientPointerSource non_client{nullptr};
    winrt::event_token regions_changed{};
    winrt::event_token caption_entered{};
    winrt::event_token caption_exited{};
    winrt::event_token caption_pressed{};
    winrt::event_token caption_released{};
    bool extends_content_into_title_bar{false};
    bool caption_buttons_transparent{false};   // прозрачный фон системных кнопок ставится один раз
    bool active{false};                      // по WM_NCACTIVATE: у неактивного окна знаки кнопок гаснут
    // Высота системных кнопок до того, как их сменили свои, -- её возвращают,
    // когда заголовок-элемент убран.
    std::optional<windowing::TitleBarHeightOption> system_buttons_height;

    xaml::FrameworkElement title_bar{nullptr};
    xaml::FrameworkElement::SizeChanged_revoker title_bar_size_changed;
    std::int64_t title_bar_visibility{0};
    xaml::DependencyProperty title_bar_background_property{nullptr};
    std::int64_t title_bar_background{0};
    xaml::XamlRoot xaml_root{nullptr};
    winrt::event_token xaml_root_changed{};
    // Свой прямоугольник среди Caption: его и только его заменяет следующий
    // пересчёт, а прямоугольники, поставленные другими, остаются.
    graphics::RectInt32 caption{};
    bool own_button_regions{false};
    bool regions_queued{false};

    // XAML этого окна закрыт (close_xaml): остров заново не заводится.
    bool xaml_closed{false};

    // Состояние живёт дольше окна: его держат хендлы приложения, и последний
    // бывает отпущен уже после Application::Start, когда XAML закрыт. От XAML
    // к тому времени ничего не осталось -- всё отпущено при уничтожении окна
    // (close_xaml); здесь -- подписка на модель масштаба, она не из XAML.
    ~Impl() {
        if (zoom_factor_token) app_zoom->zoomFactor().remove_change(zoom_factor_token);
    }

    // Окно уничтожается (WM_DESTROY), XAML ещё жив: окно закрывают, пока он жив, -- его же WM_CLOSE
    // или wxl при выходе, в ShutdownStarting очереди XAML (CompositionWindow::close_all_at_exit);
    // XAML разбирает себя следом, ещё внутри Application::Start. Всё, что состояние держит от XAML,
    // отпускается сейчас, как у Microsoft.UI.Xaml.Window при закрытии (DesktopWindowImpl::CloseImpl
    // и Shutdown в исходниках WinUI 3): подписки сняты, содержимое убрано, источник XAML закрыт.
    // XamlRoot, отпущенный после закрытия XAML, падал в своём деструкторе нарушением доступа --
    // при закрытии окна, но только после клавиш масштаба: деструктор разбирает службы,
    // заведённые по требованию, а всплывающее их заводит (Flyout -- ApplicationBarService).
    // Заголовок-элемент, если его держит приложение, переживает окно -- его обратные вызовы
    // снимаются здесь же. С ними -- подписки на неклиентский ввод: окно ещё есть, и снимать их есть
    // с кого.
    void close_xaml() {
        xaml_closed = true;
        // Загрузка фона, которая идёт, окну больше не нужна: её отмена -- ход
        // очереди, а очередь ещё принимает работу; остановленная, она хода не
        // приняла бы, и кадр загрузки ждал бы на выходе вечно. С отменой забыто и
        // то, что просили: замена устройства после этого ничего не грузит.
        background_load.cancel();
        try {
            drop_title_bar();
            if (xaml_root) xaml_root.Changed(xaml_root_changed);
            if (non_client) {
                non_client.RegionsChanged(regions_changed);
                non_client.PointerEntered(caption_entered);
                non_client.PointerExited(caption_exited);
                non_client.PointerPressed(caption_pressed);
                non_client.PointerReleased(caption_released);
            }
            if (zoom_indicator_timer) zoom_indicator_timer.Stop();
            if (zoom_indicator) zoom_indicator.IsOpen(false);
            if (chrome) {
                chrome.Content(nullptr);
                chrome.Close();
            }
        } catch (winrt::hresult_error const& error) {
            ::OutputDebugStringW((L"wxl::CompositionWindow: XAML окна закрыт не чисто: " +
                                  std::wstring{error.message()} + L"\n").c_str());
        }
        non_client = nullptr;
        xaml_root = nullptr;
        zoom_indicator_timer = nullptr;
        zoom_indicator = nullptr;
        zoom_indicator_text = nullptr;
        zoom_indicator_anchor = nullptr;
        zoom_sensor = nullptr;
        page = nullptr;
        close_button = nullptr;
        maximize_button = nullptr;
        minimize_button = nullptr;
        caption_buttons = nullptr;
        bar = nullptr;
        root = nullptr;
        chrome = nullptr;
        content_shown = false;
        island_focus_pending = false;
    }

    void resize(float width, float height) {
        winrt::Windows::Foundation::Numerics::float2 const wh{width, height};
        // Один визуал сцены -- задний. Он корень острова и картинка разом, а
        // собственные визуалы приложения висят на нём детьми и меряют себя сами,
        // так что под окно подгоняем только его.
        if (backdrop) backdrop.Size(wh);
        if (chrome) {
            chrome.SiteBridge().MoveAndResize(
                {0, 0, static_cast<int32_t>(width), static_cast<int32_t>(height)});
        }
    }

    float scale() const {
        return static_cast<float>(::GetDpiForWindow(hwnd)) / 96.0f * static_cast<float>(zoom_factor);
    }

    void show_picture(muc::CompositionSurfaceBrush brush, BackgroundFill fill, Color color) {
        picture = std::move(brush);
        picture_fill = fill;
        picture_color = color;
        fill_picture();
    }

    // Кисть задника из картинки. Сцена -- в физических пикселях, поэтому свой
    // размер картинки (None, плитка) доводится до логического множителем DPI.
    void fill_picture() {
        if (!picture) return;

        float const dpi = hwnd ? static_cast<float>(::GetDpiForWindow(hwnd)) / 96.0f : 1.0f;

        picture.StopAnimation(L"CenterPoint");
        picture.CenterPoint({0.0f, 0.0f});
        picture.Scale({1.0f, 1.0f});
        picture.HorizontalAlignmentRatio(0.5f);
        picture.VerticalAlignmentRatio(0.5f);

        // Композитор принимает источником эффекта только параметр, а не другой
        // эффект, поэтому свой размер картинке даёт масштаб её же кисти.
        winrt::Windows::Graphics::Effects::IGraphicsEffectSource shown =
            muc::CompositionEffectSourceParameter{L"picture"};
        bool tiled = false;

        switch (picture_fill) {
            case BackgroundFill::UniformToFill:
                picture.Stretch(muc::CompositionStretch::UniformToFill);
                break;
            case BackgroundFill::Uniform:
                picture.Stretch(muc::CompositionStretch::Uniform);
                break;
            case BackgroundFill::Fill:
                picture.Stretch(muc::CompositionStretch::Fill);
                break;
            case BackgroundFill::None: {
                // Масштаб вокруг середины задника, а она движется вместе с окном.
                picture.Stretch(muc::CompositionStretch::None);
                picture.Scale({dpi, dpi});
                muc::ExpressionAnimation const centre =
                    compositor.CreateExpressionAnimation(L"backdrop.Size * 0.5");
                centre.SetReferenceParameter(L"backdrop", backdrop);
                picture.StartAnimation(L"CenterPoint", centre);
                break;
            }
            case BackgroundFill::Tile:
            case BackgroundFill::TileMirrored: {
                // Повтор за краем картинки считает композитор эффектом Border, и
                // растяжка окна не стоит ни строчки кода.
                picture.Stretch(muc::CompositionStretch::None);
                picture.HorizontalAlignmentRatio(0.0f);
                picture.VerticalAlignmentRatio(0.0f);
                picture.Scale({dpi, dpi});

                auto const edge = picture_fill == BackgroundFill::Tile
                                      ? canvas::CanvasEdgeBehavior::Wrap
                                      : canvas::CanvasEdgeBehavior::Mirror;
                effects::BorderEffect tiles;
                tiles.ExtendX(edge);
                tiles.ExtendY(edge);
                tiles.Source(shown);
                shown = tiles;
                tiled = true;
                break;
            }
        }

        bool const underlay = picture_color.A != 0;
        if (!tiled && !underlay) {
            backdrop.Brush(picture);
            return;
        }

        winrt::Windows::Graphics::Effects::IGraphicsEffect graph{nullptr};
        if (underlay) {
            effects::CompositeEffect layers;
            layers.Sources().Append(muc::CompositionEffectSourceParameter{L"colour"});
            layers.Sources().Append(shown);
            graph = layers;
        } else {
            graph = shown.as<winrt::Windows::Graphics::Effects::IGraphicsEffect>();
        }

        muc::CompositionEffectBrush const brush = compositor.CreateEffectFactory(graph).CreateBrush();
        brush.SetSourceParameter(L"picture", picture);
        if (underlay)
            brush.SetSourceParameter(L"colour", compositor.CreateColorBrush(as_winrt(picture_color)));
        backdrop.Brush(brush);
    }

    ClientSize client_size() const {
        RECT client{};
        ::GetClientRect(hwnd, &client);
        return {{client.right, client.bottom}, scale()};
    }

    windowing::AppWindow const& app_window_of() {
        if (!app_window) app_window = windowing::AppWindow::GetFromWindowId(window_id_of(hwnd));
        return app_window;
    }

    input::InputNonClientPointerSource const& non_client_of() {
        if (!non_client) {
            non_client = input::InputNonClientPointerSource::GetForWindowId(window_id_of(hwnd));
            // AppWindow переписывает Minimize, Maximize и Close своими
            // прямоугольниками на каждое перемещение и активацию -- свои ставятся
            // снова. Событие приходит и на запись тех же чисел; пересчёт
            // сравнивает, прежде чем писать, иначе цикл.
            regions_changed = non_client.RegionsChanged([this](auto&&, auto&&) {
                if (own_button_regions) queue_regions();
            });
            // Над областью кнопки указатель у неклиентского ввода, не у XAML:
            // состояния кнопок ставятся отсюда.
            caption_entered = non_client.PointerEntered(
                [this](auto&&, input::NonClientPointerEventArgs const& args) {
                    show_caption_state(args.RegionKind(), L"PointerOver");
                });
            caption_exited = non_client.PointerExited(
                [this](auto&&, input::NonClientPointerEventArgs const& args) {
                    show_caption_state(args.RegionKind(), L"Normal");
                });
            caption_pressed = non_client.PointerPressed(
                [this](auto&&, input::NonClientPointerEventArgs const& args) {
                    show_caption_state(args.RegionKind(), L"Pressed");
                });
            caption_released = non_client.PointerReleased(
                [this](auto&&, input::NonClientPointerEventArgs const& args) {
                    show_caption_state(args.RegionKind(), L"PointerOver");
                });
        }
        return non_client;
    }

    // ---- Остров ----

    void ensure_island() {
        if (chrome) return;
        // Окно уничтожено -- острову не на чем стоять: RO_E_CLOSED, как у
        // всякого закрытого объекта WinRT.
        if (xaml_closed) throw winrt::hresult_error{RO_E_CLOSED, L"wxl::CompositionWindow: окно уже закрыто"};
        chrome = xaml::Hosting::DesktopWindowXamlSource{};
        chrome.Initialize(window_id_of(hwnd));
        if (system_backdrop) chrome.SystemBackdrop(system_backdrop);

        root = controls::Grid{};
        for (auto const height : {xaml::GridLengthHelper::Auto(),
                                  xaml::GridLengthHelper::FromValueAndType(1, xaml::GridUnitType::Star)}) {
            controls::RowDefinition row;
            row.Height(height);
            root.RowDefinitions().Append(row);
        }
        // Прозрачный фон -- чтобы пустые места корня принимали указатель: без
        // кисти их нет для попадания, и Ctrl+колесо над ними доставалось
        // датчику масштаба мимо обработчика колеса у корня, а датчик
        // масштабировал сам (ступени скакали, остров съезжал от угла).
        root.Background(xaml::Media::SolidColorBrush{winrt::Microsoft::UI::Colors::Transparent()});
        root.Loaded([this](auto&&, auto&&) { on_root_loaded(); });
        chrome.Content(root);
        chrome.TakeFocusRequested(
            [this](auto&&, xaml::Hosting::DesktopWindowXamlSourceTakeFocusRequestedEventArgs const& args) {
                on_take_focus_requested(args);
            });

        RECT client{};
        ::GetClientRect(hwnd, &client);
        chrome.SiteBridge().MoveAndResize({0, 0, client.right, client.bottom});
        apply_zoom();
    }

    void on_root_loaded() {
        watch_xaml_root();
        // Остров стал виден снаружи: передача фокуса, которую окно отложило.
        if (!island_focus_pending) return;
        island_focus_pending = false;
        if (island_shown() && ::GetActiveWindow() == hwnd) focus_island();
    }

    // ---- Фокус клавиатуры: как у Microsoft.UI.Xaml.Window ----

    bool island_shown() const { return chrome && content_shown; }

    // Окно ввода острова -- дочернее у окна его моста.
    bool island_owns(HWND window) const {
        auto const bridge = reinterpret_cast<HWND>(static_cast<std::uintptr_t>(chrome.SiteBridge().WindowId().Value));
        return window == bridge || ::IsChild(bridge, window);
    }

    // DesktopWindowImpl::SetFocusToContentIsland: фокус Windows -- окну ввода острова.
    void focus_island() {
        xaml::XamlRoot const current = root ? root.XamlRoot() : nullptr;
        if (!current) {
            island_focus_pending = true;
            return;
        }
        island_focus_pending = false;
        auto const controller = input::InputFocusController::GetForIsland(current.ContentIsland());
        if (!controller.HasFocus()) controller.TrySetFocus();
    }

    // DesktopWindowImpl::RestoreFocus: фокус XAML -- элементу, у которого он был.
    void restore_xaml_focus() {
        chrome.NavigateFocus(xaml::Hosting::XamlSourceFocusNavigationRequest{
            xaml::Hosting::XamlSourceFocusNavigationReason::Restore});
    }

    // WM_ACTIVATE -- DesktopWindowImpl::OnActivate. false -- острова нет или он
    // спрятан (hideContent): фокус ставит DefWindowProc на само окно, и
    // клавиши идут сцене (onKeyDown).
    bool on_activate(WPARAM wparam) {
        if (!island_shown()) return false;
        bool const minimized = HIWORD(wparam) != 0;
        bool const inactive = LOWORD(wparam) == WA_INACTIVE;
        if (inactive && !minimized) {
            // У свёрнутого GetFocus пуст -- запомненное остаётся прежним.
            HWND const focus = ::GetFocus();
            if (focus && ::IsChild(hwnd, focus)) last_focused_child = focus;
        }
        if (!minimized && !inactive) {
            if (!last_focused_child || island_owns(last_focused_child)) {
                focus_island();
                restore_xaml_focus();
            } else {
                ::SetFocus(last_focused_child);
            }
        }
        if (!minimized && initial_activation) {
            focus_island();
            initial_activation = false;
        }
        return true;
    }

    // Tab за последний элемент острова (Shift+Tab -- за первый): остров отдаёт
    // фокус окну, а кроме острова в окне ничего нет -- фокус идёт по кругу в
    // тот же остров, как у Window (OnDesktopWindowXamlSourceTakeFocusRequested).
    void on_take_focus_requested(xaml::Hosting::DesktopWindowXamlSourceTakeFocusRequestedEventArgs const& args) {
        auto const request = args.Request();
        winrt::guid const correlation = request.CorrelationId();
        if (correlation == last_take_focus_correlation) {
            // Запрос вернулся с тем же номером: в острове один элемент, фокус
            // остаётся на нём.
            restore_xaml_focus();
            return;
        }
        last_take_focus_correlation = correlation;
        auto const reason = request.Reason();
        if (reason == xaml::Hosting::XamlSourceFocusNavigationReason::First ||
            reason == xaml::Hosting::XamlSourceFocusNavigationReason::Last) {
            chrome.NavigateFocus(
                xaml::Hosting::XamlSourceFocusNavigationRequest{reason, winrt::Windows::Foundation::Rect{}, correlation});
        }
    }

    // Смена масштаба острова -- DPI или увеличение -- меняет прямоугольники в
    // физических пикселях, а логический размер элементов остаётся, и
    // SizeChanged не придёт.
    void watch_xaml_root() {
        xaml::XamlRoot const current = root ? root.XamlRoot() : nullptr;
        if (!current || current == xaml_root) return;
        if (xaml_root) xaml_root.Changed(xaml_root_changed);
        xaml_root = current;
        xaml_root_changed = current.Changed([this](auto&&, auto&&) { queue_regions(); });
    }

    // Увеличение острова: и от CompositionWindow::zoomFactor, и от модели ZoomEffect.
    void set_zoom_factor(double value) {
        if (value <= 0 || value == zoom_factor) return;
        zoom_factor = value;
        apply_zoom();
        limit_zoom_gesture();
        client_size_changed.raise(client_size());
        queue_regions();
    }

    // ---- Жест масштабирования и индикатор масштаба (ZoomEffect) ----

    // Датчик ставится над корнем, когда присоединён эффект и есть остров.
    void ensure_zoom_sensor() {
        if (zoom_sensor || !app_zoom || !chrome) return;
        zoom_sensor = controls::ScrollViewer{};
        zoom_sensor.ZoomMode(controls::ZoomMode::Enabled);
        // Без инерции: ступень выбирается там, где пальцы остановились. С ней
        // быстрый жест, остановленный на 200 %, доезжал до 300 % (проба на
        // wxl.gen.ui касанием InjectSyntheticPointerInput).
        zoom_sensor.IsZoomInertiaEnabled(false);
        zoom_sensor.HorizontalScrollMode(controls::ScrollMode::Disabled);
        zoom_sensor.VerticalScrollMode(controls::ScrollMode::Disabled);
        zoom_sensor.HorizontalScrollBarVisibility(controls::ScrollBarVisibility::Disabled);
        zoom_sensor.VerticalScrollBarVisibility(controls::ScrollBarVisibility::Disabled);
        limit_zoom_gesture();
        zoom_sensor.ViewChanged([this](auto&&, controls::ScrollViewerViewChangedEventArgs const& args) {
            on_zoom_gesture(args.IsIntermediate());
        });
        chrome.Content(zoom_sensor);
        zoom_sensor.Content(root);
    }

    // Жест не уходит за ступени модели: пределы датчика -- относительно
    // масштаба, в котором остров уже стоит.
    void limit_zoom_gesture() {
        if (!zoom_sensor) return;
        zoom_sensor.MinZoomFactor(static_cast<float>(AppZoom::minZoomFactor / zoom_factor));
        zoom_sensor.MaxZoomFactor(static_cast<float>(AppZoom::maxZoomFactor / zoom_factor));
    }

    void on_zoom_gesture(bool intermediate) {
        double const gesture = zoom_sensor.ZoomFactor();
        // ScrollViewer хранит масштаб во float: единица -- с допуском.
        if (std::abs(gesture - 1.0) < 1e-4) return;
        double const wanted = zoom_factor * gesture;
        if (intermediate) {
            show_zoom_indicator(wanted);
            return;
        }
        // Сначала датчик обратно к единице, потом модель на ступень: остров
        // берёт новый масштаб уже без увеличения картинкой поверх.
        zoom_sensor.ChangeView(nullptr, nullptr, 1.0f, true);
        double const before = zoom_factor;
        app_zoom->setZoomFactor(wanted);
        // Сменилась ступень -- индикатор уже показала подписка на модель;
        // нет -- он всё ещё показывает промежуточное число жеста.
        if (zoom_factor == before) show_zoom_indicator(zoom_factor);
    }

    static winrt::hstring percent(double factor) {
        return winrt::hstring{std::to_wstring(std::lround(factor * 100)) + L" %"};
    }

    // Зовётся и из подписки на модель, а она noexcept: ошибка XAML здесь --
    // пропущенный индикатор, а не конец программы. Острову, которого ещё не
    // показали, индикатор не нужен.
    void show_zoom_indicator(double factor) noexcept {
        if (!root || !root.XamlRoot()) return;
        try {
            show_zoom_indicator_now(factor);
        } catch (winrt::hresult_error const& error) {
            ::OutputDebugStringW((L"wxl::ZoomEffect: индикатор масштаба не показан: " +
                                  std::wstring{error.message()} + L"\n").c_str());
        }
    }

    void show_zoom_indicator_now(double factor) {
        if (!zoom_indicator) make_zoom_indicator();
        zoom_indicator_text.Text(percent(factor));
        if (!zoom_indicator.IsOpen()) {
            // Под строкой заголовка, посередине: Bottom ставит Popup по
            // середине элемента и держит там, когда тот меняет размер. Строка
            // заголовка может появиться позже индикатора -- цель выбирается
            // при каждом показе.
            if (bar) {
                zoom_indicator.PlacementTarget(bar);
            } else {
                ensure_zoom_indicator_anchor();
                zoom_indicator.PlacementTarget(zoom_indicator_anchor);
            }
            zoom_indicator.IsOpen(true);
        }
        zoom_indicator_timer.Stop();
        zoom_indicator_timer.Start();
    }

    void make_zoom_indicator() {
        zoom_indicator_text = controls::TextBlock{};
        controls::FlyoutPresenter presenter;
        presenter.Content(zoom_indicator_text);
        // Индикатор только показывает число: мышь и клавиатура до него не
        // доходят. Щелчок по всплывающему ставил фокус XAML на его Popup;
        // запрет фокуса (AllowFocusOnInteraction, IsTabStop) этого не снимает,
        // снимает только IsHitTestVisible: щелчок проходит насквозь. IsTabStop
        // -- чтобы Tab до него не доходил (проба sandbox/KeyboardZoomProbe
        // --info-flyout).
        presenter.IsHitTestVisible(false);
        presenter.IsTabStop(false);
        // Зазор под целью и въезд сверху -- как у Flyout под элементом
        // (FlyoutBase в исходниках WinUI: FlyoutMargin 4, PopupThemeTransition
        // со сдвигом -50).
        presenter.Margin(xaml::ThicknessHelper::FromLengths(0, 4, 0, 0));
        xaml::Media::Animation::PopupThemeTransition entrance;
        entrance.FromVerticalOffset(-50);
        xaml::Media::Animation::TransitionCollection transitions;
        transitions.Append(entrance);
        zoom_indicator = controls::Primitives::Popup{};
        zoom_indicator.Child(presenter);
        zoom_indicator.ChildTransitions(transitions);
        zoom_indicator.DesiredPlacement(controls::Primitives::PopupPlacementMode::Bottom);
        zoom_indicator.XamlRoot(root.XamlRoot());

        zoom_indicator_timer = winrt::Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread().CreateTimer();
        zoom_indicator_timer.Interval(zoom_indicator_delay);
        zoom_indicator_timer.IsRepeating(false);
        zoom_indicator_timer.Tick([this](auto&&, auto&&) {
            if (zoom_indicator) zoom_indicator.IsOpen(false);
        });
    }

    void ensure_zoom_indicator_anchor() {
        if (zoom_indicator_anchor) return;
        zoom_indicator_anchor = controls::Border{};
        zoom_indicator_anchor.Height(0);
        zoom_indicator_anchor.VerticalAlignment(xaml::VerticalAlignment::Top);
        zoom_indicator_anchor.IsHitTestVisible(false);
        controls::Grid::SetRow(zoom_indicator_anchor, 0);
        root.Children().Append(zoom_indicator_anchor);
    }

    // Клавиши -- у верхнего элемента острова (датчика жеста), в PreviewKeyDown:
    // он идёт сверху раньше элемента в фокусе, и Ctrl+0 не достаётся полю
    // ввода. Не у корня: фокус бывает и над ним -- на самом датчике или ни на
    // чём, -- и во всплывающем окне, где корень клавишу не видит. Такую клавишу
    // датчик масштабировал сам, как любой ScrollViewer, и оставался не в 1:
    // остров уменьшался дважды и съезжал от угла (проба
    // sandbox/KeyboardZoomProbe). Колесо -- у корня, с handledEventsToo: оно
    // всплывает снизу, и корень видит его раньше датчика; ScrollViewer и
    // списки помечают его обработанным.
    void add_zoom_handlers() {
        if (zoom_handlers_added) return;
        zoom_handlers_added = true;
        xaml::UIElement const top = zoom_sensor ? xaml::UIElement{zoom_sensor} : xaml::UIElement{root};
        top.PreviewKeyDown([this](auto&&, xaml::Input::KeyRoutedEventArgs const& args) {
            if (app_zoom && on_preview_key_down(static_cast<int>(args.Key()))) args.Handled(true);
        });
        root.AddHandler(
            xaml::UIElement::PointerWheelChangedEvent(),
            winrt::box_value(xaml::Input::PointerEventHandler(
                [this](auto&&, xaml::Input::PointerRoutedEventArgs const& args) {
                    if (app_zoom && on_pointer_wheel_changed(args)) args.Handled(true);
                })),
            true);
    }

    // Ctrl без Alt: Ctrl+Alt -- это AltGr, им набирают символы.
    static bool is_zoom_modifier_down() { return ::GetKeyState(VK_CONTROL) < 0 && ::GetKeyState(VK_MENU) >= 0; }

    // «+» и «-» -- и основного ряда, и цифрового блока; «0» -- тоже оба.
    bool on_preview_key_down(int key) {
        if (!is_zoom_modifier_down()) return false;
        switch (key) {
            case VK_ADD:
            case VK_OEM_PLUS:
                app_zoom->zoomIn();
                return true;
            case VK_SUBTRACT:
            case VK_OEM_MINUS:
                app_zoom->zoomOut();
                return true;
            case '0':
            case VK_NUMPAD0:
                app_zoom->resetZoom();
                return true;
        }
        return false;
    }

    bool on_pointer_wheel_changed(xaml::Input::PointerRoutedEventArgs const& args) {
        if (!is_zoom_modifier_down()) return false;
        auto const wheel = args.GetCurrentPoint(root).Properties();
        if (wheel.IsHorizontalMouseWheel()) return false;
        accumulated_wheel_delta += wheel.MouseWheelDelta();
        for (; accumulated_wheel_delta >= WHEEL_DELTA; accumulated_wheel_delta -= WHEEL_DELTA) app_zoom->zoomIn();
        for (; accumulated_wheel_delta <= -WHEEL_DELTA; accumulated_wheel_delta += WHEEL_DELTA) app_zoom->zoomOut();
        return true;
    }

    void apply_zoom() {
        if (!chrome || (zoom_factor == 1.0 && !zoom_applied)) return;
        zoom_applied = true;
        content::DesktopChildSiteBridge const bridge = chrome.SiteBridge();
        content::ContentSiteView const view = bridge.SiteView();
        float const parent = view.ParentScale();
        float const wanted = parent * static_cast<float>(zoom_factor);
        bridge.OverrideScale(wanted);
        // Документация называет OverrideScale и масштабом, заменяющим масштаб
        // окна, и множителем к нему. На экране 100 % разницы нет; на другом
        // видно по итогу, и множитель ставится заново.
        float const got = view.RasterizationScale();
        if (std::abs(got - wanted) > 0.001f && std::abs(got - wanted * parent) <= 0.001f) {
            bridge.OverrideScale(static_cast<float>(zoom_factor));
        }
    }

    void show_page(xaml::UIElement const& element) {
        ensure_island();
        if (page) {
            uint32_t at = 0;
            if (root.Children().IndexOf(page, at)) root.Children().RemoveAt(at);
        }
        page = element;
        if (page) {
            controls::Grid::SetRow(page.as<xaml::FrameworkElement>(), 1);
            root.Children().Append(page);
        }
        chrome.SiteBridge().Show();
        content_shown = true;
        // Фокус на самом окне -- он там для сцены, пока острова не было или он был
        // спрятан, или остров показан уже после активации. Показанному острову --
        // фокус, как при активации.
        if (::GetFocus() == hwnd) {
            focus_island();
            restore_xaml_focus();
        }
    }

    // ---- Заголовок и кнопки окна ----

    controls::Button caption_button(wchar_t const* geometry, UINT command) {
        bool const close = command == SC_CLOSE;
        controls::Button button;
        auto const resources = xaml::Application::Current().Resources();
        auto const resource = [&resources](wchar_t const* key) {
            return resources.Lookup(winrt::box_value(winrt::hstring{key}));
        };
        button.Style(resource(L"WindowCaptionButton").as<xaml::Style>());
        // Штрих стиля в полпикселя сглаживается в серый и у активного окна;
        // системные знаки -- в пиксель.
        button.Resources().Insert(winrt::box_value(winrt::hstring{L"WindowCaptionButtonStrokeWidth"}),
                                  winrt::box_value(1.0));
        // Значок стиль берёт из Content геометрией; у «развернуть» -- из своих
        // состояний WindowStateNormal и WindowStateMaximized.
        if (geometry) {
            button.Content(xaml::Markup::XamlBindingHelper::ConvertValue(
                winrt::xaml_typename<xaml::Media::Geometry>(), winrt::box_value(winrt::hstring{geometry})));
        }
        // Красные кисти закрытия в словарях тем есть, но состояния
        // CloseButtonPointerOver и CloseButtonPressed берут общие кисти кнопок --
        // у кнопки закрытия они подменены своими ресурсами.
        if (close) {
            for (auto const& [target, source] :
                 {std::pair{L"WindowCaptionButtonBackgroundPointerOver", L"CloseButtonBackgroundPointerOver"},
                  std::pair{L"WindowCaptionButtonStrokePointerOver", L"CloseButtonStrokePointerOver"},
                  std::pair{L"WindowCaptionButtonBackgroundPressed", L"CloseButtonBackgroundPressed"},
                  std::pair{L"WindowCaptionButtonStrokePressed", L"CloseButtonStrokePressed"}}) {
                button.Resources().Insert(winrt::box_value(winrt::hstring{target}), resource(source));
            }
        }
        // Высота -- строки заголовка: стиль ставит свою, она снимается.
        button.Height(std::numeric_limits<double>::quiet_NaN());
        button.VerticalAlignment(xaml::VerticalAlignment::Stretch);
        // Мышь над кнопкой -- у области окна, и команду по щелчку Windows
        // выполняет сама; до XAML щелчок не доходит, иначе команда пришла бы
        // дважды. Click остаётся UI Automation: нажатие кнопки программой чтения
        // с экрана отдаёт ту же системную команду.
        button.IsHitTestVisible(false);
        button.Click([this, command](auto&&, auto&&) {
            if (!hwnd) return;
            UINT const given = command == SC_MAXIMIZE && ::IsZoomed(hwnd) ? SC_RESTORE : command;
            ::PostMessageW(hwnd, WM_SYSCOMMAND, given, 0);
        });
        return button;
    }

    void ensure_bar() {
        if (bar) return;
        bar = controls::Grid{};
        for (auto const width : {xaml::GridLengthHelper::FromValueAndType(1, xaml::GridUnitType::Star),
                                 xaml::GridLengthHelper::Auto()}) {
            controls::ColumnDefinition column;
            column.Width(width);
            bar.ColumnDefinitions().Append(column);
        }
        controls::Grid::SetRow(bar, 0);
        root.Children().Append(bar);

        caption_buttons = controls::StackPanel{};
        caption_buttons.Orientation(controls::Orientation::Horizontal);
        caption_buttons.VerticalAlignment(xaml::VerticalAlignment::Stretch);
        controls::Grid::SetColumn(caption_buttons, 1);

        minimize_button = caption_button(L"M 0 0 L 10 0", SC_MINIMIZE);
        maximize_button = caption_button(nullptr, SC_MAXIMIZE);
        close_button = caption_button(L"M 0 0 L 9 9 M 9 0 L 0 9", SC_CLOSE);
        xaml::Automation::AutomationProperties::SetName(minimize_button, system_menu_text(hwnd, SC_MINIMIZE));
        xaml::Automation::AutomationProperties::SetName(close_button, system_menu_text(hwnd, SC_CLOSE));
        maximize_button.Loaded([this](auto&&, auto&&) { show_maximized(); });
        for (controls::Button const& button : {minimize_button, maximize_button, close_button}) {
            caption_buttons.Children().Append(button);
        }
        caption_buttons.SizeChanged([this](auto&&, auto&&) { queue_regions(); });
        bar.Children().Append(caption_buttons);
        show_active(active);
    }

    // У стиля нет неактивного состояния. Фон кнопки в покое прозрачный, так
    // что её прозрачность гасит один знак, как у системных кнопок; под
    // указателем знак полный.
    static constexpr double inactive_caption_opacity = 0.36;

    void show_active(bool value) {
        active = value;
        for (controls::Button const& button : {minimize_button, maximize_button, close_button}) {
            if (button) button.Opacity(active ? 1.0 : inactive_caption_opacity);
        }
    }

    void show_caption_state(input::NonClientRegionKind kind, std::wstring_view state) {
        controls::Button button{nullptr};
        switch (kind) {
            case input::NonClientRegionKind::Minimize: button = minimize_button; break;
            case input::NonClientRegionKind::Maximize: button = maximize_button; break;
            case input::NonClientRegionKind::Close: button = close_button; break;
            default: return;
        }
        if (!button) return;
        bool const normal = state == L"Normal";
        std::wstring name{state};
        if (kind == input::NonClientRegionKind::Close && !normal) name = L"CloseButton" + name;
        xaml::VisualStateManager::GoToState(button, name, false);
        button.Opacity(active || !normal ? 1.0 : inactive_caption_opacity);
    }

    void show_maximized() {
        if (!maximize_button || !hwnd) return;
        bool const maximized = ::IsZoomed(hwnd) != 0;
        xaml::VisualStateManager::GoToState(
            maximize_button, maximized ? L"WindowStateMaximized" : L"WindowStateNormal", false);
        xaml::Automation::AutomationProperties::SetName(
            maximize_button, system_menu_text(hwnd, maximized ? SC_RESTORE : SC_MAXIMIZE));
    }

    void drop_title_bar() {
        if (!title_bar) return;
        title_bar_size_changed.revoke();
        title_bar.UnregisterPropertyChangedCallback(xaml::UIElement::VisibilityProperty(), title_bar_visibility);
        if (title_bar_background_property) {
            title_bar.UnregisterPropertyChangedCallback(title_bar_background_property, title_bar_background);
        }
        uint32_t at = 0;
        if (bar.Children().IndexOf(title_bar, at)) bar.Children().RemoveAt(at);
        title_bar = nullptr;
        title_bar_background_property = nullptr;
    }

    void set_title_bar(xaml::FrameworkElement const& element) {
        ensure_island();
        drop_title_bar();
        if (element) {
            ensure_bar();
            title_bar = element;
            controls::Grid::SetColumn(title_bar, 0);
            bar.Children().InsertAt(0, title_bar);

            title_bar_size_changed = title_bar.SizeChanged(winrt::auto_revoke, [this](auto&&, auto&&) { queue_regions(); });
            title_bar_visibility = title_bar.RegisterPropertyChangedCallback(
                xaml::UIElement::VisibilityProperty(), [this](auto&&, auto&&) { update_caption_mode(); });
            title_bar_background_property = background_property_of(title_bar);
            if (title_bar_background_property) {
                title_bar_background = title_bar.RegisterPropertyChangedCallback(
                    title_bar_background_property, [this](auto&&, auto&&) { copy_title_bar_background(); });
            }
            copy_title_bar_background();
        }
        update_caption_mode();
    }

    void copy_title_bar_background() {
        if (!caption_buttons) return;
        caption_buttons.Background(title_bar_background_property
                                      ? title_bar.GetValue(title_bar_background_property).try_as<xaml::Media::Brush>()
                                      : nullptr);
    }

    // Чьи кнопки окна сейчас: свои -- пока клиентская область под заголовком и
    // заголовок-элемент виден; иначе системные, как у Window.
    void update_caption_mode() {
        bool const ownButtons = extends_content_into_title_bar && title_bar &&
                                title_bar.Visibility() == xaml::Visibility::Visible;
        if (bar) {
            bar.Visibility(title_bar ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);
            caption_buttons.Visibility(ownButtons ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);
        }
        if (extends_content_into_title_bar) {
            windowing::AppWindowTitleBar const system = app_window_of().TitleBar();
            if (title_bar) {
                if (!system_buttons_height) system_buttons_height = system.PreferredHeightOption();
                system.PreferredHeightOption(windowing::TitleBarHeightOption::Collapsed);
            } else if (system_buttons_height) {
                system.PreferredHeightOption(*system_buttons_height);
                system_buttons_height.reset();
            }
        }
        queue_regions();
    }

    // Пересчёт после вёрстки: и размер элемента, и размер окна приходят раньше,
    // чем XAML их разложил.
    void queue_regions() {
        if (regions_queued || !hwnd) return;
        regions_queued = true;
        core::intrusive_ptr<Impl> const self{this};
        mud::DispatcherQueue::GetForCurrentThread().TryEnqueue(
            mud::DispatcherQueuePriority::Low, [self] { self->update_regions(); });
    }

    graphics::RectInt32 physical_rect_of(xaml::FrameworkElement const& element) const {
        auto const width = static_cast<float>(element.ActualWidth());
        auto const height = static_cast<float>(element.ActualHeight());
        xaml::XamlRoot const elementRoot = element.XamlRoot();
        if (width == 0 || height == 0 || !elementRoot) return {};
        winrt::Windows::Foundation::Rect const bounds =
            element.TransformToVisual(nullptr).TransformBounds({0, 0, width, height});
        // Масштаб острова, а не DPI окна, как у Window: остров увеличен сверх
        // DPI, и прямоугольник в физических пикселях считается тем масштабом,
        // каким его нарисовали.
        double const factor = elementRoot.RasterizationScale();
        return {static_cast<int32_t>(std::lround(bounds.X * factor)),
                static_cast<int32_t>(std::lround(bounds.Y * factor)),
                static_cast<int32_t>(std::lround(bounds.Width * factor)),
                static_cast<int32_t>(std::lround(bounds.Height * factor))};
    }

    void update_regions() {
        regions_queued = false;
        if (!hwnd || ::IsIconic(hwnd)) return;
        watch_xaml_root();

        bool const own = extends_content_into_title_bar && title_bar &&
                         title_bar.Visibility() == xaml::Visibility::Visible;

        // Caption -- как CWindowChrome::SetDragRegion у Window: убирается свой
        // прежний прямоугольник, ставится новый, чужие остаются.
        graphics::RectInt32 const wanted = own ? physical_rect_of(title_bar) : graphics::RectInt32{};
        if (!same_rect(wanted, caption) && (own || caption.Width != 0 || caption.Height != 0)) {
            winrt::com_array<graphics::RectInt32> const current =
                non_client_of().GetRegionRects(input::NonClientRegionKind::Caption);
            std::vector<graphics::RectInt32> rects;
            rects.reserve(current.size() + 1);
            for (graphics::RectInt32 const& rect : current) {
                if (!same_rect(rect, caption)) rects.push_back(rect);
            }
            if (wanted.Width > 0 && wanted.Height > 0) rects.push_back(wanted);
            non_client.SetRegionRects(input::NonClientRegionKind::Caption, rects);
            caption = wanted;
        }

        if (!own) {
            if (own_button_regions) {
                for (auto const kind : {input::NonClientRegionKind::Minimize, input::NonClientRegionKind::Maximize,
                                        input::NonClientRegionKind::Close}) {
                    non_client.ClearRegionRects(kind);
                }
                own_button_regions = false;
            }
            return;
        }

        own_button_regions = true;
        auto const apply = [this](input::NonClientRegionKind kind, graphics::RectInt32 rect) {
            winrt::com_array<graphics::RectInt32> const current = non_client_of().GetRegionRects(kind);
            if (current.size() == 1 && same_rect(current[0], rect)) return;
            non_client.SetRegionRects(kind, {rect});
        };
        apply(input::NonClientRegionKind::Minimize, physical_rect_of(minimize_button));
        apply(input::NonClientRegionKind::Maximize, physical_rect_of(maximize_button));
        apply(input::NonClientRegionKind::Close, physical_rect_of(close_button));
    }

    // ---- Задник картинкой, загруженной на ходу ----

    // Загрузка начинается в самом вызове, под токеном своего источника, сделанного
    // под `owner`; ждёт её задача, которую держит сама загрузка в background_load
    // (impl/background_load.h). Путь -- уже рядом с приложением.
    void load_background(BackgroundImage image, async::cancellation_token owner);

    // Устройство под поверхностями заменено: картинка задника, загруженная на
    // ходу, пуста вместе с поверхностью кэша -- она грузится снова, под тем же
    // токеном владельца, если фон с тех пор не сменили иначе.
    void device_replaced() noexcept {
        if (auto again = background_load.last()) load_background(std::move(again->request), std::move(again->owner));
    }

    // Последним: после всего, что трогает слушатель, -- и после самого слушателя,
    // которого называет указатель на член.
    RenderingDeviceWatch<&Impl::device_replaced> device_watch;
};

namespace {

CompositionWindow::Impl* state_of(HWND hwnd) noexcept {
    return reinterpret_cast<CompositionWindow::Impl*>(::GetWindowLongPtrW(hwnd, GWLP_USERDATA));
}

// Окна, которые живы сейчас. XAML считает только свои окна: цикл приложения
// кончается с последним из них, а CompositionWindow может остаться, -- и закрыть
// его, пока XAML жив, может только wxl, который его знает
// (CompositionWindow::close_all_at_exit). Список не владеет: ссылку держит само окно, до
// WM_NCDESTROY, где узел уходит из списка.
core::intrusive_list<CompositionWindow::Impl>& live_windows() noexcept {
    static core::intrusive_list<CompositionWindow::Impl> live;
    return live;
}

// Клиентский прямоугольник, обросший рамкой. Толщину отвечает само окно на
// WM_NCCALCSIZE -- тем же путём, каким Windows получает его клиентскую область.
// AdjustWindowRectEx считает рамку по стилю и не знает, что заголовок отдан
// клиенту (ExtendsContentIntoTitleBar правит ответ на WM_NCCALCSIZE), а стиль и
// DPI монитора, на котором окно сейчас, ответ учитывает сам. Меряется на
// нынешнем прямоугольнике окна, у свёрнутого -- на месте, куда оно вернётся.
RECT with_frame(HWND hwnd, RECT client) noexcept {
    RECT window{};
    if (::IsIconic(hwnd)) {
        WINDOWPLACEMENT placement{};
        placement.length = sizeof(placement);
        ::GetWindowPlacement(hwnd, &placement);
        window = placement.rcNormalPosition;
    } else {
        ::GetWindowRect(hwnd, &window);
    }
    RECT inner = window;
    ::SendMessageW(hwnd, WM_NCCALCSIZE, FALSE, reinterpret_cast<LPARAM>(&inner));

    client.left -= inner.left - window.left;
    client.top -= inner.top - window.top;
    client.right += window.right - inner.right;
    client.bottom += window.bottom - inner.bottom;
    return client;
}

// noexcept: исключение не должно пересекать обратный вызов Win32 -- раскрутка
// через кадры user32 не поддерживается, а на x64 вызов из ядра может его
// проглотить, -- так что сбой внутри -- std::terminate, а обработчик terminate,
// который ставит wWinMain, называет исключение. И для сообщений из очереди
// (WM_DROPFILES, WM_KEYDOWN) это остановка без Teardown; WM_EXITSIZEMOVE не из
// них -- его шлёт модальный цикл перемещения.
LRESULT CALLBACK window_procedure(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) noexcept {
    CompositionWindow::Impl* state = state_of(hwnd);
    switch (message) {
        case WM_ERASEBKGND:
            return 1;

        case WM_GETMINMAXINFO:
            // Нижний предел -- в размерах клиента; переводим в размеры окна
            // его рамкой. Сообщение приходит и при создании, до установки
            // USERDATA: тогда state == nullptr, предел не наложен -- окну и так
            // задан CW_USEDEFAULT.
            if (state && (state->min_size.width > 0 || state->min_size.height > 0)) {
                RECT const frame = with_frame(hwnd, {0, 0, state->min_size.width, state->min_size.height});
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
                state->client_size_changed.raise(
                    {{static_cast<int32_t>(LOWORD(lparam)), static_cast<int32_t>(HIWORD(lparam))},
                     state->scale()});
                // Максимизация и восстановление меняют место без перетаскивания
                // (WM_EXITSIZEMOVE не придёт) -- отмечаем их здесь. Растяжка за
                // рамку тоже сюда попадает, но сохранение места отложено, так что
                // лишние отметки во время тяги схлопнутся в одну запись.
                if (wparam == SIZE_MAXIMIZED || wparam == SIZE_RESTORED) {
                    state->geometry_changed.raise(Object::Impl::empty<Object>());
                }
                state->show_maximized();
                state->queue_regions();
            }
            break;

        case WM_MOVE:
            if (state) state->queue_regions();
            break;

        case WM_NCACTIVATE:
            if (state) state->show_active(wparam != FALSE);
            break;

        case WM_DPICHANGED:
            // Окно PerMonitorV2 само встаёт в прямоугольник, предложенный под
            // новый DPI, а увеличение острова пересчитывается от масштаба нового
            // экрана.
            if (state) {
                auto const* const suggested = reinterpret_cast<RECT const*>(lparam);
                ::SetWindowPos(hwnd, nullptr, suggested->left, suggested->top,
                               suggested->right - suggested->left, suggested->bottom - suggested->top,
                               SWP_NOZORDER | SWP_NOACTIVATE);
                state->apply_zoom();
                state->fill_picture();
                return 0;
            }
            break;

        case WM_ACTIVATE:
            // Пока остров показан, фокус ведётся, как у Microsoft.UI.Xaml.Window:
            // DefWindowProc поставил бы его на само окно, мимо XAML.
            if (state && state->on_activate(wparam)) return 0;
            break;

        case WM_EXITSIZEMOVE:
            // Конец перетаскивания рамки: и сдвиг, и растяжка кончаются здесь.
            if (state) state->geometry_changed.raise(Object::Impl::empty<Object>());
            return 0;

        case WM_KEYDOWN:
            // Клавиатура у чистой сцены доходит сюда (в отличие от указателя,
            // которого перехватывает InputSite острова). Отдаём код клавиши
            // обёрткой wxl -- тем же VirtualKey, что раздаёт XAML. Не return 0:
            // пусть DefWindowProc довершит своё (WM_CHAR, системные клавиши).
            if (state) state->key_down.raise(static_cast<VirtualKey>(static_cast<int32_t>(wparam)));
            break;

        case WM_DROPFILES:
            if (state && state->on_file_drop) {
                HDROP drop = reinterpret_cast<HDROP>(wparam);
                wchar_t path[MAX_PATH] = {};
                if (::DragQueryFileW(drop, 0, path, MAX_PATH)) {
                    state->on_file_drop(std::filesystem::path{path});
                }
                ::DragFinish(drop);
            }
            return 0;

        case WM_ENTERIDLE:
            // Модальный диалог или меню, открытые из продолжения корутины, крутят свой
            // цикл, пока разбор петли стоит в этом продолжении, и передачи с рабочего
            // потока лежат в канале без поста. На простое модального цикла разбор
            // подхватывается здесь, и дальше завершения приходят в этот цикл сами.
            // Петля делает то же своим хуком простоя, но тот зовётся только у потока на
            // переднем плане; диалог окна в фоне доходит сюда.
            async::sta_loop::run_pending();
            break;

        case WM_CLOSE:
            // Просьба закрыть: Closing может её отклонить, Closed -- сохранить по
            // дороге. Обработчик с модальным циклом может разобрать вложенный
            // WM_CLOSE и уничтожить окно раньше, чем кончится этот, -- состояние
            // держится здесь, и этот кончается вместе с окном.
            if (state) {
                core::intrusive_ptr<CompositionWindow::Impl> const held{state};
                WindowClosingEventArgs request;
                held->closing.raise(request);
                if (request.cancel() || !held->hwnd) return 0;

                held->closed.raise(Object::Impl::empty<Object>());
                if (!held->hwnd) return 0;
            }
            ::DestroyWindow(hwnd);
            return 0;

        case WM_DESTROY:
            // Своё окно закрылось -- его XAML отпускается, пока XAML жив, и,
            // пока идёт цикл приложения, гасится приложение: Application::Start
            // возвращается, и отрабатывает Teardown. Окно, которое закрывает
            // wxl при выходе (ShutdownStarting очереди XAML), приложения уже не
            // гасит: цикл кончился, и Exit() положил бы в очередь потока
            // WM_QUIT, которого не ждёт уже ни один цикл, -- ожидание петли
            // потока на выходе остановилось бы на нём.
            if (state) state->close_xaml();
            if (Application::loopRunning()) xaml::Application::Current().Exit();
            return 0;

        case WM_NCDESTROY:
            // Последнее сообщение окна: оно отпускает своё состояние. Живут ещё
            // хендлы -- состояние остаётся им, без окна под собой.
            if (state) {
                ::SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
                state->hwnd = nullptr;
                core::intrusive_list<CompositionWindow::Impl>::remove(core::as_not_null(state));
                intrusive_ptr_release(state);
            }
            break;
    }
    return ::DefWindowProcW(hwnd, message, wparam, lparam);
}

void ensure_class() {
    static bool const registered = [] {
        WNDCLASSEXW wc{sizeof(WNDCLASSEXW)};
        wc.lpfnWndProc = window_procedure;
        wc.hInstance = ::GetModuleHandleW(nullptr);
        wc.lpszClassName = class_name;
        // IDC_ARROW без UNICODE -- LPSTR, а LoadCursorW хочет LPCWSTR; значение
        // (номер ресурса в указателе) для A/W одно.
        wc.hCursor = ::LoadCursorW(nullptr, reinterpret_cast<LPCWSTR>(IDC_ARROW));
        // Иконка приложения -- ресурс 1, тот же, что показывает оболочка:
        // окно получает её от класса, а не ставит себе само. Нет ресурса --
        // нет и иконки, как было.
        wc.hIcon = static_cast<HICON>(::LoadImageW(wc.hInstance, MAKEINTRESOURCEW(1),
                                                  IMAGE_ICON, ::GetSystemMetrics(SM_CXICON),
                                                  ::GetSystemMetrics(SM_CYICON), LR_SHARED));
        wc.hIconSm = static_cast<HICON>(::LoadImageW(wc.hInstance, MAKEINTRESOURCEW(1),
                                                    IMAGE_ICON, ::GetSystemMetrics(SM_CXSMICON),
                                                    ::GetSystemMetrics(SM_CYSMICON), LR_SHARED));
        wc.hbrBackground = nullptr;   // без классовой кисти -- белому взяться неоткуда
        ::RegisterClassExW(&wc);
        return true;
    }();
    (void)registered;
}

}  // namespace

// ---- Выход: окна закрываются, пока XAML жив ---------------------------------

// Первое окно списка, пока он не пуст: обработчики Closing и Closed уносят из
// него и другие окна (закрывая их), и заводят новые -- обход копией не увидел бы
// ни того, ни другого. Состояние держится на время своего закрытия: окно, его
// державшее, отпускает его в WM_NCDESTROY, изнутри DestroyWindow.
void CompositionWindow::close_all_at_exit() noexcept {
    while (Impl* const front = live_windows().front()) {
        core::intrusive_ptr<Impl> const window{front};
        ::SendMessageW(window->hwnd, WM_CLOSE, 0, 0);
        // Отказ Closing на выходе не в счёт: окно, оставленное жить, пережило бы
        // XAML.
        if (window->hwnd) ::DestroyWindow(window->hwnd);
        // WM_NCDESTROY уносит узел из списка; оставшийся -- окно, которое не
        // уничтожилось, и обход стоял бы на нём вечно.
        if (window->linked()) {
            core::abort("wxl: a CompositionWindow outlived DestroyWindow while the windows "
                        "were closed at exit");
        }
    }
}

bool CompositionWindow::all_closed() noexcept {
    return live_windows().empty();
}

// ---- Сцена: задний фон и собственные визуалы приложения --------------------

Compositor CompositionWindow::compositor() const {
    return Object::Impl::wrap<Compositor>(impl_->compositor);
}

ContainerVisual CompositionWindow::contentVisual() const {
    return Object::Impl::wrap<ContainerVisual>(impl_->backdrop);
}

SizeInt32 CompositionWindow::clientSize() const { return impl_->client_size().size; }

float CompositionWindow::rasterizationScale() const {
    // DPI окна, а не системного: окно может стоять на мониторе с другим
    // масштабом, и физические пиксели поверхности считаются по нему.
    return impl_->scale();
}

void CompositionWindow::background(Color color) const {
    impl_->background_load.cancel();
    impl_->picture = nullptr;
    impl_->backdrop.Brush(impl_->compositor.CreateColorBrush(as_winrt(color)));
}

void CompositionWindow::clearBackground() const {
    // Спрайт без кисти ничего не рисует, но остаётся корнем сцены и
    // контейнером визуалов приложения -- дерево не меняется, меняется только
    // то, что у него нет своей заливки.
    impl_->background_load.cancel();
    impl_->picture = nullptr;
    impl_->backdrop.Brush(nullptr);
}

void CompositionWindow::background(std::filesystem::path const& image) const {
    background(BackgroundImage{image});
}

void CompositionWindow::background(BackgroundImage const& image) const {
    // Синхронный декод: первый (стартовый) экран показывается только уже
    // загруженным -- окно до того скрыто. Смены фона на ходу идут асинхронно
    // через Win2D/TextureCache (backgroundAsync).
    std::filesystem::path const file = Application::makeAbsolute(image.path);

    winrt::com_ptr<IWICImagingFactory> wic;
    winrt::check_hresult(::CoCreateInstance(CLSID_WICImagingFactory, nullptr,
                                            CLSCTX_INPROC_SERVER, __uuidof(IWICImagingFactory),
                                            wic.put_void()));
    winrt::com_ptr<IWICBitmapDecoder> decoder;
    winrt::check_hresult(wic->CreateDecoderFromFilename(file.c_str(), nullptr, GENERIC_READ,
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

    // Обёртку держим именованной: get_typed отдаёт сырой winrt внутри неё, а не
    // копию, и временная обёртка умерла бы прежде, чем кисть скопируют.
    CompositionSurfaceBrush wrapped = surface.brush();
    impl_->background_load.cancel();
    impl_->show_picture(*Object::Impl::get_typed<CompositionSurfaceBrush>(wrapped), image.fill,
                        image.color);
}

void CompositionWindow::background(DrawingSurface const& surface) const {
    // Обёртку держим именованной: get_typed отдаёт сырой winrt внутри неё, а не
    // копию, а временная обёртка surface.brush() умерла бы концом выражения --
    // и ссылка повисла бы. (Цвет и картинку выше это правило тоже касается.)
    CompositionSurfaceBrush wrapped = surface.brush();
    muc::CompositionSurfaceBrush const& brush =
        *Object::Impl::get_typed<CompositionSurfaceBrush>(wrapped);
    // UniformToFill: поверхность-задник тянется под окно, как картинка-заставка,
    // и в просвете быстрой растяжки кроет окно, а не сквозит.
    brush.Stretch(muc::CompositionStretch::UniformToFill);
    impl_->background_load.cancel();
    impl_->picture = nullptr;
    impl_->backdrop.Brush(brush);
}

void CompositionWindow::backgroundAsync(std::filesystem::path const& image,
                                        async::cancellation_token token) const {
    backgroundAsync(BackgroundImage{image}, std::move(token));
}

void CompositionWindow::backgroundAsync(BackgroundImage const& image,
                                        async::cancellation_token token) const {
    // Кэш держит текстуру, так что тот же путь во второй раз сменит задник без
    // загрузки.
    impl_->load_background({Application::makeAbsolute(image.path), image.fill, image.color},
                           std::move(token));
}

void CompositionWindow::Impl::load_background(BackgroundImage image, async::cancellation_token owner) {
    // Под отменённым токеном не делается ничего, как у getAsync и у всякой
    // операции с токеном: ни загрузки, ни отмены прежней, ни отказа.
    if (owner.is_canceled()) return;
    // Окно уничтожено -- показывать загруженное негде, а загрузка, начатая после
    // его конца, не отменилась бы ничем, кроме токена владельца: RO_E_CLOSED, как
    // у острова. Замена устройства сюда после конца не приходит: close_xaml забыл,
    // что просили.
    if (xaml_closed) throw winrt::hresult_error{RO_E_CLOSED, L"wxl::CompositionWindow: окно уже закрыто"};
    async::cancellation_token const load = background_load.start(std::move(owner), image);
    // Окно держит ожидание полем (в загрузке), и оно уходит раньше окна: состояние
    // берётся простым указателем.
    background_load.hold(load, impl::show_when_loaded(
        texture_cache->getAsync(image.path, load), load,
        [state = this, fill = image.fill, color = image.color](CompositionDrawingSurface const& surface) {
            // Именованный промежуток: обёртка -> сырая поверхность (одно
            // преобразование), сырая поверхность -> ICompositionSurface у
            // CreateSurfaceBrush (второе); в одном выражении их было бы два
            // подряд, чего аргументу не дают.
            muc::CompositionDrawingSurface const& raw =
                *Object::Impl::get_typed<CompositionDrawingSurface>(surface);
            state->show_picture(state->compositor.CreateSurfaceBrush(raw), fill, color);
        }));
}

// ---- CompositionWindow ----------------------------------------------------

CompositionWindow::CompositionWindow() : impl_{new Impl(), false} {
    ensure_class();

    impl_->hwnd = ::CreateWindowExW(WS_EX_NOREDIRECTIONBITMAP, class_name, L"", WS_OVERLAPPEDWINDOW,
                                     CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                                     nullptr, nullptr, ::GetModuleHandleW(nullptr), nullptr);
    // Пустой hwnd -- отказ системы, и он бросается здесь, до своей ссылки окна и
    // до списка живых окон: состояние без окна в список не попадает -- иначе
    // закрытие окон при выходе стояло бы на нём и остановило бы процесс, назвав
    // причиной окно, пережившее DestroyWindow.
    if (!impl_->hwnd) winrt::throw_last_error();
    // Своя ссылка окна на состояние -- до WM_NCDESTROY: оконная процедура
    // находит его по USERDATA, и отпущенный последний хендл не освободит его
    // под живым окном.
    intrusive_ptr_add_ref(impl_.get());
    ::SetWindowLongPtrW(impl_->hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(impl_.get()));
    live_windows().push_back(core::as_not_null(impl_.get()));

    // Сцена: свой Microsoft.UI-композитор и один задний визуал -- он же корень
    // острова, он же фон. Подключаем его к нашему HWND attached-мостом -- без
    // дочернего окна рисования (мост InputSiteWindowClass -- только под ввод).
    impl_->compositor = muc::Compositor{};
    impl_->backdrop = impl_->compositor.CreateSpriteVisual();

    // Кэш текстур на том же компоновщике -- под асинхронные задники -- и держатель
    // замены устройства под поверхностями: на ней задник, загруженный на ходу,
    // грузится снова.
    impl_->texture_cache.emplace(Object::Impl::wrap<Compositor>(impl_->compositor));
    impl_->device_watch.attach(Object::Impl::wrap<Compositor>(impl_->compositor), *impl_);

    impl_->scene_island = content::ContentIsland::Create(impl_->backdrop);
    impl_->scene_bridge = content::DesktopAttachedSiteBridge::CreateFromWindowId(
        mud::DispatcherQueue::GetForCurrentThread(), window_id_of(impl_->hwnd));
    impl_->scene_bridge.Connect(impl_->scene_island);

    // Ввод у чистой сцены расщеплён. Клавиатура доходит до верхнего WndProc
    // (WM_KEYDOWN -- проверено пробой), а указатель перехватывает InputSite
    // острова, и его берём через InputPointerSource острова. InputKeyboardSource
    // не заводим: его GetForIsland на attached-острове падал AV, а клавиатуре он
    // и не нужен. Подписки простые, без auto_revoke: источник живёт в состоянии
    // столько же, сколько оно само. Обработчики noexcept: исключение подписчика
    // делегат cppwinrt превратил бы в HRESULT и потерял, а список события так и
    // остался бы посреди вызова, -- так что сбой внутри -- std::terminate.
    Impl* const impl = impl_.get();
    impl->pointer_input = input::InputPointerSource::GetForIsland(impl->scene_island);
    impl->pointer_input.PointerPressed(
        [impl](input::InputPointerSource const&, input::PointerEventArgs const& args) noexcept {
            impl->pointer_pressed.raise(Object::Impl::wrap<PointerPoint>(args.CurrentPoint()));
        });
    impl->pointer_input.PointerMoved(
        [impl](input::InputPointerSource const&, input::PointerEventArgs const& args) noexcept {
            impl->pointer_moved.raise(Object::Impl::wrap<PointerPoint>(args.CurrentPoint()));
        });
    impl->pointer_input.PointerReleased(
        [impl](input::InputPointerSource const&, input::PointerEventArgs const& args) noexcept {
            impl->pointer_released.raise(Object::Impl::wrap<PointerPoint>(args.CurrentPoint()));
        });
    impl->pointer_input.PointerWheelChanged(
        [impl](input::InputPointerSource const&, input::PointerEventArgs const& args) noexcept {
            impl->pointer_wheel_changed.raise(Object::Impl::wrap<PointerPoint>(args.CurrentPoint()));
        });

    RECT client{};
    ::GetClientRect(impl_->hwnd, &client);
    impl_->resize(static_cast<float>(client.right), static_cast<float>(client.bottom));
}

CompositionWindow::CompositionWindow(hstring_param const& title, SizeInt32 minSize) : CompositionWindow() {
    this->title(title);
    this->minSize(minSize);
}

CompositionWindow::~CompositionWindow() = default;
CompositionWindow::CompositionWindow(CompositionWindow const&) noexcept = default;
CompositionWindow::CompositionWindow(CompositionWindow&&) noexcept = default;
CompositionWindow& CompositionWindow::operator=(CompositionWindow const&) noexcept = default;
CompositionWindow& CompositionWindow::operator=(CompositionWindow&&) noexcept = default;

void CompositionWindow::content(UIElement const& chrome) const {
    if (!chrome) {
        impl_->show_page(nullptr);
        return;
    }
    xaml::UIElement const& raw = *Object::Impl::get_typed<UIElement>(chrome);
    impl_->show_page(raw);
}

void CompositionWindow::systemBackdrop(SystemBackdrop const& value) const {
    // Материал заводится вместе с островом: заданный раньше острова ждёт его.
    impl_->system_backdrop = value ? *Object::Impl::get_typed<SystemBackdrop>(value) : xaml::Media::SystemBackdrop{nullptr};
    if (impl_->chrome) impl_->chrome.SystemBackdrop(impl_->system_backdrop);
}

UIElement CompositionWindow::content() const {
    return Object::Impl::wrap<UIElement>(impl_->page);
}

Compositor CompositionWindow::chromeCompositor() const {
    // Композитор XAML-потока: его отдаёт хэндофф-визуал любого элемента. На потоке
    // XAML он один -- тот же, на котором окажется содержимое, показанное через
    // content(). Берём у одноразового Grid.
    controls::Grid const probe;
    return Object::Impl::wrap<Compositor>(
        xaml::Hosting::ElementCompositionPreview::GetElementVisual(probe).Compositor());
}

void CompositionWindow::hideContent() const {
    // Прячем остров и снимаем с него содержимое: без оснастки ввод должен
    // идти сцене, а пустой, но показанный остров перехватывал бы его над собой.
    if (impl_->chrome) {
        impl_->show_page(nullptr);
        // Фокус, оставшийся в спрятанном острове, увёл бы туда и клавиши сцены.
        if (HWND const focus = ::GetFocus(); focus && impl_->island_owns(focus)) ::SetFocus(impl_->hwnd);
        impl_->chrome.SiteBridge().Hide();
        impl_->content_shown = false;
    }
}

void CompositionWindow::zoomFactor(double value) const { impl_->set_zoom_factor(value); }

void CompositionWindow::attachZoom(core::intrusive_ptr<AppZoom> zoom) const {
    if (impl_->zoom_factor_token) impl_->app_zoom->zoomFactor().remove_change(impl_->zoom_factor_token);
    impl_->app_zoom = std::move(zoom);
    // Состояние снимает эту подписку в деструкторе.
    // Масштаб, с которым эффект пришёл (восстановленный приложением), --
    // без индикатора: его никто не регулировал. Индикатор -- на каждое
    // изменение после присоединения.
    impl_->zoom_factor_token = impl_->app_zoom->zoomFactor()
                             .on_change([impl = impl_.get()](double const& value) noexcept {
                                 impl->set_zoom_factor(value);
                                 impl->show_zoom_indicator(value);
                             });
    impl_->set_zoom_factor(impl_->app_zoom->zoomFactor().get());
    impl_->ensure_island();
    impl_->ensure_zoom_sensor();
    impl_->add_zoom_handlers();
}

double CompositionWindow::zoomFactor() const { return impl_->zoom_factor; }

// ---- Заголовок окна ---------------------------------------------------------

void CompositionWindow::extendsContentIntoTitleBar(bool value) const {
    // Флаг до вызова: ExtendsContentIntoTitleBar двигает рамку синхронно, и
    // WM_SIZE, который приходит изнутри него, уже пересчитывает области.
    impl_->extends_content_into_title_bar = value;

    windowing::AppWindowTitleBar const titleBar = impl_->app_window_of().TitleBar();
    titleBar.ExtendsContentIntoTitleBar(value);

    // Под системными кнопками окна видна своя полоса -- их фон прозрачный. Как
    // у Window, один раз за жизнь окна: цвет, который приложение поставило
    // позже, повторное включение не затирает.
    if (value && !impl_->caption_buttons_transparent) {
        impl_->caption_buttons_transparent = true;
        winrt::Windows::Foundation::IReference<winrt::Windows::UI::Color> const transparent{
            winrt::Windows::UI::Color{0x00, 0xFF, 0xFF, 0xFF}};
        titleBar.ButtonBackgroundColor(transparent);
        titleBar.ButtonInactiveBackgroundColor(transparent);
    }

    // Рамка пересчитывается сразу: окно, созданное скрытым, иначе показалось
    // бы с клиентской областью прежнего размера.
    ::SetWindowPos(impl_->hwnd, nullptr, 0, 0, 0, 0,
                   SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_NOACTIVATE |
                       SWP_FRAMECHANGED);
    impl_->update_caption_mode();
}

bool CompositionWindow::extendsContentIntoTitleBar() const {
    return impl_->extends_content_into_title_bar;
}

void CompositionWindow::titleBar(UIElement const& value) const {
    if (!value) {
        impl_->set_title_bar(nullptr);
        return;
    }
    xaml::UIElement const& raw = *Object::Impl::get_typed<UIElement>(value);
    impl_->set_title_bar(raw.as<xaml::FrameworkElement>());
}

AppWindow CompositionWindow::appWindow() const {
    return Object::Impl::wrap<AppWindow>(impl_->app_window_of());
}

// ---- Само окно ----------------------------------------------------------------

void CompositionWindow::title(hstring_param const& value) const {
    // The text is terminated -- the parameter's contract -- so the pointer goes to
    // Windows as it lies.
    ::SetWindowTextW(impl_->hwnd, value.text().wc_str());
}

hstring CompositionWindow::title() const {
    int const length = ::GetWindowTextLengthW(impl_->hwnd);
    core::sta_u16string text(static_cast<std::size_t>(length) + 1, u'\0');
    ::GetWindowTextW(impl_->hwnd, reinterpret_cast<wchar_t*>(text.data()), length + 1);
    return hstring{std::u16string_view{text.data(), static_cast<std::size_t>(length)}};
}

void CompositionWindow::minSize(SizeInt32 const& value) const { impl_->min_size = value; }

void CompositionWindow::activate() const {
    ::ShowWindow(impl_->hwnd, SW_SHOW);
    ::UpdateWindow(impl_->hwnd);
}

void CompositionWindow::resize(SizeInt32 size) const {
    ::SetWindowPos(impl_->hwnd, nullptr, 0, 0, size.width, size.height,
                   SWP_NOMOVE | SWP_NOZORDER | SWP_NOOWNERZORDER);
}

namespace {

// Монитор, на котором окно стоит сейчас, за вычетом панели задач.
RECT work_area_of(HWND hwnd) noexcept {
    MONITORINFO monitor{};
    monitor.cbSize = sizeof(monitor);
    ::GetMonitorInfoW(::MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &monitor);
    return monitor.rcWork;
}

}  // namespace

void CompositionWindow::centreWithClientSize(SizeInt32 client) const {
    RECT const frame = with_frame(impl_->hwnd, {0, 0, client.width, client.height});

    int const width = frame.right - frame.left;
    int const height = frame.bottom - frame.top;

    RECT const work = work_area_of(impl_->hwnd);

    ::SetWindowPos(impl_->hwnd, nullptr, work.left + ((work.right - work.left) - width) / 2,
                   work.top + ((work.bottom - work.top) - height) / 2, width, height,
                   SWP_NOZORDER | SWP_NOOWNERZORDER);
}

SizeInt32 CompositionWindow::maxClientSize() const {
    RECT const work = work_area_of(impl_->hwnd);

    // Пустой клиент, обросший рамкой, -- это и есть одна рамка: слева-справа и
    // сверху-снизу вместе с заголовком.
    RECT const frame = with_frame(impl_->hwnd, {0, 0, 0, 0});

    return {(work.right - work.left) - (frame.right - frame.left),
            (work.bottom - work.top) - (frame.bottom - frame.top)};
}

HWND__* CompositionWindow::handle() const { return impl_->hwnd; }

DispatcherQueue CompositionWindow::dispatcherQueue() const {
    return Object::Impl::wrap<DispatcherQueue>(mud::DispatcherQueue::GetForCurrentThread());
}

void CompositionWindow::placement(hstring_param const& saved) const {
    // Та же строка, что у генерируемого Window; разбор формата и подгонку к
    // сегодняшним мониторам берём общими -- impl::parse_placement и
    // fit_placement_to_displays из WindowNative.cpp.
    auto const wanted = impl::parse_placement(saved.text().view());
    if (!wanted) {
        return;  // пусто или мусор: окно остаётся там, где создано
    }

    WINDOWPLACEMENT placement{};
    placement.length = sizeof(placement);
    if (!::GetWindowPlacement(impl_->hwnd, &placement)) {
        return;
    }
    placement.rcNormalPosition = impl::fit_placement_to_displays(wanted->restore);
    placement.showCmd =
        wanted->state == impl::placement_state::maximized ? SW_SHOWMAXIMIZED : SW_SHOWNORMAL;
    ::SetWindowPlacement(impl_->hwnd, &placement);

    // Полный экран после восстановления места, не вместо: fullScreen(true)
    // запомнит только что выставленное место как то, куда возвращаться, и
    // накроет монитор.
    if (wanted->state == impl::placement_state::full_screen) {
        fullScreen(true);
    }
}

hstring CompositionWindow::placement() const {
    WINDOWPLACEMENT placement{};
    placement.length = sizeof(placement);
    if (!::GetWindowPlacement(impl_->hwnd, &placement)) {
        return {};
    }

    // Под нашим Win32-накрытием rcNormalPosition уже равно прямоугольнику во
    // весь монитор (SetWindowPos сдвинул нормальное место), поэтому в полном
    // экране прямоугольник возврата берём из сохранённого на входе; в прочих
    // состояниях верим окну.
    RECT const rc = impl_->full_screen ? impl_->saved_placement.rcNormalPosition
                                       : placement.rcNormalPosition;
    impl::placement_state const state =
        impl_->full_screen                      ? impl::placement_state::full_screen
        : placement.showCmd == SW_SHOWMAXIMIZED ? impl::placement_state::maximized
                                                : impl::placement_state::normal;

    std::u16string text{impl::name_of(state)};
    for (long const value : {rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top}) {
        text += u' ';
        wxl::core::append_number(text, value);
    }
    return hstring{text};
}

bool CompositionWindow::fullScreen() const { return impl_->full_screen; }

void CompositionWindow::fullScreen(bool on) const {
    if (on == impl_->full_screen) {
        return;
    }
    HWND const hwnd = impl_->hwnd;
    if (on) {
        // Приём Реймонда Чена: запомнить место и стиль, снять рамку и накрыть
        // монитор целиком. WM_SIZE от SetWindowPos тянет за собой сцену и
        // остров синхронно -- отставать нечему.
        impl_->saved_placement.length = sizeof(impl_->saved_placement);
        ::GetWindowPlacement(hwnd, &impl_->saved_placement);
        impl_->saved_style = ::GetWindowLongPtrW(hwnd, GWL_STYLE);

        MONITORINFO monitor{sizeof(monitor)};
        ::GetMonitorInfoW(::MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY), &monitor);
        ::SetWindowLongPtrW(hwnd, GWL_STYLE,
                            impl_->saved_style & ~static_cast<LONG_PTR>(WS_OVERLAPPEDWINDOW));
        ::SetWindowPos(hwnd, HWND_TOP, monitor.rcMonitor.left, monitor.rcMonitor.top,
                       monitor.rcMonitor.right - monitor.rcMonitor.left,
                       monitor.rcMonitor.bottom - monitor.rcMonitor.top,
                       SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    } else {
        ::SetWindowLongPtrW(hwnd, GWL_STYLE, impl_->saved_style);
        ::SetWindowPlacement(hwnd, &impl_->saved_placement);
        ::SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                       SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    }
    impl_->full_screen = on;
}

void CompositionWindow::darkFrame(bool on) const { impl::frame_dark(impl_->hwnd, on); }

void CompositionWindow::captionColor(Color caption, Color text) const {
    // DWM берёт COLORREF -- 0x00BBGGRR, без альфы.
    auto const colorref = [](Color color) {
        return static_cast<uint32_t>(color.R | (color.G << 8) | (color.B << 16));
    };
    impl::frame_caption(impl_->hwnd, colorref(caption), colorref(text));
}

void CompositionWindow::close() const {
    // PostMessage, а не Send: просьба встаёт в очередь, как от системного
    // крестика, и разбирается тем же WM_CLOSE (Closing -> Closed ->
    // DestroyWindow -> WM_DESTROY). Без окна hwnd пуст, а PostMessage с пустым
    // окном послал бы WM_CLOSE самому потоку.
    if (impl_->hwnd) ::PostMessageW(impl_->hwnd, WM_CLOSE, 0, 0);
}

void CompositionWindow::acceptFileDrops(std::function<void(std::filesystem::path)> handler) const {
    impl_->on_file_drop = std::move(handler);
    ::DragAcceptFiles(impl_->hwnd, TRUE);
}

// ---- События ----------------------------------------------------------------

EventToken CompositionWindow::add_onClosing(EventHandler<WindowClosingEventArgs> const& handler) const {
    return impl_->closing.add(impl::next_event_token(), handler);
}
bool CompositionWindow::remove_onClosing(EventToken& token) const {
    return impl_->closing.remove(token);
}

EventToken CompositionWindow::add_onClosed(EventHandler<Object> const& handler) const {
    return impl_->closed.add(impl::next_event_token(), handler);
}
bool CompositionWindow::remove_onClosed(EventToken& token) const {
    return impl_->closed.remove(token);
}

EventToken CompositionWindow::add_onGeometryChanged(EventHandler<Object> const& handler) const {
    return impl_->geometry_changed.add(impl::next_event_token(), handler);
}
bool CompositionWindow::remove_onGeometryChanged(EventToken& token) const {
    return impl_->geometry_changed.remove(token);
}

EventToken CompositionWindow::add_onClientSizeChanged(EventHandler<ClientSize> const& handler) const {
    return impl_->client_size_changed.add(impl::next_event_token(), handler);
}
bool CompositionWindow::remove_onClientSizeChanged(EventToken& token) const {
    return impl_->client_size_changed.remove(token);
}

EventToken CompositionWindow::add_onKeyDown(EventHandler<VirtualKey> const& handler) const {
    return impl_->key_down.add(impl::next_event_token(), handler);
}
bool CompositionWindow::remove_onKeyDown(EventToken& token) const {
    return impl_->key_down.remove(token);
}

EventToken CompositionWindow::add_onPointerPressed(EventHandler<PointerPoint> const& handler) const {
    return impl_->pointer_pressed.add(impl::next_event_token(), handler);
}
bool CompositionWindow::remove_onPointerPressed(EventToken& token) const {
    return impl_->pointer_pressed.remove(token);
}

EventToken CompositionWindow::add_onPointerMoved(EventHandler<PointerPoint> const& handler) const {
    return impl_->pointer_moved.add(impl::next_event_token(), handler);
}
bool CompositionWindow::remove_onPointerMoved(EventToken& token) const {
    return impl_->pointer_moved.remove(token);
}

EventToken CompositionWindow::add_onPointerReleased(EventHandler<PointerPoint> const& handler) const {
    return impl_->pointer_released.add(impl::next_event_token(), handler);
}
bool CompositionWindow::remove_onPointerReleased(EventToken& token) const {
    return impl_->pointer_released.remove(token);
}

EventToken CompositionWindow::add_onPointerWheelChanged(EventHandler<PointerPoint> const& handler) const {
    return impl_->pointer_wheel_changed.add(impl::next_event_token(), handler);
}
bool CompositionWindow::remove_onPointerWheelChanged(EventToken& token) const {
    return impl_->pointer_wheel_changed.remove(token);
}

void CompositionWindow::onClosed(std::function<void()> handler) const {
    add_onClosed([handler = std::move(handler)](Object const&, Object const&) { handler(); });
}

void CompositionWindow::onGeometryChanged(std::function<void()> handler) const {
    add_onGeometryChanged([handler = std::move(handler)](Object const&, Object const&) { handler(); });
}

void CompositionWindow::onClientSizeChanged(std::function<void(SizeInt32, float)> handler) const {
    add_onClientSizeChanged([handler = std::move(handler)](Object const&, ClientSize const& client) {
        handler(client.size, client.scale);
    });
}

void CompositionWindow::onKeyDown(std::function<void(VirtualKey)> handler) const {
    add_onKeyDown([handler = std::move(handler)](Object const&, VirtualKey const& key) { handler(key); });
}

void CompositionWindow::onPointerPressed(std::function<void(PointerPoint const&)> handler) const {
    add_onPointerPressed(
        [handler = std::move(handler)](Object const&, PointerPoint const& point) { handler(point); });
}

void CompositionWindow::onPointerMoved(std::function<void(PointerPoint const&)> handler) const {
    add_onPointerMoved(
        [handler = std::move(handler)](Object const&, PointerPoint const& point) { handler(point); });
}

void CompositionWindow::onPointerReleased(std::function<void(PointerPoint const&)> handler) const {
    add_onPointerReleased(
        [handler = std::move(handler)](Object const&, PointerPoint const& point) { handler(point); });
}

void CompositionWindow::onPointerWheel(std::function<void(PointerPoint const&)> handler) const {
    add_onPointerWheelChanged(
        [handler = std::move(handler)](Object const&, PointerPoint const& point) { handler(point); });
}

}  // namespace wxl

// ---- Сцена для эффектов ----------------------------------------------------

namespace wxl::impl {

scene scene_of(winrt::Microsoft::UI::WindowId const window) noexcept {
    HWND const hwnd = reinterpret_cast<HWND>(window.Value);
    // Только своё окно: у чужого в USERDATA что угодно.
    wchar_t name[sizeof(class_name) / sizeof(wchar_t) + 1]{};
    if (::GetClassNameW(hwnd, name, static_cast<int>(std::size(name))) == 0) return {};
    if (std::wstring_view{name} != class_name) return {};
    CompositionWindow::Impl const* const state = state_of(hwnd);
    if (!state || !state->backdrop) return {};
    return {state->compositor, state->backdrop};
}

}  // namespace wxl::impl

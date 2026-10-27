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

namespace {

// Окна, которые живы сейчас (определение -- после состояния окна).
core::intrusive_list<CompositionWindow::Impl>& live_windows() noexcept;

}  // namespace

// Общее состояние окна: сырой winrt внутри, обёртки -- только на границе.
//
// Целое, когда конструктор вернулся: окно, его компоновщик и сцена, ввод сцены, кэш текстур и
// держатель замены устройства строятся в списке инициализации, из готового окна. Части, которые
// появляются позже, -- остров XAML, неклиентский ввод, присоединённый масштаб, хватка окна за
// состояние -- по одному владеющему полю на часть, построенные целиком, когда понадобились, и
// снимающие своё сами.
//
// Узел списка живых окон (live_windows): в нём состояние, пока у него есть окно (window_hold).
struct CompositionWindow::Impl : core::refcounted, core::intrusive_list_node<Impl> {
    explicit Impl(HWND created);

    // Окно -- до WM_NCDESTROY: тогда его отпускает хватка (window_hold), и дальше состояние
    // живёт без окна под собой, пока его держат хендлы приложения.
    impl::owned_window window;
    muc::Compositor const compositor;
    // Один визуал на всю сцену: он и корень острова, и задний фон (картинка или
    // цвет). Его же отдаёт contentVisual(): собственные визуалы приложения висят
    // на нём детьми -- контейнер над контейнером не нужен.
    muc::SpriteVisual const backdrop;
    // Кэш текстур поверх compositor -- под асинхронные задники (Win2D).
    TextureCache texture_cache;
    // Ввод со сцены, когда остров спрятан или его ещё нет: источники ввода её
    // ContentIsland. Источники держим живыми -- на них висят подписки.
    content::ContentIsland const scene_island;
    content::DesktopAttachedSiteBridge const scene_bridge;
    input::InputPointerSource const pointer_input;   // указатель сцены; клавиатура -- через WndProc

    // Настройки окна -- то, что приложение ставит, когда хочет.
    xaml::Media::SystemBackdrop system_backdrop{nullptr};      // материал острова, если задан до него
    std::function<void(std::filesystem::path)> on_file_drop;
    SizeInt32 min_size{};                // нижний предел клиента (WM_GETMINMAXINFO)
    WINDOWPLACEMENT saved_placement{};   // место до полноэкранного -- куда вернуться
    LONG_PTR saved_style{};              // стиль окна до полноэкранного
    bool full_screen{false};

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

    // ---- Масштаб, который присоединил ZoomEffect ----

    // Клавиши и колесо острова двигают модель, остров следует за её масштабом. Колесо копится
    // до щелчка (WHEEL_DELTA): точный тачпад шлёт его мелкими долями. Подписку на модель
    // держит держатель и снимает сам.
    struct zoom_attachment : core::noncopyable {
        zoom_attachment(core::intrusive_ptr<AppZoom> attached, Impl& window_state)
            : model{std::move(attached)}, state{window_state}, watch{model->zoomFactor(), *this} {}

        // Индикатор -- на каждое изменение после присоединения; масштаб, с которым эффект
        // пришёл, ставит attachZoom, без индикатора: его никто не регулировал.
        void zoom_changed(double const& value) noexcept {
            state.set_zoom_factor(value);
            state.show_zoom_indicator(value);
        }

        core::intrusive_ptr<AppZoom> const model;
        Impl& state;
        int32_t accumulated_wheel_delta{0};
        core::change_watch<core::observable<double const>, &zoom_attachment::zoom_changed> watch;
    };

    std::unique_ptr<zoom_attachment> zoom;

    // ---- Остров XAML и то, что живёт только с ним ----

    // Смена масштаба острова -- DPI или увеличение -- меняет прямоугольники в физических
    // пикселях, а логический размер элементов остаётся, и SizeChanged не придёт: XamlRoot
    // острова и подписка на его смену.
    struct root_watch : core::noncopyable {
        root_watch(xaml::XamlRoot watched, Impl& state)
            : root{std::move(watched)},
              changed{root.Changed([&state](auto&&, auto&&) { state.queue_regions(); })} {}

        ~root_watch() { root.Changed(changed); }

        xaml::XamlRoot const root;
        winrt::event_token const changed;
    };

    // Строка заголовка: элемент приложения | кнопки окна. Заводится с первым заголовком.
    struct caption_bar : core::noncopyable {
        caption_bar(Impl& state, controls::Grid const& root)
            : grid{},
              buttons{},
              minimize{state.caption_button(L"M 0 0 L 10 0", SC_MINIMIZE)},
              maximize{state.caption_button(nullptr, SC_MAXIMIZE)},
              close{state.caption_button(L"M 0 0 L 9 9 M 9 0 L 0 9", SC_CLOSE)} {
            for (auto const width : {xaml::GridLengthHelper::FromValueAndType(1, xaml::GridUnitType::Star),
                                     xaml::GridLengthHelper::Auto()}) {
                controls::ColumnDefinition column;
                column.Width(width);
                grid.ColumnDefinitions().Append(column);
            }
            controls::Grid::SetRow(grid, 0);
            root.Children().Append(grid);

            buttons.Orientation(controls::Orientation::Horizontal);
            buttons.VerticalAlignment(xaml::VerticalAlignment::Stretch);
            controls::Grid::SetColumn(buttons, 1);

            HWND const window = state.window.get();
            xaml::Automation::AutomationProperties::SetName(minimize, system_menu_text(window, SC_MINIMIZE));
            xaml::Automation::AutomationProperties::SetName(close, system_menu_text(window, SC_CLOSE));
            maximize.Loaded([&state](auto&&, auto&&) { state.show_maximized(); });
            for (controls::Button const& button : {minimize, maximize, close}) {
                buttons.Children().Append(button);
            }
            buttons.SizeChanged([&state](auto&&, auto&&) { state.queue_regions(); });
            grid.Children().Append(buttons);
        }

        controls::Grid const grid;
        controls::StackPanel const buttons;
        controls::Button const minimize;
        controls::Button const maximize;
        controls::Button const close;
    };

    // Заголовок-элемент приложения в строке заголовка и обратные вызовы на нём. Элемент, если
    // его держит приложение, переживает окно -- вызовы снимает держатель, уходя.
    struct title_hold : core::noncopyable {
        title_hold(xaml::FrameworkElement given, Impl& state, controls::Grid const& row)
            : element{std::move(given)},
              bar{row},
              size_changed{element.SizeChanged(winrt::auto_revoke,
                                               [&state](auto&&, auto&&) { state.queue_regions(); })},
              visibility{element.RegisterPropertyChangedCallback(
                  xaml::UIElement::VisibilityProperty(), [&state](auto&&, auto&&) { state.update_caption_mode(); })},
              background_property{background_property_of(element)},
              background{background_property
                             ? element.RegisterPropertyChangedCallback(
                                   background_property, [&state](auto&&, auto&&) { state.copy_title_bar_background(); })
                             : 0} {
            controls::Grid::SetColumn(element, 0);
            bar.Children().InsertAt(0, element);
        }

        ~title_hold() {
            element.UnregisterPropertyChangedCallback(xaml::UIElement::VisibilityProperty(), visibility);
            if (background_property) element.UnregisterPropertyChangedCallback(background_property, background);
            uint32_t at = 0;
            if (bar.Children().IndexOf(element, at)) bar.Children().RemoveAt(at);
        }

        xaml::FrameworkElement const element;
        controls::Grid const bar;
        xaml::FrameworkElement::SizeChanged_revoker size_changed;
        std::int64_t const visibility;
        xaml::DependencyProperty const background_property;
        std::int64_t const background;
    };

    // Датчик жеста масштабирования: ScrollViewer над корнем острова со своим
    // масштабом и без прокрутки. Жест со вложенных списков доходит до него по
    // цепочке масштабирования; пока пальцы движутся, он увеличивает остров как
    // картинку, а в конце модель встаёт на ближайшую ступень, и он -- обратно к
    // 1 (проба sandbox/ZoomGestureProbe). Ctrl+колесо до него не доходит:
    // обработчик колеса у корня помечает его обработанным, и шаги остаются за
    // моделью. Ставится над корнем, когда присоединён эффект и есть остров.
    //
    // Клавиши -- у датчика, верхнего элемента острова, в PreviewKeyDown: он идёт
    // сверху раньше элемента в фокусе, и Ctrl+0 не достаётся полю ввода. Не у
    // корня: фокус бывает и над ним -- на самом датчике или ни на чём, -- и во
    // всплывающем окне, где корень клавишу не видит. Такую клавишу датчик
    // масштабировал сам, как любой ScrollViewer, и оставался не в 1: остров
    // уменьшался дважды и съезжал от угла (проба sandbox/KeyboardZoomProbe).
    // Колесо -- у корня, с handledEventsToo: оно всплывает снизу, и корень видит
    // его раньше датчика; ScrollViewer и списки помечают его обработанным.
    struct zoom_gesture : core::noncopyable {
        zoom_gesture(Impl& state, xaml::Hosting::DesktopWindowXamlSource const& chrome, controls::Grid const& root)
            : sensor{} {
            sensor.ZoomMode(controls::ZoomMode::Enabled);
            // Без инерции: ступень выбирается там, где пальцы остановились. С ней
            // быстрый жест, остановленный на 200 %, доезжал до 300 % (проба на
            // wxl.gen.ui касанием InjectSyntheticPointerInput).
            sensor.IsZoomInertiaEnabled(false);
            sensor.HorizontalScrollMode(controls::ScrollMode::Disabled);
            sensor.VerticalScrollMode(controls::ScrollMode::Disabled);
            sensor.HorizontalScrollBarVisibility(controls::ScrollBarVisibility::Disabled);
            sensor.VerticalScrollBarVisibility(controls::ScrollBarVisibility::Disabled);
            state.apply_zoom_limits(sensor);
            sensor.ViewChanged([&state](auto&&, controls::ScrollViewerViewChangedEventArgs const& args) {
                state.on_zoom_gesture(args.IsIntermediate());
            });
            chrome.Content(sensor);
            sensor.Content(root);

            sensor.PreviewKeyDown([&state](auto&&, xaml::Input::KeyRoutedEventArgs const& args) {
                if (state.zoom && state.on_preview_key_down(static_cast<int>(args.Key()))) args.Handled(true);
            });
            root.AddHandler(
                xaml::UIElement::PointerWheelChangedEvent(),
                winrt::box_value(xaml::Input::PointerEventHandler(
                    [&state](auto&&, xaml::Input::PointerRoutedEventArgs const& args) {
                        if (state.zoom && state.on_pointer_wheel_changed(args)) args.Handled(true);
                    })),
                true);
        }

        controls::ScrollViewer const sensor;
    };

    // Индикатор текущего масштаба: появляется, пока масштаб меняется, и сам
    // прячется через delay после последнего изменения. Простой Popup, а не Flyout:
    // у Flyout лёгкое закрытие, и открытый, он закрывался на каждой смене масштаба
    // острова -- она меняет размер XamlRoot, а на это Popup с лёгким закрытием
    // закрывается сам (Popup::OnXamlRootChanged в исходниках WinUI), -- и ещё при
    // уходе окна из активных и растяжке. Popup без лёгкого закрытия остаётся
    // открытым и только меняет текст (проба sandbox/KeyboardZoomProbe --flyout-scale
    // --popup). Внутри -- FlyoutPresenter: вид тот же, что у Flyout. Фокуса Popup не
    // берёт: следующее Ctrl+«+» идёт окну.
    struct zoom_indicator : core::noncopyable {
        explicit zoom_indicator(controls::Grid const& root)
            : text{},
              popup{},
              timer{mud::DispatcherQueue::GetForCurrentThread().CreateTimer()},
              tick{timer.Tick(winrt::auto_revoke, [this](auto&&, auto&&) { popup.IsOpen(false); })},
              anchor{} {
            controls::FlyoutPresenter presenter;
            presenter.Content(text);
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
            popup.Child(presenter);
            popup.ChildTransitions(transitions);
            popup.DesiredPlacement(controls::Primitives::PopupPlacementMode::Bottom);
            popup.XamlRoot(root.XamlRoot());

            timer.Interval(delay);
            timer.IsRepeating(false);

            anchor.Height(0);
            anchor.VerticalAlignment(xaml::VerticalAlignment::Top);
            anchor.IsHitTestVisible(false);
            controls::Grid::SetRow(anchor, 0);
            root.Children().Append(anchor);
        }

        ~zoom_indicator() {
            timer.Stop();
            popup.IsOpen(false);
        }

        // Под строкой заголовка, посередине: Bottom ставит Popup по середине
        // элемента и держит там, когда тот меняет размер. Строка заголовка может
        // появиться позже индикатора -- цель выбирается при каждом показе.
        void show(winrt::hstring const& percent, controls::Grid const& bar) {
            text.Text(percent);
            if (!popup.IsOpen()) {
                popup.PlacementTarget(bar ? xaml::FrameworkElement{bar} : xaml::FrameworkElement{anchor});
                popup.IsOpen(true);
            }
            timer.Stop();
            timer.Start();
        }

        static constexpr std::chrono::milliseconds delay{1500};

        controls::TextBlock const text;
        controls::Primitives::Popup const popup;
        mud::DispatcherQueueTimer const timer;
        mud::DispatcherQueueTimer::Tick_revoker tick;
        // Без строки заголовка индикатор встаёт под этот пустой элемент во всю
        // ширину верха корня.
        controls::Border const anchor;
    };

    // Остров: оснастка окна островом XAML и корень в две строки -- заголовок с кнопками окна и
    // под ним содержимое приложения. С ним живёт то, что без него не нужно: фокус клавиатуры,
    // строка заголовка, датчик и индикатор масштаба. Строится при первой нужде (ensure_island)
    // и отпускается целиком, пока XAML жив (close_xaml).
    struct xaml_island : core::noncopyable {
        explicit xaml_island(Impl& state) : chrome{}, root{} {
            chrome.Initialize(window_id_of(state.window.get()));
            if (state.system_backdrop) chrome.SystemBackdrop(state.system_backdrop);

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
            root.Loaded([&state](auto&&, auto&&) { state.on_root_loaded(); });
            chrome.Content(root);
            chrome.TakeFocusRequested(
                [&state](auto&&, xaml::Hosting::DesktopWindowXamlSourceTakeFocusRequestedEventArgs const& args) {
                    state.on_take_focus_requested(args);
                });

            RECT client{};
            ::GetClientRect(state.window.get(), &client);
            chrome.SiteBridge().MoveAndResize({0, 0, client.right, client.bottom});
        }

        // Как у Microsoft.UI.Xaml.Window при закрытии (DesktopWindowImpl::CloseImpl и Shutdown
        // в исходниках WinUI 3): части, держащие объекты XAML, уходят первыми, XamlRoot -- до
        // закрытия источника (отпущенный после закрытия XAML, он падал в своём деструкторе
        // нарушением доступа: деструктор разбирает службы, заведённые по требованию, а
        // всплывающее их заводит -- после клавиш масштаба), потом содержимое убрано и
        // источник закрыт.
        ~xaml_island() {
            indicator.reset();
            gesture.reset();
            title.reset();
            bar.reset();
            xaml_root.reset();
            chrome.Content(nullptr);
            chrome.Close();
        }

        xaml::Hosting::DesktopWindowXamlSource const chrome;
        controls::Grid const root;
        xaml::UIElement page{nullptr};   // содержимое приложения

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

        std::unique_ptr<root_watch> xaml_root;
        std::unique_ptr<caption_bar> bar;
        std::unique_ptr<title_hold> title;
        std::unique_ptr<zoom_gesture> gesture;
        std::unique_ptr<zoom_indicator> indicator;
    };

    std::unique_ptr<xaml_island> island;
    // XAML этого окна закрыт (close_xaml): остров заново не заводится.
    bool xaml_closed{false};

    // ---- Заголовок окна -- то, что у Microsoft.UI.Xaml.Window держит WindowChrome ----

    // AppWindow заводится при первой нужде: окну, которое заголовок не трогает, он ни к чему.
    windowing::AppWindow app_window{nullptr};

    // Неклиентский ввод окна, заведённый при первой нужде. Подписки на него -- токенами, а не
    // auto_revoke: отзыватель берёт у источника слабую ссылку, а InputNonClientPointerSource
    // слабых ссылок не даёт, и подписка с ним падает нарушением доступа; снимает их держатель,
    // уходя, -- пока окно есть (close_xaml). И свой прямоугольник среди Caption: его и только
    // его заменяет следующий пересчёт, а прямоугольники, поставленные другими, остаются.
    struct non_client_input : core::noncopyable {
        explicit non_client_input(Impl& state)
            : source{input::InputNonClientPointerSource::GetForWindowId(window_id_of(state.window.get()))},
              // AppWindow переписывает Minimize, Maximize и Close своими
              // прямоугольниками на каждое перемещение и активацию -- свои ставятся
              // снова. Событие приходит и на запись тех же чисел; пересчёт
              // сравнивает, прежде чем писать, иначе цикл.
              regions_changed{source.RegionsChanged([this, &state](auto&&, auto&&) {
                  if (own_button_regions) state.queue_regions();
              })},
              // Над областью кнопки указатель у неклиентского ввода, не у XAML:
              // состояния кнопок ставятся отсюда.
              caption_entered{source.PointerEntered([&state](auto&&, input::NonClientPointerEventArgs const& args) {
                  state.show_caption_state(args.RegionKind(), L"PointerOver");
              })},
              caption_exited{source.PointerExited([&state](auto&&, input::NonClientPointerEventArgs const& args) {
                  state.show_caption_state(args.RegionKind(), L"Normal");
              })},
              caption_pressed{source.PointerPressed([&state](auto&&, input::NonClientPointerEventArgs const& args) {
                  state.show_caption_state(args.RegionKind(), L"Pressed");
              })},
              caption_released{source.PointerReleased([&state](auto&&, input::NonClientPointerEventArgs const& args) {
                  state.show_caption_state(args.RegionKind(), L"PointerOver");
              })} {}

        ~non_client_input() {
            source.RegionsChanged(regions_changed);
            source.PointerEntered(caption_entered);
            source.PointerExited(caption_exited);
            source.PointerPressed(caption_pressed);
            source.PointerReleased(caption_released);
        }

        input::InputNonClientPointerSource const source;
        winrt::event_token const regions_changed;
        winrt::event_token const caption_entered;
        winrt::event_token const caption_exited;
        winrt::event_token const caption_pressed;
        winrt::event_token const caption_released;
        graphics::RectInt32 caption{};
        bool own_button_regions{false};
    };

    std::unique_ptr<non_client_input> non_client;

    bool extends_content_into_title_bar{false};
    bool caption_buttons_transparent{false};   // прозрачный фон системных кнопок ставится один раз
    bool active{false};                      // по WM_NCACTIVATE: у неактивного окна знаки кнопок гаснут
    bool regions_queued{false};

    // ---- Хватка окна за своё состояние ----

    // От того, как состояние целое, до последнего сообщения окна (WM_NCDESTROY): оконная
    // процедура находит состояние по USERDATA, состояние живёт, пока живо окно, и стоит в
    // списке живых окон. Всё это отдаётся разом, когда хватка уходит, -- и окно отпускается
    // его владельцем: оно уже уничтожено.
    struct window_hold : core::noncopyable {
        explicit window_hold(Impl& window_state) : state{&window_state} {
            ::SetWindowLongPtrW(window_state.window.get(), GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&window_state));
            live_windows().push_back(core::as_not_null(&window_state));
        }

        ~window_hold() {
            ::SetWindowLongPtrW(state->window.get(), GWLP_USERDATA, 0);
            core::intrusive_list<Impl>::remove(core::as_not_null(state.get()));
            state->window.release();
        }

        core::intrusive_ptr<Impl> const state;
    };

    std::unique_ptr<window_hold> hold;

    // Окно уничтожается (WM_DESTROY), XAML ещё жив: окно закрывают, пока он жив, -- его же WM_CLOSE
    // или wxl при выходе, в ShutdownStarting очереди XAML (CompositionWindow::close_all_at_exit);
    // XAML разбирает себя следом, ещё внутри Application::Start. Всё, что состояние держит от XAML,
    // отпускается сейчас: остров со всем, что живёт с ним, и неклиентский ввод -- окно ещё есть,
    // и снимать подписки есть с кого. Отказ XAML по дороге -- нарушенный инвариант: деструкторы
    // частей noexcept, и он останавливает процесс, названный обработчиком terminate.
    void close_xaml() noexcept {
        xaml_closed = true;
        // Загрузка фона, которая идёт, окну больше не нужна: её отмена -- ход
        // очереди, а очередь ещё принимает работу; остановленная, она хода не
        // приняла бы, и кадр загрузки ждал бы на выходе вечно. С отменой забыто и
        // то, что просили: замена устройства после этого ничего не грузит.
        background_load.cancel();
        island.reset();
        non_client.reset();
    }

    void resize(float width, float height) {
        winrt::Windows::Foundation::Numerics::float2 const wh{width, height};
        // Один визуал сцены -- задний. Он корень острова и картинка разом, а
        // собственные визуалы приложения висят на нём детьми и меряют себя сами,
        // так что под окно подгоняем только его.
        backdrop.Size(wh);
        if (island) {
            island->chrome.SiteBridge().MoveAndResize(
                {0, 0, static_cast<int32_t>(width), static_cast<int32_t>(height)});
        }
    }

    float scale() const {
        return static_cast<float>(::GetDpiForWindow(window.get())) / 96.0f * static_cast<float>(zoom_factor);
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

        float const dpi = window.get() ? static_cast<float>(::GetDpiForWindow(window.get())) / 96.0f : 1.0f;
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
        ::GetClientRect(window.get(), &client);
        return {{client.right, client.bottom}, scale()};
    }

    windowing::AppWindow const& app_window_of() {
        if (!app_window) app_window = windowing::AppWindow::GetFromWindowId(window_id_of(window.get()));
        return app_window;
    }

    non_client_input& non_client_of() {
        if (!non_client) non_client = std::make_unique<non_client_input>(*this);
        return *non_client;
    }

    // ---- Остров ----

    void ensure_island() {
        if (island) return;
        // Окно уничтожено -- острову не на чем стоять: RO_E_CLOSED, как у
        // всякого закрытого объекта WinRT.
        if (xaml_closed) throw winrt::hresult_error{RO_E_CLOSED, L"wxl::CompositionWindow: окно уже закрыто"};
        island = std::make_unique<xaml_island>(*this);
        apply_zoom();
    }

    void on_root_loaded() {
        watch_xaml_root();
        // Остров стал виден снаружи: передача фокуса, которую окно отложило.
        if (!island || !island->island_focus_pending) return;
        island->island_focus_pending = false;
        if (island_shown() && ::GetActiveWindow() == window.get()) focus_island();
    }

    // ---- Фокус клавиатуры: как у Microsoft.UI.Xaml.Window ----

    bool island_shown() const { return island && island->content_shown; }

    // Окно ввода острова -- дочернее у окна его моста.
    bool island_owns(HWND child) const {
        auto const bridge =
            reinterpret_cast<HWND>(static_cast<std::uintptr_t>(island->chrome.SiteBridge().WindowId().Value));
        return child == bridge || ::IsChild(bridge, child);
    }

    // DesktopWindowImpl::SetFocusToContentIsland: фокус Windows -- окну ввода острова.
    void focus_island() {
        xaml::XamlRoot const current = island->root.XamlRoot();
        if (!current) {
            island->island_focus_pending = true;
            return;
        }
        island->island_focus_pending = false;
        auto const controller = input::InputFocusController::GetForIsland(current.ContentIsland());
        if (!controller.HasFocus()) controller.TrySetFocus();
    }

    // DesktopWindowImpl::RestoreFocus: фокус XAML -- элементу, у которого он был.
    void restore_xaml_focus() {
        island->chrome.NavigateFocus(xaml::Hosting::XamlSourceFocusNavigationRequest{
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
            if (focus && ::IsChild(window.get(), focus)) island->last_focused_child = focus;
        }
        if (!minimized && !inactive) {
            if (!island->last_focused_child || island_owns(island->last_focused_child)) {
                focus_island();
                restore_xaml_focus();
            } else {
                ::SetFocus(island->last_focused_child);
            }
        }
        if (!minimized && island->initial_activation) {
            focus_island();
            island->initial_activation = false;
        }
        return true;
    }

    // Tab за последний элемент острова (Shift+Tab -- за первый): остров отдаёт
    // фокус окну, а кроме острова в окне ничего нет -- фокус идёт по кругу в
    // тот же остров, как у Window (OnDesktopWindowXamlSourceTakeFocusRequested).
    void on_take_focus_requested(xaml::Hosting::DesktopWindowXamlSourceTakeFocusRequestedEventArgs const& args) {
        auto const request = args.Request();
        winrt::guid const correlation = request.CorrelationId();
        if (correlation == island->last_take_focus_correlation) {
            // Запрос вернулся с тем же номером: в острове один элемент, фокус
            // остаётся на нём.
            restore_xaml_focus();
            return;
        }
        island->last_take_focus_correlation = correlation;
        auto const reason = request.Reason();
        if (reason == xaml::Hosting::XamlSourceFocusNavigationReason::First ||
            reason == xaml::Hosting::XamlSourceFocusNavigationReason::Last) {
            island->chrome.NavigateFocus(
                xaml::Hosting::XamlSourceFocusNavigationRequest{reason, winrt::Windows::Foundation::Rect{}, correlation});
        }
    }

    // XamlRoot острова, за сменой которого следят прямоугольники: заводится, когда корень
    // получил его, и меняется вместе с ним.
    void watch_xaml_root() {
        if (!island) return;
        xaml::XamlRoot const current = island->root.XamlRoot();
        if (!current || (island->xaml_root && island->xaml_root->root == current)) return;
        island->xaml_root = std::make_unique<root_watch>(current, *this);
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

    void ensure_zoom_gesture() {
        if (!zoom || !island || island->gesture) return;
        island->gesture = std::make_unique<zoom_gesture>(*this, island->chrome, island->root);
    }

    // Жест не уходит за ступени модели: пределы датчика -- относительно
    // масштаба, в котором остров уже стоит.
    void apply_zoom_limits(controls::ScrollViewer const& sensor) const {
        sensor.MinZoomFactor(static_cast<float>(AppZoom::minZoomFactor / zoom_factor));
        sensor.MaxZoomFactor(static_cast<float>(AppZoom::maxZoomFactor / zoom_factor));
    }

    void limit_zoom_gesture() {
        if (island && island->gesture) apply_zoom_limits(island->gesture->sensor);
    }

    void on_zoom_gesture(bool intermediate) {
        controls::ScrollViewer const& sensor = island->gesture->sensor;
        double const gesture = sensor.ZoomFactor();
        // ScrollViewer хранит масштаб во float: единица -- с допуском.
        if (std::abs(gesture - 1.0) < 1e-4) return;
        double const wanted = zoom_factor * gesture;
        if (intermediate) {
            show_zoom_indicator(wanted);
            return;
        }
        // Сначала датчик обратно к единице, потом модель на ступень: остров
        // берёт новый масштаб уже без увеличения картинкой поверх.
        sensor.ChangeView(nullptr, nullptr, 1.0f, true);
        double const before = zoom_factor;
        zoom->model->setZoomFactor(wanted);
        // Сменилась ступень -- индикатор уже показала подписка на модель;
        // нет -- он всё ещё показывает промежуточное число жеста.
        if (zoom_factor == before) show_zoom_indicator(zoom_factor);
    }

    static winrt::hstring percent(double factor) {
        return winrt::hstring{std::to_wstring(std::lround(factor * 100)) + L" %"};
    }

    // Зовётся и из подписки на модель, а она noexcept: отказ XAML здесь -- нарушенный инвариант,
    // и он останавливает процесс, названный обработчиком terminate. Острову, которого ещё не
    // показали, индикатор не нужен.
    void show_zoom_indicator(double factor) noexcept {
        if (!island || !island->root.XamlRoot()) return;
        if (!island->indicator) island->indicator = std::make_unique<zoom_indicator>(island->root);
        island->indicator->show(percent(factor), island->bar ? island->bar->grid : controls::Grid{nullptr});
    }

    // Ctrl без Alt: Ctrl+Alt -- это AltGr, им набирают символы.
    static bool is_zoom_modifier_down() { return ::GetKeyState(VK_CONTROL) < 0 && ::GetKeyState(VK_MENU) >= 0; }

    // «+» и «-» -- и основного ряда, и цифрового блока; «0» -- тоже оба.
    bool on_preview_key_down(int key) {
        if (!is_zoom_modifier_down()) return false;
        switch (key) {
            case VK_ADD:
            case VK_OEM_PLUS:
                zoom->model->zoomIn();
                return true;
            case VK_SUBTRACT:
            case VK_OEM_MINUS:
                zoom->model->zoomOut();
                return true;
            case '0':
            case VK_NUMPAD0:
                zoom->model->resetZoom();
                return true;
        }
        return false;
    }

    bool on_pointer_wheel_changed(xaml::Input::PointerRoutedEventArgs const& args) {
        if (!is_zoom_modifier_down()) return false;
        auto const wheel = args.GetCurrentPoint(island->root).Properties();
        if (wheel.IsHorizontalMouseWheel()) return false;
        int32_t& delta = zoom->accumulated_wheel_delta;
        delta += wheel.MouseWheelDelta();
        for (; delta >= WHEEL_DELTA; delta -= WHEEL_DELTA) zoom->model->zoomIn();
        for (; delta <= -WHEEL_DELTA; delta += WHEEL_DELTA) zoom->model->zoomOut();
        return true;
    }

    void apply_zoom() {
        if (!island || (zoom_factor == 1.0 && !zoom_applied)) return;
        zoom_applied = true;
        content::DesktopChildSiteBridge const bridge = island->chrome.SiteBridge();
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
        xaml_island& shown = *island;
        if (shown.page) {
            uint32_t at = 0;
            if (shown.root.Children().IndexOf(shown.page, at)) shown.root.Children().RemoveAt(at);
        }
        shown.page = element;
        if (shown.page) {
            controls::Grid::SetRow(shown.page.as<xaml::FrameworkElement>(), 1);
            shown.root.Children().Append(shown.page);
        }
        shown.chrome.SiteBridge().Show();
        shown.content_shown = true;
        // Фокус на самом окне -- он там для сцены, пока острова не было или он был
        // спрятан, или остров показан уже после активации. Показанному острову --
        // фокус, как при активации.
        if (::GetFocus() == window.get()) {
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
            HWND const handle = window.get();
            if (!handle) return;
            UINT const given = command == SC_MAXIMIZE && ::IsZoomed(handle) ? SC_RESTORE : command;
            ::PostMessageW(handle, WM_SYSCOMMAND, given, 0);
        });
        return button;
    }

    void ensure_bar() {
        if (island->bar) return;
        island->bar = std::make_unique<caption_bar>(*this, island->root);
        show_active(active);
    }

    // У стиля нет неактивного состояния. Фон кнопки в покое прозрачный, так
    // что её прозрачность гасит один знак, как у системных кнопок; под
    // указателем знак полный.
    static constexpr double inactive_caption_opacity = 0.36;

    void show_active(bool value) {
        active = value;
        if (!island || !island->bar) return;
        caption_bar const& bar = *island->bar;
        for (controls::Button const& button : {bar.minimize, bar.maximize, bar.close}) {
            button.Opacity(active ? 1.0 : inactive_caption_opacity);
        }
    }

    void show_caption_state(input::NonClientRegionKind kind, std::wstring_view state) {
        if (!island || !island->bar) return;
        caption_bar const& bar = *island->bar;
        controls::Button button{nullptr};
        switch (kind) {
            case input::NonClientRegionKind::Minimize: button = bar.minimize; break;
            case input::NonClientRegionKind::Maximize: button = bar.maximize; break;
            case input::NonClientRegionKind::Close: button = bar.close; break;
            default: return;
        }
        bool const normal = state == L"Normal";
        std::wstring name{state};
        if (kind == input::NonClientRegionKind::Close && !normal) name = L"CloseButton" + name;
        xaml::VisualStateManager::GoToState(button, name, false);
        button.Opacity(active || !normal ? 1.0 : inactive_caption_opacity);
    }

    void show_maximized() {
        HWND const handle = window.get();
        if (!island || !island->bar || !handle) return;
        bool const maximized = ::IsZoomed(handle) != 0;
        controls::Button const& maximize = island->bar->maximize;
        xaml::VisualStateManager::GoToState(
            maximize, maximized ? L"WindowStateMaximized" : L"WindowStateNormal", false);
        xaml::Automation::AutomationProperties::SetName(
            maximize, system_menu_text(handle, maximized ? SC_RESTORE : SC_MAXIMIZE));
    }

    // Заголовок-элемент, если он есть.
    xaml::FrameworkElement title_bar() const {
        return island && island->title ? island->title->element : xaml::FrameworkElement{nullptr};
    }

    void set_title_bar(xaml::FrameworkElement const& element) {
        ensure_island();
        island->title.reset();
        if (element) {
            ensure_bar();
            island->title = std::make_unique<title_hold>(element, *this, island->bar->grid);
            copy_title_bar_background();
        }
        update_caption_mode();
    }

    // Под кнопками окна -- тот же фон, что у заголовка рядом.
    void copy_title_bar_background() {
        if (!island || !island->bar) return;
        title_hold const* const title = island->title.get();
        island->bar->buttons.Background(
            title && title->background_property
                ? title->element.GetValue(title->background_property).try_as<xaml::Media::Brush>()
                : nullptr);
    }

    // Чьи кнопки окна сейчас: свои -- пока клиентская область под заголовком и
    // заголовок-элемент виден; иначе системные, как у Window.
    void update_caption_mode() {
        xaml::FrameworkElement const title = title_bar();
        bool const ownButtons = extends_content_into_title_bar && title &&
                                title.Visibility() == xaml::Visibility::Visible;
        if (island && island->bar) {
            island->bar->grid.Visibility(title ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);
            island->bar->buttons.Visibility(ownButtons ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);
        }
        // Высота системных кнопок -- по тому же условию, что и свои кнопки: при
        // своих системные убраны (Collapsed), иначе стоят системные обычной
        // высоты (Standard). Это значения режима по умолчанию; иное приложение
        // ставит само через appWindow(). Без растяжки свойство не трогается --
        // AppWindowTitleBar тогда его не принимает.
        if (extends_content_into_title_bar) {
            using windowing::TitleBarHeightOption;
            app_window_of().TitleBar().PreferredHeightOption(
                ownButtons ? TitleBarHeightOption::Collapsed : TitleBarHeightOption::Standard);
        }
        queue_regions();
    }

    // Пересчёт после вёрстки: и размер элемента, и размер окна приходят раньше,
    // чем XAML их разложил.
    void queue_regions() {
        if (regions_queued || !window.get()) return;
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
        HWND const handle = window.get();
        if (!handle || ::IsIconic(handle)) return;
        watch_xaml_root();

        xaml::FrameworkElement const title = title_bar();
        bool const own = extends_content_into_title_bar && title &&
                         title.Visibility() == xaml::Visibility::Visible;

        // Caption -- как CWindowChrome::SetDragRegion у Window: убирается свой
        // прежний прямоугольник, ставится новый, чужие остаются.
        graphics::RectInt32 const wanted = own ? physical_rect_of(title) : graphics::RectInt32{};
        graphics::RectInt32 const had = non_client ? non_client->caption : graphics::RectInt32{};
        if (!same_rect(wanted, had) && (own || had.Width != 0 || had.Height != 0)) {
            non_client_input& regions = non_client_of();
            winrt::com_array<graphics::RectInt32> const current =
                regions.source.GetRegionRects(input::NonClientRegionKind::Caption);
            std::vector<graphics::RectInt32> rects;
            rects.reserve(current.size() + 1);
            for (graphics::RectInt32 const& rect : current) {
                if (!same_rect(rect, regions.caption)) rects.push_back(rect);
            }
            if (wanted.Width > 0 && wanted.Height > 0) rects.push_back(wanted);
            regions.source.SetRegionRects(input::NonClientRegionKind::Caption, rects);
            regions.caption = wanted;
        }

        if (!own) {
            if (non_client && non_client->own_button_regions) {
                for (auto const kind : {input::NonClientRegionKind::Minimize, input::NonClientRegionKind::Maximize,
                                        input::NonClientRegionKind::Close}) {
                    non_client->source.ClearRegionRects(kind);
                }
                non_client->own_button_regions = false;
            }
            return;
        }

        // Свои кнопки -- значит, есть заголовок-элемент, а с ним и строка заголовка (set_title_bar).
        non_client_input& regions = non_client_of();
        regions.own_button_regions = true;
        auto const apply = [&regions](input::NonClientRegionKind kind, graphics::RectInt32 rect) {
            winrt::com_array<graphics::RectInt32> const current = regions.source.GetRegionRects(kind);
            if (current.size() == 1 && same_rect(current[0], rect)) return;
            regions.source.SetRegionRects(kind, {rect});
        };
        caption_bar const& bar = *island->bar;
        apply(input::NonClientRegionKind::Minimize, physical_rect_of(bar.minimize));
        apply(input::NonClientRegionKind::Maximize, physical_rect_of(bar.maximize));
        apply(input::NonClientRegionKind::Close, physical_rect_of(bar.close));
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

// Сцена: свой Microsoft.UI-композитор и один задний визуал -- он же корень
// острова, он же фон. Подключаем его к нашему HWND attached-мостом -- без
// дочернего окна рисования (мост InputSiteWindowClass -- только под ввод). Кэш
// текстур на том же компоновщике -- под асинхронные задники -- и держатель замены
// устройства под поверхностями: на ней задник, загруженный на ходу, грузится снова.
// Отказ по дороге разматывает построенное, окно -- его владелец (owned_window).
CompositionWindow::Impl::Impl(HWND created)
    : window{created},
      compositor{},
      backdrop{compositor.CreateSpriteVisual()},
      texture_cache{Object::Impl::wrap<Compositor>(compositor)},
      scene_island{content::ContentIsland::Create(backdrop)},
      scene_bridge{content::DesktopAttachedSiteBridge::CreateFromWindowId(mud::DispatcherQueue::GetForCurrentThread(),
                                                                         window_id_of(created))},
      pointer_input{input::InputPointerSource::GetForIsland(scene_island)},
      device_watch{Object::Impl::wrap<Compositor>(compositor), *this} {
    scene_bridge.Connect(scene_island);

    // Ввод у чистой сцены расщеплён. Клавиатура доходит до верхнего WndProc
    // (WM_KEYDOWN -- проверено пробой), а указатель перехватывает InputSite
    // острова, и его берём через InputPointerSource острова. InputKeyboardSource
    // не заводим: его GetForIsland на attached-острове падал AV, а клавиатуре он
    // и не нужен. Подписки простые, без auto_revoke: источник живёт в состоянии
    // столько же, сколько оно само. Обработчики noexcept: исключение подписчика
    // делегат cppwinrt превратил бы в HRESULT и потерял, а список события так и
    // остался бы посреди вызова, -- так что сбой внутри -- std::terminate.
    pointer_input.PointerPressed(
        [this](input::InputPointerSource const&, input::PointerEventArgs const& args) noexcept {
            pointer_pressed.raise(Object::Impl::wrap<PointerPoint>(args.CurrentPoint()));
        });
    pointer_input.PointerMoved(
        [this](input::InputPointerSource const&, input::PointerEventArgs const& args) noexcept {
            pointer_moved.raise(Object::Impl::wrap<PointerPoint>(args.CurrentPoint()));
        });
    pointer_input.PointerReleased(
        [this](input::InputPointerSource const&, input::PointerEventArgs const& args) noexcept {
            pointer_released.raise(Object::Impl::wrap<PointerPoint>(args.CurrentPoint()));
        });
    pointer_input.PointerWheelChanged(
        [this](input::InputPointerSource const&, input::PointerEventArgs const& args) noexcept {
            pointer_wheel_changed.raise(Object::Impl::wrap<PointerPoint>(args.CurrentPoint()));
        });

    RECT client{};
    ::GetClientRect(created, &client);
    resize(static_cast<float>(client.right), static_cast<float>(client.bottom));
}

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
                if (request.cancel() || !held->window.get()) return 0;

                held->closed.raise(Object::Impl::empty<Object>());
                if (!held->window.get()) return 0;
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
            // потока на выходе остановилось бы на нём. Окно без состояния не
            // гасит ничего: его уничтожает владелец (owned_window), когда
            // построение состояния не удалось, и приложение живёт дальше.
            if (state) {
                state->close_xaml();
                if (Application::loopRunning()) xaml::Application::Current().Exit();
            }
            return 0;

        case WM_NCDESTROY:
            // Последнее сообщение окна: хватка окна уходит и отпускает состояние (window_hold).
            // Живут ещё хендлы -- состояние остаётся им, без окна под собой; не живут -- оно
            // уходит здесь же, и state дальше не трогается.
            if (state) {
                std::unique_ptr<CompositionWindow::Impl::window_hold> const leaving = std::move(state->hold);
            }
            break;
    }
    return ::DefWindowProcW(hwnd, message, wparam, lparam);
}

void ensure_class();

// Само окно: класс регистрируется один раз, отказ системы бросается здесь, до состояния.
HWND create_window() {
    ensure_class();
    HWND const made = ::CreateWindowExW(WS_EX_NOREDIRECTIONBITMAP, class_name, L"", WS_OVERLAPPEDWINDOW,
                                        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, nullptr,
                                        nullptr, ::GetModuleHandleW(nullptr), nullptr);
    if (!made) winrt::throw_last_error();
    return made;
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
        core::intrusive_ptr<Impl> const state{front};
        ::SendMessageW(state->window.get(), WM_CLOSE, 0, 0);
        // Отказ Closing на выходе не в счёт: окно, оставленное жить, пережило бы
        // XAML.
        if (HWND const handle = state->window.get()) ::DestroyWindow(handle);
        // WM_NCDESTROY уносит узел из списка; оставшийся -- окно, которое не
        // уничтожилось, и обход стоял бы на нём вечно.
        if (state->linked()) {
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
        texture_cache.getAsync(image.path, load), load,
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

// Состояние строится целым из готового окна, и только целое окно берёт себе (window_hold):
// сообщения, пришедшие раньше, процедура встречает без состояния, как и при создании окна.
CompositionWindow::CompositionWindow() : impl_{new Impl(create_window()), false} {
    impl_->hold = std::make_unique<Impl::window_hold>(*impl_);
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
    if (impl_->island) impl_->island->chrome.SystemBackdrop(impl_->system_backdrop);
}

UIElement CompositionWindow::content() const {
    return Object::Impl::wrap<UIElement>(impl_->island ? impl_->island->page : xaml::UIElement{nullptr});
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
    if (impl_->island) {
        impl_->show_page(nullptr);
        // Фокус, оставшийся в спрятанном острове, увёл бы туда и клавиши сцены.
        if (HWND const focus = ::GetFocus(); focus && impl_->island_owns(focus)) ::SetFocus(impl_->window.get());
        impl_->island->chrome.SiteBridge().Hide();
        impl_->island->content_shown = false;
    }
}

void CompositionWindow::zoomFactor(double value) const { impl_->set_zoom_factor(value); }

void CompositionWindow::attachZoom(core::intrusive_ptr<AppZoom> zoom) const {
    // Новое присоединение заменяет прежнее целиком: прежний держатель снимает свою подписку,
    // уходя. Масштаб, с которым эффект пришёл (восстановленный приложением), -- без
    // индикатора: его никто не регулировал.
    impl_->zoom = std::make_unique<Impl::zoom_attachment>(std::move(zoom), *impl_);
    impl_->set_zoom_factor(impl_->zoom->model->zoomFactor().get());
    impl_->ensure_island();
    impl_->ensure_zoom_gesture();
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
    ::SetWindowPos(impl_->window.get(), nullptr, 0, 0, 0, 0,
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
    ::SetWindowTextW(impl_->window.get(), value.text().wc_str());
}

hstring CompositionWindow::title() const {
    int const length = ::GetWindowTextLengthW(impl_->window.get());
    core::sta_u16string text(static_cast<std::size_t>(length) + 1, u'\0');
    ::GetWindowTextW(impl_->window.get(), reinterpret_cast<wchar_t*>(text.data()), length + 1);
    return hstring{std::u16string_view{text.data(), static_cast<std::size_t>(length)}};
}

void CompositionWindow::minSize(SizeInt32 const& value) const { impl_->min_size = value; }

void CompositionWindow::activate() const {
    ::ShowWindow(impl_->window.get(), SW_SHOW);
    ::UpdateWindow(impl_->window.get());
}

void CompositionWindow::resize(SizeInt32 size) const {
    ::SetWindowPos(impl_->window.get(), nullptr, 0, 0, size.width, size.height,
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
    RECT const frame = with_frame(impl_->window.get(), {0, 0, client.width, client.height});

    int const width = frame.right - frame.left;
    int const height = frame.bottom - frame.top;

    RECT const work = work_area_of(impl_->window.get());

    ::SetWindowPos(impl_->window.get(), nullptr, work.left + ((work.right - work.left) - width) / 2,
                   work.top + ((work.bottom - work.top) - height) / 2, width, height,
                   SWP_NOZORDER | SWP_NOOWNERZORDER);
}

SizeInt32 CompositionWindow::maxClientSize() const {
    RECT const work = work_area_of(impl_->window.get());

    // Пустой клиент, обросший рамкой, -- это и есть одна рамка: слева-справа и
    // сверху-снизу вместе с заголовком.
    RECT const frame = with_frame(impl_->window.get(), {0, 0, 0, 0});

    return {(work.right - work.left) - (frame.right - frame.left),
            (work.bottom - work.top) - (frame.bottom - frame.top)};
}

HWND__* CompositionWindow::handle() const { return impl_->window.get(); }

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
    if (!::GetWindowPlacement(impl_->window.get(), &placement)) {
        return;
    }
    placement.rcNormalPosition = impl::fit_placement_to_displays(wanted->restore);
    placement.showCmd =
        wanted->state == impl::placement_state::maximized ? SW_SHOWMAXIMIZED : SW_SHOWNORMAL;
    ::SetWindowPlacement(impl_->window.get(), &placement);

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
    if (!::GetWindowPlacement(impl_->window.get(), &placement)) {
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
    HWND const hwnd = impl_->window.get();
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

void CompositionWindow::darkFrame(bool on) const { impl::frame_dark(impl_->window.get(), on); }

void CompositionWindow::captionColor(Color caption, Color text) const {
    // DWM берёт COLORREF -- 0x00BBGGRR, без альфы.
    auto const colorref = [](Color color) {
        return static_cast<uint32_t>(color.R | (color.G << 8) | (color.B << 16));
    };
    impl::frame_caption(impl_->window.get(), colorref(caption), colorref(text));
}

void CompositionWindow::close() const {
    // PostMessage, а не Send: просьба встаёт в очередь, как от системного
    // крестика, и разбирается тем же WM_CLOSE (Closing -> Closed ->
    // DestroyWindow -> WM_DESTROY). Без окна hwnd пуст, а PostMessage с пустым
    // окном послал бы WM_CLOSE самому потоку.
    if (impl_->window.get()) ::PostMessageW(impl_->window.get(), WM_CLOSE, 0, 0);
}

void CompositionWindow::acceptFileDrops(std::function<void(std::filesystem::path)> handler) const {
    impl_->on_file_drop = std::move(handler);
    ::DragAcceptFiles(impl_->window.get(), TRUE);
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
    if (!state) return {};
    return {state->compositor, state->backdrop};
}

}  // namespace wxl::impl

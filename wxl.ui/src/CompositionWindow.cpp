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
#include <winrt/Microsoft.UI.Xaml.Markup.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.UI.Xaml.Interop.h>    // xaml_typename -- геометрия значка кнопки окна из строки
#include <winrt/Windows.UI.h>

#include <cmath>
#include <limits>
#include <vector>

#include <windows.h>

#include <d2d1_1.h>     // ID2D1DeviceContext, DrawBitmap -- фон-картинка рисуется через DrawingSurface
#include <wincodec.h>   // WIC: декод картинки заднего фона в поверхность
#include <shellapi.h>   // HDROP, DragAcceptFiles/DragQueryFileW/DragFinish -- WIN32_LEAN_AND_MEAN их прячет

#include "CompositionWindow.h"
#include "TextureCache.h"
// Перед Object.impl.h: тот тянет import wxl.core (модульный std), а
// window_placement.h несёт обычный <optional> -- он должен встретиться до
// импорта, иначе MSVC не примет стандартный заголовок после него.
#include "impl/window_frame.h"
#include "impl/window_placement.h"
#include "impl/application_folder.h"
#include "Object.impl.h"
#include "generated/Microsoft.UI.Composition.impl.h"
#include "generated/Microsoft.UI.Dispatching.impl.h"
#include "generated/Microsoft.UI.Input.impl.h"
#include "generated/Microsoft.UI.Windowing.impl.h"
#include "generated/Microsoft.UI.Xaml.impl.h"

// Импорт последним: он несёт модульный std, а обычный заголовок после него
// MSVC уже не примет. Нужен для wxl::core::append_number -- сборки строки
// запомненного места окна.
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

winrt::Windows::UI::Color asWinrt(Color c) noexcept { return {c.A, c.R, c.G, c.B}; }

winrt::Microsoft::UI::WindowId windowIdOf(HWND hwnd) noexcept {
    return {reinterpret_cast<uint64_t>(hwnd)};
}

bool sameRect(graphics::RectInt32 const& a, graphics::RectInt32 const& b) noexcept {
    return a.X == b.X && a.Y == b.Y && a.Width == b.Width && a.Height == b.Height;
}

// Класс окна -- один на процесс. Без кисти фона и с WM_ERASEBKGND->1:
// поверхности перенаправления нет, белому взяться неоткуда.
constexpr wchar_t kClassName[] = L"wxl.CompositionWindow";

// Подписчики одного события окна. Обработчик снимают и изнутри вызова -- так
// уходит ожидание корутины, дождавшейся события, -- поэтому вызов идёт по
// индексу с копией обработчика в руках, а снятые места вычищаются, когда
// никто не вызывается.
template <typename Args>
class handler_list {
public:
    EventToken add(EventHandler<Args> handler) {
        entries_.push_back({++last_, std::move(handler)});
        return {last_};
    }

    void remove(EventToken token) {
        for (entry& each : entries_) {
            if (each.token == token.value) {
                each.token = 0;
                each.handler = nullptr;
            }
        }
        if (firing_ == 0) compact();
    }

    void fire(EventArgsRef<Args> args) {
        ++firing_;
        Object const sender = Object::Impl::empty<Object>();
        for (std::size_t at = 0; at < entries_.size(); ++at) {
            if (!entries_[at].handler) continue;
            EventHandler<Args> const handler = entries_[at].handler;
            handler(sender, args);
        }
        if (--firing_ == 0) compact();
    }

private:
    struct entry {
        std::int64_t token;
        EventHandler<Args> handler;
    };

    void compact() {
        std::erase_if(entries_, [](entry const& each) { return !each.handler; });
    }

    std::vector<entry> entries_;
    std::int64_t last_ = 0;
    int firing_ = 0;
};

// Строка системного меню окна -- «&Свернуть», «&Закрыть\tAlt+F4» -- без
// ускорителя и клавиш: ею кнопки окна называют себя для UI Automation на языке
// Windows, не заводя своих строк.
std::wstring systemMenuText(HWND hwnd, UINT command) {
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
xaml::DependencyProperty backgroundPropertyOf(xaml::FrameworkElement const& element) {
    if (element.try_as<controls::Control>()) return controls::Control::BackgroundProperty();
    if (element.try_as<controls::Panel>()) return controls::Panel::BackgroundProperty();
    if (element.try_as<controls::Border>()) return controls::Border::BackgroundProperty();
    return nullptr;
}

}  // namespace

// Общее состояние окна: сырой winrt внутри, обёртки -- только на границе.
struct WindowState : core::refcounted {
    HWND hwnd{nullptr};
    muc::Compositor compositor{nullptr};
    // Один визуал на всю сцену: он и корень острова, и задний фон (картинка или
    // цвет). Страница висит на нём ребёнком -- контейнер над контейнером не нужен.
    muc::SpriteVisual backdrop{nullptr};
    content::DesktopAttachedSiteBridge sceneBridge{nullptr};
    xaml::Hosting::DesktopWindowXamlSource chrome{nullptr};   // оснастка островом
    std::function<void(std::filesystem::path)> onFileDrop;

    SizeInt32 minSize{};                // нижний предел клиента (WM_GETMINMAXINFO)
    WINDOWPLACEMENT savedPlacement{};   // место до полноэкранного -- куда вернуться
    LONG_PTR savedStyle{};              // стиль окна до полноэкранного
    bool fullScreen{false};

    // Кэш текстур поверх compositor -- под асинхронные задники (Win2D). Заводится
    // в конструкторе, когда компоновщик готов.
    std::optional<TextureCache> textureCache;

    // Картинка заднего фона в свой размер и то, как она кроет окно. Кисть
    // задника из неё зависит от DPI экрана и собирается заново, когда окно
    // переезжает на другой экран.
    muc::CompositionSurfaceBrush picture{nullptr};
    BackgroundFill pictureFill{};
    Color pictureColor{};

    // Номер последней смены фона. Асинхронная загрузка ставит свою картинку,
    // только если за время загрузки фон никто не сменил.
    std::uint64_t backgroundChange{0};

    // Ввод со сцены (режим чтения без острова): источники ввода её ContentIsland.
    // Источники держим живыми -- на них висят подписки.
    content::ContentIsland sceneIsland{nullptr};
    input::InputPointerSource pointerInput{nullptr};   // указатель сцены; клавиатура -- через WndProc

    handler_list<Object> closed;
    handler_list<Object> geometryChanged;
    handler_list<ClientSize> clientSizeChanged;
    handler_list<VirtualKey> keyDown;
    handler_list<PointerPoint> pointerPressed;
    handler_list<PointerPoint> pointerMoved;
    handler_list<PointerPoint> pointerReleased;
    handler_list<PointerPoint> pointerWheelChanged;

    // Увеличение острова и то, как его понимает OverrideScale моста:
    // документация называет его и масштабом вместо масштаба окна, и множителем к
    // нему, так что это меряется на деле (applyZoom) и запоминается.
    double zoom{1.0};
    bool zoomApplied{false};

    // Раскладка острова: корень в две строки -- заголовок с кнопками окна и под
    // ним содержимое приложения. Заводится вместе с островом.
    controls::Grid root{nullptr};
    controls::Grid bar{nullptr};                 // строка заголовка: элемент | кнопки окна
    controls::StackPanel captionButtons{nullptr};
    controls::Button minimizeButton{nullptr};
    controls::Button maximizeButton{nullptr};
    controls::Button closeButton{nullptr};
    xaml::UIElement page{nullptr};               // содержимое приложения

    // Заголовок окна -- то, что у Microsoft.UI.Xaml.Window держит WindowChrome.
    // AppWindow и источник неклиентского ввода заводятся при первой нужде: окну,
    // которое заголовок не трогает, они ни к чему.
    windowing::AppWindow appWindow{nullptr};
    // Подписки на неклиентский ввод -- токенами, а не auto_revoke: отзыватель
    // берёт у источника слабую ссылку, а InputNonClientPointerSource слабых
    // ссылок не даёт, и подписка с ним падает нарушением доступа.
    input::InputNonClientPointerSource nonClient{nullptr};
    winrt::event_token regionsChanged{};
    winrt::event_token captionEntered{};
    winrt::event_token captionExited{};
    winrt::event_token captionPressed{};
    winrt::event_token captionReleased{};
    bool extendsContentIntoTitleBar{false};
    bool captionButtonsTransparent{false};   // прозрачный фон системных кнопок ставится один раз
    // Высота системных кнопок до того, как их сменили свои, -- её возвращают,
    // когда заголовок-элемент убран.
    std::optional<windowing::TitleBarHeightOption> systemButtonsHeight;

    xaml::FrameworkElement titleBar{nullptr};
    xaml::FrameworkElement::SizeChanged_revoker titleBarSizeChanged;
    std::int64_t titleBarVisibility{0};
    xaml::DependencyProperty titleBarBackgroundProperty{nullptr};
    std::int64_t titleBarBackground{0};
    xaml::XamlRoot xamlRoot{nullptr};
    winrt::event_token xamlRootChanged{};
    // Свой прямоугольник среди Caption: его и только его заменяет следующий
    // пересчёт, а прямоугольники, поставленные другими, остаются.
    graphics::RectInt32 caption{};
    bool ownButtonRegions{false};
    bool regionsQueued{false};

    // Заголовок-элемент переживает окно, если его держит приложение, -- его
    // обратные вызовы указывают сюда и снимаются вместе с состоянием.
    //
    // Состояние может умирать и после того, как XAML уже закрыт: ручку окна
    // держит обработчик в дереве острова, и последней её отпускает сам остров,
    // уходя вместе с приложением. Тогда снимать подписки уже не с кого -- вызов
    // бросает, а из деструктора исключению идти некуда.
    ~WindowState() {
        try {
            dropTitleBar();
            if (xamlRoot) xamlRoot.Changed(xamlRootChanged);
            if (nonClient) {
                nonClient.RegionsChanged(regionsChanged);
                nonClient.PointerEntered(captionEntered);
                nonClient.PointerExited(captionExited);
                nonClient.PointerPressed(captionPressed);
                nonClient.PointerReleased(captionReleased);
            }
        } catch (winrt::hresult_error const& error) {
            ::OutputDebugStringW((L"wxl::CompositionWindow: подписки не сняты: " +
                                  std::wstring{error.message()} + L"\n").c_str());
        }
    }

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

    float scale() const {
        return static_cast<float>(::GetDpiForWindow(hwnd)) / 96.0f * static_cast<float>(zoom);
    }

    void showPicture(muc::CompositionSurfaceBrush brush, BackgroundFill fill, Color color) {
        picture = std::move(brush);
        pictureFill = fill;
        pictureColor = color;
        fillPicture();
    }

    // Кисть задника из картинки. Сцена -- в физических пикселях, поэтому свой
    // размер картинки (None, плитка) доводится до логического множителем DPI.
    void fillPicture() {
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

        switch (pictureFill) {
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

                auto const edge = pictureFill == BackgroundFill::Tile
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

        bool const underlay = pictureColor.A != 0;
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
            brush.SetSourceParameter(L"colour", compositor.CreateColorBrush(asWinrt(pictureColor)));
        backdrop.Brush(brush);
    }

    ClientSize clientSize() const {
        RECT client{};
        ::GetClientRect(hwnd, &client);
        return {{client.right, client.bottom}, scale()};
    }

    windowing::AppWindow const& appWindowOf() {
        if (!appWindow) appWindow = windowing::AppWindow::GetFromWindowId(windowIdOf(hwnd));
        return appWindow;
    }

    input::InputNonClientPointerSource const& nonClientOf() {
        if (!nonClient) {
            nonClient = input::InputNonClientPointerSource::GetForWindowId(windowIdOf(hwnd));
            // AppWindow переписывает Minimize, Maximize и Close своими
            // прямоугольниками на каждое перемещение и активацию -- свои ставятся
            // снова. Событие приходит и на запись тех же чисел; пересчёт
            // сравнивает, прежде чем писать, иначе цикл.
            regionsChanged = nonClient.RegionsChanged([this](auto&&, auto&&) {
                if (ownButtonRegions) queueRegions();
            });
            // Над областью кнопки указатель у неклиентского ввода, не у XAML:
            // состояния кнопок ставятся отсюда.
            captionEntered = nonClient.PointerEntered(
                [this](auto&&, input::NonClientPointerEventArgs const& args) {
                    showCaptionState(args.RegionKind(), L"PointerOver");
                });
            captionExited = nonClient.PointerExited(
                [this](auto&&, input::NonClientPointerEventArgs const& args) {
                    showCaptionState(args.RegionKind(), L"Normal");
                });
            captionPressed = nonClient.PointerPressed(
                [this](auto&&, input::NonClientPointerEventArgs const& args) {
                    showCaptionState(args.RegionKind(), L"Pressed");
                });
            captionReleased = nonClient.PointerReleased(
                [this](auto&&, input::NonClientPointerEventArgs const& args) {
                    showCaptionState(args.RegionKind(), L"PointerOver");
                });
        }
        return nonClient;
    }

    // ---- Остров ----

    void ensureIsland() {
        if (chrome) return;
        chrome = xaml::Hosting::DesktopWindowXamlSource{};
        chrome.Initialize(windowIdOf(hwnd));

        root = controls::Grid{};
        for (auto const height : {xaml::GridLengthHelper::Auto(),
                                  xaml::GridLengthHelper::FromValueAndType(1, xaml::GridUnitType::Star)}) {
            controls::RowDefinition row;
            row.Height(height);
            root.RowDefinitions().Append(row);
        }
        root.Loaded([this](auto&&, auto&&) { watchXamlRoot(); });
        chrome.Content(root);

        RECT client{};
        ::GetClientRect(hwnd, &client);
        chrome.SiteBridge().MoveAndResize({0, 0, client.right, client.bottom});
        applyZoom();
    }

    // Смена масштаба острова -- DPI или увеличение -- меняет прямоугольники в
    // физических пикселях, а логический размер элементов остаётся, и
    // SizeChanged не придёт.
    void watchXamlRoot() {
        xaml::XamlRoot const current = root ? root.XamlRoot() : nullptr;
        if (!current || current == xamlRoot) return;
        if (xamlRoot) xamlRoot.Changed(xamlRootChanged);
        xamlRoot = current;
        xamlRootChanged = current.Changed([this](auto&&, auto&&) { queueRegions(); });
    }

    void applyZoom() {
        if (!chrome || (zoom == 1.0 && !zoomApplied)) return;
        zoomApplied = true;
        content::DesktopChildSiteBridge const bridge = chrome.SiteBridge();
        content::ContentSiteView const view = bridge.SiteView();
        float const parent = view.ParentScale();
        float const wanted = parent * static_cast<float>(zoom);
        bridge.OverrideScale(wanted);
        // Документация называет OverrideScale и масштабом, заменяющим масштаб
        // окна, и множителем к нему. На экране 100 % разницы нет; на другом
        // видно по итогу, и множитель ставится заново.
        float const got = view.RasterizationScale();
        if (std::abs(got - wanted) > 0.001f && std::abs(got - wanted * parent) <= 0.001f) {
            bridge.OverrideScale(static_cast<float>(zoom));
        }
    }

    void showPage(xaml::UIElement const& element) {
        ensureIsland();
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
    }

    // ---- Заголовок и кнопки окна ----

    controls::Button captionButton(wchar_t const* geometry, UINT command) {
        bool const close = command == SC_CLOSE;
        controls::Button button;
        auto const resources = xaml::Application::Current().Resources();
        auto const resource = [&resources](wchar_t const* key) {
            return resources.Lookup(winrt::box_value(winrt::hstring{key}));
        };
        button.Style(resource(L"WindowCaptionButton").as<xaml::Style>());
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
        // дважды. Click остаётся UI Automation: нажатие кнопки читалкой экрана
        // отдаёт ту же системную команду.
        button.IsHitTestVisible(false);
        button.Click([this, command](auto&&, auto&&) {
            if (!hwnd) return;
            UINT const given = command == SC_MAXIMIZE && ::IsZoomed(hwnd) ? SC_RESTORE : command;
            ::PostMessageW(hwnd, WM_SYSCOMMAND, given, 0);
        });
        return button;
    }

    void ensureBar() {
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

        captionButtons = controls::StackPanel{};
        captionButtons.Orientation(controls::Orientation::Horizontal);
        captionButtons.VerticalAlignment(xaml::VerticalAlignment::Stretch);
        controls::Grid::SetColumn(captionButtons, 1);

        minimizeButton = captionButton(L"M 0 0 L 10 0", SC_MINIMIZE);
        maximizeButton = captionButton(nullptr, SC_MAXIMIZE);
        closeButton = captionButton(L"M 0 0 L 9 9 M 9 0 L 0 9", SC_CLOSE);
        xaml::Automation::AutomationProperties::SetName(minimizeButton, systemMenuText(hwnd, SC_MINIMIZE));
        xaml::Automation::AutomationProperties::SetName(closeButton, systemMenuText(hwnd, SC_CLOSE));
        maximizeButton.Loaded([this](auto&&, auto&&) { showMaximized(); });
        for (controls::Button const& button : {minimizeButton, maximizeButton, closeButton}) {
            captionButtons.Children().Append(button);
        }
        captionButtons.SizeChanged([this](auto&&, auto&&) { queueRegions(); });
        bar.Children().Append(captionButtons);
    }

    void showCaptionState(input::NonClientRegionKind kind, std::wstring_view state) {
        controls::Button button{nullptr};
        switch (kind) {
            case input::NonClientRegionKind::Minimize: button = minimizeButton; break;
            case input::NonClientRegionKind::Maximize: button = maximizeButton; break;
            case input::NonClientRegionKind::Close: button = closeButton; break;
            default: return;
        }
        if (!button) return;
        std::wstring name{state};
        if (kind == input::NonClientRegionKind::Close && name != L"Normal") name = L"CloseButton" + name;
        xaml::VisualStateManager::GoToState(button, name, false);
    }

    void showMaximized() {
        if (!maximizeButton || !hwnd) return;
        bool const maximized = ::IsZoomed(hwnd) != 0;
        xaml::VisualStateManager::GoToState(
            maximizeButton, maximized ? L"WindowStateMaximized" : L"WindowStateNormal", false);
        xaml::Automation::AutomationProperties::SetName(
            maximizeButton, systemMenuText(hwnd, maximized ? SC_RESTORE : SC_MAXIMIZE));
    }

    void dropTitleBar() {
        if (!titleBar) return;
        titleBarSizeChanged.revoke();
        titleBar.UnregisterPropertyChangedCallback(xaml::UIElement::VisibilityProperty(), titleBarVisibility);
        if (titleBarBackgroundProperty) {
            titleBar.UnregisterPropertyChangedCallback(titleBarBackgroundProperty, titleBarBackground);
        }
        uint32_t at = 0;
        if (bar.Children().IndexOf(titleBar, at)) bar.Children().RemoveAt(at);
        titleBar = nullptr;
        titleBarBackgroundProperty = nullptr;
    }

    void setTitleBar(xaml::FrameworkElement const& element) {
        ensureIsland();
        dropTitleBar();
        if (element) {
            ensureBar();
            titleBar = element;
            controls::Grid::SetColumn(titleBar, 0);
            bar.Children().InsertAt(0, titleBar);

            titleBarSizeChanged = titleBar.SizeChanged(winrt::auto_revoke, [this](auto&&, auto&&) { queueRegions(); });
            titleBarVisibility = titleBar.RegisterPropertyChangedCallback(
                xaml::UIElement::VisibilityProperty(), [this](auto&&, auto&&) { updateCaptionMode(); });
            titleBarBackgroundProperty = backgroundPropertyOf(titleBar);
            if (titleBarBackgroundProperty) {
                titleBarBackground = titleBar.RegisterPropertyChangedCallback(
                    titleBarBackgroundProperty, [this](auto&&, auto&&) { copyTitleBarBackground(); });
            }
            copyTitleBarBackground();
        }
        updateCaptionMode();
    }

    void copyTitleBarBackground() {
        if (!captionButtons) return;
        captionButtons.Background(titleBarBackgroundProperty
                                      ? titleBar.GetValue(titleBarBackgroundProperty).try_as<xaml::Media::Brush>()
                                      : nullptr);
    }

    // Чьи кнопки окна сейчас: свои -- пока клиентская область под заголовком и
    // заголовок-элемент виден; иначе системные, как у Window.
    void updateCaptionMode() {
        bool const ownButtons = extendsContentIntoTitleBar && titleBar &&
                                titleBar.Visibility() == xaml::Visibility::Visible;
        if (bar) {
            bar.Visibility(titleBar ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);
            captionButtons.Visibility(ownButtons ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);
        }
        if (extendsContentIntoTitleBar) {
            windowing::AppWindowTitleBar const system = appWindowOf().TitleBar();
            if (titleBar) {
                if (!systemButtonsHeight) systemButtonsHeight = system.PreferredHeightOption();
                system.PreferredHeightOption(windowing::TitleBarHeightOption::Collapsed);
            } else if (systemButtonsHeight) {
                system.PreferredHeightOption(*systemButtonsHeight);
                systemButtonsHeight.reset();
            }
        }
        queueRegions();
    }

    // Пересчёт после вёрстки: и размер элемента, и размер окна приходят раньше,
    // чем XAML их разложил.
    void queueRegions() {
        if (regionsQueued || !hwnd) return;
        regionsQueued = true;
        core::intrusive_ptr<WindowState> const self{this};
        mud::DispatcherQueue::GetForCurrentThread().TryEnqueue(
            mud::DispatcherQueuePriority::Low, [self] { self->updateRegions(); });
    }

    graphics::RectInt32 physicalRectOf(xaml::FrameworkElement const& element) const {
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

    void updateRegions() {
        regionsQueued = false;
        if (!hwnd || ::IsIconic(hwnd)) return;
        watchXamlRoot();

        bool const own = extendsContentIntoTitleBar && titleBar &&
                         titleBar.Visibility() == xaml::Visibility::Visible;

        // Caption -- как CWindowChrome::SetDragRegion у Window: убирается свой
        // прежний прямоугольник, ставится новый, чужие остаются.
        graphics::RectInt32 const wanted = own ? physicalRectOf(titleBar) : graphics::RectInt32{};
        if (!sameRect(wanted, caption) && (own || caption.Width != 0 || caption.Height != 0)) {
            winrt::com_array<graphics::RectInt32> const current =
                nonClientOf().GetRegionRects(input::NonClientRegionKind::Caption);
            std::vector<graphics::RectInt32> rects;
            rects.reserve(current.size() + 1);
            for (graphics::RectInt32 const& rect : current) {
                if (!sameRect(rect, caption)) rects.push_back(rect);
            }
            if (wanted.Width > 0 && wanted.Height > 0) rects.push_back(wanted);
            nonClient.SetRegionRects(input::NonClientRegionKind::Caption, rects);
            caption = wanted;
        }

        if (!own) {
            if (ownButtonRegions) {
                for (auto const kind : {input::NonClientRegionKind::Minimize, input::NonClientRegionKind::Maximize,
                                        input::NonClientRegionKind::Close}) {
                    nonClient.ClearRegionRects(kind);
                }
                ownButtonRegions = false;
            }
            return;
        }

        ownButtonRegions = true;
        auto const apply = [this](input::NonClientRegionKind kind, graphics::RectInt32 rect) {
            winrt::com_array<graphics::RectInt32> const current = nonClientOf().GetRegionRects(kind);
            if (current.size() == 1 && sameRect(current[0], rect)) return;
            nonClient.SetRegionRects(kind, {rect});
        };
        apply(input::NonClientRegionKind::Minimize, physicalRectOf(minimizeButton));
        apply(input::NonClientRegionKind::Maximize, physicalRectOf(maximizeButton));
        apply(input::NonClientRegionKind::Close, physicalRectOf(closeButton));
    }
};

namespace {

WindowState* stateOf(HWND hwnd) noexcept {
    return reinterpret_cast<WindowState*>(::GetWindowLongPtrW(hwnd, GWLP_USERDATA));
}

// Клиентский прямоугольник, обросший рамкой. Толщину отвечает само окно на
// WM_NCCALCSIZE -- тем же путём, каким Windows получает его клиентскую область.
// AdjustWindowRectEx считает рамку по стилю и не знает, что заголовок отдан
// клиенту (ExtendsContentIntoTitleBar правит ответ на WM_NCCALCSIZE), а стиль и
// DPI монитора, на котором окно сейчас, ответ учитывает сам. Меряется на
// нынешнем прямоугольнике окна, у свёрнутого -- на месте, куда оно вернётся.
RECT withFrame(HWND hwnd, RECT client) noexcept {
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

LRESULT CALLBACK windowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    WindowState* state = stateOf(hwnd);
    switch (message) {
        case WM_ERASEBKGND:
            return 1;

        case WM_GETMINMAXINFO:
            // Нижний предел -- в размерах клиента; переводим в размеры окна
            // его рамкой. Сообщение приходит и при создании, до установки
            // USERDATA: тогда state == nullptr, предел не наложен -- окну и так
            // задан CW_USEDEFAULT.
            if (state && (state->minSize.width > 0 || state->minSize.height > 0)) {
                RECT const frame = withFrame(hwnd, {0, 0, state->minSize.width, state->minSize.height});
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
                state->clientSizeChanged.fire(
                    {{static_cast<int32_t>(LOWORD(lparam)), static_cast<int32_t>(HIWORD(lparam))},
                     state->scale()});
                // Максимизация и восстановление меняют место без перетаскивания
                // (WM_EXITSIZEMOVE не придёт) -- отмечаем их здесь. Растяжка за
                // рамку тоже сюда попадает, но сохранение места отложено, так что
                // лишние отметки во время тяги схлопнутся в одну запись.
                if (wparam == SIZE_MAXIMIZED || wparam == SIZE_RESTORED) {
                    state->geometryChanged.fire(Object::Impl::empty<Object>());
                }
                state->showMaximized();
                state->queueRegions();
            }
            break;

        case WM_MOVE:
            if (state) state->queueRegions();
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
                state->applyZoom();
                state->fillPicture();
                return 0;
            }
            break;

        case WM_EXITSIZEMOVE:
            // Конец перетаскивания рамки: и сдвиг, и растяжка кончаются здесь.
            if (state) state->geometryChanged.fire(Object::Impl::empty<Object>());
            return 0;

        case WM_KEYDOWN:
            // Клавиатура у чистой сцены доходит сюда (в отличие от указателя,
            // которого перехватывает InputSite острова). Отдаём код клавиши
            // обёрткой wxl -- тем же VirtualKey, что раздаёт XAML. Не return 0:
            // пусть DefWindowProc довершит своё (WM_CHAR, системные клавиши).
            if (state) state->keyDown.fire(static_cast<VirtualKey>(static_cast<int32_t>(wparam)));
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
            if (state) state->closed.fire(Object::Impl::empty<Object>());
            ::DestroyWindow(hwnd);
            return 0;

        case WM_DESTROY:
            // Своё окно закрылось -- гасим приложение; после этого
            // Application::Start возвращается и отрабатывает Teardown.
            xaml::Application::Current().Exit();
            return 0;

        case WM_NCDESTROY:
            // Последнее сообщение окна: оно отпускает своё состояние. Живут ещё
            // ручки -- состояние остаётся им, без окна под собой.
            if (state) {
                ::SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
                state->hwnd = nullptr;
                intrusive_ptr_release(state);
            }
            break;
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
// вернувшись на UI-поток, ставит её картинкой фона -- если за время загрузки фон
// не сменили. Состояние держится ручкой, картинка -- по значению: корутина
// владеет обоими через ожидание.
fire_and_forget swapBackground(core::intrusive_ptr<WindowState> state, BackgroundImage image,
                               std::uint64_t change) {
    CompositionDrawingSurface const surface = co_await state->textureCache->getAsync(image.path);
    if (state->backgroundChange != change) co_return;

    // Именованные промежутки: обёртка -> сырая поверхность (одно преобразование),
    // сырая поверхность -> ICompositionSurface у CreateSurfaceBrush (второе); в
    // одном выражении их было бы два подряд, чего аргументу не дают.
    muc::CompositionDrawingSurface const& raw =
        *Object::Impl::get_typed<CompositionDrawingSurface>(surface);
    state->showPicture(state->compositor.CreateSurfaceBrush(raw), image.fill, image.color);
}

}  // namespace

// ---- Сцена: задний фон и страница ------------------------------------------

Compositor CompositionWindow::compositor() const {
    return Object::Impl::wrap<Compositor>(state_->compositor);
}

ContainerVisual CompositionWindow::contentVisual() const {
    return Object::Impl::wrap<ContainerVisual>(state_->backdrop);
}

SizeInt32 CompositionWindow::clientSize() const { return state_->clientSize().size; }

float CompositionWindow::rasterizationScale() const {
    // DPI окна, а не системного: окно может стоять на мониторе с другим
    // масштабом, и физические пиксели поверхности считаются по нему.
    return state_->scale();
}

void CompositionWindow::background(Color color) const {
    ++state_->backgroundChange;
    state_->picture = nullptr;
    state_->backdrop.Brush(state_->compositor.CreateColorBrush(asWinrt(color)));
}

void CompositionWindow::clearBackground() const {
    // Спрайт без кисти ничего не рисует, но остаётся корнем сцены и
    // контейнером визуалов приложения -- дерево не меняется, меняется только
    // то, что у него нет своей заливки.
    ++state_->backgroundChange;
    state_->picture = nullptr;
    state_->backdrop.Brush(nullptr);
}

void CompositionWindow::background(std::filesystem::path const& image) const {
    background(BackgroundImage{image});
}

void CompositionWindow::background(BackgroundImage const& image) const {
    // Синхронный декод: первый (стартовый) экран показывается только уже
    // загруженным -- окно до того скрыто. Смены фона на ходу идут асинхронно
    // через Win2D/TextureCache (backgroundAsync).
    std::filesystem::path const file = impl::beside_application(image.path);

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
    ++state_->backgroundChange;
    state_->showPicture(*Object::Impl::get_typed<CompositionSurfaceBrush>(wrapped), image.fill,
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
    ++state_->backgroundChange;
    state_->picture = nullptr;
    state_->backdrop.Brush(brush);
}

void CompositionWindow::backgroundAsync(std::filesystem::path const& image) const {
    backgroundAsync(BackgroundImage{image});
}

void CompositionWindow::backgroundAsync(BackgroundImage const& image) const {
    // Запускаем и забываем: swapBackground сам доведёт себя до конца на UI-потоке
    // и уберёт свой кадр. Кэш держит текстуру, так что тот же путь во второй раз
    // сменит задник уже без загрузки.
    swapBackground(state_, {impl::beside_application(image.path), image.fill, image.color},
                   ++state_->backgroundChange);
}

// ---- CompositionWindow ----------------------------------------------------

CompositionWindow::CompositionWindow() : state_{new WindowState(), false} {
    ensureClass();

    state_->hwnd = ::CreateWindowExW(WS_EX_NOREDIRECTIONBITMAP, kClassName, L"", WS_OVERLAPPEDWINDOW,
                                     CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                                     nullptr, nullptr, ::GetModuleHandleW(nullptr), nullptr);
    // Своя ссылка окна на состояние -- до WM_NCDESTROY: оконная процедура
    // находит его по USERDATA, и отпущенная последняя ручка не освободит его
    // под живым окном.
    intrusive_ptr_add_ref(state_.get());
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
    // и не нужен. Подписки простые, без auto_revoke: источник живёт в состоянии
    // столько же, сколько оно само.
    WindowState* const st = state_.get();
    st->pointerInput = input::InputPointerSource::GetForIsland(st->sceneIsland);
    st->pointerInput.PointerPressed(
        [st](input::InputPointerSource const&, input::PointerEventArgs const& args) {
            st->pointerPressed.fire(Object::Impl::wrap<PointerPoint>(args.CurrentPoint()));
        });
    st->pointerInput.PointerMoved(
        [st](input::InputPointerSource const&, input::PointerEventArgs const& args) {
            st->pointerMoved.fire(Object::Impl::wrap<PointerPoint>(args.CurrentPoint()));
        });
    st->pointerInput.PointerReleased(
        [st](input::InputPointerSource const&, input::PointerEventArgs const& args) {
            st->pointerReleased.fire(Object::Impl::wrap<PointerPoint>(args.CurrentPoint()));
        });
    st->pointerInput.PointerWheelChanged(
        [st](input::InputPointerSource const&, input::PointerEventArgs const& args) {
            st->pointerWheelChanged.fire(Object::Impl::wrap<PointerPoint>(args.CurrentPoint()));
        });

    RECT client{};
    ::GetClientRect(state_->hwnd, &client);
    state_->resize(static_cast<float>(client.right), static_cast<float>(client.bottom));
}

CompositionWindow::CompositionWindow(std::wstring_view title, SizeInt32 minSize) : CompositionWindow() {
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
        state_->showPage(nullptr);
        return;
    }
    xaml::UIElement const& raw = *Object::Impl::get_typed<UIElement>(chrome);
    state_->showPage(raw);
}

UIElement CompositionWindow::content() const {
    return Object::Impl::wrap<UIElement>(state_->page);
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
    // Прячем остров и снимаем с него содержимое: в режиме чтения ввод должен
    // идти сцене, а пустой, но показанный остров перехватывал бы его над собой.
    if (state_->chrome) {
        state_->showPage(nullptr);
        state_->chrome.SiteBridge().Hide();
    }
}

void CompositionWindow::zoom(double value) const {
    if (value <= 0 || value == state_->zoom) return;
    state_->zoom = value;
    state_->applyZoom();
    state_->clientSizeChanged.fire(state_->clientSize());
    state_->queueRegions();
}

double CompositionWindow::zoom() const { return state_->zoom; }

// ---- Заголовок окна ---------------------------------------------------------

void CompositionWindow::extendsContentIntoTitleBar(bool value) const {
    // Флаг до вызова: ExtendsContentIntoTitleBar двигает рамку синхронно, и
    // WM_SIZE, который приходит изнутри него, уже пересчитывает области.
    state_->extendsContentIntoTitleBar = value;

    windowing::AppWindowTitleBar const titleBar = state_->appWindowOf().TitleBar();
    titleBar.ExtendsContentIntoTitleBar(value);

    // Под системными кнопками окна видна своя полоса -- их фон прозрачный. Как
    // у Window, один раз за жизнь окна: цвет, который приложение поставило
    // позже, повторное включение не затирает.
    if (value && !state_->captionButtonsTransparent) {
        state_->captionButtonsTransparent = true;
        winrt::Windows::Foundation::IReference<winrt::Windows::UI::Color> const transparent{
            winrt::Windows::UI::Color{0x00, 0xFF, 0xFF, 0xFF}};
        titleBar.ButtonBackgroundColor(transparent);
        titleBar.ButtonInactiveBackgroundColor(transparent);
    }

    // Рамка пересчитывается сразу: окно, созданное скрытым, иначе показалось
    // бы с клиентской областью прежнего размера.
    ::SetWindowPos(state_->hwnd, nullptr, 0, 0, 0, 0,
                   SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_NOACTIVATE |
                       SWP_FRAMECHANGED);
    state_->updateCaptionMode();
}

bool CompositionWindow::extendsContentIntoTitleBar() const {
    return state_->extendsContentIntoTitleBar;
}

void CompositionWindow::titleBar(UIElement const& value) const {
    if (!value) {
        state_->setTitleBar(nullptr);
        return;
    }
    xaml::UIElement const& raw = *Object::Impl::get_typed<UIElement>(value);
    state_->setTitleBar(raw.as<xaml::FrameworkElement>());
}

AppWindow CompositionWindow::appWindow() const {
    return Object::Impl::wrap<AppWindow>(state_->appWindowOf());
}

// ---- Само окно ----------------------------------------------------------------

void CompositionWindow::title(string_param value) const {
    std::wstring const text{value.wide()};
    ::SetWindowTextW(state_->hwnd, text.c_str());
}

wstring CompositionWindow::title() const {
    int const length = ::GetWindowTextLengthW(state_->hwnd);
    wstring text(static_cast<std::size_t>(length) + 1, L'\0');
    ::GetWindowTextW(state_->hwnd, reinterpret_cast<wchar_t*>(text.data()), length + 1);
    text.resize(static_cast<std::size_t>(length));
    return text;
}

void CompositionWindow::minSize(SizeInt32 const& value) const { state_->minSize = value; }

void CompositionWindow::activate() const {
    ::ShowWindow(state_->hwnd, SW_SHOW);
    ::UpdateWindow(state_->hwnd);
}

void CompositionWindow::resize(SizeInt32 size) const {
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

}  // namespace

void CompositionWindow::centreWithClientSize(SizeInt32 client) const {
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

void CompositionWindow::placement(std::wstring_view saved) const {
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

void CompositionWindow::fullScreen(bool on) const {
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

void CompositionWindow::darkFrame(bool on) const { impl::frame_dark(state_->hwnd, on); }

void CompositionWindow::captionColor(Color caption, Color text) const {
    // DWM берёт COLORREF -- 0x00BBGGRR, без альфы.
    auto const colorref = [](Color color) {
        return static_cast<uint32_t>(color.R | (color.G << 8) | (color.B << 16));
    };
    impl::frame_caption(state_->hwnd, colorref(caption), colorref(text));
}

void CompositionWindow::close() const {
    // PostMessage, а не Send: закрытие встаёт в очередь, как системный крестик,
    // и разбирается тем же WM_CLOSE (Closed -> DestroyWindow -> WM_DESTROY).
    ::PostMessageW(state_->hwnd, WM_CLOSE, 0, 0);
}

void CompositionWindow::acceptFileDrops(std::function<void(std::filesystem::path)> handler) const {
    state_->onFileDrop = std::move(handler);
    ::DragAcceptFiles(state_->hwnd, TRUE);
}

// ---- События ----------------------------------------------------------------

EventToken CompositionWindow::add_onClosed(EventHandler<Object> const& handler) const {
    return state_->closed.add(handler);
}
void CompositionWindow::remove_onClosed(EventToken token) const { state_->closed.remove(token); }

EventToken CompositionWindow::add_onGeometryChanged(EventHandler<Object> const& handler) const {
    return state_->geometryChanged.add(handler);
}
void CompositionWindow::remove_onGeometryChanged(EventToken token) const {
    state_->geometryChanged.remove(token);
}

EventToken CompositionWindow::add_onClientSizeChanged(EventHandler<ClientSize> const& handler) const {
    return state_->clientSizeChanged.add(handler);
}
void CompositionWindow::remove_onClientSizeChanged(EventToken token) const {
    state_->clientSizeChanged.remove(token);
}

EventToken CompositionWindow::add_onKeyDown(EventHandler<VirtualKey> const& handler) const {
    return state_->keyDown.add(handler);
}
void CompositionWindow::remove_onKeyDown(EventToken token) const { state_->keyDown.remove(token); }

EventToken CompositionWindow::add_onPointerPressed(EventHandler<PointerPoint> const& handler) const {
    return state_->pointerPressed.add(handler);
}
void CompositionWindow::remove_onPointerPressed(EventToken token) const {
    state_->pointerPressed.remove(token);
}

EventToken CompositionWindow::add_onPointerMoved(EventHandler<PointerPoint> const& handler) const {
    return state_->pointerMoved.add(handler);
}
void CompositionWindow::remove_onPointerMoved(EventToken token) const {
    state_->pointerMoved.remove(token);
}

EventToken CompositionWindow::add_onPointerReleased(EventHandler<PointerPoint> const& handler) const {
    return state_->pointerReleased.add(handler);
}
void CompositionWindow::remove_onPointerReleased(EventToken token) const {
    state_->pointerReleased.remove(token);
}

EventToken CompositionWindow::add_onPointerWheelChanged(EventHandler<PointerPoint> const& handler) const {
    return state_->pointerWheelChanged.add(handler);
}
void CompositionWindow::remove_onPointerWheelChanged(EventToken token) const {
    state_->pointerWheelChanged.remove(token);
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

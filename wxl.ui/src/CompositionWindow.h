#pragma once

#include "generated/Microsoft.UI.Composition.h"
#include "generated/Microsoft.UI.Dispatching.h"
#include "generated/Microsoft.UI.Input.h"
#include "generated/Microsoft.UI.Windowing.h"
#include "generated/Microsoft.UI.Xaml.Controls.h"
#include "generated/Microsoft.UI.Xaml.h"
#include "generated/Windows.System.Enums.h"

#include "Color.h"
#include "DrawingSurface.h"
#include "events.h"
#include "string_param.h"
#include "impl/member.h"

#include <filesystem>
#include <functional>
#include <string>
#include <string_view>

// wxl::CompositionWindow -- своё окно верхнего уровня, чьи пиксели кладёт
// композитор, а не GDI.
//
// Зачем оно, если у wxl уже есть генерируемый `Window` (проекция
// Microsoft.UI.Xaml.Window): у того верхнее окно перенаправляемое, и при
// быстрой растяжке за угол в просвете белеет его поверхность перенаправления,
// стёртая системной кистью. Убрать это на нём нельзя -- стиль после создания
// не сменить. Здесь окно создаётся своими руками с WS_EX_NOREDIRECTIONBITMAP:
// поверхности перенаправления нет вовсе, белому взяться неоткуда, а содержимое
// целиком даёт композитор.
//
// Отсюда два слоя, и окно признаёт их прямо:
//
//   * СЦЕНА -- визуалы композитора под всем: единственный задний фон
//     (`background`) и страница на композиторе окна (`compositor`,
//     `contentVisual`), которую приложение рисует само. Они ресайзятся
//     синхронно в WM_SIZE, оттого держатся за рамку без отставания -- то, чего
//     XAML со своей отложенной вёрсткой не даёт (проверено).
//   * ОСНАСТКА (`content()`) -- XAML приходит островом поверх сцены:
//     прозрачным, во всё окно. Списки, поля, диалоги -- всё, чем силён WinUI,
//     остаётся на нём; сквозь прозрачные места острова видна сцена под ним.
//
// Окно -- ручка к общему состоянию, как всякая обёртка wxl: копия смотрит в то
// же окно, все члены const, и лямбда захватывает окно по значению. Поэтому оно
// и пишется в скобках, как контрол:
//
//     CompositionWindow window{
//         title = L"Беседка",
//         minSize = {820, 560},
//         extendsContentIntoTitleBar = true,
//         titleBar = {leftHeader = ..., content = ..., rightHeader = ...},
//         onClosed = [] { ... },
//         page,
//     };
//
// Само окно Windows держит своё состояние до WM_NCDESTROY: отпущенная последняя
// ручка не освобождает его под живым окном.
//
// Окно не самостоятельное приложение: его создают внутри уже поднятого wxl
// (`wxl_launched`), на его STA-потоке и его цикле сообщений. Ни загрузчика, ни
// своего RunEventLoop здесь нет -- всё это принадлежит `launch`.
//
// Прозрачность по умолчанию. Без поверхности перенаправления неокрашенное окно
// сквозит на рабочий стол, поэтому сцена всегда что-то рисует: приложение
// ставит задний фон (цвет темы или картинку) до показа окна. wxl про темы
// чтения не знает, поэтому цвет и картинку диктует приложение. Обратный случай
// -- собственные визуалы приложения кроют окно целиком: тогда задний фон
// снимается вовсе (`clearBackground`), потому что закрашивать под ними нечего,
// а заливка, которую никто не видит, -- всё равно работа композитора на каждом
// кадре. Сквозить при этом нельзя ни одному пикселю, в том числе в просвете
// быстрой растяжки: визуалы, кроющие окно, берут размер у сцены выражением
// (ExpressionAnimation), а не из отложенной вёрстки острова.

// Объявлено, не включено: держится хэндл окна, а вызывающему, которому нужно
// лишь его открыть, незачем разбирать заголовок Windows. Так же у WindowShade.
struct HWND__;

namespace wxl {

// Общее состояние окна: HWND, острова, визуалы сцены, обработчики. Определено в
// CompositionWindow.cpp; окно смотрит в него и делит его с копиями.
struct WindowState;

/// Что приносит ClientSizeChanged: клиентская область в физических пикселях и
/// масштаб, которым их делят, чтобы получить логические, -- DPI экрана с
/// увеличением окна (`zoom`) в нём.
struct ClientSize {
    SizeInt32 size;
    float scale;
};

/// Как картинка заднего фона кроет окно. Свой размер картинки -- в логических
/// пикселях: на экране 150 % она крупнее в физических и мельче не становится.
enum class BackgroundFill {
    UniformToFill,  ///< всё окно, пропорции целы: обрезана по краям, по центру
    Uniform,        ///< вся картинка, пропорции целы: поля по сторонам, по центру
    Fill,           ///< всё окно, пропорции теряются
    None,           ///< своего размера, по центру
    Tile,           ///< своего размера, плиткой от левого верхнего угла
    TileMirrored,   ///< плиткой, где каждая вторая копия отражена и швы сходятся
};

/// Картинка заднего фона: файл, то, как она кроет окно, и цвет под ней.
/// Относительный путь ищется рядом с исполняемым файлом, как источник Image:
///
///     background = BackgroundImage{u"Assets/paper.png", BackgroundFill::Tile}
///
/// Цвет виден там, где картинка окна не кроет (None, Uniform), и сквозь её
/// прозрачные места. Прозрачный по умолчанию -- и там окно сквозит.
struct BackgroundImage {
    std::filesystem::path path;
    BackgroundFill fill = BackgroundFill::UniformToFill;
    Color color{};
};

class CompositionWindow {
public:
    /// Заводит окно: класс без кисти фона, WS_EX_NOREDIRECTIONBITMAP, свой
    /// композитор, цель на HWND. Окно создаётся скрытым -- показывает его
    /// activate(), после того как приложение поставило фон и оснастку.
    CompositionWindow();

    /// То же с именем и нижним пределом клиентской области.
    CompositionWindow(std::wstring_view title, SizeInt32 minSize);

    /// Окно в скобках декларативного синтаксиса: свойства, события и
    /// безымянные аргументы -- содержимое и заголовок -- применяются по порядку.
    template <typename... Setters>
        requires(sizeof...(Setters) > 0) && impl::setter_pack<CompositionWindow, Setters...> &&
                (impl::applicable_to<Setters, CompositionWindow> && ...)
    explicit CompositionWindow(Setters&&... setters) : CompositionWindow() {
        (impl::apply_argument(*this, std::forward<Setters>(setters)), ...);
    }

    ~CompositionWindow();
    CompositionWindow(CompositionWindow const&) noexcept;
    CompositionWindow(CompositionWindow&&) noexcept;
    CompositionWindow& operator=(CompositionWindow const&) noexcept;
    CompositionWindow& operator=(CompositionWindow&&) noexcept;

    // ---- Сцена: задний фон и страница на композиторе окна ----
    //
    // Визуалы под XAML-островом: единственный задний фон и страница, которую
    // приложение рисует само. Ресайзятся синхронно в WM_SIZE, оттого держатся
    // за рамку без отставания. Об экранах и странице окно не знает: приложение
    // ставит фон и цепляет свои визуалы к contentVisual().

    /// Композитор окна (Microsoft.UI) -- на нём приложение строит визуалы
    /// страницы, тот же, что несёт задний фон.
    Compositor compositor() const;

    /// Задний фон -- ровным цветом темы. Ставит кисть на ЕДИНСТВЕННЫЙ задний
    /// визуал, вытесняя прежнюю: правило одного задника выходит само.
    void background(Color color) const;

    /// Задний фон снят вовсе: визуалы приложения на contentVisual() кроют окно
    /// целиком, и красить под ними нечего -- даже ровный цвет был бы лишней
    /// заливкой на каждом кадре. Обязанность приложения -- не оставить ни
    /// одного непокрытого пикселя (окно сквозит на рабочий стол) и при уходе с
    /// такого экрана вернуть фон тем же кадром.
    void clearBackground() const;

    /// Задний фон -- картинкой из файла. wxl декодирует её (WIC) в
    /// композиторную поверхность синхронно: первый экран показывается уже с
    /// ней. Путь -- как у BackgroundImage; без способа картинка кроет окно
    /// UniformToFill.
    void background(BackgroundImage const& image) const;
    void background(std::filesystem::path const& image) const;

    /// Задний фон -- готовой поверхностью: так подаётся уже нарисованное,
    /// например изогнутый снимок обложки или свёрстанная страница книги.
    void background(DrawingSurface const& surface) const;

    /// Задний фон -- картинкой, загруженной асинхронно (Win2D/TextureCache):
    /// декод идёт на потоках WinRT, задник сменится, когда картинка готова, а
    /// UI не подвисает. Под смену фона на ходу; первый (стартовый) экран ставит
    /// синхронный background(path). Запускается и забывается; фон, поставленный
    /// позже, загрузка не перебивает.
    void backgroundAsync(BackgroundImage const& image) const;
    void backgroundAsync(std::filesystem::path const& image) const;

    /// Куда приложение цепляет визуалы страницы -- над задним фоном, под
    /// островом. Контейнер во всё окно, ресайзится вместе с ним.
    ContainerVisual contentVisual() const;

    /// Размер клиентской области окна в физических пикселях и масштаб, которым
    /// их делят на логические: DPI экрана (DPI/96) с увеличением окна (`zoom`).
    /// Страница живёт задником окна (сценой), не в острове, поэтому и меру берёт
    /// отсюда: острова при автооткрытии на старте ещё нет, спрашивать не у кого.
    SizeInt32 clientSize() const;
    float rasterizationScale() const;

    // ---- Оснастка: XAML-остров поверх сцены ----

    /// XAML-оснастка островом поверх сцены -- по требованию: оглавление,
    /// настройки, визард обложки, витрина. Ставит содержимое острова и
    /// показывает его; пока остров показан, ввод идёт ему. В режиме чтения
    /// острова нет вовсе (hideContent), и ввод идёт со сцены -- см. onKeyDown и
    /// onPointer* ниже. Если у окна есть заголовок (`titleBar`), содержимое
    /// стоит под ним.
    void content(UIElement const& chrome) const;

    /// Что сейчас в острове под заголовком -- или пусто, если оснастки нет
    /// (режим чтения). Читалке нужно, чтобы спросить у показанного экрана его
    /// XamlRoot: размер и масштаб, под которые готовить страницу до показа.
    UIElement content() const;

    /// Безымянный элемент в скобках окна -- его содержимое; безымянный
    /// TitleBar -- его заголовок, как у генерируемого Window.
    void setPositional(UIElement const& value) const { content(value); }
    void setPositional(TitleBar const& value) const { titleBar(value); }

    /// Композитор XAML-острова -- тот, на котором строят визуалы экраны,
    /// показанные через content(). НЕ сценовый: сцена под островом живёт на
    /// своём (compositor()), а XAML-дерево острова -- на потоковом
    /// композиторе XAML. Визуалы, которые экран вешает в собственное дерево
    /// (ElementCompositionPreview::setElementChildVisual), обязаны быть отсюда:
    /// объекты композиции привязаны к своему композитору, и визуал с чужого
    /// в дерево не встанет. Когда страница чтения станет визуалом сцены, она
    /// пойдёт на compositor(); пока экраны -- острова, их визуалы отсюда.
    Compositor chromeCompositor() const;

    /// Прячет XAML-остров: режим чтения. Скрытый остров ввод не перехватывает --
    /// клавиши, щелчки и колесо идут сцене (onKeyDown / onPointer*).
    void hideContent() const;

    /// Увеличение всего острова -- заголовка, кнопок окна, панелей и страниц
    /// разом, как масштаб страницы в браузере: элементы крупнее, а логическая
    /// ширина окна меньше, и вёрстка переливается под неё. 1 -- как задал
    /// экран; DPI экрана остаётся в множителе и при переезде окна на другой
    /// монитор. Сцена под островом не увеличивается: она в физических пикселях.
    void zoom(double value) const;
    double zoom() const;

    // ---- Заголовок окна ----
    //
    // Как у Microsoft.UI.Xaml.Window: extendsContentIntoTitleBar отдаёт полосу
    // заголовка клиентской области, заголовок -- элемент, за который таскают
    // окно. В отличие от Window, элемент ставит на место само окно -- строкой
    // над содержимым, -- поэтому он и пишется прямо в скобках окна:
    //
    //     titleBar = {leftHeader = ..., content = ..., rightHeader = ...}
    //
    // Кнопки окна рядом с ним рисует тоже окно, в XAML: встроенным стилем
    // WindowCaptionButton, высотой полосы заголовка и с её масштабом --
    // системные кнопки AppWindow увеличению острова не следуют. Всё
    // системное поведение у них своё: раскладки Windows 11 над «развернуть»,
    // подсказки, команды по щелчку -- их дают области Minimize, Maximize и
    // Close. Интерактивные элементы внутри полосы получают ввод, только если
    // над ними вырезаны области Passthrough; это делает контрол TitleBar.

    /// Как Window.ExtendsContentIntoTitleBar: клиентская область поднимается
    /// под заголовок, системный заголовок пропадает. Пока заголовка-элемента
    /// нет, кнопки окна системные, как у Window; с ним -- свои. Звать до
    /// показа окна, иначе системный заголовок успеет мелькнуть.
    void extendsContentIntoTitleBar(bool value) const;
    bool extendsContentIntoTitleBar() const;

    /// Заголовок окна: элемент встаёт строкой над содержимым, рядом -- кнопки
    /// окна его высоты, а сам он становится местом, за которое таскают окно
    /// (область Caption, следом за его размером, размером окна и масштабом).
    /// Элемент не должен стоять в другом дереве. Пустая обёртка убирает
    /// заголовок; скрытый (Collapsed) заголовок прячет и кнопки.
    void titleBar(UIElement const& value) const;

    /// То же под именем Window.SetTitleBar.
    void setTitleBar(UIElement const& value) const { titleBar(value); }

    /// Как Window.AppWindow: объект Windows App SDK над этим же HWND -- место,
    /// размер, значок и представление окна.
    AppWindow appWindow() const;

    // ---- Само окно ----

    /// Текст окна: его Windows показывает на панели задач и в Alt+Tab.
    void title(string_param value) const;
    wstring title() const;

    /// Нижний предел клиентской области -- в пикселях клиента, рамку окно
    /// прибавляет само.
    void minSize(SizeInt32 const& value) const;

    /// Показывает окно. До него окно скрыто, чтобы не мигнуть прозрачностью и
    /// не показать себя раньше, чем встали место и содержимое.
    void activate() const;

    /// Задать размер окна, не трогая положения: начальный размер до
    /// восстановления запомненного места (иначе окно открылось бы системным
    /// CW_USEDEFAULT).
    void resize(SizeInt32 size) const;

    /// Ставит окно посреди своего экрана, дав ему клиентскую область ровно
    /// такого размера.
    ///
    /// Клиентскую, а не оконную: содержимое живёт в ней, и окно, которому
    /// задали пропорции картинки, показало бы её кадрированной ровно на рамку
    /// с заголовком. Толщину рамки окно меряет само, тем же WM_NCCALCSIZE,
    /// которым Windows получает его клиентскую область, -- так в неё входит и
    /// заголовок, отданный клиенту (extendsContentIntoTitleBar), и приложению
    /// не приходится вычитать клиентскую сторону из оконной, чтобы её угадать.
    ///
    /// Одним движением, а не «сначала размер, потом место»: показанное окно
    /// иначе прыгнуло бы дважды.
    void centreWithClientSize(SizeInt32 client) const;

    /// Наибольшая клиентская область, какая помещается на экране этого окна:
    /// рабочая область монитора (без панели задач) за вычетом рамки.
    ///
    /// Ответ на вопрос «сколько у меня места», заданный в тех же единицах, в
    /// каких его зададут обратно, -- заставке, которая берёт пропорции своей
    /// картинки и растёт, насколько экран пускает. Ни рабочей области, ни
    /// толщины рамки приложению для этого знать не надо.
    SizeInt32 maxClientSize() const;

    /// Хэндл окна -- для системных диалогов, которым нужен владелец.
    HWND__* handle() const;

    /// Очередь STA-потока -- та же, на которой поднят wxl; ею фоновый поток
    /// возвращает работу интерфейсному.
    DispatcherQueue dispatcherQueue() const;

    /// Запомненное место окна (та же строка, что у генерируемого Window): при
    /// восстановлении проверяются мониторы, при отсутствии строки не делается
    /// ничего.
    void placement(std::wstring_view saved) const;
    std::wstring placement() const;

    /// Полноэкранный режим -- на своём HWND через Win32 (стиль и рамка), а не
    /// через presenter WinUI, которого у своего окна нет.
    bool fullScreen() const;
    void fullScreen(bool on) const;

    /// Тёмная рамка: заголовок и его кнопки. Рамку красит DWM, о теме острова
    /// он не знает, и приложение, сменившее тему, говорит ему отдельно.
    void darkFrame(bool on) const;

    /// Цвет заголовка и текста в нём. Есть с Windows 11; там, где DWM его не
    /// знает, вызов ничего не делает, и рамка остаётся системной.
    void captionColor(Color caption, Color text) const;

    /// Закрывает окно, как системный крестик -- через WM_CLOSE, так что Closed
    /// успевает сохранить, а следом приложение гаснет. Для «Выйти из читалки».
    void close() const;

    /// Книга, брошенная в окно (DragAcceptFiles/WM_DROPFILES): путь к файлу.
    void acceptFileDrops(std::function<void(std::filesystem::path)> handler) const;

    // ---- События ----
    //
    // Пары add_on.../remove_on..., как у генерируемых обёрток: в скобках окна
    // они пишутся тегами (`onClosed = ...`), а on_event<EventKey::...>(window)
    // делает каждое ожидаемым в корутине. Отправитель -- пустой Object: окно не
    // объект WinRT; обработчик без аргументов или с одними аргументами события
    // его не видит. Все зовутся на интерфейсном потоке.

    /// Закрытие окна: приложению есть что сохранить по дороге (место чтения,
    /// геометрию). Зовётся до разрушения.
    EventToken add_onClosed(EventHandler<Object> const& handler) const;
    void remove_onClosed(EventToken token) const;

    /// Место окна сменилось -- двигали, растягивали, максимизировали или
    /// восстановили. Под отложенное сохранение места.
    EventToken add_onGeometryChanged(EventHandler<Object> const& handler) const;
    void remove_onGeometryChanged(EventToken token) const;

    /// Клиентская область сменила размер или масштаб (ClientSize).
    ///
    /// Зовётся прямо из WM_SIZE -- то есть **до** того, как XAML начнёт
    /// считать вёрстку, и синхронно с тем, как двигаются сцена и мост
    /// острова. Это и есть место, где приложение перестраивает раскладку под
    /// новую ширину: то же самое из `FrameworkElement::SizeChanged` пришло бы
    /// уже изнутри прохода вёрстки, а править дерево оттуда нельзя. Смена
    /// увеличения (`zoom`) зовёт его тоже: логическая ширина от неё меняется.
    EventToken add_onClientSizeChanged(EventHandler<ClientSize> const& handler) const;
    void remove_onClientSizeChanged(EventToken token) const;

    // Ввод со сцены (режим чтения, XAML-острова нет). У чистой сцены ввод
    // расщеплён: указатель перехватывает InputSite острова (верхний WndProc
    // его не видит) -- его берём через InputPointerSource, -- а клавиатура
    // доходит до WndProc (WM_KEYDOWN). Окно сводит оба в эти события; они
    // приносят те же обёртки, что раздаёт XAML (VirtualKey, PointerPoint).
    EventToken add_onKeyDown(EventHandler<VirtualKey> const& handler) const;
    void remove_onKeyDown(EventToken token) const;
    EventToken add_onPointerPressed(EventHandler<PointerPoint> const& handler) const;
    void remove_onPointerPressed(EventToken token) const;
    EventToken add_onPointerMoved(EventHandler<PointerPoint> const& handler) const;
    void remove_onPointerMoved(EventToken token) const;
    EventToken add_onPointerReleased(EventHandler<PointerPoint> const& handler) const;
    void remove_onPointerReleased(EventToken token) const;
    EventToken add_onPointerWheelChanged(EventHandler<PointerPoint> const& handler) const;
    void remove_onPointerWheelChanged(EventToken token) const;

    // Те же события одним вызовом, как их подписывали до тегов: обработчик
    // добавляется и остаётся до конца жизни окна.
    void onClosed(std::function<void()> handler) const;
    void onGeometryChanged(std::function<void()> handler) const;
    void onClientSizeChanged(std::function<void(SizeInt32 client, float scale)> handler) const;
    void onKeyDown(std::function<void(VirtualKey)> handler) const;
    void onPointerPressed(std::function<void(PointerPoint const&)> handler) const;
    void onPointerMoved(std::function<void(PointerPoint const&)> handler) const;
    void onPointerReleased(std::function<void(PointerPoint const&)> handler) const;
    void onPointerWheel(std::function<void(PointerPoint const&)> handler) const;

private:
    core::intrusive_ptr<WindowState> state_;
};

}  // namespace wxl

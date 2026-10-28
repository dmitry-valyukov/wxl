# Эффекты современного GUI — каталог

Приоритеты каталога выбраны с учётом цели **wxl** — работать сопоставимо с
WebView2/Electron на десктопе — и доступных средств: **WinUI**, композитора
`Microsoft.UI.Composition`, Direct2D и Direct3D. Рекомендации Fluent здесь не
служат фильтром: то, чего нет в **WinUI**, **wxl** реализует сам, как уже реализован
`wxl::HaloEffect`. Каталог основан на коде `wxl.ui`, примерах, пробах `sandbox/` и
профилях генератора; ID пункта не меняется, когда пункт переходит из группы в
группу. Итог: готово 34, P1 — 10, P2 — 28, P3 — 19, вне каталога 5. Из 13 пунктов
фундамента готовы пять, открыты семь, один вне каталога.

- **Готово** — эффект уже есть в **wxl** или предоставляется самим **WinUI** (шаблоны
  и переходы элементов управления, материалы окна, ресурсы Fluent).
- **P1** — минимальный уровень паритета: то, что приложение на Electron
  получает из CSS без дополнительных усилий, а также системные материалы
  Windows, которых у Electron нет.
- **P2** — выразительные приёмы, распространённые в вебе: собственные эффекты
  **wxl** по образцу `wxl::HaloEffect`, выражения композитора, Direct2D в
  `wxl::DrawingSurface`.
- **P3** — демонстрационные и ресурсоёмкие эффекты: собственные шейдеры HLSL
  через Direct3D, частицы, 3D, кинетический текст. Именно в этой группе **wxl**
  может оказаться дешевле браузера за кадр.
- **Вне каталога** — пункт не имеет смысла на десктопе, не является эффектом
  или закрыт другим пунктом; пункт фундамента — когда на него не опирается ни
  одна строка каталога.

Механизм выбирается по одному правилу: движение выполняется на композиторе, а
не в потоке интерфейса; пиксели рисуются один раз в одну поверхность, без
второй копии.

## Готово

34 эффекта каталога уже реализованы: часть написана в **wxl**, часть
предоставляет **WinUI**. Готовы и пять пунктов фундамента (INF-…), а семь
наработок **wxl** (WXL-…) не входят в перечень эффектов, но на них опираются
будущие эффекты.

| ID | Эффект | Чем сделано |
| --- | --- | --- |
| MAT-01 | Размытие фона | `wxl::GlassEffect`: то, что находится за элементом, размывается через backdrop композитора — и в острове XAML, и на сцене `wxl::CompositionWindow` |
| MAT-02 | Слои полупрозрачности | Кисти слоёв Fluent через `brushes.*` — 755 ключей в виде дерева; путь к кисти следует за темой |
| MAT-03 | Волосяные обводки | `wxl::Card` и `wxl::OverlayCard`: рамка 1 px кистью `brushes.Card.StrokeColorDefault` или светлой линией поверх картинки |
| MAT-04 | Стекло (glassmorphism) | `wxl::GlassEffect`: размытие и тонирование того, что за элементом, в скобках элемента (`Card { GlassEffect {…} }`) |
| MAT-05 | Acrylic окна | `DesktopAcrylicBackdrop {}` в `systemBackdrop` окна; материал на части окна — `SystemBackdropElement`, акрил внутри окна — `AcrylicBrush`. У `wxl::CompositionWindow` материал рисует остров XAML там, где его содержимое прозрачно, и задний фон сцены под ним не виден: фон-картинка и материал у этого окна не сочетаются; его `systemBackdrop` в примерах и пробах не показан |
| MAT-12 | Mica / Mica Alt | `MicaBackdrop {kind = MicaKind::BaseAlt}` в `systemBackdrop` окна, на части окна — `SystemBackdropElement`; у `wxl::CompositionWindow` — с тем же ограничением, что в MAT-05. У Electron этого материала нет, и это преимущество **wxl** |
| LGT-01 | Тени и высота | `ThemeShadow` + `translation = {0, 0, 32}` в `wxl::Card` |
| LGT-03 | Градиенты | `LinearGradientBrush`, `RadialGradientBrush` в профиле; на композиторе — `CompositionLinearGradientBrush` и `CompositionRadialGradientBrush`. Конического градиента нет ни в **WinUI**, ни в Direct2D, для него нужен собственный эффект |
| LGT-04 | Смена темы на ходу | Кисть, заданная путём `brushes.…`, подписывается на `ActualThemeChanged` элемента и перечитывается для его новой темы, как `{ThemeResource}` в разметке; с `backgroundTransition = BrushTransition {}` смена темы становится плавной |
| LGT-06 | Цветные тени | `wxl::HaloEffect` с тегом `offset`; пока работает только для элементов с `GetAlphaMask` — текста, картинки, фигуры |
| LGT-07 | Свечение / неон | `wxl::HaloEffect` — `DropShadow` без смещения по альфа-каналу глифов, `wxl::GaussianBlurEffect` — то же свечение графом эффектов с затуханием на выбор приложения (`gamma`); неоновая вывеска показана в `samples/Gallery` (группа Effects) |
| LGT-10 | Градиентная обводка | `wxl::BevelEffect`: обводка — `ShapeVisual` поверх элемента с линейным градиентом из двух цветов; размер, ось и опорные точки градиента следуют за размером элемента выражениями композитора |
| LGT-11 | Системный акцент (вместо Material You) | Кисти `brushes.Accent.FillColor.*` следуют за акцентным цветом Windows; палитра из картинки описана в MAT-11 |
| LGT-13 | Свет за курсором (spotlight) | `wxl::RevealEffect`: два прожектора композитора, `wxl::HoverLight` и `wxl::BorderLight`, следуют за указателем по выражению — световое пятно на элементе под указателем и свечение краёв соседних элементов |
| MOT-02 | Пружины | `SpringVector3NaturalMotionAnimation` (`createSpringVector3Animation` композитора) в профиле full: пружина по `Scale` или `Translation` выполняется на композиторе, и её можно прервать; показана на странице Animation interop примера `samples/Gallery`. Скалярной пружины в профиле нет (INF-04) |
| MOT-03 | Появление и исчезновение | `transitions[EntranceThemeTransition {}]` и `opacityTransition = ScalarTransition {}` у элементов XAML; неявных анимаций показа и скрытия для визуалов сцены в профиле нет (INF-04) |
| MOT-04 | Модальные окна и выпадающие панели | `wxl::showDialog` для `ContentDialog`, `Flyout` / `MenuFlyout` в профиле; анимации встроенные |
| MOT-05 | Смена содержимого (fade through) | `ContentThemeTransition` в `contentTransitions[…]` или `itemContainerTransitions[…]`. Навигацию `Frame` без страниц XAML даёт `wxl::navigatePage`: `Frame.Navigate` к пустой `Page` платформы с переходом и стеком назад, содержимое страницы ставит приложение (`PagedFrame` примера `samples/Gallery`) |
| MOT-06 | Каскад (stagger) | `childrenTransitions[EntranceThemeTransition {isStaggeringEnabled = true}]` — одна строка в скобках панели |
| MOT-08 | Трансформация контейнера | `ConnectedAnimationService` (`prepareToAnimate` / `tryStart`) работает между любыми элементами, без `Frame`; показан на странице Connected Animation примера `samples/Gallery` |
| MOT-09 | Общий элемент (hero) | Тот же `ConnectedAnimationService`: на странице Connected Animation примера `samples/Gallery` плитка `GridView` переходит в подробный вид на той же странице. Для визуалов сцены готового перехода нет, путь к нему — ключевые кадры по прямоугольнику |
| MOT-11 | Макетная анимация (FLIP) | `RepositionThemeTransition` в `transitions[…]` или `childrenTransitions[…]` даёт её без дополнительного кода; неявной анимации `Offset` для визуалов сцены в профиле нет (INF-04) |
| MOT-13 | Анимация иконок | `AnimatedIcon` со встроенными источниками (`AnimatedBackVisualSource` и др.); Lottie — `AnimatedVisualPlayer` с источником, который строит объекты композитора кодом. В примере `samples/Gallery` это `LottieLogo.cpp` — вывод Lottie-Windows на C#, переписанный на C++/WinRT; приложению такой код даёт LottieGen с выводом на C++ |
| INT-01 | Наведение | Состояния шаблонов **WinUI**; `wxl::ThemeBrush` задаёт состояния элемента управления в цветах приложения |
| INT-03 | Кольцо фокуса | `wxl::FocusEffect`: фокус с клавиатуры в стиле окна вместо системной рамки — подъём с тенью или ореол акцентного цвета на композиторе (`wxl::FocusLook`), присоединяется к `wxl::CompositionWindow`; без него — системные визуалы фокуса **WinUI** |
| INT-04 | Переключатели | Шаблоны `ToggleSwitch`, `CheckBox`, `RadioButton` |
| INT-08 | Резиновая прокрутка | `ScrollViewer` — на сенсорном экране и тачпаде |
| INT-10 | Потяни, чтобы обновить | `RefreshContainer` из **WinUI** (страница PullToRefresh примера `samples/Gallery`); на десктопе с мышью этот приём встречается редко |
| INT-16 | Звуковой отклик | `ElementSoundPlayer` из **WinUI** включается одним свойством: `ElementSoundPlayer::state(ElementSoundPlayerState::On)` (страница Sound примера `samples/Gallery`) |
| SCR-09 | Параллакс | `ParallaxView {source = …, verticalShift = …}` из **WinUI**: картинка сдвигается медленнее прокрутки списка; фона сцены, управляемого прокруткой острова, нет |
| STA-03 | Спиннер | `ProgressRing` |
| STA-04 | Прогресс-бар | `ProgressBar` |
| STA-07 | Тост / снэкбар | `InfoBar` (профиль extended) |
| ADV-01 | Градиентный текст | `TextBlock.Foreground` градиентной кистью |
| INF-03 | Переходы XAML в профиле | Профиль full: `transitions[…]`, `childrenTransitions[…]`, `contentTransitions[…]`, `itemContainerTransitions[…]` с `EntranceThemeTransition`, `RepositionThemeTransition`, `ContentThemeTransition`, `AddDeleteThemeTransition`, `PopupThemeTransition`, `NavigationThemeTransition`; неявные `opacityTransition` и `rotationTransition` (`ScalarTransition`), `scaleTransition` и `translationTransition` (`Vector3Transition`), `backgroundTransition` (`BrushTransition`); показаны на страницах Theme Transitions, Implicit Transitions и Page Transitions примера `samples/Gallery` |
| INF-05 | Наборы свойств для выражений | `ElementCompositionPreview::getScrollViewerManipulationPropertySet` и `getPointerPositionPropertySet` в профиле full: от первого масштабируются элементы `ItemsRepeater` в примере `samples/Gallery`, от второго свет `wxl::RevealEffect` и `wxl::Button3DEffect` следует за указателем. Выражение вычисляется на композиторе, как `animation-timeline` в CSS |
| INF-09 | Источники света | `AmbientLight`, `DistantLight`, `PointLight`, `SpotLight` с `Targets` и `SceneLightingEffect` в профиле full; на них построены `wxl::RevealEffect` (прожекторы над указателем) и `wxl::Button3DEffect` (далёкая лампа и общий свет над картой нормалей), опыты со светом — `sandbox/LightingLab` |
| INF-11 | Потеря устройства у `wxl::DrawingSurface` | **wxl** заменяет потерянное устройство сам: до замены `draw()` отвечает false, после неё `wxl::RenderingDeviceWatch` зовёт владельца нарисовать поверхности заново. Сами восстанавливаются снимок `wxl::WindowShade` и фон `wxl::CompositionWindow`, загруженный `backgroundAsync`; `wxl::TextureCache` забывает всё, и картинку держатель просит снова по своему `RenderingDeviceWatch`. Поверхности `wxl::Button3DEffect` и `wxl::RevealEffect` и фон окна, заданный синхронным `background`, после замены пока остаются пустыми |
| INF-12 | Ожидание события типа | Событие самого типа, без объекта для подписки, ожидается парой его статических членов: `on_event(&CompositionTarget::add_onRendering, &CompositionTarget::remove_onRendering)`; токен отмены передаётся последним аргументом (`event_awaitable.h`). Эффекты `wxl.ui` ждут кадр через `when_drawn` (`effect_helpers.h`): если у визуала элемента уже есть дочерний визуал, в который XAML рисует элемент, слой размещается сразу, иначе `when_drawn` подписывается на `CompositionTarget::Rendering` проекции C++/WinRT и снимает подписку на первом кадре, в котором этот визуал появился |
| WXL-01 | Полёт окна к точке и обратно | `wxl::WindowShade`: снимок окна перемещается ключевыми кадрами по `Offset`, `Scale`, `Opacity` в потоке DWM |
| WXL-02 | Сцена окна под островом XAML | `wxl::CompositionWindow`: окно без поверхности перенаправления, фон — картинка с режимами заполнения, размер визуалов задаёт `ExpressionAnimation` от сцены |
| WXL-03 | Собственное рисование и текстуры | `wxl::DrawingSurface` (цепочка D3D11 → D2D → `CompositionGraphicsDevice`), `wxl::VirtualDrawingSurface` (поверхность длиннее текстуры устройства, пиксели — только у нарисованных частей) и `wxl::TextureCache` (асинхронное декодирование через Win2D с кэшем по пути) |
| WXL-04 | Рост элемента под указателем | `wxl::MagnifyEffect`: увеличение (или сжатие) элемента, пока над ним указатель, за фиксированное время; движение идёт на композиторе, а в конце масштаб рисует сам XAML, и глифы остаются чёткими |
| WXL-05 | Кнопка как предмет под лампой | `wxl::Button3DEffect`: форма клавиши задана картой нормалей, которую освещает композитор (`SceneLightingEffect`) — далёкая лампа, общий свет и две лампы над указателем; тень на панель — проецируемая тень композитора |
| WXL-06 | Масштаб всего острова | `wxl::ZoomEffect`: масштаб острова XAML с клавиатуры и колеса ступенями `wxl::AppZoom`, присоединяется к `wxl::CompositionWindow` |
| WXL-07 | Светотень одной лампы | `wxl::Relief`: цвета выпуклой и вдавленной грани, плоского лица, плеча клавиши и освещённой панели из одной лампы `wxl::Light` по закону Ламберта, `constexpr`. `wxl::BevelEffect` принимает его кольца (`wxl::Relief::Edges`), но в дереве зовут только `wxl::Relief::under` — тона радиального градиента табло в `samples/Calculator`; обводки калькулятора заданы литералами |

## Фундамент: что нужно в wxl до самих эффектов

Большинство эффектов ниже ограничено не собственной сложностью, а одним из
этих пунктов. Каждый пункт, реализованный один раз, открывает сразу несколько
строк каталога.

| ID | Что | Механизм | Открывает | Приор. |
| --- | --- | --- | --- | --- |
| INF-01 | Общий каркас «эффекта в скобках» | Эффекты `wxl.ui` повторяют одну форму: дескриптор, копии которого делят настройки, теги в скобках и `operator()`, которым элемент или окно «надевает» эффект. Общие части собраны в `effect_helpers.h`: ожидание визуала, в который XAML рисует элемент (`when_drawn`), порядок слоёв по z (`insert_layer`), скругление элемента (`corner_of`), сцена окна (`scene_of`), жизнь между `Loaded` и `Unloaded` (`attach_while_loaded`). Саму форму по правилу **wxl** стоит оформить отдельным типом | Все собственные эффекты P1–P3 | P1 |
| INF-04 | Пружины и неявные анимации композитора | `SpringVector3NaturalMotionAnimation` и `ColorKeyFrameAnimation` в профиле full уже есть; нет `SpringScalarNaturalMotionAnimation`, `ImplicitAnimationCollection`, `SetImplicitShowAnimation` / `HideAnimation` | SCR-01; скалярная пружина в MOT-02, визуалы сцены в MOT-03, MOT-11 | P1 |
| INF-06 | Выключатель анимаций и прозрачности | `UISettings.AnimationsEnabled` (+ `AnimationsEnabledChanged`, Windows 10 2004) и `AdvancedEffectsEnabled` (+ `Changed`). Переходы XAML и `AcrylicBrush` учитывают эти настройки сами, анимации и эффекты **wxl** — нет: нужно одно место, которое принимает решение для всех | Все движущиеся и прозрачные эффекты | P1 |
| INF-07 | Композиция — отдельным модулем | Открытая задача: `wxl::CompositionWindow`, `wxl::DrawingSurface`, `wxl::TextureCache`, Win2D и эффекты живут в `wxl.ui`; вынос в отдельный модуль не начат. Новые эффекты лучше сразу создавать рядом с ними | — | P1 |
| INF-08 | InteractionTracker | Жест с инерцией, упорами и пружинным возвратом; весь жест обрабатывается на композиторе | INT-07, INT-09, SCR-07 | P2 |
| INF-10 | Фигуры композитора | `ShapeVisual`, `CompositionSpriteShape`, `CompositionPathGeometry`, `PathKeyFrameAnimation`, `TrimStart` / `TrimEnd`. Внутри **wxl** `ShapeVisual` и `CompositionSpriteShape` рисуют обводку `wxl::BevelEffect` и подложку `wxl::FocusEffect`, а `LottieLogo.cpp` примера `samples/Gallery` строит фигуры с `CompositionPathGeometry` и анимирует `TrimStart` / `TrimEnd` через C++/WinRT; в профиле фигур композитора нет | MOT-14, ADV-02, ADV-07, STA-08 | P2 |
| INF-13 | Слой Direct3D с собственными шейдерами | Цепочка обмена (swap chain) как поверхность композитора (`ICompositorInterop::CreateCompositionSurfaceForSwapChain`) и HLSL; для статичного изображения — собственный эффект Direct2D в `wxl::DrawingSurface`. Проба `sandbox/LightingLab` рисует свой пиксельный шейдер HLSL на устройстве Direct3D под Direct2D **wxl** в текстуру и выводит её в `wxl::DrawingSurface` без цепочки обмена, кадр на каждое изменение. Собственные шейдеры идут только через Direct3D: композитор их не принимает | MAT-08, MAT-09, ADV-09…ADV-13, LGT-03 (конический) | P3 |

## P1 — минимальный уровень паритета

Без этих 10 эффектов приложение на **wxl** выглядит беднее аналогичного
приложения на Electron. Почти все они опираются на встроенные механизмы
платформы, поэтому основная работа — фундамент INF-01, INF-04, INF-06 и форма
записи в DSL.

| ID | Эффект | Как сделать в wxl | Опора |
| --- | --- | --- | --- |
| LGT-09 | Анимированный градиент / «аврора» | `ColorKeyFrameAnimation` по опорным точкам градиента `CompositionLinearGradientBrush` — на композиторе, без перерисовки | — |
| MOT-01 | Кривые и длительности | Именованные токены Fluent 2 (`curveDecelerateMax` и др.) в виде кривых композитора: `CreateCubicBezierEasingFunction` уже есть в профиле rich | — |
| INT-02 | Сжатие при нажатии | Пружина по `Scale` на `PointerPressed` / `Released`; собственный эффект в скобках по образцу `wxl::MagnifyEffect` | INF-01 |
| SCR-01 | Появление при прокрутке | `EffectiveViewportChanged` + неявная анимация появления или выражение от прокрутки | INF-04, INF-05 |
| SCR-02 | Сжимающаяся шапка | Выражение над набором свойств прокрутки `ScrollViewer` — классический приём композитора | INF-05 |
| SCR-03 | Привязка прокрутки | `ScrollViewer.HorizontalSnapPointsType` / `VerticalSnapPointsType` и `FlipView`; в профиле пока только `FlipView` (extended) | профиль |
| SCR-04 | Анимации от прокрутки | Общий механизм: `ExpressionAnimation` от прокрутки, записанный в DSL так же коротко, как `animation-timeline: scroll()` | INF-05 |
| STA-01 | Скелетон | Заглушки из `Rectangle` с кистями Fluent; собственного скелетона у **WinUI** нет | INF-01 |
| STA-02 | Мерцание (shimmer) | `CompositionLinearGradientBrush` с анимированным смещением опорных точек градиента — одна анимация на все заглушки экрана | INF-01 |
| STA-11 | Потоковый вывод текста (AI) | Дописывание уже есть: `wxl::FormattedBlock` и `wxl::HtmlBlock::append` добавляют фрагмент в конец без повторного разбора (пример HtmlView); плавное появление фрагмента — собственный эффект. Это основной сценарий приложений на Electron | INF-01 |

## P2 — выразительные приёмы веба

28 эффектов. Почти все они — собственные эффекты **wxl** по образцу `wxl::HaloEffect`:
элемент подключает их в своих скобках, а внутри работают визуалы, выражения и
источники света композитора или одна отрисовка Direct2D.

| ID | Эффект | Как сделать в wxl | Опора |
| --- | --- | --- | --- |
| MAT-06 | Шум / зерно | Эффект Turbulence Direct2D рисуется один раз в `wxl::DrawingSurface` и выводится плиткой (`wxl::BackgroundFill::Tile` уже есть); у Acrylic собственный шум. Встроенные эффекты Direct2D в `wxl::DrawingSurface` опробованы в пробе `sandbox/LightingLab` | INF-11 |
| MAT-07 | Внутреннее свечение | `DropShadow` не умеет отбрасывать тень внутрь: нужна инверсная маска формы через `CompositionMaskBrush` или отрисовка Direct2D | INF-01 |
| MAT-10 | Спекулярный блик | `SpotLight` / `PointLight` с `SceneLightingEffect`; следование за указателем — выражением. Для кнопок такой блик уже даёт `wxl::Button3DEffect` (`specularAmount`, `specularShine`) | INF-05, INF-09 |
| MAT-11 | Адаптивный тинт | Средний и доминирующий цвета картинки фона вычисляются один раз во время её декодирования в `wxl::TextureCache` и передаются в цвет `wxl::GlassEffect` и в акцентный цвет экрана | MAT-04 |
| LGT-02 | Многослойные мягкие тени | Стопка `DropShadow` с растущим радиусом для любого прямоугольника — то, чего не даёт `ThemeShadow` (у него нет ни цвета, ни радиуса). Это также снимает ограничение LGT-06 на `GetAlphaMask` | INF-01 |
| LGT-08 | Меш-градиент | `ID2D1GradientMesh` Direct2D — настоящий градиент из патчей, которого нет в CSS; рисуется один раз в поверхность фона сцены. Рисование Direct2D в `wxl::DrawingSurface` опробовано в пробе `sandbox/LightingLab` | INF-11 |
| LGT-12 | Анимированная смена темы | Круговое раскрытие геометрической обрезкой поверх снимка старой темы. Снимок хранится только на время перехода и сразу освобождается, иначе он становится второй копией экрана | LGT-04 |
| LGT-15 | Режимы смешивания | `BlendEffect` композитора | INF-01 |
| MOT-07 | Общая ось | Ключевые кадры по `Translation` и `Opacity` для уходящего и приходящего содержимого. Для навигации `Frame` сдвиг по оси уже даёт `SlideNavigationTransitionInfo` (профиль full) через `wxl::navigatePage`, страница Page Transitions примера `samples/Gallery` | — |
| MOT-12 | Анимированные числа | Цифры как отдельные визуалы-ленты с обрезкой, прокрутка лент по `Offset`. Кандидат на применение: табло калькулятора | INF-01 |
| INT-05 | Тряска поля при ошибке | Ключевые кадры по `Translation` (`SetIsTranslationEnabled` уже есть в профиле) | INF-01 |
| INT-06 | Волна от нажатия (ripple) | Круговой `SpriteVisual` в точке нажатия, `Scale` и `Opacity` ключевыми кадрами, обрезка по скруглению кнопки | INF-01 |
| INT-07 | Перетаскивание с подъёмом | В списках перестановку уже даёт `canReorderItems` (страница ListView примера `samples/Gallery`); в собственных элементах — `Translation` с подъёмом по Z и `ThemeShadow` | INF-08 |
| INT-09 | Свайп с учётом скорости | `InteractionTracker` с упорами. Смахивание строки списка с командами уже даёт `SwipeControl` (профиль full); кандидат на применение — смахивание карточки, которое учитывает скорость жеста | INF-08 |
| INT-13 | Магнитная кнопка | Выражение по позиции указателя с затуханием по расстоянию, возврат пружиной | INF-05 |
| INT-14 | Собственный курсор и следящий элемент | Тег `cursor` (его ставит `impl::set_cursor` из `element_helpers.h` через `ProtectedCursor`) и визуал-«хвост», который следует на пружине за позицией указателя | INF-05 |
| INT-15 | 3D-наклон с бликом | `RotationAxis` и перспектива в `TransformMatrix` родителя, вычисляемые от позиции указателя; блик — MAT-10 | INF-05, INF-09 |
| SCR-05 | Прогресс чтения | `Scale.X` полосы из выражения от прокрутки | INF-05 |
| SCR-06 | Схлопывание крупного заголовка | То же выражение, что в SCR-02, по `Scale` и `Offset` заголовка | INF-05 |
| SCR-07 | Карусель с масштабом и перспективой | `InteractionTracker` или прокрутка с упорами + выражение на каждую карточку | INF-05, INF-08 |
| SCR-08 | Глубина под модальным окном | Сцена `wxl::CompositionWindow` уходит на задний план: `Scale`, яркость и размытие под островом; двухслойное окно создано именно для таких задач | — |
| STA-08 | Анимация успеха и ошибки | Галочка прорисовывается через `TrimEnd` фигуры композитора или с помощью Lottie; путь через Lottie уже открыт (`AnimatedVisualPlayer`, MOT-13) | INF-10 |
| STA-09 | Пульс бейджа | `InfoBadge` + кольцо ключевыми кадрами `Scale` / `Opacity` с числом повторов | — |
| STA-10 | Индикатор набора | Три `Ellipse` со сдвинутыми по фазе ключевыми кадрами | INF-01 |
| STA-12 | Конфетти | Десятки `SpriteVisual` с ключевыми кадрами и одной общей поверхностью-атласом; тысячи частиц относятся уже к ADV-09 | INF-01 |
| ADV-02 | Прорисовка линии | `TrimEnd` у `CompositionSpriteShape` — то же, что `stroke-dashoffset` в SVG; так анимирует фигуры `LottieLogo.cpp` примера `samples/Gallery` через C++/WinRT | INF-10 |
| ADV-03 | Появление текста по буквам | Глифы DirectWrite в `wxl::DrawingSurface` — отдельный визуал на слово или букву, каскад ключевыми кадрами; текст для программ чтения с экрана — `AutomationName` | INF-11 |
| ADV-04 | «Расшифровка» текста | `DispatcherQueueTimer` подменяет символы в `TextBlock`; собственный эффект занимает несколько строк | INF-01 |

## P3 — демонстрационные эффекты и собственные шейдеры

19 эффектов. В этой группе браузер расходует больше всего ресурсов (WebGL поверх
страницы), а **wxl** — меньше: тот же HLSL выводится непосредственно в цепочку
обмена на сцене. Почти все эффекты группы зависят от INF-13.

| ID | Эффект | Как сделать в wxl | Опора |
| --- | --- | --- | --- |
| MAT-08 | Liquid Glass | Собственный шейдер: линза, блик, адаптивный тинт. Композитор не передаёт шейдеру чужие пиксели, поэтому эффект применим только к тому, что **wxl** рисует сам: к фону сцены, к содержимому `wxl::DrawingSurface` | INF-13 |
| MAT-09 | Преломление / линза | Эффект DisplacementMap Direct2D над собственной картинкой, в динамике — шейдер. Встроенные эффекты Direct2D в `wxl::DrawingSurface` и свой шейдер HLSL опробованы в пробе `sandbox/LightingLab` | INF-11, INF-13 |
| MAT-13 | Вибрантность | Текст и иконки, смешанные с размытым фоном: `BlendEffect` над backdrop | INF-01 |
| MAT-14 | Нейморфизм | Две `DropShadow` (светлая и тёмная) или свет композитора; контраст слабый, поэтому эффект подходит как тема оформления, а не как вариант по умолчанию | LGT-02, INF-09 |
| MAT-15 | Клейморфизм | Внешняя тень + внутреннее свечение | LGT-02, MAT-07 |
| LGT-05 | Тональная высота (Material) | Оттенок поверхности зависит от `Translation.Z`: выражение по цвету кисти. Для приложений в стиле Material на Windows | INF-05 |
| LGT-14 | Дуотон | `ColorMatrixEffect` / `TintEffect` композитора над картинкой | — |
| MOT-14 | Морфинг форм | `PathKeyFrameAnimation` между путями с одинаковым составом сегментов; выравнивание сегментов выполняет **wxl** | INF-10 |
| INT-12 | Контекстное меню с превью | `MenuFlyout` + подъём элемента по Z и размытие сцены на время показа меню | SCR-08 |
| SCR-10 | Горизонтальная секция при вертикальной прокрутке | Закреплённый блок со `Translation.X` из выражения от прокрутки | INF-05 |
| ADV-05 | Анимация вариативного шрифта | Оси шрифта через DirectWrite (`IDWriteFontFace5`), покадровая отрисовка в `wxl::DrawingSurface` | INF-11 |
| ADV-06 | Кинетическая типографика | ADV-03 + временные шкалы композитора (`AnimationController` в профиле пока нет) | ADV-03 |
| ADV-07 | Морфинг блобов | `PathKeyFrameAnimation`, как в MOT-14 | INF-10 |
| ADV-08 | Анимированное зерно | Плитка MAT-06, сдвигаемая ступенчато по `Offset`; шум не перерисовывается | MAT-06 |
| ADV-09 | Системы частиц | Direct3D: частицы на GPU (вычислительные шейдеры) в цепочку обмена сцены | INF-13 |
| ADV-10 | Шейдерный фон | Фрагментный шейдер на всю сцену `wxl::CompositionWindow` — естественное место для него, остров XAML располагается поверх | INF-13 |
| ADV-11 | Искажение / «жидкость» | DisplacementMap Direct2D или шейдер смещения | INF-13 |
| ADV-12 | 3D-объекты | Собственный конвейер Direct3D в цепочку обмена, модели glTF. Сетка треугольников с материалом «металличность — шероховатость» в дереве композитора (`Microsoft.UI.Composition.Scenes`, `SceneMesh`) опробована в пробе `sandbox/LightingLab` | INF-13 |
| ADV-13 | Глитч | Сдвиг каналов RGB и нарезка шейдером; нужна проверка на вспышки (WCAG 2.3.1) | INF-13 |

## Вне каталога

Пять пунктов исключены не потому, что их нет во Fluent, а потому, что они не
являются эффектами или не имеют смысла на десктопе. Шестой — пункт фундамента
INF-02: на него не опирается ни одна строка каталога.

| ID | Эффект | Почему |
| --- | --- | --- |
| MOT-10 | View Transitions | Веб-API, а не эффект; его результат покрывают MOT-05, MOT-08, MOT-09 |
| STA-05 | Оптимистичный UI | Приём уровня модели, а не отрисовки: в **wxl** это `wxl::core::observable` и привязки |
| STA-06 | Пустое состояние | Приём вёрстки из готовых элементов управления; оживляется с помощью MOT-03 и MOT-13 |
| INT-11 | Тактильный отклик | На десктопе тактильный отклик возможен разве что у пера; к пункту стоит вернуться, если появится сценарий с пером |
| SCR-11 | Пространственный UI (visionOS) | Другая платформа и другой ввод |
| INF-02 | Эффекты композитора в профиле | Собственные эффекты **wxl** строят графы эффектов сами, из описаний Win2D через C++/WinRT, мимо профиля: так сделаны `wxl::GlassEffect` (`GlassEffect.cpp`), `wxl::GaussianBlurEffect` (`GaussianBlurEffect.cpp`), `wxl::RevealEffect` и `wxl::Button3DEffect`. Ни одна строка каталога на этот пункт не опирается. Профиль full даёт `CompositionEffectFactory` / `CompositionEffectBrush`, `CreateBackdropBrush` и только нужные свету описания Win2D (`CompositeEffect`, `ArithmeticCompositeEffect`, `ColorSourceEffect`, `ColorMatrixEffect`, эффекты освещения); описания размытия, `BlendEffect` и `TintEffect` в профиле нужны только приложению, которое строит граф эффектов само |

## Требования ко всем эффектам на Windows

Эффект считается готовым, когда он учитывает настройки специальных
возможностей Windows, выполняется на композиторе и показан в примере.

**Специальные возможности**

- [ ] «Эффекты анимации» выключены (`UISettings.AnimationsEnabled`): анимация
  **wxl** сразу принимает конечное значение, бесконечная анимация останавливается.
  Переходы XAML делают это сами, композитор — нет (INF-06).
- [ ] «Эффекты прозрачности» выключены (`AdvancedEffectsEnabled`): стекло и
  размытие **wxl** заменяются сплошной заливкой того же тона, как это делает
  `AcrylicBrush`.
- [ ] Высокая контрастность: декоративные эффекты (свечение, шум, шейдеры)
  отключаются, цвета берутся из системной палитры.
- [ ] Не больше трёх вспышек в секунду (WCAG 2.3.1): требование относится к
  глитчу, пульсации и миганию.
- [ ] Текст, разбитый на визуалы или нарисованный в `wxl::DrawingSurface`,
  дублируется для программ чтения с экрана (`AutomationName`).
- [ ] Контраст текста на стекле и картинке не ниже 4.5:1 на самом
  неблагоприятном фоне; у `wxl::OverlayCard` для этого тёмный затемняющий
  слой, а текст на нём пишется кистью тёмной темы
  (`brushes.Text.FillColor.Primary(ElementTheme::Dark)`).

**Производительность**

- [ ] Движение задаётся анимациями и выражениями композитора, а не кадрами в
  STA-потоке: тогда оно остаётся плавным, пока поток занят. В этом главное
  преимущество перед браузером с анимациями на JS.
- [ ] Пиксели рисуются один раз в одну поверхность; `wxl::DrawingSurface`
  перерисовывается при изменении, а не каждый кадр. Снимок для перехода
  хранится только на время перехода.
- [ ] Размытие и backdrop обходятся дороже всего: их площадь ограничивается,
  слои стекла не накладываются друг на друга, радиус не анимируется.
- [ ] Шейдеры и частицы приостанавливаются, когда окно свёрнуто или визуал не
  виден, и восстанавливаются после потери устройства (`wxl::RenderingDeviceWatch`,
  INF-11).
- [ ] Стоимость измеряется, а не предполагается: в `wxl.ui/benchmarks` уже
  есть сцена для сравнения с cppwinrt; такое же сравнение стоит провести с
  той же сценой в WebView2.

**Проверка**

- [ ] Каждый эффект получает страницу в группе Effects примера
  `samples/Gallery`. Снимок окна через `PrintWindow` или `BitBlt` не видит
  содержимого DirectComposition (пробы `sandbox/GlassProbe` и
  `sandbox/WindowlessIslandProbe` снимают его с экрана, поставив окно поверх
  остальных), поэтому пример пока остаётся основным покрытием; захват через
  `Windows.Graphics.Capture` позволил бы написать настоящий тест.

## Источники

**Код wxl:** `wxl.ui/src` — `HaloEffect.h`, `GaussianBlurEffect.h`,
`GlassEffect.h`, `BevelEffect.h`, `Button3DEffect.h`, `Relief.h`,
`RevealEffect.h`, `MagnifyEffect.h`, `FocusEffect.h`, `ZoomEffect.h`, `Color.h`,
`Card.h`, `ThemeBrush.h`, `resource_lookup.h`, `CompositionWindow.h`,
`DrawingSurface.h`, `TextureCache.h`, `WindowShade.h`, `Navigation.h`,
`ShowDialog.h`, `FormattedBlock.h`, `HtmlBlock.h`, `event_awaitable.h`,
`effect_helpers.h`, `element_helpers.h`; профили `base`, `extended`, `rich`,
`full`; примеры `samples/Gallery`, `samples/Calculator`, `samples/HtmlView`,
`samples/Quadratic`; пробы `sandbox/LightingLab`, `sandbox/GlassProbe`,
`sandbox/WindowlessIslandProbe`.

**Microsoft Learn:**

- [Composition effects](https://learn.microsoft.com/windows/apps/develop/composition/composition-effects)
- [CompositionEffectBrush — Windows App SDK](https://learn.microsoft.com/windows/windows-app-sdk/api/winrt/microsoft.ui.composition.compositioneffectbrush?view=windows-app-sdk-2.0)
- [Compositor.CreateBackdropBrush](https://learn.microsoft.com/windows/windows-app-sdk/api/winrt/microsoft.ui.composition.compositor.createbackdropbrush?view=windows-app-sdk-2.0)
- [Composition native interop with DirectX and Direct2D](https://learn.microsoft.com/windows/apps/develop/composition/composition-native-interop)
- [ICompositorInterop::CreateCompositionSurfaceForSwapChain](https://learn.microsoft.com/windows/win32/api/windows.ui.composition.interop/nf-windows-ui-composition-interop-icompositorinterop-createcompositionsurfaceforswapchain)
- [Implicit transitions (WinUI 3)](https://learn.microsoft.com/windows/apps/develop/motion/implicit-transitions)
- [UISettings.AnimationsEnabledChanged](https://learn.microsoft.com/uwp/api/windows.ui.viewmanagement.uisettings.animationsenabledchanged?view=winrt-28000)
- [Tailoring effects and experiences](https://learn.microsoft.com/windows/apps/develop/composition/composition-tailoring#animations-settings)

**Веб и другие платформы:**

- [Apple — Applying Liquid Glass to custom views](https://developer.apple.com/documentation/swiftui/applying-liquid-glass-to-custom-views)
- [NN/g — Liquid Glass](https://www.nngroup.com/articles/liquid-glass/)
- [MacRumors — iOS 26.1: настройка Liquid Glass](https://www.macrumors.com/how-to/ios-26-1-reduce-liquid-glass-effects/)
- [Microsoft — Mica](https://learn.microsoft.com/en-us/windows/apps/design/style/mica)
- [Microsoft — Acrylic](https://learn.microsoft.com/en-us/windows/apps/design/style/acrylic)
- [Fluent 2 — Motion](https://fluent2.microsoft.design/motion)
- [Material Components Android — Motion (пружины и длительности M3)](https://github.com/material-components/material-components-android/blob/master/docs/theming/Motion.md)
- [Android — Material 3 в Compose](https://developer.android.com/develop/ui/compose/designsystems/material3)
- [Android — Haptic feedback](https://developer.android.com/develop/ui/views/haptics/haptic-feedback)
- [9to5Google — Material 3 Expressive](https://9to5google.com/2025/05/13/android-16-material-3-expressive-redesign/)
- [Chrome — View Transitions](https://developer.chrome.com/docs/web-platform/view-transitions)
- [Chrome — Scroll-driven animations](https://developer.chrome.com/docs/css-ui/scroll-driven-animations)
- [MDN — backdrop-filter](https://developer.mozilla.org/en-US/docs/Web/CSS/backdrop-filter)
- [MDN — prefers-reduced-transparency](https://developer.mozilla.org/en-US/docs/Web/CSS/@media/prefers-reduced-transparency)
- [Motion — layout animations](https://motion.dev/docs/react-layout-animations)
- [SF Symbols — анимации в SwiftUI](https://nilcoalescing.com/blog/AnimatingSFSymbolsInSwiftUI/)
- [web.dev — Animations guide](https://web.dev/articles/animations-guide)
- [NN/g — Skeleton screens](https://www.nngroup.com/articles/skeleton-screens/)
- [W3C — WCAG 2.3.3 Animation from Interactions](https://www.w3.org/WAI/WCAG22/Understanding/animation-from-interactions.html)
- [W3C — WCAG 2.3.1 Three Flashes](https://www.w3.org/WAI/WCAG22/Understanding/three-flashes-or-below-threshold.html)

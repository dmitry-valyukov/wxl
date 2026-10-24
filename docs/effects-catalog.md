# Эффекты современного GUI — каталог

Приоритеты каталога пересчитаны с учётом цели **wxl** — работать сопоставимо с
WebView2/Electron на десктопе — и доступных средств: **WinUI**, композитора
`Microsoft.UI.Composition`, Direct2D и Direct3D. Рекомендации Fluent здесь не
служат фильтром: то, чего нет в **WinUI**, **wxl** реализует сам, как уже реализован
`wxl::HaloEffect`. Каталог основан на коде `wxl.ui`, примерах и профилях
генератора; ID из первой версии каталога сохранены. Итог: готово 20, P1 — 20,
P2 — 30, P3 — 21, вне каталога 5.

- **Готово** — эффект уже есть в **wxl** или предоставляется самим **WinUI** (шаблоны
  элементов управления, ресурсы Fluent).
- **P1** — минимальный уровень паритета: то, что приложение на Electron
  получает из CSS без дополнительных усилий, а также системные материалы
  Windows, которых у Electron нет.
- **P2** — выразительные приёмы, распространённые в вебе: собственные эффекты
  **wxl** по образцу `HaloEffect`, выражения композитора, Direct2D в
  `wxl::DrawingSurface`.
- **P3** — демонстрационные и ресурсоёмкие эффекты: собственные шейдеры HLSL
  через Direct3D, частицы, 3D, кинетический текст. Именно в этой группе **wxl**
  может оказаться дешевле браузера за кадр.
- **Вне каталога** — пункт не имеет смысла на десктопе, не является эффектом
  или закрыт другим пунктом.

Механизм выбирается по одному правилу: движение выполняется на композиторе, а
не в потоке интерфейса; пиксели рисуются один раз в одну поверхность, без
второй копии.

## Готово

20 эффектов каталога уже реализованы: часть написана в **wxl**, часть
предоставляет **WinUI**. Ещё шести наработок **wxl** (WXL-…) в каталоге не было, но на
них опираются будущие эффекты.

| ID | Эффект | Чем сделано |
| --- | --- | --- |
| MAT-01 | Размытие фона | `wxl::GlassEffect`: то, что находится за элементом, размывается через backdrop композитора — и в острове XAML, и на сцене `CompositionWindow` |
| MAT-02 | Слои полупрозрачности | Кисти слоёв Fluent через `brushes.*` — 755 ключей в виде дерева; путь к кисти следует за темой |
| MAT-03 | Волосяные обводки | `wxl::Card` и `wxl::OverlayCard`: рамка 1 px кистью `Card.StrokeColorDefault` или светлой линией поверх картинки |
| MAT-04 | Стекло (glassmorphism) | `wxl::GlassEffect`: размытие и тонирование того, что за элементом, в скобках элемента (`Card { GlassEffect {…} }`) |
| LGT-01 | Тени и высота | `ThemeShadow` + `translation = {0, 0, 32}` в `Card` |
| LGT-03 | Градиенты | `LinearGradientBrush`, `RadialGradientBrush` в профиле; на композиторе — `CompositionLinearGradientBrush`. Конического градиента нет ни в **WinUI**, ни в Direct2D, для него нужен собственный эффект |
| LGT-06 | Цветные тени | `HaloEffect` со смещением (настройки задаёт `wxl::Preset<DropShadow>`); пока работает только для элементов с `GetAlphaMask` — текста, картинки, фигуры |
| LGT-07 | Свечение / неон | `HaloEffect` — `DropShadow` без смещения по альфа-каналу глифов, `wxl::GaussianBlurEffect` — то же свечение с затуханием на выбор приложения; неоновая вывеска показана в `samples/Gallery` (группа Effects) |
| LGT-10 | Градиентная обводка | `wxl::BevelEffect`: ось градиента направлена по диагонали и следует за `SizeChanged` |
| LGT-11 | Системный акцент (вместо Material You) | Кисти `AccentFillColor*` следуют за акцентным цветом Windows; палитра из картинки описана в MAT-11 |
| LGT-13 | Свет за курсором (spotlight) | `wxl::RevealEffect`: световое пятно на элементе под указателем и свечение краёв соседних элементов |
| MOT-04 | Модальные окна и выпадающие панели | `wxl::showDialog` для `ContentDialog`, `Flyout` / `MenuFlyout` в профиле; анимации встроенные |
| INT-01 | Наведение | Состояния шаблонов **WinUI**; `wxl::ThemeBrush` задаёт состояния элемента управления в цветах приложения |
| INT-03 | Кольцо фокуса | `wxl::FocusEffect`: фокус с клавиатуры в стиле окна вместо системной рамки; без него — системные визуалы фокуса **WinUI** |
| INT-04 | Переключатели | Шаблоны `ToggleSwitch`, `CheckBox`, `RadioButton` |
| INT-08 | Резиновая прокрутка | `ScrollViewer` — на сенсорном экране и тачпаде |
| STA-03 | Спиннер | `ProgressRing` |
| STA-04 | Прогресс-бар | `ProgressBar` |
| STA-07 | Тост / снэкбар | `InfoBar` (профиль extended) |
| ADV-01 | Градиентный текст | `TextBlock.Foreground` градиентной кистью |
| WXL-01 | Полёт окна к точке и обратно | `wxl::WindowShade`: снимок окна перемещается ключевыми кадрами по `Offset`, `Scale`, `Opacity` в потоке DWM |
| WXL-02 | Сцена окна под островом XAML | `wxl::CompositionWindow`: окно без поверхности перенаправления, фон — картинка с режимами заполнения, размер визуалов задаёт `ExpressionAnimation` от сцены |
| WXL-03 | Собственное рисование и текстуры | `DrawingSurface` (цепочка D3D11 → D2D → `CompositionGraphicsDevice`) и `wxl::TextureCache` (асинхронное декодирование через Win2D с кэшем по пути) |
| WXL-04 | Рост элемента под указателем | `wxl::MagnifyEffect`: увеличение (или сжатие) элемента, пока над ним указатель, за фиксированное время |
| WXL-05 | Кнопка как предмет под лампой | `wxl::Button3DEffect`: грани, обводка и блики выводятся из цвета материала и цвета знаков |
| WXL-06 | Масштаб всего острова | `wxl::ZoomEffect`: масштаб острова XAML с клавиатуры и колеса, присоединяется к `CompositionWindow` |

## Фундамент: что нужно в **wxl** до самих эффектов

Большинство эффектов ниже ограничено не собственной сложностью, а одним из
этих пунктов. Каждый пункт, реализованный один раз, открывает сразу несколько
строк каталога.

| ID | Что | Механизм | Открывает | Приор. |
| --- | --- | --- | --- | --- |
| INF-01 | Общий каркас «эффекта в скобках» | `HaloEffect` и `BevelEffect` уже повторяют одну форму: функтор, который элемент «надевает», корутина без владельца, ожидание визуала элемента, завершение при уничтожении окна. По правилу **wxl** повторяющийся механизм оформляется отдельным типом | Все собственные эффекты P1–P3 | P1 |
| INF-02 | Эффекты композитора | `CompositionEffectFactory` / `CompositionEffectBrush` с описаниями эффектов Win2D (проекция Win2D уже есть в дереве для `TextureCache`), `CreateBackdropBrush`. Доступны только встроенные шейдеры: собственные композитор не принимает | MAT-01, MAT-04, MAT-06, MAT-13, LGT-14, LGT-15, SCR-08 | P1 |
| INF-03 | Переходы XAML в профиле | `Transitions`, `ChildrenTransitions`, `ContentTransitions`; `EntranceThemeTransition`, `RepositionThemeTransition`, `ContentThemeTransition`, `AddDeleteThemeTransition`; неявные `OpacityTransition`, `ScaleTransition`, `TranslationTransition`, `RotationTransition`, `BackgroundTransition` | MOT-03, MOT-05, MOT-06, MOT-11, LGT-04 | P1 |
| INF-04 | Пружины и неявные анимации композитора | `SpringVector3NaturalMotionAnimation`, `SpringScalarNaturalMotionAnimation`, `ImplicitAnimationCollection`, `SetImplicitShowAnimation` / `HideAnimation`, `ColorKeyFrameAnimation` | MOT-02, MOT-03, MOT-11, INT-02, LGT-09 | P1 |
| INF-05 | Наборы свойств для выражений | `GetScrollViewerManipulationPropertySet`, `GetPointerPositionPropertySet`. Выражение вычисляется на композиторе, как `animation-timeline` в CSS | SCR-01…SCR-09, LGT-13, INT-13, INT-15 | P1 |
| INF-06 | Выключатель анимаций и прозрачности | `UISettings.AnimationsEnabled` (+ `AnimationsEnabledChanged`, Windows 10 2004) и `AdvancedEffectsEnabled` (+ `Changed`). Переходы XAML и `AcrylicBrush` учитывают эти настройки сами, анимации и эффекты **wxl** — нет: нужно одно место, которое принимает решение для всех | Все движущиеся и прозрачные эффекты | P1 |
| INF-07 | Композиция — отдельным модулем | Открытая задача: `CompositionWindow`, `DrawingSurface`, `TextureCache` и Win2D сегодня живут в `wxl.ui`; вынос в отдельный модуль не начат. Новые эффекты лучше сразу создавать рядом с ними | — | P1 |
| INF-08 | InteractionTracker | Жест с инерцией, упорами и пружинным возвратом; весь жест обрабатывается на композиторе | INT-07, INT-09, SCR-07, INT-10 | P2 |
| INF-09 | Источники света | `AmbientLight`, `PointLight`, `SpotLight`, `DistantLight` с `Targets`; блик — `SceneLightingEffect` | MAT-10, LGT-13, INT-15, MAT-14 | P2 |
| INF-10 | Фигуры композитора | `ShapeVisual`, `CompositionSpriteShape`, `CompositionPathGeometry`, `PathKeyFrameAnimation`, `TrimStart` / `TrimEnd` | MOT-14, ADV-02, ADV-07, STA-08 | P2 |
| INF-11 | Потеря устройства у `DrawingSurface` | Задача уже запланирована: событие `RenderingDeviceReplaced` в профиле есть, но на него никто не подписан. Её нужно решить до первого эффекта на Direct2D | Все эффекты на Direct2D | P2 |
| INF-12 | `wxl::impl::event_adder` для событий типа | Задача уже запланирована: `on_event<EventKey::Rendering, CompositionTarget>()` вместо членов; именно так `HaloEffect` ожидает кадр | Эффекты, ожидающие кадр | P2 |
| INF-13 | Слой Direct3D с собственными шейдерами | Цепочка обмена (swap chain) как поверхность композитора (`ICompositorInterop::CreateCompositionSurfaceForSwapChain`) и HLSL; для статичного изображения — собственный эффект Direct2D в `DrawingSurface`. Единственный путь к собственным шейдерам | MAT-08, MAT-09, ADV-09…ADV-12, LGT-03 (конический) | P3 |

## P1 — минимальный уровень паритета

Без этих 20 эффектов приложение на **wxl** выглядит беднее аналогичного
приложения на Electron. Почти все они опираются на встроенные механизмы
платформы, поэтому основная работа — фундамент INF-01…INF-06 и форма записи в
DSL.

| ID | Эффект | Как сделать в **wxl** | Опора |
| --- | --- | --- | --- |
| MAT-05 | Acrylic окна | `DesktopAcrylicBackdrop` через `Window.SystemBackdrop`. Для `CompositionWindow` нужна отдельная проба: фон сцены убирается, системный фон передаётся самому HWND | профиль |
| MAT-12 | Mica / Mica Alt | `MicaBackdrop` (`Kind = BaseAlt`) через `Window.SystemBackdrop`; для `CompositionWindow` — та же проба, что и для MAT-05. У Electron этого материала нет, и это преимущество **wxl** | профиль |
| LGT-04 | Смена темы на ходу | Открытая задача: запоминать тройку (элемент, свойство, кисть) и применять её заново по `ActualThemeChanged`; с `BackgroundTransition` смена темы также становится плавной | INF-03 |
| LGT-09 | Анимированный градиент / «аврора» | `ColorKeyFrameAnimation` по опорным точкам градиента `CompositionLinearGradientBrush` — на композиторе, без перерисовки | INF-04 |
| MOT-01 | Кривые и длительности | Именованные токены Fluent 2 (`curveDecelerateMax` и др.) в виде `CubicBezierEasingFunction`; классы кривых уже есть в профиле rich | — |
| MOT-02 | Пружины | Анимации NaturalMotion композитора: их можно прервать, они выполняются в потоке DWM | INF-04 |
| MOT-03 | Появление и исчезновение | Неявные анимации появления и скрытия (show/hide) композитора, `EntranceThemeTransition`, `OpacityTransition` | INF-03, INF-04 |
| MOT-05 | Смена содержимого (fade through) | `ContentThemeTransition` в `ContentTransitions`. `Frame` и страниц в **wxl** нет: навигация по типам страниц без XAML неприменима | INF-03 |
| MOT-06 | Каскад (stagger) | `EntranceThemeTransition { IsStaggeringEnabled }` в `ChildrenTransitions` — одна строка в скобках панели | INF-03 |
| MOT-08 | Трансформация контейнера | `ConnectedAnimationService` (`PrepareToAnimate` / `TryStart`) работает между любыми элементами, без `Frame`; в вебе это теперь стандарт (View Transitions) | профиль |
| MOT-09 | Общий элемент (hero) | Тот же `ConnectedAnimationService`; для визуалов сцены — ключевые кадры по прямоугольнику. Кандидат на применение: переход от обложки в списке к открытой книге | профиль |
| MOT-11 | Макетная анимация (FLIP) | `RepositionThemeTransition` в `ChildrenTransitions` даёт её без дополнительного кода; для визуалов сцены — неявная анимация `Offset` | INF-03, INF-04 |
| INT-02 | Сжатие при нажатии | Пружина по `Scale` на `PointerPressed` / `Released`; собственный эффект в скобках (`PressEffect`) | INF-01, INF-04 |
| SCR-01 | Появление при прокрутке | `EffectiveViewportChanged` + неявная анимация появления или выражение от прокрутки | INF-04, INF-05 |
| SCR-02 | Сжимающаяся шапка | Выражение над набором свойств прокрутки `ScrollViewer` — классический приём композитора | INF-05 |
| SCR-03 | Привязка прокрутки | `ScrollViewer.HorizontalSnapPointsType` / `VerticalSnapPointsType` и `FlipView` | профиль |
| SCR-04 | Анимации от прокрутки | Общий механизм: `ExpressionAnimation` от прокрутки, записанный в DSL так же коротко, как `animation-timeline: scroll()` | INF-05 |
| STA-01 | Скелетон | Заглушки из `Rectangle` с кистями Fluent; собственного скелетона у **WinUI** нет | INF-01 |
| STA-02 | Мерцание (shimmer) | `CompositionLinearGradientBrush` с анимированным смещением опорных точек градиента — одна анимация на все заглушки экрана | INF-01, INF-04 |
| STA-11 | Потоковый вывод текста (AI) | Дописывание в `wxl::FormattedBlock` без перевёрстки всего блока; плавное появление фрагмента — собственный эффект. Сегодня это основной сценарий приложений на Electron | INF-01 |

## P2 — выразительные приёмы веба

30 эффектов. Почти все они — собственные эффекты **wxl** по образцу `HaloEffect`:
элемент подключает их в своих скобках, а внутри работают визуалы, выражения и
источники света композитора или одна отрисовка Direct2D.

| ID | Эффект | Как сделать в **wxl** | Опора |
| --- | --- | --- | --- |
| MAT-06 | Шум / зерно | Эффект Turbulence Direct2D рисуется один раз в `DrawingSurface` и выводится плиткой (`wxl::BackgroundFill::Tile` уже есть); у Acrylic собственный шум | INF-11 |
| MAT-07 | Внутреннее свечение | `DropShadow` не умеет отбрасывать тень внутрь: нужна инверсная маска формы через `CompositionMaskBrush` или отрисовка Direct2D | INF-01, INF-02 |
| MAT-10 | Спекулярный блик | `SpotLight` / `PointLight` с `SceneLightingEffect`; следование за указателем — выражением | INF-05, INF-09 |
| MAT-11 | Адаптивный тинт | Средний и доминирующий цвета картинки фона вычисляются один раз во время её декодирования в `TextureCache` и передаются в `TintColor` стекла и в акцентный цвет экрана | MAT-04 |
| LGT-02 | Многослойные мягкие тени | Стопка `DropShadow` с растущим радиусом для любого прямоугольника — то, чего не даёт `ThemeShadow` (у него нет ни цвета, ни радиуса). Это также снимает ограничение LGT-06 на `GetAlphaMask` | INF-01 |
| LGT-08 | Меш-градиент | `ID2D1GradientMesh` Direct2D — настоящий градиент из патчей, которого нет в CSS; рисуется один раз в поверхность фона сцены | INF-11 |
| LGT-12 | Анимированная смена темы | Круговое раскрытие геометрической обрезкой поверх снимка старой темы. Снимок хранится только на время перехода и сразу освобождается, иначе он становится второй копией экрана | LGT-04 |
| LGT-15 | Режимы смешивания | `BlendEffect` композитора | INF-02 |
| MOT-07 | Общая ось | Ключевые кадры по `Translation` и `Opacity` для уходящего и приходящего содержимого | INF-04 |
| MOT-12 | Анимированные числа | Цифры как отдельные визуалы-ленты с обрезкой, прокрутка лент по `Offset`. Кандидат на применение: табло калькулятора | INF-01, INF-04 |
| MOT-13 | Анимация иконок | `AnimatedIcon` со встроенными анимациями; Lottie — через `AnimatedVisualPlayer` и LottieGen с выводом кода на C++ | профиль |
| INT-05 | Тряска поля при ошибке | Ключевые кадры по `Translation` (`SetIsTranslationEnabled` уже есть в профиле) | INF-01 |
| INT-06 | Волна от нажатия (ripple) | Круговой `SpriteVisual` в точке нажатия, `Scale` и `Opacity` ключевыми кадрами, обрезка по скруглению кнопки | INF-01 |
| INT-07 | Перетаскивание с подъёмом | В списках — `ListView.CanReorderItems`; в собственных элементах — `Translation` с подъёмом по Z и `ThemeShadow` | INF-08 |
| INT-09 | Свайп с учётом скорости | `InteractionTracker` с упорами; кандидат на применение — листание страниц жестом | INF-08 |
| INT-13 | Магнитная кнопка | Выражение по позиции указателя с затуханием по расстоянию, возврат пружиной | INF-04, INF-05 |
| INT-14 | Собственный курсор и следящий элемент | `ProtectedCursor` (в **wxl** есть `impl::set_cursor`, `element_helpers.h`) и визуал-«хвост», который следует на пружине за позицией указателя | INF-04, INF-05 |
| INT-15 | 3D-наклон с бликом | `RotationAxis` и перспектива в `TransformMatrix` родителя, вычисляемые от позиции указателя; блик — MAT-10 | INF-05, INF-09 |
| SCR-05 | Прогресс чтения | `Scale.X` полосы из выражения от прокрутки | INF-05 |
| SCR-06 | Схлопывание крупного заголовка | То же выражение, что в SCR-02, по `Scale` и `Offset` заголовка | INF-05 |
| SCR-07 | Карусель с масштабом и перспективой | `InteractionTracker` или прокрутка с упорами + выражение на каждую карточку | INF-05, INF-08 |
| SCR-08 | Глубина под модальным окном | Сцена `CompositionWindow` уходит на задний план: `Scale`, яркость и размытие под островом; двухслойное окно создано именно для таких задач | INF-02, INF-04 |
| SCR-09 | Параллакс | `ParallaxView` из **WinUI** или фон сцены, управляемый выражением от прокрутки острова | INF-05 |
| STA-08 | Анимация успеха и ошибки | Галочка прорисовывается через `TrimEnd` фигуры композитора или с помощью Lottie | INF-10 |
| STA-09 | Пульс бейджа | `InfoBadge` + кольцо ключевыми кадрами `Scale` / `Opacity` с числом повторов | INF-04 |
| STA-10 | Индикатор набора | Три `Ellipse` со сдвинутыми по фазе ключевыми кадрами | INF-01 |
| STA-12 | Конфетти | Десятки `SpriteVisual` с ключевыми кадрами и одной общей поверхностью-атласом; тысячи частиц относятся уже к ADV-09 | INF-01 |
| ADV-02 | Прорисовка линии | `TrimEnd` у `CompositionSpriteShape` — то же, что `stroke-dashoffset` в SVG | INF-10 |
| ADV-03 | Появление текста по буквам | Глифы DirectWrite в `DrawingSurface` — отдельный визуал на слово или букву, каскад ключевыми кадрами; текст для программ чтения с экрана — `AutomationName` | INF-11 |
| ADV-04 | «Расшифровка» текста | `DispatcherQueueTimer` подменяет символы в `TextBlock`; собственный эффект занимает несколько строк | INF-01 |

## P3 — демонстрационные эффекты и собственные шейдеры

21 эффект. В этой группе браузер расходует больше всего ресурсов (WebGL поверх
страницы), а **wxl** — меньше: тот же HLSL выводится непосредственно в цепочку
обмена на сцене. Почти все эффекты группы зависят от INF-13.

| ID | Эффект | Как сделать в **wxl** | Опора |
| --- | --- | --- | --- |
| MAT-08 | Liquid Glass | Собственный шейдер: линза, блик, адаптивный тинт. Композитор не передаёт шейдеру чужие пиксели, поэтому эффект применим только к тому, что **wxl** рисует сам: к фону сцены, к странице `DrawingSurface` | INF-13 |
| MAT-09 | Преломление / линза | Эффект DisplacementMap Direct2D над собственной картинкой, в динамике — шейдер | INF-11, INF-13 |
| MAT-13 | Вибрантность | Текст и иконки, смешанные с размытым фоном: `BlendEffect` над backdrop | INF-02 |
| MAT-14 | Нейморфизм | Две `DropShadow` (светлая и тёмная) или свет композитора; контраст слабый, поэтому эффект подходит как тема оформления, а не как вариант по умолчанию | LGT-02, INF-09 |
| MAT-15 | Клейморфизм | Внешняя тень + внутреннее свечение | LGT-02, MAT-07 |
| LGT-05 | Тональная высота (Material) | Оттенок поверхности зависит от `Translation.Z`: выражение по цвету кисти. Для приложений в стиле Material на Windows | INF-05 |
| LGT-14 | Дуотон | `ColorMatrixEffect` / `TintEffect` композитора над картинкой | INF-02 |
| MOT-14 | Морфинг форм | `PathKeyFrameAnimation` между путями с одинаковым составом сегментов; выравнивание сегментов выполняет **wxl** | INF-10 |
| INT-10 | Потяни, чтобы обновить | `RefreshContainer` из **WinUI**; на десктопе с мышью этот приём встречается редко | профиль |
| INT-12 | Контекстное меню с превью | `MenuFlyout` + подъём элемента по Z и размытие сцены на время показа меню | SCR-08 |
| INT-16 | Звуковой отклик | `ElementSoundPlayer` из **WinUI** включается одним свойством | профиль |
| SCR-10 | Горизонтальная секция при вертикальной прокрутке | Закреплённый блок со `Translation.X` из выражения от прокрутки | INF-05 |
| ADV-05 | Анимация вариативного шрифта | Оси шрифта через DirectWrite (`IDWriteFontFace5`), покадровая отрисовка в `DrawingSurface` | INF-11, INF-12 |
| ADV-06 | Кинетическая типографика | ADV-03 + временные шкалы композитора (`AnimationController` уже есть в профиле) | ADV-03 |
| ADV-07 | Морфинг блобов | `PathKeyFrameAnimation`, как в MOT-14 | INF-10 |
| ADV-08 | Анимированное зерно | Плитка MAT-06, сдвигаемая ступенчато по `Offset`; шум не перерисовывается | MAT-06 |
| ADV-09 | Системы частиц | Direct3D: частицы на GPU (вычислительные шейдеры) в цепочку обмена сцены | INF-13 |
| ADV-10 | Шейдерный фон | Фрагментный шейдер на всю сцену `CompositionWindow` — естественное место для него, остров XAML располагается поверх | INF-13 |
| ADV-11 | Искажение / «жидкость» | DisplacementMap Direct2D или шейдер смещения | INF-13 |
| ADV-12 | 3D-объекты | Собственный конвейер Direct3D в цепочку обмена, модели glTF | INF-13 |
| ADV-13 | Глитч | Сдвиг каналов RGB и нарезка шейдером; нужна проверка на вспышки (WCAG 2.3.1) | INF-13 |

## Вне каталога

Пять пунктов исключены не потому, что их нет во Fluent, а потому, что они не
являются эффектами или не имеют смысла на десктопе.

| ID | Эффект | Почему |
| --- | --- | --- |
| MOT-10 | View Transitions | Веб-API, а не эффект; его результат покрывают MOT-05, MOT-08, MOT-09 |
| STA-05 | Оптимистичный UI | Приём уровня модели, а не отрисовки: в **wxl** это `observable` и привязки |
| STA-06 | Пустое состояние | Приём вёрстки из готовых элементов управления; оживляется с помощью MOT-03 и MOT-13 |
| INT-11 | Тактильный отклик | На десктопе тактильный отклик возможен разве что у пера; к пункту стоит вернуться, если появится сценарий с пером |
| SCR-11 | Пространственный UI (visionOS) | Другая платформа и другой ввод |

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
- [ ] Текст, разбитый на визуалы или нарисованный в `DrawingSurface`,
  дублируется для программ чтения с экрана (`AutomationName`).
- [ ] Контраст текста на стекле и картинке не ниже 4.5:1 на самом
  неблагоприятном фоне; это уже учтено в `OverlayCard`: тёмный затемняющий
  слой и цвет текста тёмной темы.

**Производительность**

- [ ] Движение задаётся анимациями и выражениями композитора, а не кадрами в
  STA-потоке: тогда оно остаётся плавным, пока поток занят. В этом главное
  преимущество перед браузером с анимациями на JS.
- [ ] Пиксели рисуются один раз в одну поверхность; `DrawingSurface`
  перерисовывается при изменении, а не каждый кадр. Снимок для перехода
  хранится только на время перехода.
- [ ] Размытие и backdrop обходятся дороже всего: их площадь ограничивается,
  слои стекла не накладываются друг на друга, радиус не анимируется.
- [ ] Шейдеры и частицы приостанавливаются, когда окно свёрнуто или визуал не
  виден, и восстанавливаются после потери устройства (INF-11).
- [ ] Стоимость измеряется, а не предполагается: в `wxl.ui/benchmarks` уже
  есть сцена для сравнения с cppwinrt; такое же сравнение стоит провести с
  той же сценой в WebView2.

**Проверка**

- [ ] Каждый эффект получает страницу в группе Effects примера
  `samples/Gallery`. Снимок через `PrintWindow` / `BitBlt` не захватывает
  содержимое DirectComposition, поэтому пример и драйвер интерфейса пока
  остаются единственным покрытием; захват через `Windows.Graphics.Capture`
  позволил бы написать настоящий тест.

## Источники

**Код wxl:** `wxl.ui/src` — `HaloEffect.h`, `BevelEffect.h`, `Card.h`,
`CompositionWindow.h`, `DrawingSurface.h`, `TextureCache.h`, `WindowShade.h`,
`WindowNative.h`; профили `base`, `extended`, `rich`; `samples/Quadratic`.

**Microsoft Learn:**

- [Composition effects](https://learn.microsoft.com/windows/apps/develop/composition/composition-effects)
- [CompositionEffectBrush — Windows App SDK](https://learn.microsoft.com/windows/windows-app-sdk/api/winrt/microsoft.ui.composition.compositioneffectbrush?view=windows-app-sdk-2.0)
- [Compositor.CreateBackdropBrush](https://learn.microsoft.com/windows/windows-app-sdk/api/winrt/microsoft.ui.composition.compositor.createbackdropbrush?view=windows-app-sdk-2.0)
- [Composition native interop with DirectX and Direct2D](https://learn.microsoft.com/windows/apps/develop/composition/composition-native-interop)
- [ICompositorInterop::CreateCompositionSurfaceForSwapChain](https://learn.microsoft.com/windows/win32/api/windows.ui.composition.interop/nf-windows-ui-composition-interop-icompositorinterop-createcompositionsurfaceforswapchain)
- [Implicit transitions (WinUI 3)](https://learn.microsoft.com/windows/apps/develop/motion/implicit-transitions)
- [UISettings.AnimationsEnabledChanged](https://learn.microsoft.com/uwp/api/windows.ui.viewmanagement.uisettings.animationsenabledchanged?view=winrt-28000)
- [Tailoring effects and experiences](https://learn.microsoft.com/windows/apps/develop/composition/composition-tailoring#animations-settings)

**Первая версия каталога (веб и платформы):**

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

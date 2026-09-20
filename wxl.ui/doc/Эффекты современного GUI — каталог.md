# Эффекты современного GUI — каталог

Приоритеты пересчитаны под цель wxl — на равных конкурировать с WebView2/Electron на десктопе — и под средства: WinUI 3, композитор `Microsoft.UI.Composition`, Direct2D и Direct3D. Рекомендации Fluent здесь не фильтр: чего нет в WinUI, wxl пишет сам, как уже написан `HaloEffect`. Основа — код `wxl.ui`, примеры, профили генератора; ID из первой версии сохранены. Итог: готово 17, P1 — 22, P2 — 31, P3 — 21, вне каталога 5.

- **Готово** — уже есть в wxl или WinUI даёт само (шаблоны контролов, ресурсы Fluent).
- **P1** — нижняя планка паритета: то, что приложение на Electron получает из CSS даром, плюс родные материалы Windows, которых у Electron нет.
- **P2** — выразительные приёмы, частые в вебе: свои эффекты wxl по образцу `HaloEffect`, выражения композитора, Direct2D в `DrawingSurface`.
- **P3** — витринное и тяжёлое: свои шейдеры HLSL через Direct3D, частицы, 3D, кинетический текст. Именно здесь wxl может обогнать браузер по цене кадра.
- **Вне каталога** — на десктопе нет смысла, или это не эффект, или пункт закрыт другим.

Механизм выбирается по одному правилу: движение — на композиторе, а не в интерфейсном потоке; пиксели — один раз в одну поверхность, без второй копии.

## Готово

17 эффектов из каталога уже есть: часть написана в wxl, часть WinUI даёт само. Ещё три наработки wxl (WXL-…) в каталоге не было, но на них стоят будущие эффекты.

| ID | Эффект | Чем сделано |
| --- | --- | --- |
| MAT-02 | Слои полупрозрачности | Кисти слоёв Fluent через `brushes.*` — 755 ключей деревом, путь к кисти следует за темой |
| MAT-03 | Волосяные обводки | `wxl::Card` и `OverlayCard`: рамка 1 px кистью `Card.StrokeColorDefault` или светлой линией поверх картинки |
| LGT-01 | Тени и высота | `ThemeShadow` + `translation = {0, 0, 32}` в `Card` |
| LGT-03 | Градиенты | `LinearGradientBrush`, `RadialGradientBrush` в профиле; на композиторе — `CompositionLinearGradientBrush`. Конического нет ни в WinUI, ни в Direct2D — свой эффект |
| LGT-06 | Цветные тени | `HaloEffect` со смещением (настройки — `Preset<DropShadow>`); пока только для элементов с `GetAlphaMask` — текста, картинки, фигуры |
| LGT-07 | Свечение / неон | `HaloEffect` — `DropShadow` без смещения по альфе глифов; неоновая вывеска в `samples/Effects` (ветка `effects`, не влита) |
| LGT-10 | Градиентная обводка | `BevelEffect`: ось градиента нацелена по диагонали и следует за `SizeChanged` |
| LGT-11 | Системный акцент (вместо Material You) | Кисти `AccentFillColor*` следуют за акцентом Windows; палитра из картинки — это MAT-11 |
| MOT-04 | Модальные окна и выпадающие панели | `wxl::showDialog` для `ContentDialog`, `Flyout` / `MenuFlyout` в профиле; анимации встроенные |
| INT-01 | Наведение | Состояния шаблонов WinUI; `ThemeBrush` — состояния контрола в цветах приложения |
| INT-03 | Кольцо фокуса | Системные визуалы фокуса WinUI |
| INT-04 | Переключатели | Шаблоны `ToggleSwitch`, `CheckBox`, `RadioButton` |
| INT-08 | Резиновая прокрутка | `ScrollViewer` — на сенсоре и тачпаде |
| STA-03 | Спиннер | `ProgressRing` |
| STA-04 | Прогресс-бар | `ProgressBar` |
| STA-07 | Тост / снэкбар | `InfoBar` (профиль extended) |
| ADV-01 | Градиентный текст | `TextBlock.Foreground` градиентной кистью |
| WXL-01 | Полёт окна к точке и обратно | `WindowShade`: снимок окна летит ключевыми кадрами по `Offset`, `Scale`, `Opacity` на потоке DWM |
| WXL-02 | Сцена окна под островом XAML | `CompositionWindow`: окно без поверхности перенаправления, фон картинкой с режимами заполнения, размер визуалов — `ExpressionAnimation` от сцены |
| WXL-03 | Своё рисование и текстуры | `DrawingSurface` (цепочка D3D11 → D2D → `CompositionGraphicsDevice`) и `TextureCache` (декод через Win2D, асинхронно, с кэшем по пути) |

## Фундамент: что нужно в wxl до самих эффектов

Большинство эффектов ниже упирается не в себя, а в один из этих пунктов. Сделанный один раз, каждый открывает сразу несколько строк каталога.

| ID | Что | Механизм | Открывает | Приор. |
| --- | --- | --- | --- | --- |
| INF-01 | Общий каркас «эффекта в скобках» | `HaloEffect` и `BevelEffect` уже повторяют одну форму: функтор, который элемент «надевает», ничей корутины, ожидание визуала элемента, выход при смерти окна. По правилу wxl повторяющийся механизм становится своим типом | Все свои эффекты P1–P3 | P1 |
| INF-02 | Эффекты композитора | `CompositionEffectFactory` / `CompositionEffectBrush` с описаниями эффектов Win2D (проекция Win2D уже в дереве ради `TextureCache`), `CreateBackdropBrush`. Шейдеры только встроенные — свой композитор не принимает | MAT-01, MAT-04, MAT-06, MAT-13, LGT-14, LGT-15, SCR-08 | P1 |
| INF-03 | Переходы XAML в профиле | `Transitions`, `ChildrenTransitions`, `ContentTransitions`; `EntranceThemeTransition`, `RepositionThemeTransition`, `ContentThemeTransition`, `AddDeleteThemeTransition`; неявные `OpacityTransition`, `ScaleTransition`, `TranslationTransition`, `RotationTransition`, `BackgroundTransition` | MOT-03, MOT-05, MOT-06, MOT-11, LGT-04 | P1 |
| INF-04 | Пружины и неявные анимации композитора | `SpringVector3NaturalMotionAnimation`, `SpringScalarNaturalMotionAnimation`, `ImplicitAnimationCollection`, `SetImplicitShowAnimation` / `HideAnimation`, `ColorKeyFrameAnimation` | MOT-02, MOT-03, MOT-11, INT-02, LGT-09 | P1 |
| INF-05 | Наборы свойств для выражений | `GetScrollViewerManipulationPropertySet`, `GetPointerPositionPropertySet`. Выражение считается на композиторе, как `animation-timeline` в CSS | SCR-01…SCR-09, LGT-13, INT-13, INT-15 | P1 |
| INF-06 | Выключатель анимаций и прозрачности | `UISettings.AnimationsEnabled` (+ `AnimationsEnabledChanged`, Windows 10 2004) и `AdvancedEffectsEnabled` (+ `Changed`). Переходы XAML и `AcrylicBrush` слушаются сами, анимации и эффекты wxl — нет: нужно одно место, которое решает за всех | Все движущиеся и прозрачные эффекты | P1 |
| INF-07 | Композиция — в `wxl.dwm` | Уже в TODO: `CompositionWindow`, `DrawingSurface`, `TextureCache` и Win2D уезжают из `wxl.ui`. Новым эффектам лучше сразу рождаться там | — | P1 |
| INF-08 | InteractionTracker | Жест с инерцией, упорами и пружинным возвратом — целиком на композиторе | INT-07, INT-09, SCR-07, INT-10 | P2 |
| INF-09 | Источники света | `AmbientLight`, `PointLight`, `SpotLight`, `DistantLight` с `Targets`; блик — `SceneLightingEffect` | MAT-10, LGT-13, INT-15, MAT-14 | P2 |
| INF-10 | Фигуры композитора | `ShapeVisual`, `CompositionSpriteShape`, `CompositionPathGeometry`, `PathKeyFrameAnimation`, `TrimStart` / `TrimEnd` | MOT-14, ADV-02, ADV-07, STA-08 | P2 |
| INF-11 | Потеря устройства у `DrawingSurface` | Уже в TODO: `RenderingDeviceReplaced` в профиле есть, никто не подписан. До первого эффекта на Direct2D | Все эффекты на Direct2D | P2 |
| INF-12 | `EventAdder` для событий типа | Уже в TODO: `on_event<EventKey::Rendering, CompositionTarget>()` вместо членов; `HaloEffect` ждёт кадр именно так | Эффекты, ждущие кадр | P2 |
| INF-13 | Слой Direct3D со своими шейдерами | Свап-чейн как поверхность композитора (`ICompositorInterop::CreateCompositionSurfaceForSwapChain`) и HLSL; для статики — свой эффект Direct2D в `DrawingSurface`. Единственная дверь к своим шейдерам | MAT-08, MAT-09, ADV-09…ADV-12, LGT-03 (конический) | P3 |

## P1 — нижняя планка паритета

Без этих 22 эффектов приложение на wxl выглядит беднее соседа на Electron. Почти все они стоят на родных механизмах, поэтому главная работа — фундамент INF-01…INF-06 и форма в DSL.

| ID | Эффект | Как сделать в wxl | Опора |
| --- | --- | --- | --- |
| MAT-01 | Размытие фона | `AcrylicBrush` (около 40 ключей в ресурсах) или свой `GaussianBlur` над `CreateBackdropBrush`. Проверить, видит ли backdrop в острове сцену `CompositionWindow` под ним: эффекты композитора не читают чужое дерево | INF-02 |
| MAT-04 | Стекло (glassmorphism) | `AcrylicBrush` со своими `TintColor`, `TintOpacity`, `TintLuminosityOpacity` — вариант `OverlayCard` со стеклом вместо скрима | класс в профиле |
| MAT-05 | Acrylic окна | `DesktopAcrylicBackdrop` через `Window.SystemBackdrop`. Для `CompositionWindow` — отдельная проба: фон сцены снят, системный задник подан самому HWND | профиль |
| MAT-12 | Mica / Mica Alt | `MicaBackdrop` (`Kind = BaseAlt`) через `Window.SystemBackdrop`; для `CompositionWindow` — та же проба, что у MAT-05. У Electron этого материала нет — это преимущество | профиль |
| LGT-04 | Смена темы на ходу | Открытая задача в TODO: запоминать (элемент, свойство, кисть) и переприменять на `ActualThemeChanged`; с `BackgroundTransition` смена ещё и плавная | INF-03 |
| LGT-09 | Анимированный градиент / «аврора» | `ColorKeyFrameAnimation` по стопам `CompositionLinearGradientBrush` — на композиторе, без перерисовки | INF-04 |
| MOT-01 | Кривые и длительности | Именованные токены Fluent 2 (`curveDecelerateMax` и др.) как `CubicBezierEasingFunction`; классы кривых уже в профиле rich | — |
| MOT-02 | Пружины | NaturalMotion-анимации композитора: прерываемы, идут в потоке DWM | INF-04 |
| MOT-03 | Появление и исчезновение | Неявные show/hide-анимации композитора, `EntranceThemeTransition`, `OpacityTransition` | INF-03, INF-04 |
| MOT-05 | Смена содержимого (fade through) | `ContentThemeTransition` на `ContentTransitions`. `Frame` и страниц в wxl нет: навигация по типам страниц без XAML не ложится | INF-03 |
| MOT-06 | Каскад (stagger) | `EntranceThemeTransition { IsStaggeringEnabled }` в `ChildrenTransitions` — одна строка в скобках панели | INF-03 |
| MOT-08 | Трансформация контейнера | `ConnectedAnimationService` (`PrepareToAnimate` / `TryStart`) работает между любыми элементами, без `Frame`; у веба это теперь стандарт (View Transitions) | профиль |
| MOT-09 | Общий элемент (hero) | Тот же `ConnectedAnimationService`; для визуалов сцены — ключевые кадры по прямоугольнику. Кандидат: обложка в витрине Буквицы → открытая книга | профиль |
| MOT-11 | Макетная анимация (FLIP) | `RepositionThemeTransition` в `ChildrenTransitions` даёт её даром; для визуалов сцены — неявная анимация `Offset` | INF-03, INF-04 |
| INT-02 | Сжатие при нажатии | Пружина по `Scale` на `PointerPressed` / `Released`; свой эффект в скобках (`PressEffect`) | INF-01, INF-04 |
| SCR-01 | Появление при прокрутке | `EffectiveViewportChanged` + неявная show-анимация, или выражение от прокрутки | INF-04, INF-05 |
| SCR-02 | Шапка, которая сжимается | Выражение над набором свойств прокрутки `ScrollViewer` — классический приём композитора | INF-05 |
| SCR-03 | Привязка прокрутки | `ScrollViewer.HorizontalSnapPointsType` / `VerticalSnapPointsType` и `FlipView` | профиль |
| SCR-04 | Анимации от прокрутки | Общий механизм: `ExpressionAnimation` от прокрутки, оформленный в DSL так же коротко, как `animation-timeline: scroll()` | INF-05 |
| STA-01 | Скелетон | Заглушки из `Rectangle` с кистями Fluent; у WinUI своего нет | INF-01 |
| STA-02 | Мерцание (shimmer) | `CompositionLinearGradientBrush` с анимированным смещением стопов — одна анимация на все заглушки экрана | INF-01, INF-04 |
| STA-11 | Потоковый вывод текста (AI) | Дописывание в `FormattedBlock` без перевёрстки всего блока; плавное появление куска — свой эффект. Сегодня это главный сценарий приложений на Electron | INF-01 |

## P2 — выразительные приёмы веба

31 эффект. Почти все — свои эффекты wxl по образцу `HaloEffect`: элемент надевает их в своих скобках, а внутри — визуалы, выражения и свет композитора или одна отрисовка Direct2D.

| ID | Эффект | Как сделать в wxl | Опора |
| --- | --- | --- | --- |
| MAT-06 | Шум / зерно | Эффект Turbulence Direct2D один раз в `DrawingSurface` и плиткой (`BackgroundFill::Tile` уже есть); у Acrylic шум свой | INF-11 |
| MAT-07 | Внутреннее свечение | `DropShadow` внутрь не умеет: инверсная маска формы через `CompositionMaskBrush` или отрисовка Direct2D | INF-01, INF-02 |
| MAT-10 | Спекулярный блик | `SpotLight` / `PointLight` с `SceneLightingEffect`; за указателем — выражением | INF-05, INF-09 |
| MAT-11 | Адаптивный тинт | Средний и доминирующий цвет картинки фона считаются один раз, пока она декодируется в `TextureCache`, и идут в `TintColor` стекла и в акцент экрана | MAT-04 |
| LGT-02 | Многослойные мягкие тени | Стопка `DropShadow` с растущим радиусом для любого прямоугольника — то, чего не даёт `ThemeShadow` (у него ни цвета, ни радиуса). Заодно снимает ограничение LGT-06 на `GetAlphaMask` | INF-01 |
| LGT-08 | Меш-градиент | `ID2D1GradientMesh` Direct2D — настоящий меш из патчей, которого в CSS нет; один раз в поверхность фона сцены | INF-11 |
| LGT-12 | Анимированная смена темы | Круговое раскрытие геометрической обрезкой поверх снимка старой темы. Снимок — на время перехода и сразу отдаётся, иначе это вторая копия экрана | LGT-04 |
| LGT-13 | Свет за курсором (spotlight) | `PointLight` с позицией из выражения по указателю; без событий на STA и пересчёта стилей, как в вебе | INF-05, INF-09 |
| LGT-15 | Режимы смешивания | `BlendEffect` композитора | INF-02 |
| MOT-07 | Общая ось | Ключевые кадры по `Translation` и `Opacity` для уходящего и приходящего содержимого | INF-04 |
| MOT-12 | Анимированные числа | Цифры отдельными визуалами-лентами с обрезкой, карусель по `Offset`. Кандидат: табло Калькулятора | INF-01, INF-04 |
| MOT-13 | Анимация иконок | `AnimatedIcon` с встроенными анимациями; Lottie — `AnimatedVisualPlayer` и LottieGen с выводом в C++ | профиль |
| INT-05 | Тряска поля при ошибке | Ключевые кадры по `Translation` (`SetIsTranslationEnabled` уже в профиле) | INF-01 |
| INT-06 | Волна от нажатия (ripple) | Круговой `SpriteVisual` в точке нажатия, `Scale` и `Opacity` ключевыми кадрами, обрезка по скруглению кнопки | INF-01 |
| INT-07 | Перетаскивание с подъёмом | В списках — `ListView.CanReorderItems`; своё — `Translation` с подъёмом по Z и `ThemeShadow` | INF-08 |
| INT-09 | Свайп с учётом скорости | `InteractionTracker` с упорами; кандидат — листание страниц Буквицы жестом | INF-08 |
| INT-13 | Магнитная кнопка | Выражение по позиции указателя с затуханием по расстоянию, возврат пружиной | INF-04, INF-05 |
| INT-14 | Свой курсор и следящий элемент | `ProtectedCursor` (в wxl есть `impl/cursor`) и визуал-«хвост» на пружине за позицией указателя | INF-04, INF-05 |
| INT-15 | 3D-наклон с бликом | `RotationAxis` и перспектива в `TransformMatrix` родителя от позиции указателя, блик — MAT-10 | INF-05, INF-09 |
| SCR-05 | Прогресс чтения | `Scale.X` полосы из выражения от прокрутки | INF-05 |
| SCR-06 | Схлопывание крупного заголовка | То же выражение, что SCR-02, по `Scale` и `Offset` заголовка | INF-05 |
| SCR-07 | Карусель с масштабом и перспективой | `InteractionTracker` или прокрутка с упорами + выражение на каждую карточку | INF-05, INF-08 |
| SCR-08 | Глубина под модальным окном | Сцена `CompositionWindow` уходит назад: `Scale`, яркость и размытие под островом — двухслойное окно сделано именно для таких вещей | INF-02, INF-04 |
| SCR-09 | Параллакс | `ParallaxView` из WinUI, или фон сцены по выражению от прокрутки острова | INF-05 |
| STA-08 | Анимация успеха и ошибки | Галочка рисуется `TrimEnd` фигуры композитора, или Lottie | INF-10 |
| STA-09 | Пульс бейджа | `InfoBadge` + кольцо ключевыми кадрами `Scale` / `Opacity` с числом повторов | INF-04 |
| STA-10 | Индикатор набора | Три `Ellipse` со сдвинутыми по фазе ключевыми кадрами | INF-01 |
| STA-12 | Конфетти | Десятки `SpriteVisual` с ключевыми кадрами и одной общей поверхностью-атласом; тысячи частиц — уже ADV-09 | INF-01 |
| ADV-02 | Прорисовка линии | `TrimEnd` у `CompositionSpriteShape` — то же, что `stroke-dashoffset` в SVG | INF-10 |
| ADV-03 | Появление текста по буквам | Глифы DirectWrite в `DrawingSurface` — по визуалу на слово или букву, каскад ключевыми кадрами; текст для скринридера — `AutomationName` | INF-11 |
| ADV-04 | «Расшифровка» текста | `DispatcherQueueTimer` подменяет символы в `TextBlock` — несколько строк своего эффекта | INF-01 |

## P3 — витрина и свои шейдеры

21 эффект. Здесь браузер платит больше всего (WebGL поверх страницы), а wxl — меньше: тот же HLSL идёт прямо в свап-чейн на сцене. Почти всё зависит от INF-13.

| ID | Эффект | Как сделать в wxl | Опора |
| --- | --- | --- | --- |
| MAT-08 | Liquid Glass | Свой шейдер: линза, блик, адаптивный тинт. Композитор чужих пикселей шейдеру не отдаёт, поэтому — над тем, что wxl рисует сам: фон сцены, страница `DrawingSurface` | INF-13 |
| MAT-09 | Преломление / линза | Эффект DisplacementMap Direct2D над своей картинкой, в динамике — шейдер | INF-11, INF-13 |
| MAT-13 | Вибрантность | Текст и иконки, смешанные с размытым фоном: `BlendEffect` над backdrop | INF-02 |
| MAT-14 | Нейморфизм | Две `DropShadow` (светлая и тёмная) или свет композитора; контраст слабый — как тема оформления, не по умолчанию | LGT-02, INF-09 |
| MAT-15 | Клейморфизм | Внешняя тень + внутреннее свечение | LGT-02, MAT-07 |
| LGT-05 | Тональная высота (Material) | Оттенок поверхности от `Translation.Z`: выражение по цвету кисти. Для приложений в стиле Material на Windows | INF-05 |
| LGT-14 | Дуотон | `ColorMatrixEffect` / `TintEffect` композитора над картинкой | INF-02 |
| MOT-14 | Морфинг форм | `PathKeyFrameAnimation` между путями с одинаковым составом сегментов; выравнивание сегментов — работа wxl | INF-10 |
| INT-10 | Потяни, чтобы обновить | `RefreshContainer` из WinUI; на десктопе с мышью редкость | профиль |
| INT-12 | Контекстное меню с превью | `MenuFlyout` + подъём элемента по Z и размытие сцены на время меню | SCR-08 |
| INT-16 | Звуковой отклик | `ElementSoundPlayer` из WinUI — включается одним свойством | профиль |
| SCR-10 | Горизонтальная секция при вертикальной прокрутке | Закреплённый блок со `Translation.X` из выражения от прокрутки | INF-05 |
| ADV-05 | Анимация вариативного шрифта | Оси шрифта через DirectWrite (`IDWriteFontFace5`), отрисовка в `DrawingSurface` по кадрам | INF-11, INF-12 |
| ADV-06 | Кинетическая типографика | ADV-03 + таймлайны композитора (`AnimationController` уже в профиле) | ADV-03 |
| ADV-07 | Морфинг блобов | `PathKeyFrameAnimation`, как MOT-14 | INF-10 |
| ADV-08 | Анимированное зерно | Плитка MAT-06, сдвигаемая ступеньками по `Offset`; без перерисовки шума | MAT-06 |
| ADV-09 | Системы частиц | Direct3D: частицы на GPU (compute) в свап-чейн сцены | INF-13 |
| ADV-10 | Шейдерный фон | Фрагментный шейдер во всю сцену `CompositionWindow` — естественное место, остров XAML поверх | INF-13 |
| ADV-11 | Искажение / «жидкость» | DisplacementMap Direct2D или шейдер смещения | INF-13 |
| ADV-12 | 3D-объекты | Свой конвейер Direct3D в свап-чейн, модели glTF | INF-13 |
| ADV-13 | Глитч | Сдвиг каналов RGB и нарезка шейдером; проверять на вспышки (WCAG 2.3.1) | INF-13 |

## Вне каталога

Пять пунктов сняты не потому, что их нет во Fluent, а потому, что это не эффекты или у них нет смысла на десктопе.

| ID | Эффект | Почему |
| --- | --- | --- |
| MOT-10 | View Transitions | Веб-API, а не эффект; его результат — MOT-05, MOT-08, MOT-09 |
| STA-05 | Оптимистичный UI | Приём модели, а не отрисовки: в wxl это `observable` и привязки |
| STA-06 | Пустое состояние | Приём вёрстки из готовых контролов; оживляется MOT-03 и MOT-13 |
| INT-11 | Тактильный отклик | На десктопе его даёт разве что перо; вернуться, если появится сценарий с пером |
| SCR-11 | Пространственный UI (visionOS) | Другая платформа и другой ввод |

## Требования ко всем эффектам на Windows

Эффект готов, когда он слушается настроек специальных возможностей Windows, движется на композиторе и есть в примере, который его показывает.

**Специальные возможности**

- [ ] «Эффекты анимации» выключены (`UISettings.AnimationsEnabled`) — анимация wxl сразу приходит в конечное значение, бесконечная — стоит. Переходы XAML делают это сами, композитор — нет (INF-06).
- [ ] «Эффекты прозрачности» выключены (`AdvancedEffectsEnabled`) — стекло и размытие wxl становятся сплошной заливкой того же тона, как это делает `AcrylicBrush`.
- [ ] Высокая контрастность — декоративные эффекты (свечение, шум, шейдеры) снимаются, цвета берутся из системной палитры.
- [ ] Не больше трёх вспышек в секунду (WCAG 2.3.1) — глитч, пульсы, мигание.
- [ ] Текст, разобранный на визуалы или нарисованный в `DrawingSurface`, повторён для скринридера (`AutomationName`).
- [ ] Контраст текста на стекле и картинке — не ниже 4.5:1 на худшем фоне; то, что уже выучил `OverlayCard`: тёмный скрим и чернила тёмной темы.

**Производительность**

- [ ] Движение — анимациями и выражениями композитора, а не кадрами на STA-потоке: тогда оно не дёргается, пока поток занят. Это и есть главное преимущество перед браузером с JS-анимациями.
- [ ] Пиксели — один раз в одну поверхность; `DrawingSurface` перерисовывается при изменении, а не каждый кадр. Снимок для перехода живёт только на время перехода.
- [ ] Размытие и backdrop — самое дорогое: ограниченная площадь, без стопок стекла друг над другом, радиус не анимируется.
- [ ] Шейдеры и частицы ставятся на паузу, когда окно свёрнуто или визуал не виден; переживают потерю устройства (INF-11).
- [ ] Цена измерена, а не предположена: у wxl уже есть сцена сравнения с cppwinrt в `wxl.ui/benchmarks`; то же стоит сделать против той же сцены в WebView2.

**Проверка**

- [ ] Каждый эффект — страница в `samples/Effects`. Снимок через `PrintWindow` / `BitBlt` DirectComposition не видит, поэтому пример и драйвер `run-wxl` — пока единственное покрытие; захват через `Windows.Graphics.Capture` дал бы настоящий тест.

## Источники

**Код и записи wxl:** `wxl.ui/src` — `HaloEffect.h`, `BevelEffect.h`, `Card.h`, `CompositionWindow.h`, `DrawingSurface.h`, `TextureCache.h`, `WindowShade.h`, `WindowBackdrop.h`; профили `base`, `extended`, `rich`; `samples/Quadratic`; в `.claude` — `TODO.txt`, `next-session-prompt.md` и разделы `solutions.md` про профиль rich, `DrawingSurface`, Win2D, свечение букв и белое при ресайзе.

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

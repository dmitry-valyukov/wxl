# Примеры wxl

Каждый пример — отдельное приложение с собственным `CMakeLists.txt` и
собственными ресурсами; вместе они показывают синтаксис **wxl** на задачах
разного размера. Все примеры собираются вместе с деревом; один пример
собирается так:

```
tools\build.ps1 -Target sample.gallery
```

Исполняемый файл находится в `build\x64\samples\<Пример>\Debug\`.

| Пример | Цель сборки | Что показывает |
| --- | --- | --- |
| [HelloHere](https://github.com/dmitry-valyukov/wxl/tree/main/samples/HelloHere) | `sample.hello` | Целое приложение в одной функции: `wxl_launched()` с описанием окна, без `wWinMain`, `Application` и XAML. Пресеты как константы. |
| [Quadratic](Quadratic/README.md) | `sample.quadratic` | Квадратное уравнение: три `NumberBox` на входе, дискриминант и корни на выходе. Модель — поля `observable`, интерфейс — одно выражение с односторонними привязками; обработчиков ввода нет. Образец для небольших расчётных утилит. |
| [Calculator](https://github.com/dmitry-valyukov/wxl/tree/main/samples/Calculator) | `sample.calc` | Калькулятор: сетка кнопок строится обходом строкового литерала (`iterate`), внешний вид кнопок задают эффекты `BevelEffect` и `Button3DEffect`, свечение табло — `HaloEffect`; табло привязано к модели, клавиатура работает с окном без единого щелчка мышью. |
| [CustomTitleBar](https://github.com/dmitry-valyukov/wxl/tree/main/samples/CustomTitleBar) | `sample.custom-titlebar` | Окно с собственной строкой заголовка, написанной как обычный элемент: окно само размещает кнопки заголовка и передаёт Windows области перетаскивания. Масштабирование увеличивает весь остров разом — заголовок, кнопки и содержимое, как браузер увеличивает страницу. |
| [Trayed](Trayed/README.md) | `sample.trayed` | Запускает программу из командной строки и размещает её консоль в собственном окне; управление из значка в области уведомлений, сворачивание в область уведомлений с анимацией, окно настроек как второе окно. |
| [HtmlView](https://github.com/dmitry-valyukov/wxl/tree/main/samples/HtmlView) | `sample.htmlview` | Просмотр разметки: `HtmlBlock`, `BbBlock` и `RsdnBlock` над общим `MarkupBlock`. Кнопки добавляют фрагменты, как лента чата; любой файл `.html`, перетащенный в окно, открывается вместо образцов. |
| [Gallery](Gallery/README.md) | `sample.gallery` | Портированная [WinUI Gallery](https://github.com/microsoft/WinUI-Gallery): те же типы **WinUI**, без XAML. Оболочка с навигацией и поиском, страница на каждый портированный элемент управления, код примера показан под ним. |

У части примеров есть собственный `README.md` с подробностями:
[Quadratic](Quadratic/README.md), [Trayed](Trayed/README.md),
[Gallery](Gallery/README.md).

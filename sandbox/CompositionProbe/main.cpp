// Проба своего окна на композиторе. Открывает wxl::CompositionWindow: сцена с
// цветным задним фоном (визуал композитора) и XAML-карточка островом поверх.
// Показать, что окно WS_EX_NOREDIRECTIONBITMAP компонуется без белого,
// ресайзится за рамкой и держит оснастку прозрачным островом.
//
// Как у любого приложения на wxl: ни wWinMain, ни загрузчика -- их поднимает
// wxl и зовёт это.
#include "pch.h"

using namespace wxl;
using namespace wxl::dsl;

wxl::Teardown wxl_launched() {
    CompositionWindow const window{L"Проба своего окна", SizeInt32{720, 520}};

    // Задний фон сцены -- визуал композитора, тёмно-бирюзовый: любой белый
    // просвет на нём кричал бы. (Картинкой -- тот же background, синхронно
    // перегрузкой от path или на ходу backgroundAsync через Win2D.)
    window.background(rgb(30, 58, 58));

    // Оснастка приходит островом поверх сцены: прозрачный грид во всё окно с
    // карточкой по центру. Сквозь прозрачные места острова видна сцена под ним.
    auto chrome = Grid {
        Border {
            hAlign.center,
            vAlign.center,
            CornerRadius {16},
            Padding {28},
            background = brushes.Card.BackgroundFillColor.Default,
            borderBrush = brushes.Card.StrokeColorDefault,
            BorderThickness {1},
            Button {
                content = L"Своё окно на композиторе",
                onClick = {content = L"Ввод дошёл!"},
            },
        },
    };
    window.content(chrome);

    window.activate();

    return {};
}

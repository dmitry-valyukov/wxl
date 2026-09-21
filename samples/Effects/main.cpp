// Витрина эффектов wxl: одно окно, слева список эффектов, справа страница
// выбранного.
//
// Переключение — одна строка: `host.child(...)`. Ни `Frame`, ни `Page`, ни
// стека навигации здесь нет и не нужно — показана всегда ровно одна
// страница, и список рядом с ней. Что такое страница и почему она
// `FrameworkElement`, сказано в Pages.h.

#include "Pages.h"

#include <memory>
#include <vector>

using namespace wxl;
using namespace wxl::dsl;

namespace {

// Каталог: имя на кнопке и в полосе над страницей, пояснение в подсказке и
// функция, строящая страницу. Новый эффект — строка здесь и свой .cpp.
struct Effect {
    const char16_t* name;
    const char16_t* about;
    effects::Page page;
};

constexpr Effect catalogue[] = {
    {u"Halo Effect",
     u"Свечение вокруг глифов: тень без смещения, вырезанная по альфе текста",
     &effects::haloPage},
    {u"Bevel Effect",
     u"Скошенная кромка: светлая и тёмная половины обводки, перелом точно в углах",
     &effects::bevelPage},
    {u"Magnify Effect",
     u"Элемент растёт под указателем, тем больше, чем ближе тот к центру",
     &effects::magnifyPage},
};

}  // namespace

wxl::Teardown wxl_launched() {
    // Правая часть окна: полоса с именем эффекта и под ней его страница. При
    // выборе меняются текст полосы и единственный ребёнок `host`, и больше
    // ничего.
    auto heading = TextBlock {styles.TextBlock.Subtitle};
    auto host = Border {row = 1};

    // Кнопки списка, чтобы выбранную отличать от прочих: она одна в стиле
    // Accent.
    auto buttons = std::make_shared<std::vector<Button>>();

    auto list = StackPanel {
        spacing = 8.0,
        Margin {16},
    };

    auto const show = [heading, host, buttons](std::size_t index) {
        // Стили -- разные типы, по одному на ресурс, так что не тернарным
        // оператором.
        for (std::size_t i = 0; i < buttons->size(); ++i) {
            if (i == index) {
                (*buttons)[i].style(styles.Button.Accent);
            } else {
                (*buttons)[i].style(styles.Button.Default);
            }
        }
        heading.text(catalogue[index].name);
        host.child(catalogue[index].page());
    };

    for (std::size_t i = 0; i < std::size(catalogue); ++i) {
        auto button = Button {
            content = catalogue[i].name,
            toolTip = catalogue[i].about,
            hAlign.stretch,
            horizontalContentAlignment = HorizontalAlignment::Left,
            Padding {14, 8},
            onClick = [show, i] { show(i); },
        };
        buttons->push_back(button);
        list.children().append(button);
    }

    auto window = Window {
        title = u"wxl — эффекты",
        minSize = {1000, 560},
        Grid {
            columnDefinitions = u"340,*",
            background = brushes.SolidBackgroundFillColor.Base,

            // Слева: заголовок полосой и под ним список.
            Border {
                column = 0,
                borderBrush = brushes.Card.StrokeColorDefault,
                BorderThickness {0, 0, 1, 0},
                Grid {
                    rowDefinitions = u"auto,*",
                    Border {
                        row = 0,
                        background = brushes.SolidBackgroundFillColor.Secondary,
                        borderBrush = brushes.Card.StrokeColorDefault,
                        BorderThickness {0, 0, 0, 1},
                        Padding {12, 8},
                        TextBlock {u"Эффекты", styles.TextBlock.Subtitle},
                    },
                    ScrollViewer {row = 1, content = list},
                },
            },
            // Справа: имя эффекта полосой и под ним его страница.
            Grid {
                column = 1,
                rowDefinitions = u"auto,*",
                Border {
                    row = 0,
                    background = brushes.SolidBackgroundFillColor.Secondary,
                    borderBrush = brushes.Card.StrokeColorDefault,
                    BorderThickness {0, 0, 0, 1},
                    Padding {12, 8},
                    heading,
                },
                host,
            },
        },
    };

    show(0);

    auto appWindow = window.appWindow();
    appWindow.resize({1440, 820});
    window.activate();

    return {};
}

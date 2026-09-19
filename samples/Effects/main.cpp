// Витрина эффектов wxl: наверху панель навигации со стрелкой назад, под ней
// список эффектов, и он же подменяется страницей выбранного эффекта.
//
// Переключение — одна строка: `host.child(...)`. Ни `Frame`, ни `Page`, ни
// стека навигации здесь нет и не нужно — экрана всего два, список и
// страница, и стрелка возвращает на первый. Что такое страница и почему она
// `FrameworkElement`, сказано в Pages.h.

#include "Pages.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

// Каталог: имя на кнопке и в заголовке, пояснение в подсказке и функция,
// строящая страницу. Новый эффект — строка здесь и свой .cpp.
struct Effect {
    const wchar_t* name;
    const wchar_t* about;
    effects::Page page;
};

constexpr Effect catalogue[] = {
    {L"Halo Effect",
     L"Свечение вокруг глифов: тень без смещения, вырезанная по альфе текста",
     &effects::haloPage},
};

constexpr const wchar_t* listTitle = L"Эффекты";

}  // namespace

wxl::Teardown wxl_launched() {
    // Переменная названа не `title`: тег `title` пишется ниже в скобках окна,
    // и локальное имя перекрыло бы его.
    auto heading = TextBlock {
        listTitle,
        styles.TextBlock.Subtitle,
        vAlign.center,
    };

    // Список живёт всё время работы: стрелка возвращает тот же объект, а не
    // строит его заново.
    auto list = StackPanel {
        spacing = 8.0,
        Margin {24},
        hAlign.left,
        width = 320,
    };

    // Прокрутка списка — один объект на всё время работы, а не свежий на
    // каждый возврат: у показанного элемента уже есть родитель, и второго
    // фреймворк ему не даёт.
    auto listView = ScrollViewer {content = list};

    // Область под панелью. Меняется её единственный ребёнок, и больше ничего.
    auto host = Border {
        row = 1,
        listView,
    };

    auto back = Button {
        content = SymbolIcon {symbol = FluentSymbol::Back},
        toolTip = L"Назад, к списку эффектов",
        vAlign.center,
        Margin {0, 0, 12, 0},
        visibility.collapsed,
        // Кнопка приходит обработчику отправителем, поэтому не захватывается:
        // захват замкнул бы её на саму себя.
        onClick = [host, heading, listView](Button const& self) {
            heading.text(listTitle);
            self.visibility(Visibility::Collapsed);
            host.child(listView);
        },
    };

    for (auto const& effect : catalogue) {
        list.children().append(Button {
            content = effect.name,
            toolTip = effect.about,
            hAlign.stretch,
            horizontalContentAlignment = HorizontalAlignment::Left,
            Padding {16, 10},
            onClick = [host, heading, back, effect] {
                heading.text(effect.name);
                back.visibility(Visibility::Visible);
                host.child(effect.page());
            },
        });
    }

    auto window = Window {
        title = L"wxl — эффекты",
        minSize = {720, 520},
        Grid {
            rowDefinitions = L"auto,*",
            background = brushes.SolidBackgroundFillColor.Base,
            Border {
                row = 0,
                background = brushes.SolidBackgroundFillColor.Secondary,
                borderBrush = brushes.Card.StrokeColorDefault,
                BorderThickness {0, 0, 0, 1},
                Padding {12, 8},
                StackPanel {
                    orientation.horizontal,
                    back,
                    heading,
                },
            },
            host,
        },
    };

    auto appWindow = window.appWindow();
    appWindow.resize({1100, 760});
    window.activate();

    return {};
}

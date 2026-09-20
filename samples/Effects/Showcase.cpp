// Страница эффекта: описание сверху, примеры слева, исходник показанного
// примера справа. Все три части прокручиваются сами по себе.
//
// Код форматировать не понадобилось: [code=cpp] разбирает RsdnBlock, подложку
// даёт <pre> темы, подсветку — wxl.highlight.

#include "Pages.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

// Код для правой половины. Разметка RSDN берёт тело [code] буквально, так что
// экранировать в нём нечего; язык включает подсветку wxl.highlight.
std::wstring codeMarkup(const wchar_t* code) {
    std::wstring_view body{code};
    while (!body.empty() && (body.front() == L'\n' || body.front() == L'\r')) {
        body.remove_prefix(1);
    }
    while (!body.empty() && (body.back() == L'\n' || body.back() == L'\r')) {
        body.remove_suffix(1);
    }
    return L"[code=cpp]" + std::wstring{body} + L"[/code]";
}

}  // namespace

wxl::FrameworkElement effects::showcase(const wchar_t* description,
                                        std::span<const Sample> samples) {
    // Правая половина: исходник того примера, чью кнопку нажали последней.
    auto code = RsdnBlock {
        isTextSelectionEnabled = true,
        Margin {16, 12},
        L"[i]Нажмите «Показать код» под любым примером.[/i]",
    };

    auto left = StackPanel {
        spacing = 16.0,
        Margin {16, 12},
    };

    for (auto const& sample : samples) {
        left.children().append(Card {
            Padding {16},
            StackPanel {
                spacing = 10.0,
                TextBlock {
                    sample.title,
                    fontSize = 16,
                    FontWeight {600},
                },
                sample.build(),
                Button {
                    content = u"Показать код",
                    toolTip = u"Показать справа исходник этого примера",
                    hAlign.left,
                    onClick = [code, text = sample.code] { code.rsdn(codeMarkup(text)); },
                },
            },
        });
    }

    return Grid {
        rowDefinitions = u"2*,3*",

        // Описание и код — страницы разметки, и подложка у них своя, почти
        // белая, а не серая оконная.
        Border {
            row = 0,
            background = brushes.SolidBackgroundFillColor.Quarternary,
            ScrollViewer {
                content = HtmlBlock {
                    isTextSelectionEnabled = true,
                    Margin {20, 14},
                    description,
                },
            },
        },

        Border {
            row = 1,
            BorderThickness {0, 1, 0, 0},
            borderBrush = brushes.Card.StrokeColorDefault,
            Columns {
                ScrollViewer {content = left},
                Border {
                    BorderThickness {1, 0, 0, 0},
                    borderBrush = brushes.Card.StrokeColorDefault,
                    background = brushes.SolidBackgroundFillColor.Quarternary,
                    ScrollViewer {content = code},
                },
            },
        },
    };
}

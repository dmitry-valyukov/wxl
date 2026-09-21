// Страница эффекта: описание сверху, примеры слева, исходник показанного
// примера справа. Все три части прокручиваются сами по себе.
//
// Код форматировать не понадобилось: [code=cpp] разбирает RsdnBlock, подложку
// даёт <pre> темы, подсветку — wxl.highlight.

#include "Pages.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

// Текст сниппета: из UTF-8 в UTF-16, без BOM, без CR (git на Windows мог
// отдать файл с CRLF) и без пустых строк по краям.
std::wstring snippetText(effects::Snippet bytes) {
    auto const checked = core::unicode::checked(bytes);
    if (!checked) {
        return {};
    }
    std::wstring wide = checked->to_utf16();
    std::erase(wide, L'\r');
    std::wstring_view body{wide};
    if (body.starts_with(L'\uFEFF')) {
        body.remove_prefix(1);
    }
    while (!body.empty() && body.front() == L'\n') {
        body.remove_prefix(1);
    }
    while (!body.empty() && body.back() == L'\n') {
        body.remove_suffix(1);
    }
    return std::wstring{body};
}

// Код для правой половины: куски сниппета подряд через пустую строку.
// Разметка RSDN берёт тело [code] буквально, так что экранировать в нём
// нечего; язык включает подсветку wxl.highlight.
std::wstring codeMarkup(std::span<const effects::Snippet> parts) {
    std::wstring joined;
    for (auto const part : parts) {
        if (part.empty()) {
            continue;
        }
        if (!joined.empty()) {
            joined += L"\n\n";
        }
        joined += snippetText(part);
    }
    return L"[code=cpp]" + joined + L"[/code]";
}

}  // namespace

wxl::FrameworkElement effects::showcase(Snippet description,
                                        std::span<const Sample> samples) {
    auto const html = snippetText(description);

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
                    onClick = [code, text = codeMarkup(sample.code)] { code.rsdn(text); },
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
                    std::wstring_view{html},
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

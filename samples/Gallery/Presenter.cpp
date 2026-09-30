// Раскладка одного примера — ControlExample оригинала: введение сверху, под
// ним один блок в рамке — показ, справа от него «Output:» и параметры, а
// внизу раскрывающийся «Source code» с вкладкой языка и кнопкой копирования.
//
// Введение оригинал даёт статичным TextBlock, исходник — SampleCodePresenter;
// здесь введение — HtmlBlock, исходник — RsdnBlock с подсветкой, как в Effects.
// Вкладок XAML и C# у оригинала здесь одна: код примера на C++.

#include "Pages.h"

#include "generated/Windows.ApplicationModel.DataTransfer.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

// Текст сниппета: из UTF-8 в UTF-16, без BOM, без CR (git на Windows мог
// отдать файл с CRLF) и без пустых строк по краям.
std::wstring snippetText(gallery::Snippet bytes) {
    auto const checked = core::unicode::checked(bytes);
    if (!checked) {
        return {};
    }
    std::wstring wide = checked->to_utf16();
    std::erase(wide, L'\r');
    std::wstring_view body{wide};
    if (body.starts_with(L'﻿')) {
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

// Код на плоском фоне, как в SampleCodePresenter: подложку карточки,
// отступы и подъём, которые тема даёт `<pre>`, здесь убираем.
HtmlTheme flatCode() {
    HtmlTheme theme;
    theme.preMargin = Thickness {0};
    theme.prePadding = Thickness {0};
    theme.preRadius = 0;
    theme.preElevation = 0;
    theme.preBackground = colors.transparent;
    theme.preBorderColor = colors.transparent;
    return theme;
}

// Кнопка копирования кода: после щелчка значок — галочка, пока указатель не
// ушёл с кнопки.
FrameworkElement copyButton(std::wstring text) {
    return Button {
        Margin {0, 0, 8, 0},
        hAlign.right,
        vAlign.top,
        Padding {11, 5, 11, 6},
        styles.Button.Subtle,
        toolTip = u"Copy code",
        content = FontIcon {glyph = u"", fontSize = 16},
        onClick = [text = std::move(text)](Button const& self) {
            auto package = DataPackage {};
            package.requestedOperation(DataPackageOperation::Copy);
            package.setText(text);
            Clipboard::setContent(package);
            self.content(FontIcon {glyph = u"", fontSize = 16});
            self.toolTip(u"Copied to clipboard");
        },
        onPointerExited = [](Button const& self) {
            self.content(FontIcon {glyph = u"", fontSize = 16});
            self.toolTip(u"Copy code");
        },
    };
}

}  // namespace

wxl::FrameworkElement gallery::controlExample(ExampleParts const& parts) {
    auto const intro = snippetText(parts.header);
    auto const source = snippetText(parts.code);

    // Показ, «Output:» и параметры: три колонки одной полосы. Показ и
    // «Output:» на фоне окна, параметры — на карточке, отделённые чертой.
    auto strip = Grid {
        columnDefinitions = u"*,auto,auto",
        Border {
            column = 0,
            Padding {12},
            parts.example,
        },
    };

    if (!parts.output.empty()) {
        auto output = StackPanel {TextBlock {u"Output:"}};
        for (auto const& element : parts.output) {
            output.children().append(element);
        }
        strip.children().append(Border {
            column = 1,
            Margin {0, 12, 12, 12},
            Padding {16},
            hAlign.right,
            vAlign.stretch,
            CornerRadius {8},
            background = brushes.SolidBackgroundFillColor.Base,
            output,
        });
    }

    if (!parts.options.empty()) {
        auto options = StackPanel {spacing = 8.0};
        for (auto const& element : parts.options) {
            options.children().append(element);
        }
        strip.children().append(Border {
            column = 2,
            Padding {16},
            CornerRadius {0, 8, 0, 0},
            background = brushes.Card.BackgroundFillColor.Default,
            borderBrush = brushes.DividerStrokeColorDefault,
            BorderThickness {1, 0, 0, 0},
            options,
        });
    }

    auto code = RsdnBlock {isTextSelectionEnabled = true, Padding {16, 0, 16, 16}};
    code.theme(flatCode());
    code.rsdn(L"[code=cpp]" + source + L"[/code]");

    auto language = SelectorBar {Margin {4, 0, 0, 0}};
    auto cpp = SelectorBarItem {text = u"C++"};
    language.items().append(cpp);
    language.selectedItem(cpp);

    return StackPanel {
        Margin {0, 16, 0, 0},
        HtmlBlock {
            Margin {0, 0, 0, 12},
            isTextSelectionEnabled = true,
            std::wstring_view{intro},
        },
        Border {
            CornerRadius {8, 8, 0, 0},
            background = brushes.SolidBackgroundFillColor.Base,
            borderBrush = brushes.Card.StrokeColorDefault,
            BorderThickness {1},
            strip,
        },
        Expander {
            hAlign.stretch,
            horizontalContentAlignment = HorizontalAlignment::Stretch,
            Padding {0},
            CornerRadius {0, 0, 8, 8},
            background = brushes.Card.BackgroundFillColor.Secondary,
            header = u"Source code",
            content = StackPanel {
                spacing = 16.0,
                language,
                Grid {
                    ScrollViewer {
                        horizontalScrollBarVisibility = ScrollBarVisibility::Auto,
                        content = code,
                    },
                    copyButton(source),
                },
            },
        },
    };
}

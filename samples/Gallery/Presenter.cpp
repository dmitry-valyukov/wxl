// Раскладка одного примера — ControlExample оригинала: введение сверху, под
// ним один блок в рамке — показ, справа от него «Output:» и параметры, а
// внизу раскрывающийся «Source code» с кнопкой копирования.
//
// Введение оригинал даёт статичным TextBlock, исходник — SampleCodePresenter;
// здесь введение — HtmlBlock, исходник — RsdnBlock с подсветкой, как в Effects.
// Вкладок языка нет: у оригинала XAML и C#, здесь всё без альтернатив C++.

#include "Pages.h"

#include <wxl/Windows.ApplicationModel.DataTransfer.h>

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
        Margin {0, 8, 8, 0},
        hAlign.right,
        vAlign.top,
        Padding {11, 5, 11, 6},
        styles.Button.Subtle,
        gallery::appPop(),
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
        rowDefinitions = u"auto,auto",
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
        // PhoneLayout оригинала: ниже 740 параметры уходят под пример на всю ширину, черта — сверху, а не слева.
        auto& beside = gallery::pageSize().optionsBeside;
        strip.children().append(Border {
            column = BindOutput {beside, [](bool side) { return side ? 2 : 0; }},
            row = BindOutput {beside, [](bool side) { return side ? 0 : 1; }},
            columnSpan = BindOutput {beside, [](bool side) { return side ? 1 : 3; }},
            margin = BindOutput {beside, [](bool side) { return side ? Thickness {0} : Thickness {0, 24, 0, 0}; }},
            borderThickness = BindOutput {beside, [](bool side) { return side ? Thickness {1, 0, 0, 0} : Thickness {0, 1, 0, 0}; }},
            Padding {16},
            CornerRadius {0, 8, 0, 0},
            background = brushes.Card.BackgroundFillColor.Default,
            borderBrush = brushes.DividerStrokeColorDefault,
            options,
        });
    }

    auto code = RsdnBlock {isTextSelectionEnabled = true, Padding {16, 12, 56, 12}};
    code.theme(flatCode());
    code.rsdn(L"[code=cpp]" + source + L"[/code]");

    return StackPanel {
        Margin {0, 16, 0, 0},
        HtmlBlock {
            Margin {0, 0, 0, 12},
            isTextSelectionEnabled = true,
            intro,
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
            // Кнопка копирования лежит поверх кода в правом верхнем углу, как у
            // оригинала: своей строки у неё нет, и код начинается сразу.
            content = Grid {
                ScrollViewer {
                    horizontalScrollBarVisibility = ScrollBarVisibility::Auto,
                    wheelToParent = true,
                    content = code,
                },
                copyButton(source),
            },
        },
    };
}

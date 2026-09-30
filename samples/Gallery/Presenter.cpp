// Раскладка одного примера — ControlExample оригинала: введение сверху,
// показ слева и его параметры справа, исходник под ними.
//
// Введение оригинал даёт статичным TextBlock, исходник — SampleCodePresenter;
// здесь введение — HtmlBlock, исходник — RsdnBlock с подсветкой, как в Effects.

#include "Pages.h"

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

}  // namespace

wxl::FrameworkElement gallery::controlExample(ExampleParts const& parts) {
    auto const intro = snippetText(parts.header);
    auto const code = L"[code=cpp]" + snippetText(parts.code) + L"[/code]";

    // Показ и подпись под ним; параметры — во второй колонке.
    auto shown = StackPanel {spacing = 12.0, parts.example};
    for (auto const& element : parts.output) {
        shown.children().append(element);
    }

    auto body = Grid {
        columnDefinitions = u"*,auto",
        columnSpacing = 24.0,
        Border {column = 0, shown},
    };
    if (!parts.options.empty()) {
        auto optionsColumn = StackPanel {spacing = 8.0};
        for (auto const& element : parts.options) {
            optionsColumn.children().append(element);
        }
        body.children().append(Border {column = 1, optionsColumn});
    }

    return StackPanel {
        Margin {0, 16, 0, 0},
        spacing = 8.0,
        HtmlBlock {
            isTextSelectionEnabled = true,
            std::wstring_view{intro},
        },
        Card {
            Padding {16},
            body,
        },
        Expander {
            hAlign.stretch,
            header = u"Source code",
            content = RsdnBlock {
                isTextSelectionEnabled = true,
                std::wstring_view{code},
            },
        },
    };
}

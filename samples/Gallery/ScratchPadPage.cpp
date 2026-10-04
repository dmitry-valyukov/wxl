// Страница ScratchPad — ScratchPadPage оригинала: сверху то, что описывает XAML снизу, по кнопке Load.
// Текст XAML печатает человек, поэтому он читается loadXaml, а не пишется описанием. Оригинал красит текст
// (XamlTextFormatter) и закрывает теги сам; здесь поле простое.

#include "Pages.h"
#include "Shell.h"
#include "Failure.h"
#include "LoadXaml.h"

#include "generated/Microsoft.UI.Xaml.Controls.h"
#include "generated/Microsoft.UI.Xaml.Input.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char16_t const* defaultXaml =
    u"<StackPanel  BorderThickness=\"1\" BorderBrush=\"Green\" CornerRadius=\"4\" Padding=\"3\">\n"
    u"    <!-- Note: {x:Bind} is not supported in Scratch Pad. -->\n"
    u"    <TextBlock>This is a sample TextBlock.</TextBlock>\n"
    u"    <Button Content=\"Click me!\"/>\n"
    u"\n"
    u"    <!-- Note: Syntax highlighting updates on 'Load'. -->\n"
    u"</StackPanel>";

// Корню текста даются пространства имён XAML, чтобы человек их не писал.
std::u16string withNamespaces(std::u16string xml) {
    auto const first = xml.find_first_not_of(u" \t\r\n");
    auto const last = xml.find_last_not_of(u" \t\r\n");
    if (first == std::u16string::npos) {
        throw std::invalid_argument("No end tag.");
    }
    xml = xml.substr(first, last - first + 1);
    auto const at = xml.find_first_of(u" />");
    if (at == std::u16string::npos) {
        throw std::invalid_argument("No end tag.");
    }
    return xml.substr(0, at) +
           u" xmlns='http://schemas.microsoft.com/winfx/2006/xaml/presentation' xmlns:x='http://schemas.microsoft.com/winfx/2006/xaml' " +
           xml.substr(at);
}

struct Model {
    ScrollViewer pad {background = brushes.SolidBackgroundFillColor.Base, horizontalScrollBarVisibility = ScrollBarVisibility::Visible,
                      horizontalScrollMode = ScrollMode::Auto, verticalScrollMode = ScrollMode::Auto};
    TextBox editor {acceptsReturn = true, fontFamily = u"Consolas", fontSize = 12, isSpellCheckEnabled = false, automationName = u"XAML markup textbox"};
    TextBlock status {row = 1, columnSpan = 2};

    void empty() {
        pad.content(TextBlock {u"Click the Load button to load the content below.", hAlign.center, vAlign.center, textWrapping = TextWrapping::Wrap});
    }

    void load() {
        status.text(u"");
        std::u16string message;
        try {
            auto const element = loadXaml(withNamespaces(std::u16string {editor.text().c_str()}));
            if (!element.is<UIElement>()) {
                throw std::invalid_argument("The root is not an element.");
            }
            pad.content(element.try_as<UIElement>());
            message = u"Load successful.";
        } catch (...) {
            auto const failure = failureOf(std::current_exception());
            message = std::u16string {failure.message.c_str()};
        }
        status.text(message);
    }

    void reset() {
        editor.text(defaultXaml);
        empty();
        status.text(u"");
    }
};

}  // namespace

FrameworkElement gallery::scratchPadPage() {
    auto const model = gallery::hold<Model>();
    model->reset();
    std::weak_ptr<Model> const weak = model;
    model->editor.add_onKeyDown([weak](auto const&, KeyRoutedEventArgs& args) {
        if (args.key() == VirtualKey::F5) {
            if (auto const self = weak.lock()) {
                self->load();
            }
        }
    });

    return Grid {
        height = 640,
        rowDefinitions = u"*,*",
        Border {borderBrush = brushes.DividerStrokeColorDefault, BorderThickness {0, 0, 0, 1}, child = model->pad},
        Grid {
            row = 1,
            Padding {12},
            columnSpacing = 12.0,
            rowSpacing = 8.0,
            rowDefinitions = u"*,auto",
            columnDefinitions = u"*,auto",
            model->editor,
            StackPanel {
                column = 1,
                hAlign.stretch,
                vAlign.top,
                spacing = 8.0,
                minWidth = 168,
                Button {hAlign.stretch, styles.Button.Accent, content = u"Load", onClick = [weak](auto&&...) {
                            if (auto const self = weak.lock()) { self->load(); }
                        }},
                Button {hAlign.stretch, content = u"Reset", toolTip = u"Resets to the default scratch pad content", onClick = [weak](auto&&...) {
                            if (auto const self = weak.lock()) { self->reset(); }
                        }},
            },
            model->status,
        },
    };
}

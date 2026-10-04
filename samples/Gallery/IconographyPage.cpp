// Страница Iconography — IconographyPage оригинала: поиск значков, их сетка и панель выбранного.
// Оригинал ищет в отдельном потоке и показывает код на C# и XAML; здесь поиск по полутора тысячам значков занимает
// меньше кадра и идёт на месте, а код — на C++.

#include "Pages.h"

#include "Icons.h"
#include "Shell.h"
#include "Announce.h"
#include "Box.h"
#include "ItemBuilder.h"

#include <wxl/Microsoft.UI.Xaml.Controls.h>

using namespace wxl;
using namespace wxl::dsl;

namespace {

char16_t lower(char16_t letter) {
    return letter >= u'A' && letter <= u'Z' ? static_cast<char16_t>(letter + (u'a' - u'A')) : letter;
}

bool contains(std::u16string_view text, std::u16string_view part) {
    if (part.empty()) {
        return true;
    }
    return !std::ranges::search(text, part, [](char16_t a, char16_t b) { return lower(a) == lower(b); }).empty();
}

std::vector<std::u16string> words(std::u16string_view query) {
    std::vector<std::u16string> result;
    std::u16string current;
    for (char16_t const letter : query) {
        if (letter == u' ') {
            result.push_back(std::move(current));
            current.clear();
        } else {
            current.push_back(letter);
        }
    }
    result.push_back(std::move(current));
    return result;
}

bool fits(gallery::Icon const& icon, std::vector<std::u16string> const& filter) {
    return std::ranges::all_of(filter, [&](std::u16string const& word) {
        return contains(icon.code, word) || contains(icon.name, word) ||
               std::ranges::any_of(icon.tags, [&](std::u16string const& tag) { return !tag.empty() && contains(tag, word); });
    });
}

TextBlock heading(char16_t const* text) {
    return TextBlock {foreground = brushes.Text.FillColor.Secondary, styles.TextBlock.Caption, text};
}

struct Model {
    std::vector<size_t> shown;
    std::vector<std::u16string> tags;
    ItemsView view {minWidth = 100, Padding {16}, height = 560, layout = UniformGridLayout {minColumnSpacing = 8.0, minRowSpacing = 8.0}};
    AutoSuggestBox box {minWidth = 304, maxWidth = 320, hAlign.left, Margin {0, 16, 0, 0}, placeholderText = u"Search icons by name, code, or tags",
                        queryIcon = SymbolIcon {symbol = Symbol::Find}};
    FontIcon big {fontSize = 48};
    StackPanel warning {orientation.horizontal, spacing = 8.0, Margin {0, 4, 0, 0}, vAlign.top, column = 1,
                        toolTip = u"This icon is only available in Segoe Fluent Icons (the default icon font on Windows 11). On Windows 10, "
                                  u"Segoe MDL2 Assets is used by default, so the icon may not appear unless Segoe Fluent Icons is installed."};
    TextBlock name, textGlyph, codeGlyph, fontIconCode, symbolCode;
    TextBlock symbolLabel = heading(u"SymbolIcon");
    ItemsView tagView {Margin {0, 8, 0, 4}, isItemInvokedEnabled = true, selectionMode = ItemsViewSelectionMode::None,
                       layout = FlowLayout {orientation.horizontal, lineSpacing = 4.0, minItemSpacing = 4.0}};
    TextBlock noTags {Margin {0, 4, 0, 0}, u"No tags available."};

    static TextBlock code() {
        return TextBlock {isTextSelectionEnabled = true, fontFamily = u"Consolas", Margin {0, 0, 0, 8}, minHeight = 32,
                          textWrapping = TextWrapping::Wrap};
    }

    Model() : name(code()), textGlyph(code()), codeGlyph(code()), fontIconCode(code()), symbolCode(code()) {
        warning.children().append(FontIcon {fontSize = 12, foreground = brushes.SystemFillColor.Caution, glyph = u"", vAlign.center});
        warning.children().append(TextBlock {u"Only supported in Segoe Fluent Icons", textWrapping = TextWrapping::Wrap, fontSize = 12,
                                             foreground = brushes.SystemFillColor.Caution, vAlign.center});
    }

    void show(gallery::Icon const& icon) {
        big.glyph(icon.glyph);
        warning.visibility(icon.segoeFluentOnly ? Visibility::Visible : Visibility::Collapsed);
        name.text(icon.name);
        textGlyph.text(u"&#x" + icon.code + u";");
        codeGlyph.text(u"\\u" + icon.code);
        fontIconCode.text(u"FontIcon {glyph = u\"\\u" + icon.code + u"\"}");
        symbolLabel.visibility(icon.isSymbol ? Visibility::Visible : Visibility::Collapsed);
        symbolCode.visibility(icon.isSymbol ? Visibility::Visible : Visibility::Collapsed);
        symbolCode.text(icon.isSymbol ? u"SymbolIcon {symbol = Symbol::" + icon.name + u"}" : std::u16string {});
        tags.clear();
        for (auto const& tag : icon.tags) {
            if (!tag.empty()) {
                tags.push_back(tag);
            }
        }
        tagView.itemsSource(indexList(static_cast<int64_t>(tags.size())));
        tagView.visibility(tags.empty() ? Visibility::Collapsed : Visibility::Visible);
        noTags.visibility(tags.empty() ? Visibility::Visible : Visibility::Collapsed);
    }

    void filter(std::u16string_view query) {
        auto const needles = words(query);
        shown.clear();
        auto const& all = gallery::icons();
        for (size_t i = 0; i < all.size(); ++i) {
            if (fits(all[i], needles)) {
                shown.push_back(i);
            }
        }
        view.itemsSource(indexList(static_cast<int64_t>(shown.size())));
        std::u16string outcome = u"No icons found.";
        if (!shown.empty()) {
            show(all[shown.front()]);
            view.select(0);
            auto const count = std::to_string(shown.size());
            outcome = shown.size() == 1 ? u"1 icon found." : std::u16string {count.begin(), count.end()} + u" icons found.";
        }
        announce(box, outcome, u"AutoSuggestBoxNumberIconsFoundId");
    }
};

FrameworkElement iconTile(std::weak_ptr<Model> weak, Object const& item) {
    auto const model = weak.lock();
    auto const& icon = gallery::icons()[model->shown[static_cast<size_t>(intOf(item))]];
    return ItemContainer {
        width = 96,
        height = 96,
        automationName = icon.name,
        toolTip = icon.name,
        cornerRadius = CornerRadius {4},
        child = Grid {
            background = brushes.Card.BackgroundFillColor.Default,
            borderBrush = brushes.Card.StrokeColorDefault,
            BorderThickness {1},
            cornerRadius = CornerRadius {4},
            Viewbox {width = 28, height = 28, Margin {0, 0, 0, 16}, child = FontIcon {glyph = icon.glyph}},
            TextBlock {Margin {8, 0, 8, 8}, hAlign.center, vAlign.bottom, foreground = brushes.Text.FillColor.Secondary, styles.TextBlock.Caption,
                       textTrimming = TextTrimming::CharacterEllipsis, textWrapping = TextWrapping::NoWrap, icon.name},
        },
    };
}

FrameworkElement tagTile(std::weak_ptr<Model> weak, Object const& item) {
    auto const model = weak.lock();
    auto const& tag = model->tags[static_cast<size_t>(intOf(item))];
    return ItemContainer {
        hAlign.left,
        automationName = tag,
        cornerRadius = CornerRadius {12},
        child = Grid {
            minHeight = 24,
            Padding {8, 2},
            background = brushes.Card.BackgroundFillColor.Default,
            borderBrush = brushes.Card.StrokeColorDefault,
            BorderThickness {1},
            cornerRadius = CornerRadius {12},
            TextBlock {vAlign.center, foreground = brushes.Text.FillColor.Secondary, styles.TextBlock.Caption,
                       textTrimming = TextTrimming::CharacterEllipsis, tag},
        },
    };
}

}  // namespace

FrameworkElement gallery::iconographyPage() {
    auto const model = gallery::hold<Model>();
    std::weak_ptr<Model> const weak = model;

    model->view.itemTemplate([weak](Object const& item) { return iconTile(weak, item); });
    model->tagView.itemTemplate([weak](Object const& item) { return tagTile(weak, item); });
    model->view.add_onSelectionChanged([weak](auto const&, auto&&...) {
        auto const self = weak.lock();
        if (!self) {
            return;
        }
        if (auto const item = self->view.selectedItem()) {
            self->show(gallery::icons()[self->shown[static_cast<size_t>(intOf(item))]]);
        }
    });
    model->tagView.add_onItemInvoked([weak](auto const&, ItemsViewItemInvokedEventArgs& args) {
        auto const self = weak.lock();
        if (self) {
            self->box.text(self->tags[static_cast<size_t>(intOf(args.invokedItem()))]);
        }
    });
    model->box.add_onTextChanged([weak](AutoSuggestBox const& sender, AutoSuggestBoxTextChangedEventArgs&) {
        if (auto const self = weak.lock()) {
            self->filter(std::u16string {sender.text().c_str()});
        }
    });
    model->filter(u"");

    auto const side = Border {
        column = 1,
        background = brushes.Card.BackgroundFillColor.Default,
        borderBrush = brushes.DividerStrokeColorDefault,
        BorderThickness {1, 0, 0, 0},
        cornerRadius = CornerRadius {0, 8, 8, 0},
        child = ScrollViewer {
            content = StackPanel {
                Margin {16, 16, 8, 16},
                spacing = 2.0,
                Grid {
                    Margin {0, 0, 0, 24},
                    hAlign.stretch,
                    columnDefinitions = u"auto,*",
                    Border {Margin {0, 0, 8, 0}, Padding {8}, hAlign.left, background = brushes.Control.FillColor.Default,
                            borderBrush = brushes.Control.StrokeColor.Default, BorderThickness {1}, cornerRadius = CornerRadius {4}, child = model->big},
                    model->warning,
                },
                heading(u"Icon name"),
                model->name,
                heading(u"Text glyph"),
                model->textGlyph,
                heading(u"Code glyph"),
                model->codeGlyph,
                heading(u"FontIcon"),
                model->fontIconCode,
                model->symbolLabel,
                model->symbolCode,
                TextBlock {Margin {0, 4, 0, 0}, foreground = brushes.Text.FillColor.Secondary, styles.TextBlock.Caption, u"Tags"},
                model->tagView,
                model->noTags,
            },
        },
    };

    return Grid {
        rowSpacing = 8.0,
        rowDefinitions = u"auto,auto",
        model->box,
        Border {
            row = 1,
            background = brushes.SolidBackgroundFillColor.Base,
            borderBrush = brushes.Card.StrokeColorDefault,
            BorderThickness {1},
            cornerRadius = CornerRadius {8},
            child = Grid {columnDefinitions = u"*,334", model->view, side},
        },
    };
}

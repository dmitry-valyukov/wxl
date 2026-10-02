// Typing goes on in the colour last chosen.
auto const current = std::make_shared<Color>(colors.green);

auto editor = RichEditBox {
    height = 200,
    minWidth = 300,
    onTextChanged = [current](RichEditBox const& self) {
        self.document().selection().characterFormat().foregroundColor(*current);
    },
};

const Preset tool {BorderThickness {0}, background = colors.transparent, Margin {0, 0, 8, 0}};

const Preset dot {width = 32, height = 32};

// One coroutine per swatch of the flyout: it waits for its click, colours the
// selection, closes the flyout and ends.
auto const pick = [](Flyout flyout, Button swatch, RichEditBox editor,
                     std::shared_ptr<Color> current) -> async::detached_task {
    auto clicks = onClick(swatch);
    if (co_await clicks.next()) {
        auto const picked = swatch.content().try_as<Rectangle>().fill().try_as<SolidColorBrush>().color();
        *current = picked;
        editor.document().selection().characterFormat().foregroundColor(picked);
        flyout.hide();
        editor.focus(FocusState::Keyboard);
    }
};

const Preset cell {Padding {0}, minWidth = 0, minHeight = 0, Margin {6}};

auto toolbar = Grid {
    columnDefinitions = u"auto,auto,*,auto",
    Button {
        tool,
        gallery::appPop(),
        toolTip = u"Bold",
        content = FontIcon {glyph = u"\uE8DD"},
        onClick = [editor](Object const&, RoutedEventArgs&) {
            editor.document().selection().characterFormat().bold(FormatEffect::Toggle);
        },
    },
    Button {
        tool,
        gallery::appPop(),
        column = 1,
        toolTip = u"Italic",
        content = FontIcon {glyph = u"\uE8DB"},
        onClick = [editor](Object const&, RoutedEventArgs&) {
            editor.document().selection().characterFormat().italic(FormatEffect::Toggle);
        },
    },
    DropDownButton {
        gallery::appPop(),
        column = 3,
        background = colors.transparent,
        BorderThickness {0},
        toolTip = u"Font color",
        content = SymbolIcon {symbol = Symbol::FontColor},
        flyout = Flyout {
            placement = FlyoutPlacementMode::Bottom,
            onOpened = [pick, editor, current](Object const& sender) {
                auto const flyout = sender.try_as<Flyout>();
                auto const swatches = flyout.content().try_as<VariableSizedWrapGrid>().children();
                for (uint32_t i = 0; i < swatches.size(); ++i) {
                    pick(flyout, swatches[i].try_as<Button>(), editor, current);
                }
            },
            content = VariableSizedWrapGrid {
                maximumRowsOrColumns = 3,
                orientation.horizontal,
                Button {cell, content = Rectangle {dot, fill = rgb(255, 0, 0)}},
                Button {cell, content = Rectangle {dot, fill = rgb(255, 165, 0)}},
                Button {cell, content = Rectangle {dot, fill = rgb(255, 255, 0)}},
                Button {cell, content = Rectangle {dot, fill = rgb(0, 128, 0)}},
                Button {cell, content = Rectangle {dot, fill = rgb(0, 0, 255)}},
                Button {cell, content = Rectangle {dot, fill = rgb(75, 0, 130)}},
                Button {cell, content = Rectangle {dot, fill = rgb(238, 130, 238)}},
                Button {cell, content = Rectangle {dot, fill = rgb(128, 128, 128)}},
            },
        },
    },
};

// Find: the matches are painted with the system highlight colours, and the
// editor's own colours are put back when the box stops looking.
constexpr int32_t maxUnits = std::numeric_limits<int32_t>::max();

auto const removeHighlights = [editor] {
    auto const background = editor.background().try_as<SolidColorBrush>();
    auto const foreground = editor.foreground().try_as<SolidColorBrush>();
    if (!background || !foreground) {
        return;
    }
    auto const format = editor.document().getRange(0, maxUnits).characterFormat();
    format.backgroundColor(background.color());
    format.foregroundColor(foreground.color());
};

auto const highlightMatches = [editor, removeHighlights](TextBox const& self) {
    removeHighlights();
    auto const text = self.text();
    if (text.empty()) {
        return;
    }
    auto range = editor.document().getRange(0, 0);
    while (range.findText(text, maxUnits, FindOptions::None) > 0) {
        range.characterFormat().backgroundColor(rgb(0, 120, 215));
        range.characterFormat().foregroundColor(colors.white);
    }
};

auto findBar = StackPanel {
    orientation.horizontal,
    Margin {0, 10, 0, 0},
    TextBlock {u"Find:", vAlign.center},
    TextBox {
        width = 224,
        Margin {10, 0, 0, 0},
        placeholderText = u"Enter search text",
        onGotFocus = highlightMatches,
        onTextChanged = highlightMatches,
        onLostFocus = [removeHighlights] { removeHighlights(); },
    },
};
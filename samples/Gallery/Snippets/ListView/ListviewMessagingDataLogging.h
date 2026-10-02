struct Model {
    // The list grows from the bottom up: new items are put at the end and the list keeps the last in view.
    ListView log {height = 400, borderBrush = brushes.Control.Strong.StrokeColorDefault, borderThickness = 1};
    std::vector<std::pair<std::u16string, bool>> messages;  // text, and whether it is ours

    void add(bool ours) {
        std::u16string text = u"Message ";
        text += core::to_u16(static_cast<int>(messages.size()) + 1).plain();
        messages.emplace_back(text, ours);
        log.items().append(intBox(static_cast<int64_t>(messages.size()) - 1));
    }
};
auto const model = gallery::hold<Model>();

model->log.itemTemplate([weak = std::weak_ptr<Model>(model)](Object const& item) {
    auto const model = weak.lock();
    auto const& message = model->messages[static_cast<size_t>(intOf(item))];
    return Grid {
        Margin {4},
        horizontalAlignment = message.second ? HorizontalAlignment::Right : HorizontalAlignment::Left,
        Border {
            width = 350,
            minHeight = 75,
            Padding {10, 0, 0, 10},
            background = brushes.SystemColor.HighlightColor,
            cornerRadius = CornerRadius {4},
            StackPanel {
                TextBlock {Padding {0, 10, 0, 0}, fontSize = 20, foreground = brushes.SystemColor.HighlightTextColor, message.first},
                TextBlock {Padding {0, 0, 0, 10}, fontSize = 15, foreground = brushes.SystemColor.HighlightTextColor, u"just now"},
            },
        },
    };
});
model->add(true);

// ItemsStackPanel is the panel of a ListView; what it does when items are added is told once it exists.
model->log.add_onLoaded([model](auto&&...) {
    if (auto const stack = model->log.itemsPanelRoot().try_as<ItemsStackPanel>()) {
        stack.itemsUpdatingScrollMode(ItemsUpdatingScrollMode::KeepLastItemInView);
    }
});

auto example = StackPanel {
    TextBlock {Margin {0, 0, 0, 15}, textWrapping = TextWrapping::Wrap,
               u"This ListView is inverted to grow from the bottom up. It's a good way to display logs or messages, with most "
               u"recent at the bottom."},
    model->log,
    StackPanel {
        orientation.horizontal,
        spacing = 8,
        Margin {0, 8, 0, 0},
        Button {content = u"Add message", onClick = [model](Button const&) { model->add(true); }},
        Button {content = u"Message received", onClick = [model](Button const&) { model->add(false); }},
    },
};
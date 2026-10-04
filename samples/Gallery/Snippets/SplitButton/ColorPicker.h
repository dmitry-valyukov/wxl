auto const current = std::make_shared<Color>(rgb(0, 128, 0));

auto richBox = RichEditBox {
    width = 240,
    minHeight = 96,
    placeholderText = u"Type something here",
    onTextChanged = [current](RichEditBox const& box) {
        box.document().selection().characterFormat().foregroundColor(*current);
    },
};

auto swatch = Border {
    width = 32,
    height = 32,
    CornerRadius {4, 0, 0, 4},
    background = SolidColorBrush {color = *current},
};

const Preset dot {width = 32, height = 32, radiusX = 4.0, radiusY = 4.0};

auto colorButton = SplitButton {
    minWidth = 0,
    minHeight = 0,
    Padding {0},
    vAlign.top,
    automationName = u"Font color",
    content = swatch,
    onClick = [richBox, current] {
        richBox.document().selection().characterFormat().foregroundColor(*current);
    },
    flyout = Flyout {
        placement = FlyoutPlacementMode::Bottom,
        // Opening the flyout starts a coroutine: it waits for a swatch to be
        // clicked and ends when the content leaves the tree, that is, when the
        // flyout closes. The flyout is its own sender, so nothing captures the
        // button that owns it.
        onOpened = [richBox, swatch, current](Object const& sender) -> async::detached_task {
            auto const flyout = sender.try_as<Flyout>();
            auto clicks = onItemClick(flyout.content().try_as<GridView>());
            while (auto const got = co_await clicks.next()) {
                auto const picked =
                    got->get().clickedItem().try_as<Rectangle>().fill().try_as<SolidColorBrush>().color();
                *current = picked;
                richBox.document().selection().characterFormat().foregroundColor(picked);
                swatch.background(SolidColorBrush {color = picked});
                flyout.hide();
            }
        },
        content = GridView {
            isItemClickEnabled = true,
            width = 3 * 52,
            Rectangle {dot, automationName = u"Red", fill = rgb(255, 0, 0)},
            Rectangle {dot, automationName = u"Orange", fill = rgb(255, 165, 0)},
            Rectangle {dot, automationName = u"Yellow", fill = rgb(255, 255, 0)},
            Rectangle {dot, automationName = u"Green", fill = rgb(0, 128, 0)},
            Rectangle {dot, automationName = u"Blue", fill = rgb(0, 0, 255)},
            Rectangle {dot, automationName = u"Indigo", fill = rgb(75, 0, 130)},
            Rectangle {dot, automationName = u"Violet", fill = rgb(238, 130, 238)},
            Rectangle {dot, automationName = u"Gray", fill = rgb(128, 128, 128)},
        },
    },
};

richBox.document().selection().characterFormat().foregroundColor(*current);
richBox.document().selection().setText(
    TextSetOptions::None,
    u"Lorem ipsum dolor sit amet, consectetur adipiscing elit, "
    u"sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. "
    u"Tempor commodo ullamcorper a lacus.");
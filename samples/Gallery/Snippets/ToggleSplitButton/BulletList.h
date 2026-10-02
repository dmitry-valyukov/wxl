auto const marker = std::make_shared<MarkerType>(MarkerType::Bullet);

auto richBox = RichEditBox {width = 240, minHeight = 96};

auto icon = SymbolIcon {symbol = Symbol::List};

const Preset choice {
    Padding {4},
    minWidth = 0,
    minHeight = 0,
    Margin {6},
    CornerRadius {4},
};

// One coroutine per choice: it waits for its click, applies its marker and
// closes the flyout. The button the flyout belongs to is found through the
// flyout's Target, so nothing captures it.
auto const choose = [](Flyout flyout, Button button, Symbol symbol, MarkerType type, SymbolIcon icon,
                       RichEditBox richBox, std::shared_ptr<MarkerType> marker) -> async::detached_task {
    auto clicks = onClick(button);
    if (co_await clicks.next()) {
        *marker = type;
        icon.symbol(symbol);
        richBox.document().selection().paragraphFormat().listType(type);
        flyout.target().try_as<ToggleSplitButton>().isChecked(true);
        flyout.hide();
        richBox.focus(FocusState::Keyboard);
    }
};

auto listButton = ToggleSplitButton {
    vAlign.top,
    content = icon,
    onIsCheckedChanged = [richBox, marker](ToggleSplitButton const& self) {
        richBox.document().selection().paragraphFormat().listType(
            self.isChecked() ? *marker : MarkerType::None);
    },
    flyout = Flyout {
        placement = FlyoutPlacementMode::Bottom,
        onOpened = [choose, richBox, icon, marker](Object const& sender) {
            auto const flyout = sender.try_as<Flyout>();
            auto const buttons = flyout.content().try_as<StackPanel>().children();
            choose(flyout, buttons[0].try_as<Button>(), Symbol::List, MarkerType::Bullet, icon, richBox, marker);
            choose(flyout, buttons[1].try_as<Button>(), Symbol::Bullets, MarkerType::UppercaseRoman, icon,
                   richBox, marker);
        },
        content = StackPanel {
            orientation.horizontal,
            Button {choice, content = SymbolIcon {symbol = Symbol::List}},
            Button {choice, content = SymbolIcon {symbol = Symbol::Bullets}},
        },
    },
};
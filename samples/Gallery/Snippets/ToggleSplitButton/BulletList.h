auto const marker = std::make_shared<MarkerType>(MarkerType::Bullet);

auto richBox = RichEditBox {width = 240, minHeight = 96, automationName = u"Text entry"};

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
// flyout's Target, so nothing captures it. The page's model keeps them, and
// the next opening of the flyout replaces them.
struct Model {
    std::vector<async::task<>> choosing;
};
auto const model = gallery::hold<Model>();

auto const choose = [](Flyout flyout, Button button, Symbol symbol, MarkerType type, SymbolIcon icon,
                       RichEditBox richBox, std::shared_ptr<MarkerType> marker) -> async::task<> {
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
    automationName = u"Bullets",
    content = icon,
    onIsCheckedChanged = [richBox, marker](ToggleSplitButton const& self) {
        richBox.document().selection().paragraphFormat().listType(
            self.isChecked() ? *marker : MarkerType::None);
    },
    flyout = Flyout {
        placement = FlyoutPlacementMode::Bottom,
        onOpened = [model, choose, richBox, icon, marker](Object const& sender) {
            auto const flyout = sender.try_as<Flyout>();
            auto const buttons = flyout.content().try_as<StackPanel>().children();
            model->choosing.clear();
            model->choosing.push_back(
                choose(flyout, buttons[0].try_as<Button>(), Symbol::List, MarkerType::Bullet, icon, richBox, marker));
            model->choosing.push_back(choose(flyout, buttons[1].try_as<Button>(), Symbol::Bullets,
                                             MarkerType::UppercaseRoman, icon, richBox, marker));
        },
        content = StackPanel {
            orientation.horizontal,
            Button {choice, automationName = u"Bulleted list", content = SymbolIcon {symbol = Symbol::List}},
            Button {choice, automationName = u"Roman numerals list", content = SymbolIcon {symbol = Symbol::Bullets}},
        },
    },
};
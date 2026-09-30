auto const marker = std::make_shared<MarkerType>(MarkerType::Bullet);

auto richBox = RichEditBox {width = 240, minHeight = 96};

auto icon = SymbolIcon {symbol = Symbol::List};

auto listButton = ToggleSplitButton {
    vAlign.top,
    content = icon,
    onIsCheckedChanged = [richBox, marker](ToggleSplitButton const& self) {
        richBox.document().selection().paragraphFormat().listType(
            self.isChecked() ? *marker : MarkerType::None);
    },
};

const Preset choice {
    Padding {4},
    minWidth = 0,
    minHeight = 0,
    Margin {6},
    CornerRadius {4},
};

listButton.flyout(Flyout {
    placement = FlyoutPlacementMode::Bottom,
    content = StackPanel {
        orientation.horizontal,
        Button {
            choice,
            content = SymbolIcon {symbol = Symbol::List},
            onClick = [listButton, icon, richBox, marker] {
                *marker = MarkerType::Bullet;
                icon.symbol(Symbol::List);
                richBox.document().selection().paragraphFormat().listType(*marker);
                listButton.isChecked(true);
                listButton.flyout().hide();
                richBox.focus(FocusState::Keyboard);
            },
        },
        Button {
            choice,
            content = SymbolIcon {symbol = Symbol::Bullets},
            onClick = [listButton, icon, richBox, marker] {
                *marker = MarkerType::UppercaseRoman;
                icon.symbol(Symbol::Bullets);
                richBox.document().selection().paragraphFormat().listType(*marker);
                listButton.isChecked(true);
                listButton.flyout().hide();
                richBox.focus(FocusState::Keyboard);
            },
        },
    },
});
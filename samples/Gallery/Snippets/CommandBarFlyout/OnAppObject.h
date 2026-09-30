auto output = TextBlock {};

auto command = [output](Symbol which, char16_t const* caption) {
    return AppBarButton {
        icon = SymbolIcon {symbol = which},
        label = caption,
        toolTip = caption,
        onClick = [output, caption](AppBarButton const&) { output.text(std::u16string {u"You clicked: "} + caption); },
    };
};

auto plain = [output](char16_t const* caption) {
    return AppBarButton {
        label = caption,
        onClick = [output, caption](AppBarButton const&) { output.text(std::u16string {u"You clicked: "} + caption); },
    };
};

auto flyout = CommandBarFlyout {
    placement = FlyoutPlacementMode::Right,
    primaryCommands[command(Symbol::Share, u"Share"), command(Symbol::Save, u"Save"), command(Symbol::Delete, u"Delete")],
    secondaryCommands[plain(u"Resize"), plain(u"Move")],
};

// Standard: the focus moves to the flyout. Transient: it stays where it was.
auto show = [flyout](DependencyObject const& target, FlyoutShowMode mode) {
    flyout.showAt(target, FlyoutShowOptions {showMode = mode, placement = FlyoutPlacementMode::RightEdgeAlignedTop});
};

auto picture = Button {
    Margin {0, 12},
    Padding {0},
    toolTip = u"mountain",
    content = Image {height = 300, source = u"Assets/SampleMedia/rainier.jpg"},
    onClick = [show](Button const& self) { show(self, FlyoutShowMode::Transient); },
    onContextRequested = [show](Button const& self) { show(self, FlyoutShowMode::Standard); },
};

auto example = StackPanel {
    TextBlock {u"Click or right click the image to open a CommandBarFlyout"},
    picture,
    output,
};
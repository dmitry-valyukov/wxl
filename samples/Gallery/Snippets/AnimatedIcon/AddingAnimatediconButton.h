// The animation to play, by the kind picked in the list.
auto makeSource = [](int kind) -> AnimatedVisualSource2 {
    switch (kind) {
        case 0: return AnimatedBackVisualSource {};
        case 1: return AnimatedChevronDownSmallVisualSource {};
        case 2: return AnimatedChevronRightDownSmallVisualSource {};
        case 3: return AnimatedChevronUpDownSmallVisualSource {};
        case 4: return AnimatedFindVisualSource {};
        case 5: return AnimatedGlobalNavigationButtonVisualSource {};
        default: return AnimatedSettingsVisualSource {};
    }
};

auto searchIcon = AnimatedIcon {
    makeSource(4),
    fallbackIconSource = SymbolIconSource {symbol = Symbol::Find},
};

auto example = StackPanel {
    TextBlock {
        textWrapping = TextWrapping::WrapWholeWords,
        u"The following example is a button that the user clicks to load a search experience. The AnimatedIcon "
        u"consumes the animation created using Adobe AfterEffects and translated into Microsoft.UI.Composition "
        u"objects using Lottie-Windows. For guidance on how to properly structure your animation file see the "
        u"AnimatedIcon Guidance page.",
    },
    Button {
        width = 75,
        Margin {0, 12, 0, 0},
        automationName = u"AnimatedIcon Example",
        content = searchIcon,
        // The button does not move the icon through its states by itself: the page says when.
        onPointerEntered = [searchIcon](Object const&, PointerRoutedEventArgs&) { AnimatedIcon::setState(searchIcon, u"PointerOver"); },
        onPointerExited = [searchIcon](Object const&, PointerRoutedEventArgs&) { AnimatedIcon::setState(searchIcon, u"Normal"); },
    },
};

auto options = ComboBox {
    minWidth = 340,
    vAlign.center,
    header = u"Kind",
    ComboBoxItem {content = u"AnimatedBackVisualSource"},
    ComboBoxItem {content = u"AnimatedChevronDownSmallVisualSource"},
    ComboBoxItem {content = u"AnimatedChevronRightDownSmallVisualSource"},
    ComboBoxItem {content = u"AnimatedChevronUpDownSmallVisualSource"},
    ComboBoxItem {content = u"AnimatedFindVisualSource"},
    ComboBoxItem {content = u"AnimatedGlobalNavigationButtonVisualSource"},
    ComboBoxItem {content = u"AnimatedSettingsVisualSource"},
    selectedIndex = 4,
    onSelectionChanged = [searchIcon, makeSource](ComboBox const& self) { searchIcon.source(makeSource(self.selectedIndex())); },
};
// The toggle and the pane say the same thing, so the field is theirs both.
struct Model {
    core::observable<bool> paneOpen{true};
};
auto const model = gallery::hold<Model>();

struct NavLink {
    zstring_view label;
    FluentSymbol symbol;
};
static constexpr NavLink links[] = {
    {u"People", FluentSymbol::People},
    {u"Globe", FluentSymbol::Globe},
    {u"Message", FluentSymbol::Message},
    {u"Mail", FluentSymbol::Mail},
};

// A link: the icon before the label, or after it when the pane is on the right.
auto const linkItem = [](NavLink const& link, bool iconAfter) -> FrameworkElement {
    return Grid {
        name = link.label,
        automationName = link.label,
        iconAfter ? Margin {0, 0, 2, 0} : Margin {2, 0, 0, 0},
        columnDefinitions = iconAfter ? u"*,auto" : u"auto,*",
        SymbolIcon {symbol = link.symbol, column = iconAfter ? 1 : 0},
        TextBlock {
            link.label,
            column = iconAfter ? 0 : 1,
            vAlign.center,
            iconAfter ? Margin {0, 0, 24, 0} : Margin {24, 0, 0, 0},
        },
    };
};

auto pageText = TextBlock {Margin {12, 12, 0, 0}, styles.TextBlock.Body};

auto navLinks = ListView {
    row = 1,
    Margin {0, 12, 0, 0},
    vAlign.stretch,
    isItemClickEnabled = true,
    selectionMode = ListViewSelectionMode::Single,
    onItemClick = [pageText](Object const&, ItemClickEventArgs& args) {
        if (auto const link = args.clickedItem().try_as<FrameworkElement>()) {
            std::u16string page{static_cast<std::u16string_view>(link.name())};
            page += u" Page";
            pageText.text(page);
        }
    },
};

auto const fillLinks = [navLinks, linkItem](bool iconAfter) {
    navLinks.items().clear();
    for (NavLink const& link : links) {
        navLinks.items().append(linkItem(link, iconAfter));
    }
};

auto splitView = SplitView {
    compactPaneLength = 48.0,
    displayMode = SplitViewDisplayMode::Inline,
    isPaneOpen = BindOutput {model->paneOpen},
    maxWidth = 400,
    openPaneLength = 256.0,
    paneBackground = brushes.SystemControl.Background.Chrome.MediumLow,
    onPaneClosing = [model](SplitView const&, SplitViewPaneClosingEventArgs&) { model->paneOpen.set(false); },
    onPaneOpened = [model](SplitView const&, Object const&) { model->paneOpen.set(true); },
    pane = Grid {
        rowDefinitions = u"auto,*,auto",
        TextBlock {u"PANE CONTENT", Margin {60, 12, 0, 0}, styles.TextBlock.BodyStrong},
        navLinks,
    },
    Grid {
        rowDefinitions = u"auto,*",
        TextBlock {u"SPLITVIEW CONTENT", Margin {12, 12, 0, 0}, styles.TextBlock.BodyStrong},
        Border {row = 1, pageText},
    },
};

auto example = Grid {height = 300, width = 400, vAlign.top, splitView};

auto togglePane = ToggleButton {content = u"IsPaneOpen", isChecked = Bind {model->paneOpen}};

auto placement = ToggleSwitch {
    minWidth = 120,
    Margin {0, 12, 0, 0},
    header = u"Placement",
    offContent = u"Left",
    onContent = u"Right",
    onToggled = [splitView, fillLinks](ToggleSwitch const& self) {
        bool const right = self.isOn();
        splitView.panePlacement(right ? SplitViewPanePlacement::Right : SplitViewPanePlacement::Left);
        fillLinks(right);
    },
};

auto displayMode = ComboBox {
    width = 196,
    Margin {0, 4, 0, 0},
    vAlign.center,
    header = u"DisplayMode",
    ComboBoxItem {content = u"Inline"},
    ComboBoxItem {content = u"CompactInline"},
    ComboBoxItem {content = u"Overlay"},
    ComboBoxItem {content = u"CompactOverlay"},
    selectedIndex = 0,
    onSelectionChanged = [splitView](ComboBox const& self) {
        static constexpr SplitViewDisplayMode modes[] = {
            SplitViewDisplayMode::Inline, SplitViewDisplayMode::CompactInline,
            SplitViewDisplayMode::Overlay, SplitViewDisplayMode::CompactOverlay};
        int const index = self.selectedIndex();
        if (index >= 0) {
            splitView.displayMode(modes[index]);
        }
    },
};

auto paneBackground = ComboBox {
    width = 196,
    Margin {0, 12, 0, 0},
    vAlign.center,
    header = u"PaneBackground",
    ComboBoxItem {content = u"SystemControlBackgroundChromeMediumLowBrush"},
    ComboBoxItem {content = u"Red"},
    ComboBoxItem {content = u"Blue"},
    ComboBoxItem {content = u"Green"},
    selectedIndex = 0,
    onSelectionChanged = [splitView](ComboBox const& self) {
        static constexpr Color colors[] = {rgb(255, 0, 0), rgb(0, 0, 255), rgb(0, 128, 0)};
        int const index = self.selectedIndex();
        if (index == 0) {
            splitView.paneBackground(brushes.SystemControl.Background.Chrome.MediumLow);
        } else if (index > 0) {
            splitView.paneBackground(SolidColorBrush {color = colors[index - 1]});
        }
    },
};

auto openPaneLength = Slider {
    width = 196,
    Margin {0, 12, 0, 0},
    header = u"OpenPaneLength",
    minimum = 128.0,
    maximum = 500.0,
    snapsTo = SliderSnapsTo::StepValues,
    stepFrequency = 8.0,
    value = 256.0,
    onValueChanged = [splitView](Slider const& self) { splitView.openPaneLength(self.value()); },
};

auto compactPaneLength = Slider {
    width = 196,
    header = u"CompactPaneLength",
    minimum = 24.0,
    maximum = 128.0,
    snapsTo = SliderSnapsTo::StepValues,
    stepFrequency = 8.0,
    value = 48.0,
    onValueChanged = [splitView](Slider const& self) { splitView.compactPaneLength(self.value()); },
};

fillLinks(false);
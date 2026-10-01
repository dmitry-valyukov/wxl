struct Model {
    core::observable<core::u16_text> title {core::u16_text {u"WinUI Gallery"}};
    core::observable<core::u16_text> subtitle {core::u16_text {u"Preview"}};
    core::observable<bool> backButton {false};
    core::observable<bool> paneToggle {false};
};
auto const model = gallery::hold<Model>();

auto example = Border {
    hAlign.stretch,
    vAlign.center,
    background = brushes.Card.BackgroundFillColor.Default,
    borderBrush = brushes.Card.StrokeColorDefault,
    borderThickness = Thickness {1},
    cornerRadius = CornerRadius {8},
    child = TitleBar {
        title = BindOutput {model->title},
        subtitle = BindOutput {model->subtitle},
        isBackButtonVisible = BindOutput {model->backButton},
        isPaneToggleButtonVisible = BindOutput {model->paneToggle},
        // TitleBar.Content uses Center alignment by default. Override to Stretch so
        // the search box can grow with the title bar.
        dsl::resources = ResourceDictionary {
            entry = Resource {u"TitleBarContentHorizontalAlignment", HorizontalAlignment::Stretch},
        },
        iconSource = ImageIconSource {imageSource = u"Assets/Tiles/GalleryIcon.ico"},
        rightHeader = PersonPicture {width = 30, height = 30, initials = u"JD"},
        content = AutoSuggestBox {
            maxWidth = 580.0,
            hAlign.stretch,
            vAlign.center,
            placeholderText = u"Search...",
            queryIcon = SymbolIcon {symbol = Symbol::Find},
        },
    },
};

auto options = StackPanel {
    width = 240,
    spacing = 12,
    TextBox {header = u"Title", text = Bind {model->title}},
    TextBox {header = u"Subtitle", text = Bind {model->subtitle}},
    ToggleSwitch {header = u"IsBackButtonVisible", isOn = Bind {model->backButton}},
    ToggleSwitch {header = u"IsPaneToggleButtonVisible", isOn = Bind {model->paneToggle}},
};
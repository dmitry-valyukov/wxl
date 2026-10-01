// A TitleBar over a NavigationView: the title bar asks for the pane and for the way back,
// the pane answers by choosing a page. The history of the pages is the model's, in place of
// the navigation frame of the original.
struct Model {
    std::vector<int> history;
    Border host;

    TitleBar titleBar {
        title = u"WinUI Gallery",
        subtitle = u"TitleBar sample",
        isBackButtonVisible = false,
        isPaneToggleButtonVisible = true,
        // Make TitleBar.Content stretch (default is Center).
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
        onBackRequested = [this] { goBack(); },
        onPaneToggleRequested = [this] { navView.isPaneOpen(!navView.isPaneOpen()); },
    };

    NavigationView navView {
        row = 1,
        isBackButtonVisible = NavigationViewBackButtonVisible::Collapsed,
        isPaneToggleButtonVisible = false,
        isSettingsVisible = false,
        menuItems[
            item(u"Menu Item1", u"1", Symbol::Play),
            item(u"Menu Item2", u"2", Symbol::Save),
            item(u"Menu Item3", u"3", Symbol::Refresh),
            item(u"Menu Item4", u"4", Symbol::Download)
        ],
        content = host,
        onSelectionChanged = [this](Object const& sender, NavigationViewSelectionChangedEventArgs&) {
            auto const selected = sender.try_as<NavigationView>().selectedItem().try_as<FrameworkElement>();
            if (selected) {
                show(selected.name()[0] - u'0', true);
            }
        },
    };

    static NavigationViewItem item(char16_t const* label, char16_t const* page, Symbol glyph) {
        return NavigationViewItem {content = label, name = page, icon = SymbolIcon {symbol = glyph}};
    }

    void show(int page, bool remember) {
        static constexpr char16_t const* titles[] = {u"Sample Page 1", u"Sample Page 2", u"Sample Page 3", u"Sample Page 4"};
        navView.header(Object::from_text(titles[page - 1]));
        host.child(gallery::samplePage(page));
        if (remember && (history.empty() || history.back() != page)) {
            history.push_back(page);
        }
        titleBar.isBackButtonVisible(history.size() > 1);
    }

    void goBack() {
        if (history.size() > 1) {
            history.pop_back();
            show(history.back(), false);
        }
    }
};

auto openWindow = [] {
    auto model = std::make_shared<Model>();
    Window window {
        systemBackdrop = MicaBackdrop {},
        extendsContentIntoTitleBar = true,
        content = Grid {rowDefinitions = u"auto,*", model->titleBar, model->navView},
    };
    window.appWindow().titleBar().preferredHeightOption(TitleBarHeightOption::Tall);
    window.setTitleBar(model->titleBar);
    window.appWindow().setIcon(u"Assets/Tiles/GalleryIcon.ico");
    model->show(1, true);
    gallery::trackWindow(window, model);
    window.activate();
};

auto example = StackPanel {
    maxWidth = 560,
    spacing = 12,
    TextBlock {
        hAlign.center,
        textAlignment = TextAlignment::Center,
        textWrapping = TextWrapping::WrapWholeWords,
        u"Click the button below to see an end to end sample of a TitleBar in an new window, binding some of its "
        u"properties to the NavigationView and navigation frame.",
    },
    Button {hAlign.center, styles.Button.Accent, content = u"Show window", onClick = [openWindow](Button const&) { openWindow(); }},
};
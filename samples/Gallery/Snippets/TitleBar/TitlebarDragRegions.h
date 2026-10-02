// Starting with WindowsAppSDK 2.1, TitleBar walks TitleBar.Content, auto-excludes
// interactive controls from the drag region, and lets non-interactive visuals (and empty
// space) remain draggable. TitleBar.IsDragRegion overrides the framework decision:
// true -> always draggable, false -> always clickable, unset -> the framework decides.
//
// The model is the window's own: the window keeps it until it is closed and every
// handler reaches the elements through it.
struct Model {
    TextBlock status {
        foreground = brushes.Text.FillColor.Secondary,
        textWrapping = TextWrapping::WrapWholeWords,
    };

    // Interactive Button. Its drag behaviour is controlled through TitleBar.IsDragRegion.
    Button statusBadge {
        vAlign.center,
        styles.Button.Accent,
        content = u"Status",
        // Only fires when IsDragRegion is not true; otherwise the framework consumes the
        // pointer for window dragging.
        onClick = [this](Button const&) { status.text(u"Status badge clicked"); },
    };
    Button extraButton {vAlign.center, content = u"Extra"};
    bool hasExtraButton = false;

    // Extra elements added at run time are inserted here, so they do not collide with
    // the status badge.
    StackPanel rightHeader {
        column = 1,
        orientation.horizontal,
        spacing = 8,
        vAlign.center,
        statusBadge,
    };

    TitleBar titleBar {
        title = u"Drag regions",
        // TitleBar.Content uses Center alignment by default, so a child with
        // HorizontalAlignment=Stretch will not actually grow. Override to Stretch to let
        // the search box fill the content area.
        dsl::resources = ResourceDictionary {
            entry = Resource {u"TitleBarContentHorizontalAlignment", HorizontalAlignment::Stretch},
        },
        iconSource = ImageIconSource {imageSource = u"Assets/Tiles/GalleryIcon.ico"},
        content = Grid {
            columnSpacing = 8,
            hAlign.stretch,
            columnDefinitions = u"*,auto",
            // Interactive control: auto-excluded from drag.
            AutoSuggestBox {
                maxWidth = 580.0,
                hAlign.stretch,
                vAlign.center,
                placeholderText = u"Search...",
                queryIcon = SymbolIcon {symbol = Symbol::Find},
            },
            rightHeader,
        },
    };

    FrameworkElement body() {
        return ScrollViewer {
            row = 1,
            Padding {32, 24},
            content = StackPanel {
                maxWidth = 640,
                spacing = 16,
                TextBlock {styles.TextBlock.Subtitle, u"Custom drag regions"},
                TextBlock {
                    textWrapping = TextWrapping::WrapWholeWords,
                    u"Try dragging the window from different parts of the title bar. Interactive controls (like the "
                    u"search box) are automatically excluded from the drag region by the new default behavior in "
                    u"Windows App SDK 2.1.",
                },
                TextBlock {styles.TextBlock.BodyStrong, u"Status badge: TitleBar.IsDragRegion"},
                TextBlock {
                    foreground = brushes.Text.FillColor.Secondary,
                    textWrapping = TextWrapping::WrapWholeWords,
                    u"Pick a value for the badge in the title bar.",
                },
                RadioButtons {
                    RadioButton {content = u"Unset (framework decides \u2014 clickable, since Button is interactive)"},
                    RadioButton {content = u"True (always draggable \u2014 overrides the framework default)"},
                    RadioButton {content = u"False (always clickable)"},
                    selectedIndex = 0,
                    onSelectionChanged = [this](RadioButtons const& self) {
                        switch (self.selectedIndex()) {
                            case 0: statusBadge.isDragRegion(core::nullable<bool> {}); break;
                            case 1: statusBadge.isDragRegion(true); break;
                            case 2: statusBadge.isDragRegion(false); break;
                        }
                    },
                },
                TextBlock {styles.TextBlock.BodyStrong, u"Dynamic content"},
                TextBlock {
                    foreground = brushes.Text.FillColor.Secondary,
                    textWrapping = TextWrapping::WrapWholeWords,
                    u"When you add or remove elements in TitleBar.Content at runtime, call RecomputeDragRegions() to "
                    u"refresh.",
                },
                StackPanel {
                    orientation.horizontal,
                    spacing = 8,
                    Button {
                        content = u"Toggle extra title bar button",
                        onClick = [this](Button const&) {
                            if (!hasExtraButton) {
                                rightHeader.children().insertAt(0, extraButton);
                                status.text(u"Added a Button to TitleBar.Content. Call RecomputeDragRegions() to refresh drag regions.");
                            } else {
                                rightHeader.children().removeAt(0);
                                status.text(u"Removed the Button. Call RecomputeDragRegions() to refresh drag regions.");
                            }
                            hasExtraButton = !hasExtraButton;
                        },
                    },
                    Button {
                        content = u"RecomputeDragRegions()",
                        onClick = [this](Button const&) {
                            titleBar.recomputeDragRegions();
                            status.text(u"RecomputeDragRegions() called.");
                        },
                    },
                },
                status,
            },
        };
    }
};

auto openWindow = [] {
    auto model = std::make_shared<Model>();
    Window window {
        systemBackdrop = MicaBackdrop {},
        extendsContentIntoTitleBar = true,
        content = Grid {rowDefinitions = u"auto,*", model->titleBar, model->body()},
    };
    window.appWindow().titleBar().preferredHeightOption(TitleBarHeightOption::Tall);
    window.setTitleBar(model->titleBar);
    window.appWindow().setIcon(u"Assets/Tiles/GalleryIcon.ico");
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
        u"Drag regions can only be observed on a real window. Click the button below to open a sample window where "
        u"you can toggle TitleBar.IsDragRegion on a status badge and call RecomputeDragRegions() after dynamic content "
        u"changes.",
    },
    Button {hAlign.center, styles.Button.Accent, content = u"Show window", onClick = [openWindow](Button const&) { openWindow(); }},
};
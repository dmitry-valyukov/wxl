// A backdrop is a property of the window, and the kinds of it are objects: Mica (base or
// alt), desktop acrylic, or none.
struct Model {
    enum class Backdrop { None, Mica, MicaAlt, Acrylic };

    TextBlock status {hAlign.center};
    ComboBox backdropBox {
        hAlign.stretch,
        header = u"Current backdrop ",
        ComboBoxItem {content = u"Mica"},
        ComboBoxItem {content = u"MicaAlt"},
        ComboBoxItem {content = u"Acrylic"},
        ComboBoxItem {content = u"None"},
        selectedIndex = 0,
    };
    ComboBox themeBox {
        hAlign.stretch,
        header = u"Window theme ",
        ComboBoxItem {content = u"Use system setting"},
        ComboBoxItem {content = u"Light"},
        ComboBoxItem {content = u"Dark"},
        selectedIndex = 0,
    };
    Grid root {
        rowDefinitions = u"32,*",
        TitleBar {
            title = u"SystemBackdrop sample window",
            iconSource = ImageIconSource {imageSource = u"Assets/Tiles/BadgeLogo.png"},
        },
        StackPanel {row = 1, hAlign.center, vAlign.center, spacing = 20, status, backdropBox, themeBox},
    };
    Window window {systemBackdrop = MicaBackdrop {}, extendsContentIntoTitleBar = true, content = root};
    Backdrop current = Backdrop::Mica;

    Model() {
        window.appWindow().setIcon(u"Assets/Tiles/GalleryIcon.ico");
        backdropBox.add_onSelectionChanged([this](auto&&...) {
            static constexpr Backdrop kinds[] = {Backdrop::Mica, Backdrop::MicaAlt, Backdrop::Acrylic, Backdrop::None};
            setBackdrop(kinds[std::clamp(backdropBox.selectedIndex(), 0, 3)]);
        });
        themeBox.add_onSelectionChanged([this](auto&&...) {
            static constexpr ElementTheme themes[] = {ElementTheme::Default, ElementTheme::Light, ElementTheme::Dark};
            root.requestedTheme(themes[std::clamp(themeBox.selectedIndex(), 0, 2)]);
            setNoneBackground();
        });
    }

    void setBackdrop(Backdrop type) {
        // Reset to the default colour. If the requested kind is supported, it is set.
        current = Backdrop::None;
        std::u16string note;
        window.noSystemBackdrop(true);

        if (type == Backdrop::Mica || type == Backdrop::MicaAlt) {
            if (MicaController::isSupported()) {
                window.systemBackdrop(
                    MicaBackdrop {kind = type == Backdrop::MicaAlt ? MicaKind::BaseAlt : MicaKind::Base});
                current = type;
            } else {
                // Mica isn't supported. Try Acrylic.
                note += type == Backdrop::Mica ? u"  Mica isn't supported. Trying Acrylic."
                                               : u"  MicaAlt isn't supported. Trying Acrylic.";
                type = Backdrop::Acrylic;
            }
        }
        if (type == Backdrop::Acrylic) {
            if (DesktopAcrylicController::isSupported()) {
                window.systemBackdrop(DesktopAcrylicBackdrop {});
                current = type;
            } else {
                // Acrylic isn't supported, so take the next option, which is the default colour.
                note += u"  Acrylic isn't supported. Switching to default color.";
            }
        }
        status.text(note);
        setNoneBackground();
    }

    // Fixes the background colour not changing when switching between themes.
    void setNoneBackground() {
        if (current == Backdrop::None && themeBox.selectedIndex() != 0) {
            root.background(SolidColorBrush {color = themeBox.selectedIndex() == 1 ? colors.white : colors.black});
        } else {
            root.background(SolidColorBrush {color = colors.transparent});
        }
    }
};

auto example = StackPanel {
    RichTextBlock {
        Paragraph {
            Run {u"A window can use one of the following system backdrops:"},
            LineBreak {},
            Run {u"1. "},
            Run {u"Mica", FontWeight {700}},
            Run {u" — An opaque material that samples the desktop wallpaper once to tint the window background. Best for main app windows."},
            LineBreak {},
            Run {u"2. "},
            Run {u"Mica Alt", FontWeight {700}},
            Run {u" — A variant of Mica with stronger tinting. Recommended for apps with a tabbed title bar."},
            LineBreak {},
            Run {u"3. "},
            Run {u"Desktop Acrylic (Base)", FontWeight {700}},
            Run {u" — A semi-transparent material that shows a blurred view of the content behind the window."},
            LineBreak {},
            Run {u"4. "},
            Run {u"Desktop Acrylic (Thin)", FontWeight {700}},
            Run {u" — A lighter variant of Desktop Acrylic with more transparency."},
            LineBreak {},
            LineBreak {},
            Run {u"Mica vs. Acrylic:", FontWeight {700}},
            Run {u" Mica is opaque and renders the desktop wallpaper within the window background. Desktop Acrylic is "
                 u"semi-transparent and reveals a blurred view of what is behind the window in real-time. Mica is more "
                 u"performant because it captures the wallpaper only once, while Acrylic updates continuously."},
            LineBreak {},
            LineBreak {},
            Run {u"There are three backdrop types in the API:"},
            LineBreak {},
            Run {u"• "},
            Run {u"SystemBackdrop", FontWeight {700}},
            Run {u" — The base class of every backdrop type."},
            LineBreak {},
            Run {u"• "},
            Run {u"MicaBackdrop", FontWeight {700}},
            Run {u" — Applies the Mica material. Set the Kind property to switch between Base and Alt."},
            LineBreak {},
            Run {u"• "},
            Run {u"DesktopAcrylicBackdrop", FontWeight {700}},
            Run {u" — Applies the Desktop Acrylic material (Base type only)."},
            LineBreak {},
            LineBreak {},
            Run {u"All Mica variants require Windows 11 build 22000 or later. In-app acrylic (AcrylicBrush) is a separate "
                 u"XAML brush used within UI elements, not a window backdrop."},
        },
    },
    Button {
        Margin {0, 10, 0, 0},
        content = u"Show window",
        onClick = [](Button const&) {
            auto model = std::make_shared<Model>();
            gallery::trackWindow(model->window, model);
            model->window.activate();
        },
    },
};
